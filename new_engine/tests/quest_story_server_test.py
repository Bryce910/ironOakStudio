#!/usr/bin/env python3
"""Exercise the public, read-only Quest & Story Studio HTTP contract.

Runs an ephemeral loopback server in-process. No browser, game project, or
user saves are opened or modified.
"""
from contextlib import contextmanager
import hashlib
from http.client import HTTPConnection
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import importlib.util
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import threading
import unittest
from unittest.mock import patch


ENGINE = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location(
    "quest_story_editor_server", ENGINE / "tools" / "quest_story_editor.py")
STUDIO = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(STUDIO)


@contextmanager
def serving(handler=None):
    server = ThreadingHTTPServer(("127.0.0.1", 0), handler or STUDIO.Handler)
    thread = threading.Thread(target=server.serve_forever, daemon=True)
    thread.start()
    try:
        yield server.server_address[1]
    finally:
        server.shutdown()
        server.server_close()
        thread.join(timeout=2)
        if thread.is_alive():
            raise AssertionError("HTTP server did not shut down")


def request(port, target, method="GET", body=None):
    connection = HTTPConnection("127.0.0.1", port, timeout=3)
    try:
        connection.request(method, target, body=body)
        response = connection.getresponse()
        return response.status, dict(response.getheaders()), response.read()
    finally:
        connection.close()


def fingerprints():
    return {path.name: hashlib.sha256(path.read_bytes()).hexdigest()
            for path in STUDIO.ROOT.iterdir() if path.is_file()}


class StudioHTTPTests(unittest.TestCase):
    def test_known_resources_and_document_security_headers(self):
        resources = {
            "/": ("index.html", "text/html"),
            "/index.html": ("index.html", "text/html"),
            "/style.css": ("style.css", "text/css"),
            "/app.mjs": ("app.mjs", "text/javascript"),
            "/story.mjs": ("story.mjs", "text/javascript"),
        }
        with serving() as port:
            for target, (filename, mime) in resources.items():
                with self.subTest(target=target):
                    status, headers, body = request(port, target)
                    self.assertEqual(status, 200)
                    self.assertEqual(body, (STUDIO.ROOT / filename).read_bytes())
                    self.assertTrue(headers["Content-Type"].startswith(mime))
                    self.assertEqual(int(headers["Content-Length"]), len(body))
                    self.assertEqual(headers["Cache-Control"], "no-store")
                    self.assertEqual(headers["X-Content-Type-Options"], "nosniff")
                    policy = headers["Content-Security-Policy"]
                    for directive in ("default-src 'self'", "script-src 'self'",
                                      "connect-src 'none'", "object-src 'none'",
                                      "base-uri 'none'", "frame-ancestors 'none'"):
                        self.assertIn(directive, policy)
                    self.assertNotIn("'unsafe-eval'", policy)
                    self.assertNotIn("http:", policy)
                    self.assertNotIn("https:", policy)
            self.assertEqual(request(port, "/index.html?cache=ignored")[2],
                             (STUDIO.ROOT / "index.html").read_bytes())

    def test_health_identity_and_existing_instance_probe(self):
        with serving() as port:
            status, headers, body = request(port, "/health")
            self.assertEqual(status, 200)
            self.assertTrue(headers["Content-Type"].startswith("application/json"))
            health = json.loads(body)
            self.assertEqual(health["app"], "valwood-quest-story-studio-v1")
            self.assertGreater(health["pid"], 0)
            self.assertEqual(STUDIO.health(port), health)

        # Another service on a local port must never be mistaken for this app.
        class ForeignService(BaseHTTPRequestHandler):
            def do_GET(self):
                body = b'{"app":"some-other-application","pid":1}'
                self.send_response(200)
                self.send_header("Content-Length", str(len(body)))
                self.end_headers()
                self.wfile.write(body)
            def log_message(self, *args):
                pass
        with serving(ForeignService) as port:
            self.assertIsNone(STUDIO.health(port))

    def test_cli_reuses_existing_instance_and_rejects_invalid_ports(self):
        script = ENGINE / "tools" / "quest_story_editor.py"
        with serving() as port:
            process = subprocess.run(
                [sys.executable, "-B", str(script), "--no-browser", "--port", str(port)],
                text=True, capture_output=True, timeout=5,
            )
            self.assertEqual(process.returncode, 0, process.stderr)
            info = json.loads(process.stdout)
            self.assertEqual(info["url"], f"http://127.0.0.1:{port}/")
            self.assertEqual(info["pid"], json.loads(request(port, "/health")[2])["pid"])
        for invalid in ("0", "1023", "65526", "99999", "not-a-port"):
            process = subprocess.run(
                [sys.executable, "-B", str(script), "--serve", "--port", invalid],
                text=True, capture_output=True, timeout=3,
            )
            with self.subTest(port=invalid):
                self.assertEqual(process.returncode, 2)
                self.assertIn("error:", process.stderr)

    def test_unknown_traversal_and_private_files_never_served(self):
        paths = (
            "/missing", "/tools/", "/dist/", "/../quest_story_editor.py",
            "/%2e%2e/quest_story_editor.py", "/..%2fquest_story_editor.py",
            "/%2e%2e%2f%2e%2e%2fetc/passwd", "/etc/passwd",
            "/index.html/../story.mjs", "/index.html/", "/.git/config",
            "/.env", "/%00", "/story.mjs%00", "/app.mjs.map",
            "/memory.mjs", "/health/", "/%2fhealth", "/%5c..%5c.env",
        )
        with serving() as port:
            for target in paths:
                with self.subTest(target=target):
                    status, _, body = request(port, target)
                    self.assertEqual(status, 404)
                    self.assertNotIn(b"root:x:", body)
                    self.assertNotIn(b"#!/usr/bin/env python", body)
                    self.assertNotIn(b"Index of", body)

    def test_no_mutating_or_install_api_and_assets_preserved(self):
        before = fingerprints()
        with serving() as port:
            for method in ("POST", "PUT", "PATCH", "DELETE", "OPTIONS", "CONNECT"):
                for target in ("/", "/health", "/app.mjs", "/install", "/save"):
                    with self.subTest(method=method, target=target):
                        status, _, _ = request(port, target, method, b'{"path":"../../saves","data":"test"}')
                        self.assertIn(status, (404, 405, 501))
            self.assertEqual(request(port, "/")[0], 200)
        self.assertEqual(fingerprints(), before)

    def test_missing_bundled_resource_is_clean_unavailable_response(self):
        with tempfile.TemporaryDirectory(prefix="quest-story-missing-") as folder:
            with patch.object(STUDIO, "ROOT", Path(folder)):
                with serving() as port:
                    status, _, body = request(port, "/story.mjs")
                    self.assertEqual(status, 503)
                    self.assertNotIn(folder.encode(), body)
                    self.assertEqual(request(port, "/health")[0], 200)

    def test_symlink_cannot_expose_outside_resource(self):
        with tempfile.TemporaryDirectory(prefix="quest-story-link-") as folder:
            root = Path(folder) / "dist"
            root.mkdir()
            private = Path(folder) / "private.txt"
            private.write_text("private content must not be served")
            (root / "story.mjs").symlink_to(private)
            with patch.object(STUDIO, "ROOT", root):
                with serving() as port:
                    status, _, body = request(port, "/story.mjs")
                    self.assertEqual(status, 503)
                    self.assertNotIn(private.read_bytes(), body)


if __name__ == "__main__":
    unittest.main(verbosity=2)
