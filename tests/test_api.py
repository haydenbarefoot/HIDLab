
"""HTTP API integration tests for the HIDLab simulator."""

import json
import sys
import threading
import unittest
from pathlib import Path
from urllib.error import HTTPError
from urllib.request import Request, urlopen

PROJECT_ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(PROJECT_ROOT))

from simulator import main as backend


class TestHIDLabAPI(unittest.TestCase):
    """Verify API behavior using real HTTP requests."""

    @classmethod
    def setUpClass(cls):
        """Start an isolated HTTP server on an available local port."""
        cls.server = backend.ThreadingHTTPServer(
            ("127.0.0.1", 0),
            backend.Handler,
        )

        cls.base_url = (
            f"http://127.0.0.1:{cls.server.server_address[1]}"
        )

        cls.server_thread = threading.Thread(
            target=cls.server.serve_forever,
            daemon=True,
        )
        cls.server_thread.start()

    @classmethod
    def tearDownClass(cls):
        """Shut down the test server and release its socket."""
        cls.server.shutdown()
        cls.server.server_close()
        cls.server_thread.join(timeout=5)

    def setUp(self):
        """Reset backend state before each test."""
        with backend.lock:
            backend.queue.clear()
            backend.current_text = None
            backend.position = 0
            backend.state = "idle"

    def request(self, path, payload=None):
        """
        Send an HTTP request and return its status and JSON response.

        A missing payload produces a GET request. Otherwise, POST
        is used. HTTP errors are returned for test assertions.
        """
        body = None
        headers = {}

        if payload is not None:
            body = json.dumps(payload).encode("utf-8")
            headers["Content-Type"] = "application/json"

        request = Request(
            self.base_url + path,
            data=body,
            headers=headers,
        )

        try:
            response = urlopen(request, timeout=5)
        except HTTPError as error:
            response = error

        with response:
            status = response.status
            content = response.read().decode("utf-8")

        return status, json.loads(content)

    def action(self, action, **kwargs):
        """Submit a simulator command through the HTTP API."""
        return self.request(
            "/api/action",
            {"action": action, **kwargs},
        )

    def test_homepage_loads(self):
        """GET / should serve the HIDLab webpage."""
        with urlopen(self.base_url + "/", timeout=5) as response:
            html = response.read().decode("utf-8")

            self.assertEqual(response.status, 200)
            self.assertIn("<title>HIDLab</title>", html)

    def test_initial_status(self):
        status, data = self.request("/api/status")

        self.assertEqual(status, 200)
        self.assertEqual(data["status"], "idle")
        self.assertEqual(data["queued"], 0)
        self.assertEqual(data["passages"], [])

    def test_add_passage(self):
        status, result = self.action("add", text="Hello HIDLab")

        self.assertEqual(status, 200)
        self.assertTrue(result["ok"])

        _, data = self.request("/api/status")

        self.assertEqual(data["queued"], 1)
        self.assertEqual(data["passages"], ["Hello HIDLab"])

    def test_reject_empty_passage(self):
        status, result = self.action("add", text="   ")

        self.assertEqual(status, 400)
        self.assertFalse(result["ok"])

    def test_reject_oversized_passage(self):
        oversized = "A" * (backend.MAX_CHARACTERS + 1)

        status, result = self.action("add", text=oversized)

        self.assertEqual(status, 400)
        self.assertFalse(result["ok"])

    def test_queue_capacity(self):
        self.action("add", text="First")
        self.action("add", text="Second")

        status, result = self.action("add", text="Third")

        self.assertEqual(status, 400)
        self.assertFalse(result["ok"])

        _, data = self.request("/api/status")
        self.assertEqual(data["queued"], 2)

    def test_remove_passage(self):
        self.action("add", text="First")
        self.action("add", text="Second")

        status, result = self.action("remove", index=0)

        self.assertEqual(status, 200)
        self.assertTrue(result["ok"])

        _, data = self.request("/api/status")
        self.assertEqual(data["passages"], ["Second"])

    def test_edit_passage(self):
        self.action("add", text="Original")

        status, result = self.action(
            "edit",
            index=0,
            text="Updated",
        )

        self.assertEqual(status, 200)
        self.assertTrue(result["ok"])

        _, data = self.request("/api/status")
        self.assertEqual(data["passages"], ["Updated"])

    def test_reorder_passages(self):
        self.action("add", text="First")
        self.action("add", text="Second")

        status, result = self.action(
            "move",
            index=1,
            direction="up",
        )

        self.assertEqual(status, 200)
        self.assertTrue(result["ok"])

        _, data = self.request("/api/status")

        self.assertEqual(
            data["passages"],
            ["Second", "First"],
        )

    def test_reject_invalid_queue_index(self):
        self.action("add", text="First")

        status, result = self.action(
            "remove",
            index=99,
        )

        self.assertEqual(status, 400)
        self.assertFalse(result["ok"])

    def test_playback_controls(self):
        """Verify the Start, Pause, Resume, Stop API workflow."""
        self.action("add", text="Test passage")

        for command, expected_state in [
            ("start", "typing"),
            ("pause", "paused"),
            ("resume", "typing"),
            ("stop", "idle"),
        ]:
            status, result = self.action(command)

            self.assertEqual(status, 200)
            self.assertTrue(result["ok"])

            _, data = self.request("/api/status")
            self.assertEqual(data["status"], expected_state)

        # Stopping must restore the original passage.
        _, data = self.request("/api/status")
        self.assertEqual(data["passages"], ["Test passage"])

    def test_reject_invalid_playback_command(self):
        status, result = self.action("resume")

        self.assertEqual(status, 400)
        self.assertFalse(result["ok"])

    def test_reject_unknown_command(self):
        status, result = self.action("unknown")

        self.assertEqual(status, 400)
        self.assertFalse(result["ok"])

    def test_unknown_endpoint(self):
        status, data = self.request("/api/unknown")

        self.assertEqual(status, 404)
        self.assertEqual(data["error"], "Not found")

    def test_reject_malformed_json(self):
        """Malformed JSON should return HTTP 400."""
        request = Request(
            self.base_url + "/api/action",
            data=b"{invalid json",
            headers={"Content-Type": "application/json"},
            method="POST",
        )

        with self.assertRaises(HTTPError) as context:
            urlopen(request, timeout=5)

        self.assertEqual(context.exception.code, 400)


if __name__ == "__main__":
    unittest.main(verbosity=2)
