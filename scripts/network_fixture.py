#!/usr/bin/env python3
import json
import os
import re
import selectors
import shutil
import signal
import socket
import subprocess
import sys
import tempfile
import time
from pathlib import Path
from urllib.error import URLError
from urllib.request import urlopen


class LocalNetworkFixture:
    def __init__(self, repo_root: Path, config: Path, artifact_dir: Path):
        self.repo_root = repo_root.resolve()
        self.config = config
        self.artifact_dir = artifact_dir
        self.process = None
        self.readiness_path = artifact_dir / "readiness.json"
        self.records_path = artifact_dir / "requests.jsonl"
        self.log = None

    def __enter__(self):
        self.artifact_dir.mkdir(parents=True, exist_ok=True)
        self.records_path.write_text("", encoding="utf-8")
        self.log = (self.artifact_dir / "server.log").open("wb")
        try:
            self.process = subprocess.Popen(
                [os.environ.get("SMOKE_FIXTURE_PYTHON", sys.executable), str(self.repo_root / "scripts/local_network_fixture.py"),
                 "--config", str(self.repo_root / self.config), "--repo-root", str(self.repo_root), "--records", str(self.records_path)],
                stdout=subprocess.PIPE, stderr=self.log,
            )
            deadline = time.monotonic() + 10
            record = bytearray()
            with selectors.DefaultSelector() as selector:
                selector.register(self.process.stdout, selectors.EVENT_READ)
                while b"\n" not in record:
                    remaining = deadline - time.monotonic()
                    if remaining <= 0 or not selector.select(remaining):
                        raise RuntimeError("network fixture readiness timed out")
                    chunk = os.read(self.process.stdout.fileno(), 4097 - len(record))
                    if not chunk:
                        raise RuntimeError("network fixture exited before readiness; see server.log")
                    record.extend(chunk)
                    if len(record) > 4096:
                        raise RuntimeError("network fixture readiness exceeds bound")
            readiness = json.loads(record.split(b"\n", 1)[0])
            self.validate_readiness(readiness)
            self.readiness_path.write_text(json.dumps(readiness), encoding="utf-8")
            return self
        except BaseException:
            self.close()
            raise

    def validate_readiness(self, readiness):
        if not isinstance(readiness, dict) or set(readiness) - {"http", "websocket", "https", "ca", "records"} or not {"http", "websocket", "records"} <= set(readiness):
            raise RuntimeError("malformed network fixture readiness")
        for name, scheme in (("http", "http"), ("websocket", "ws"), ("https", "https")):
            if name in readiness and (not isinstance(readiness[name], str) or re.fullmatch(scheme + r"://(?:127\.0\.0\.1|localhost):[0-9]{1,5}", readiness[name]) is None):
                raise RuntimeError("fixture endpoints must use ephemeral localhost ports")
        if readiness["records"] != str(self.records_path):
            raise RuntimeError("fixture records path does not match runner")
        if ("ca" in readiness) != ("https" in readiness):
            raise RuntimeError("TLS readiness requires explicit CA")
        if "ca" in readiness:
            try:
                Path(readiness["ca"]).resolve().relative_to(self.repo_root)
            except (TypeError, ValueError) as error:
                raise RuntimeError("fixture CA escapes repository") from error

    def close(self):
        if self.process:
            if self.process.poll() is None:
                self.process.terminate()
                try:
                    self.process.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    self.process.kill()
                    self.process.wait(timeout=5)
            self.process.stdout.close()
        if self.log:
            self.log.close()

    def __exit__(self, *_args):
        self.close()


class MetroFixture:
    """Serves a copy of a Metro app through the react-native-godot start script,
    so a smoke test can edit the copy while Metro serves it."""

    STARTUP_SECONDS = 60
    BUNDLE_SECONDS = 180

    def __init__(self, repo_root: Path, config: Path, artifact_dir: Path):
        self.repo_root = repo_root.resolve()
        self.config = config
        self.artifact_dir = artifact_dir
        self.process = None
        self.app_copy = None
        self.readiness_path = artifact_dir / "readiness.json"
        self.log = None

    def __enter__(self):
        settings = json.loads((self.repo_root / self.config).read_text(encoding="utf-8"))
        project = self.repo_root / settings["project"]
        self.artifact_dir.mkdir(parents=True, exist_ok=True)
        self.log = (self.artifact_dir / "server.log").open("wb")
        try:
            # Metro's watcher skips dot-directories, so the copy has a plain name.
            self.app_copy = Path(tempfile.mkdtemp(prefix="dev-server-", dir=project))
            shutil.copytree(self.repo_root / settings["app"], self.app_copy, dirs_exist_ok=True)
            port = self.free_port()
            self.process = subprocess.Popen(
                ["node", str(self.repo_root / "packages/react-native-godot/scripts/start.cjs"), "--port", str(port), "--host", "127.0.0.1"],
                cwd=project, stdin=subprocess.DEVNULL, stdout=self.log, stderr=subprocess.STDOUT, start_new_session=True,
            )
            url = f"http://127.0.0.1:{port}"
            entry = f"{self.app_copy.name}/{settings['entry']}"
            self.wait_until_serving(url)
            # Metro builds the bundle once here, so the test's own download is quick.
            with urlopen(f"{url}/{entry}.bundle?platform=godot&dev=true&lazy=true&minify=false&modulesOnly=false&runModule=true&inlineSourceMap=false&excludeSource=true&sourcePaths=url-server", timeout=self.BUNDLE_SECONDS) as response:
                response.read()
            readiness = {"metro": url, "host": "127.0.0.1", "port": port, "entry": entry, "app": str(self.app_copy)}
            self.readiness_path.write_text(json.dumps(readiness), encoding="utf-8")
            return self
        except BaseException:
            self.close()
            raise

    @staticmethod
    def free_port() -> int:
        with socket.socket() as probe:
            probe.bind(("127.0.0.1", 0))
            return probe.getsockname()[1]

    def wait_until_serving(self, url: str):
        deadline = time.monotonic() + self.STARTUP_SECONDS
        while time.monotonic() < deadline:
            if self.process.poll() is not None:
                raise RuntimeError("Metro exited before it served requests; see server.log")
            try:
                with urlopen(url + "/json/list", timeout=2):
                    return
            except (URLError, OSError):
                time.sleep(0.2)
        raise RuntimeError("Metro did not start serving in time; see server.log")

    def close(self):
        if self.process and self.process.poll() is None:
            os.killpg(self.process.pid, signal.SIGTERM)
            try:
                self.process.wait(timeout=10)
            except subprocess.TimeoutExpired:
                os.killpg(self.process.pid, signal.SIGKILL)
                self.process.wait(timeout=5)
        if self.app_copy:
            shutil.rmtree(self.app_copy, ignore_errors=True)
        if self.log:
            self.log.close()

    def __exit__(self, *_args):
        self.close()


FIXTURES = {"local_network": LocalNetworkFixture, "metro": MetroFixture}
