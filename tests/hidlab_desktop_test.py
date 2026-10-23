
"""Desktop API simulator for the firmware-embedded HIDLab webpage.

Run this file in VS Code and visit http://localhost:8001.
The simulator does not send USB keyboard events or modify Wi-Fi settings.
"""

from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
import json
import re
import threading
import time

ROOT = Path(__file__).resolve().parents[1]
WEBPAGE = ROOT / "firmware" / "HIDLab" / "WebPage.h"

PORT = 8001
MAX_PASSAGES = 2
MAX_CHARACTERS = 12000
KEY_INTERVAL_SECONDS = 0.150
LEASE_SECONDS = 30

lock = threading.Lock()

waiting = []
active = None
position = 0
state = "idle"
next_key_time = 0.0

controller = ""
controller_time = 0.0

networks = ["Example-Home-WiFi"]


def load_page():
    """Extract the browser UI directly from the firmware header."""
    source = WEBPAGE.read_text(encoding="utf-8")

    match = re.search(
        r'R"([A-Za-z0-9_]*)\((.*?)\)\1"\s*;',
        source,
        re.DOTALL,
    )

    if not match:
        raise ValueError("Cannot find embedded HTML in WebPage.h")

    return match.group(2).encode("utf-8")


def valid_text(value):
    """Match firmware ASCII and passage-length constraints."""
    return (
        isinstance(value, str)
        and 0 < len(value) <= MAX_CHARACTERS
        and any(ch not in " \r\n" for ch in value)
        and all(
            32 <= ord(ch) <= 126 or ch in "\r\n"
            for ch in value
        )
    )


def update_playback():
    """Advance simulated playback based on elapsed time."""
    global active, position, state, next_key_time

    if state != "typing" or active is None:
        return

    now = time.monotonic()

    if now < next_key_time:
        return

    elapsed_characters = (
        int((now - next_key_time) / KEY_INTERVAL_SECONDS) + 1
    )

    position += elapsed_characters
    next_key_time += (
        elapsed_characters * KEY_INTERVAL_SECONDS
    )

    if position >= len(active):
        active = None
        position = 0
        state = "idle"
        next_key_time = 0.0

        print("[Playback] Passage completed; status is IDLE.")


class Handler(BaseHTTPRequestHandler):
    """Serve the real HIDLab webpage with simulated API endpoints."""

    def log_message(self, fmt, *args):
        print("[HTTP]", fmt % args)

    def send_json(self, status, data):
        payload = json.dumps(data).encode("utf-8")

        self.send_response(status)
        self.send_header(
            "Content-Type",
            "application/json; charset=utf-8",
        )
        self.send_header("Cache-Control", "no-store")
        self.send_header(
            "Content-Length",
            str(len(payload)),
        )
        self.end_headers()
        self.wfile.write(payload)

    def read_json(self):
        try:
            size = int(
                self.headers.get("Content-Length", "0")
            )

            if not 0 < size <= 32000:
                raise ValueError("Invalid request size")

            data = json.loads(self.rfile.read(size))

            if not isinstance(data, dict):
                raise ValueError("Expected a JSON object")

            return data

        except (ValueError, UnicodeDecodeError):
            self.send_json(
                400,
                {
                    "ok": False,
                    "error": "Invalid JSON request.",
                },
            )
            return None

    def authorized(self):
        global controller_time

        token = self.headers.get(
            "X-HIDLab-Session", ""
        )

        if (
            not controller
            or token != controller
            or time.monotonic() - controller_time
            >= LEASE_SECONDS
        ):
            self.send_json(
                423,
                {
                    "ok": False,
                    "error": "Controller unavailable.",
                },
            )
            return False

        controller_time = time.monotonic()
        return True

    def do_GET(self):
        with lock:
            update_playback()

            if self.path == "/":
                self.send_response(200)
                self.send_header(
                    "Content-Type",
                    "text/html; charset=utf-8",
                )
                self.send_header(
                    "Content-Length",
                    str(len(PAGE)),
                )
                self.end_headers()
                self.wfile.write(PAGE)
                return

            if not self.authorized():
                return

            if self.path == "/api/status":
                self.send_json(
                    200,
                    {
                        "status": state,
                        "queued": len(waiting),
                        "usb_ready": True,
                        "usb_paused": False,
                        "network": "Example-Home-WiFi",
                        "ip": "127.0.0.1",
                        "setup_mode": False,
                        "network_exhausted": False,
                        "passages": waiting,
                    },
                )

            elif self.path == "/api/networks":
                self.send_json(
                    200,
                    {"networks": networks},
                )

            else:
                self.send_json(
                    404,
                    {
                        "ok": False,
                        "error": "Not found.",
                    },
                )

    def do_POST(self):
        global controller, controller_time
        global active, position, state, next_key_time

        with lock:
            update_playback()

            if self.path == "/api/release":
                if self.authorized():
                    controller = ""
                    self.send_json(200, {"ok": True})
                return

            data = self.read_json()

            if data is None:
                return

            if self.path == "/api/session":
                token = data.get("token", "")

                if (
                    not isinstance(token, str)
                    or not 32 <= len(token) <= 128
                ):
                    self.send_json(
                        400,
                        {
                            "ok": False,
                            "error": "Invalid token.",
                        },
                    )

                elif (
                    controller
                    and controller != token
                    and time.monotonic() - controller_time
                    < LEASE_SECONDS
                ):
                    self.send_json(
                        423,
                        {
                            "ok": False,
                            "error": (
                                "Another controller is connected."
                            ),
                        },
                    )

                else:
                    controller = token
                    controller_time = time.monotonic()

                    self.send_json(
                        200,
                        {"ok": True},
                    )

                return

            if not self.authorized():
                return

            if self.path == "/api/networks/action":
                command = data.get("action")
                success = False

                if command == "add":
                    ssid = data.get("ssid")
                    password = data.get("password")

                    success = (
                        isinstance(ssid, str)
                        and 0 < len(ssid) <= 32
                        and isinstance(password, str)
                        and (
                            len(password) == 0
                            or 8 <= len(password) <= 63
                        )
                        and (
                            ssid in networks
                            or len(networks) < 4
                        )
                    )

                    if success and ssid not in networks:
                        networks.append(ssid)

                elif command == "remove":
                    index = data.get("index")

                    success = (
                        type(index) is int
                        and 0 <= index < len(networks)
                    )

                    if success:
                        networks.pop(index)

                self.send_json(
                    200 if success else 400,
                    {
                        "ok": success,
                        "message": (
                            "Network updated (simulation)."
                            if success
                            else "Invalid network operation."
                        ),
                    },
                )
                return

            if self.path != "/api/action":
                self.send_json(
                    404,
                    {
                        "ok": False,
                        "error": "Not found.",
                    },
                )
                return

            command = data.get("action")
            index = data.get("index")

            valid_index = (
                type(index) is int
                and 0 <= index < len(waiting)
            )

            success = False

            if command in ("add", "edit"):
                text = data.get("text")

                if valid_text(text):
                    text = (
                        text.replace("\r\n", "\n")
                        .replace("\r", "\n")
                    )

                    if (
                        command == "add"
                        and len(waiting) + (active is not None)
                        < MAX_PASSAGES
                    ):
                        waiting.append(text)
                        success = True

                    elif (
                        command == "edit"
                        and valid_index
                    ):
                        waiting[index] = text
                        success = True

            elif command == "remove" and valid_index:
                waiting.pop(index)
                success = True

            elif command == "move" and valid_index:
                direction = {
                    "up": -1,
                    "down": 1,
                }.get(data.get("direction"), 0)

                target = index + direction

                if (
                    direction
                    and 0 <= target < len(waiting)
                ):
                    waiting[index], waiting[target] = (
                        waiting[target],
                        waiting[index],
                    )
                    success = True

            elif (
                command == "start"
                and state == "idle"
                and waiting
            ):
                active = waiting.pop(0)
                position = 0
                state = "typing"

                next_key_time = (
                    time.monotonic()
                    + KEY_INTERVAL_SECONDS
                )

                success = True

            elif (
                command == "pause"
                and state == "typing"
            ):
                state = "paused"
                success = True

            elif (
                command == "resume"
                and state == "paused"
            ):
                state = "typing"

                next_key_time = (
                    time.monotonic()
                    + KEY_INTERVAL_SECONDS
                )

                success = True

            elif (
                command == "stop"
                and state != "idle"
                and active is not None
            ):
                waiting.insert(0, active)

                active = None
                position = 0
                state = "idle"
                next_key_time = 0.0

                success = True

            self.send_json(
                200 if success else 400,
                {
                    "ok": success,
                    "message": (
                        "Operation accepted (simulation)."
                        if success
                        else "Operation rejected."
                    ),
                },
            )


def main():
    print(f"Loaded firmware webpage: {WEBPAGE}")
    print(f"Open http://localhost:{PORT}")
    print("Playback, Wi-Fi, and USB are simulated.")

    server = ThreadingHTTPServer(
        ("127.0.0.1", PORT),
        Handler,
    )

    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        server.server_close()


PAGE = load_page()

if __name__ == "__main__":
    main()
