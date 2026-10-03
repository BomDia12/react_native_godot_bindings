#!/usr/bin/env python3
import argparse
import asyncio
import base64
import gzip
import json
import signal
import ssl
import threading
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import parse_qs, urlsplit

from websockets.asyncio.server import serve


def load_config(path, repo_root):
    config = json.loads(path.read_text(encoding="utf-8"))
    if not isinstance(config, dict) or set(config) - {"routes", "websocket_messages", "tls"}:
        raise ValueError("unknown fixture configuration keys")
    routes = config.get("routes", {})
    if not isinstance(routes, dict) or len(routes) > 128:
        raise ValueError("fixture routes must be a bounded object")
    for name, route in routes.items():
        if not isinstance(name, str) or not name.startswith("/") or not isinstance(route, dict) or set(route) - {"json", "asset"} or len(route) != 1:
            raise ValueError("invalid fixture route")
        if "asset" in route:
            asset = (repo_root / route["asset"]).resolve()
            asset.relative_to(repo_root.resolve())
            if not asset.is_file() or asset.stat().st_size > 8 * 1024 * 1024:
                raise ValueError("fixture asset unavailable or oversized")
    return config


async def run(config, repo_root, records):
    stop = asyncio.Event()
    loop = asyncio.get_running_loop()
    for number in (signal.SIGTERM, signal.SIGINT):
        loop.add_signal_handler(number, stop.set)
    records_lock = threading.Lock()
    def record(value):
        with records_lock:
            with records.open("a", encoding="utf-8") as output:
                output.write(json.dumps(value) + "\n")
    class Handler(BaseHTTPRequestHandler):
        protocol_version = "HTTP/1.1"
        def log_message(self, *_args):
            pass
        def do_GET(self):
            self.respond()
        def do_POST(self):
            self.respond()
        def do_PUT(self):
            self.respond()
        def respond(self):
            length = int(self.headers.get("Content-Length", 0))
            if not 0 <= length <= 32 * 1024 * 1024:
                self.send_error(413)
                return
            body = self.rfile.read(length)
            parsed = urlsplit(self.path)
            record({"path": parsed.path, "method": self.command, "headers": dict(self.headers), "body": base64.b64encode(body).decode()})
            status = 200
            headers = []
            content = b"fixture"
            if parsed.path in config.get("routes", {}):
                route = config["routes"][parsed.path]
                if "json" in route:
                    content = json.dumps(route["json"]).encode()
                    headers.append(("Content-Type", "application/json"))
                else:
                    content = (repo_root / route["asset"]).read_bytes()
                    headers.append(("Content-Type", "image/png"))
            elif parsed.path == "/echo":
                content = body
            elif parsed.path == "/binary":
                content = b"A\x00B\xff"
            elif parsed.path.startswith("/status/"):
                status = int(parsed.path.rsplit("/", 1)[-1])
                content = b"status"
            elif parsed.path == "/compressed":
                content = gzip.compress(b"compressed\x00payload")
                headers.append(("Content-Encoding", "gzip"))
            elif parsed.path == "/redirect":
                status = 302
                headers.extend([("Location", "/cookies"), ("Set-Cookie", "redirect=seen; Path=/; HttpOnly")])
            elif parsed.path == "/set-cookies":
                headers.extend([("Set-Cookie", "a=one; Path=/"), ("Set-Cookie", "b=two; Path=/; Max-Age=60")])
            elif parsed.path == "/cookies":
                content = self.headers.get("Cookie", "").encode()
            elif parsed.path == "/slow":
                delay = min(3.0, max(0.0, float(parse_qs(parsed.query).get("seconds", ["1"])[0])))
                threading.Event().wait(delay)
            elif parsed.path == "/large":
                content = b"x" * min(33 * 1024 * 1024, int(parse_qs(parsed.query).get("bytes", ["1024"])[0]))
            self.send_response(status)
            for key, value in headers:
                self.send_header(key, value)
            self.send_header("Content-Length", str(len(content)))
            self.end_headers()
            try:
                self.wfile.write(content)
            except (BrokenPipeError, ConnectionResetError):
                pass
    http = ThreadingHTTPServer(("127.0.0.1", 0), Handler)
    http.daemon_threads = True
    http_thread = threading.Thread(target=http.serve_forever, daemon=True)
    http_thread.start()
    async def websocket(socket):
        record({"websocket": "connected", "protocol": socket.subprotocol})
        async for message in socket:
            record({"websocket": "received", "binary": isinstance(message, bytes)})
            if message == "updates":
                for value in config.get("websocket_messages", []):
                    await socket.send(json.dumps(value))
            elif message == "flood":
                for start in range(0, 300, 32):
                    for index in range(start, min(start + 32, 300)):
                        await socket.send(str(index))
                    if min(start + 32, 300) < 300:
                        if await socket.recv() != "ack":
                            raise ValueError("flood acknowledgement missing")
            elif message == "unclean":
                socket.transport.abort()
                return
            else:
                await socket.send(message)
    https = None
    try:
        async with serve(websocket, "127.0.0.1", 0, subprotocols=["fixture"], select_subprotocol=lambda connection, protocols: "fixture" if "fixture" in protocols else None, max_size=1024*1024) as ws:
            readiness = {"http": f"http://127.0.0.1:{http.server_port}", "websocket": f"ws://127.0.0.1:{ws.sockets[0].getsockname()[1]}", "records": str(records)}
            if "tls" in config:
                tls = config["tls"]
                if set(tls) != {"certificate", "key", "ca"}:
                    raise ValueError("invalid TLS fixture keys")
                files = {}
                for name, value in tls.items():
                    files[name] = (repo_root / value).resolve()
                    files[name].relative_to(repo_root.resolve())
                context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
                context.load_cert_chain(files["certificate"], files["key"])
                https = ThreadingHTTPServer(("127.0.0.1", 0), Handler)
                https.daemon_threads = True
                https.socket = context.wrap_socket(https.socket, server_side=True)
                threading.Thread(target=https.serve_forever, daemon=True).start()
                readiness.update({"https": f"https://localhost:{https.server_port}", "ca": str(files["ca"])})
            print(json.dumps(readiness), flush=True)
            await stop.wait()
    finally:
        if https:
            https.shutdown()
            https.server_close()
        http.shutdown()
        http.server_close()
        http_thread.join(timeout=5)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--config", type=Path, required=True)
    parser.add_argument("--repo-root", type=Path, required=True)
    parser.add_argument("--records", type=Path, required=True)
    args = parser.parse_args()
    asyncio.run(run(load_config(args.config, args.repo_root), args.repo_root, args.records))


if __name__ == "__main__":
    main()
