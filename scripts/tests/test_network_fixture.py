import json
import sys
import tempfile
import unittest
from pathlib import Path
from urllib.request import urlopen

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from network_fixture import LocalNetworkFixture


class NetworkFixtureTests(unittest.TestCase):
    def test_ephemeral_ports_records_and_cleanup_on_failure(self):
        root = Path(__file__).resolve().parents[2]
        config = Path("samples/game-ui/fixtures/network.json")
        with tempfile.TemporaryDirectory() as directory:
            first = LocalNetworkFixture(root, config, Path(directory) / "first")
            second = LocalNetworkFixture(root, config, Path(directory) / "second")
            with self.assertRaisesRegex(ValueError, "test failure"):
                with first, second:
                    a = json.loads(first.readiness_path.read_text())
                    b = json.loads(second.readiness_path.read_text())
                    self.assertNotEqual(a["http"], b["http"])
                    self.assertNotEqual(a["websocket"], b["websocket"])
                    with urlopen(a["http"] + "/binary", timeout=5) as response:
                        self.assertEqual(response.read(), b"A\x00B\xff")
                    self.assertEqual(len(first.records_path.read_text().splitlines()), 1)
                    raise ValueError("test failure")
            self.assertIsNotNone(first.process.poll())
            self.assertIsNotNone(second.process.poll())

    def test_malformed_readiness_and_startup_failure_cleanup(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            scripts = root / "scripts"
            scripts.mkdir()
            entry = scripts / "local_network_fixture.py"
            entry.write_text("print('{}', flush=True)\n", encoding="utf-8")
            fixture = LocalNetworkFixture(root, Path("unused.json"), root / "artifacts")
            with self.assertRaisesRegex(RuntimeError, "malformed"):
                with fixture:
                    self.fail("invalid readiness accepted")
            self.assertIsNotNone(fixture.process.poll())
            entry.write_text("raise RuntimeError('startup failure')\n", encoding="utf-8")
            fixture = LocalNetworkFixture(root, Path("unused.json"), root / "failure")
            with self.assertRaisesRegex(RuntimeError, "before readiness"):
                with fixture:
                    self.fail("failed fixture accepted")
            self.assertIsNotNone(fixture.process.poll())

    def test_readiness_rejects_external_endpoints_and_escaping_ca(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            fixture = LocalNetworkFixture(root, Path("unused"), root / "artifacts")
            value = {"http": "http://example.com:80", "websocket": "ws://127.0.0.1:9000", "records": str(fixture.records_path)}
            with self.assertRaisesRegex(RuntimeError, "localhost"):
                fixture.validate_readiness(value)
            value["http"] = "http://127.0.0.1:9001"
            value.update({"https": "https://localhost:9002", "ca": "/outside.crt"})
            with self.assertRaisesRegex(RuntimeError, "escapes"):
                fixture.validate_readiness(value)
