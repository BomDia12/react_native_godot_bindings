#!/usr/bin/env python3
import json
import os
import re
import selectors
import subprocess
import sys
import time
from pathlib import Path


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
