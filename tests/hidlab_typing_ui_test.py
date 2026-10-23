
"""Serve the final HIDLab webpage with simulated typing settings.

Place alongside hidlab_desktop_test.py and run from VS Code.
Visit http://localhost:8002. No USB keyboard output is generated.
"""

from http.server import ThreadingHTTPServer
from pathlib import Path
import importlib.util

SOURCE = Path(__file__).with_name("hidlab_desktop_test.py")
spec = importlib.util.spec_from_file_location("hidlab_base_simulator", SOURCE)
sim = importlib.util.module_from_spec(spec)
spec.loader.exec_module(sim)

settings = {"mode": "standard", "wpm": 35, "typo_rate": 3}


class Handler(sim.Handler):
    """Extend the desktop simulator with typing settings endpoints."""

    def do_GET(self):
        if self.path != "/api/settings":
            return super().do_GET()

        with sim.lock:
            if self.authorized():
                self.send_json(200, settings.copy())

    def do_POST(self):
        if self.path != "/api/settings":
            return super().do_POST()

        with sim.lock:
            if not self.authorized():
                return

            payload = self.read_json()
            if payload is None:
                return

            if sim.state != "idle":
                self.send_json(409, {
                    "ok": False,
                    "error": "Stop playback before changing typing settings.",
                })
                return

            mode = payload.get("mode")
            wpm = payload.get("wpm")
            typo_rate = payload.get("typo_rate")

            valid = (
                mode in ("standard", "humanized")
                and type(wpm) is int and 15 <= wpm <= 100
                and type(typo_rate) is int and 0 <= typo_rate <= 10
            )

            if valid:
                settings.update(
                    mode=mode,
                    wpm=wpm,
                    typo_rate=typo_rate,
                )

            self.send_json(200 if valid else 400, {
                "ok": valid,
                "message": (
                    "Typing settings saved."
                    if valid else "Invalid settings."
                ),
            })


if __name__ == "__main__":
    server = ThreadingHTTPServer(("127.0.0.1", 8002), Handler)
    print("HIDLab typing UI simulator: http://localhost:8002")
    print("USB output and actual typing rhythm are not simulated.")

    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        server.server_close()
