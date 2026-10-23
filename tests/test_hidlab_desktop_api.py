
"""Automated regression tests for the HIDLab desktop API simulator.

Run this file in VS Code with Run Python File. The separate desktop
simulator server does not need to be running.
"""

import importlib.util
import json
from pathlib import Path
import threading
import time
import unittest
from urllib.error import HTTPError
from urllib.request import Request, urlopen
from http.server import ThreadingHTTPServer

SIMULATOR = Path(__file__).with_name("hidlab_desktop_test.py")
spec = importlib.util.spec_from_file_location("hidlab_simulator", SIMULATOR)
sim = importlib.util.module_from_spec(spec)
spec.loader.exec_module(sim)


class HIDLabDesktopAPITests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.server = ThreadingHTTPServer(("127.0.0.1", 0), sim.Handler)
        cls.address = f"http://127.0.0.1:{cls.server.server_port}"
        cls.worker = threading.Thread(
            target=cls.server.serve_forever,
            daemon=True,
        )
        cls.worker.start()

    @classmethod
    def tearDownClass(cls):
        cls.server.shutdown()
        cls.server.server_close()
        cls.worker.join(timeout=2)

    def setUp(self):
        with sim.lock:
            sim.waiting.clear()
            sim.active = None
            sim.position = 0
            sim.state = "idle"
            sim.next_key_time = 0.0
            sim.controller = ""
            sim.controller_time = 0.0
            sim.networks[:] = ["Example-Home-WiFi"]
            sim.KEY_INTERVAL_SECONDS = 0.150

        self.token = "a" * 48
        self.claim(self.token)

    def request(self, method, path, payload=None, token=None):
        headers = {}
        body = None

        if payload is not None:
            body = json.dumps(payload).encode("utf-8")
            headers["Content-Type"] = "application/json"

        if token is not None:
            headers["X-HIDLab-Session"] = token

        req = Request(
            self.address + path,
            data=body,
            headers=headers,
            method=method,
        )

        try:
            response = urlopen(req, timeout=3)
        except HTTPError as error:
            response = error

        with response:
            raw = response.read()
            return response.status, json.loads(raw) if raw else None

    def claim(self, token):
        return self.request("POST", "/api/session", {"token": token})

    def action(self, command, **kwargs):
        return self.request(
            "POST",
            "/api/action",
            {"action": command, **kwargs},
            self.token,
        )

    def status(self):
        return self.request(
            "GET",
            "/api/status",
            token=self.token,
        )

    def test_webpage_is_embedded(self):
        req = Request(self.address + "/")
        with urlopen(req, timeout=3) as response:
            self.assertIn(b"HIDLab", response.read())

    def test_initial_state(self):
        code, result = self.status()
        self.assertEqual(
            (code, result["status"], result["queued"]),
            (200, "idle", 0),
        )

    def test_controller_blocks_other_browser(self):
        code, _ = self.claim("b" * 48)
        self.assertEqual(code, 423)

    def test_existing_controller_can_renew(self):
        code, result = self.claim(self.token)
        self.assertEqual(code, 200)
        self.assertTrue(result["ok"])

    def test_expired_session_can_be_reclaimed(self):
        with sim.lock:
            sim.controller_time -= 31

        code, result = self.claim("b" * 48)
        self.assertEqual(code, 200)
        self.assertTrue(result["ok"])

    def test_status_requires_session(self):
        code, _ = self.request("GET", "/api/status")
        self.assertEqual(code, 423)

    def test_action_requires_session(self):
        code, _ = self.request(
            "POST",
            "/api/action",
            {"action": "start"},
        )
        self.assertEqual(code, 423)

    def test_add_two_and_reject_third(self):
        self.assertEqual(
            self.action("add", text="First")[0], 200
        )
        self.assertEqual(
            self.action("add", text="Second")[0], 200
        )
        self.assertEqual(
            self.action("add", text="Third")[0], 400
        )
        self.assertEqual(
            self.status()[1]["passages"],
            ["First", "Second"],
        )

    def test_reject_non_ascii_and_whitespace(self):
        self.assertEqual(
            self.action("add", text="café")[0], 400
        )
        self.assertEqual(
            self.action("add", text=" \n \r ")[0], 400
        )

    def test_normalize_windows_newlines(self):
        self.assertEqual(
            self.action("add", text="A\r\nB\rC")[0], 200
        )
        self.assertEqual(
            self.status()[1]["passages"],
            ["A\nB\nC"],
        )

    def test_edit_passage(self):
        self.action("add", text="First")
        self.assertEqual(
            self.action("edit", index=0, text="Edited")[0],
            200,
        )
        self.assertEqual(
            self.status()[1]["passages"],
            ["Edited"],
        )

    def test_move_passages(self):
        self.action("add", text="First")
        self.action("add", text="Second")

        self.assertEqual(
            self.action(
                "move",
                index=1,
                direction="up",
            )[0],
            200,
        )
        self.assertEqual(
            self.status()[1]["passages"],
            ["Second", "First"],
        )

    def test_remove_passage(self):
        self.action("add", text="First")
        self.assertEqual(
            self.action("remove", index=0)[0], 200
        )
        self.assertEqual(
            self.status()[1]["passages"],
            [],
        )

    def test_start_pause_resume_stop(self):
        text = "A passage long enough for a pause test"

        self.action("add", text=text)
        self.assertEqual(self.action("start")[0], 200)
        self.assertEqual(
            self.status()[1]["status"], "typing"
        )

        self.assertEqual(self.action("pause")[0], 200)
        self.assertEqual(
            self.status()[1]["status"], "paused"
        )

        self.assertEqual(self.action("resume")[0], 200)
        self.assertEqual(self.action("stop")[0], 200)
        self.assertEqual(
            self.status()[1]["status"], "idle"
        )
        self.assertEqual(
            self.status()[1]["passages"], [text]
        )

    def test_passage_completes_automatically(self):
        with sim.lock:
            sim.KEY_INTERVAL_SECONDS = 0.005

        self.action("add", text="Hi!")
        self.action("start")

        time.sleep(0.04)

        self.assertEqual(
            self.status()[1]["status"], "idle"
        )
        self.assertEqual(
            self.status()[1]["queued"], 0
        )

    def test_network_add_and_remove(self):
        endpoint = "/api/networks/action"

        code, _ = self.request(
            "POST",
            endpoint,
            {
                "action": "add",
                "ssid": "Phone Hotspot",
                "password": "abcdefgh",
            },
            self.token,
        )
        self.assertEqual(code, 200)

        code, result = self.request(
            "GET",
            "/api/networks",
            token=self.token,
        )
        self.assertEqual(code, 200)
        self.assertIn(
            "Phone Hotspot", result["networks"]
        )
        self.assertNotIn(
            "abcdefgh", json.dumps(result)
        )

        code, _ = self.request(
            "POST",
            endpoint,
            {"action": "remove", "index": 1},
            self.token,
        )
        self.assertEqual(code, 200)

    def test_reject_short_wifi_password(self):
        code, _ = self.request(
            "POST",
            "/api/networks/action",
            {
                "action": "add",
                "ssid": "Test",
                "password": "short",
            },
            self.token,
        )
        self.assertEqual(code, 400)

    def test_release_session(self):
        code, _ = self.request(
            "POST",
            "/api/release",
            token=self.token,
        )
        self.assertEqual(code, 200)
        self.assertEqual(
            self.claim("b" * 48)[0], 200
        )


if __name__ == "__main__":
    unittest.main(verbosity=2)
