
"""Desktop simulator for HIDLab's typing settings interface.

Run this file using VS Code's Run Python File command.
Open http://localhost:8002 in your browser.

USB keyboard output and Wi-Fi connections are simulated.
"""

from http.server import ThreadingHTTPServer
from pathlib import Path
import importlib.util

SOURCE = Path(__file__).with_name(
    "hidlab_desktop_test.py"
)

spec = importlib.util.spec_from_file_location(
    "hidlab_base_simulator",
    SOURCE,
)

sim = importlib.util.module_from_spec(spec)
spec.loader.exec_module(sim)

settings = {
    "mode": "standard",
    "min_wpm": 30,
    "max_wpm": 35,
    "typo_rate": 3,
}


class Handler(sim.Handler):
    """Extend the desktop API with adjustable typing profiles."""

    def do_GET(self):
        if self.path != "/api/settings":
            return super().do_GET()

        with sim.lock:
            if not self.authorized():
                return

            response = settings.copy()
            response["current_wpm"] = settings["min_wpm"]

            self.send_json(200, response)

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
                self.send_json(
                    409,
                    {
                        "ok": False,
                        "error": (
                            "Stop playback before "
                            "changing typing settings."
                        ),
                    },
                )
                return

            mode = payload.get("mode")
            min_wpm = payload.get("min_wpm")
            max_wpm = payload.get("max_wpm")
            typo_rate = payload.get("typo_rate")

            valid = (
                mode in ("standard", "humanized")
                and type(min_wpm) is int
                and type(max_wpm) is int
                and type(typo_rate) is int
                and 15 <= min_wpm <= 100
                and 15 <= max_wpm <= 100
                and min_wpm <= max_wpm
                and 0 <= typo_rate <= 10
            )

            if valid:
                settings.update(
                    mode=mode,
                    min_wpm=min_wpm,
                    max_wpm=max_wpm,
                    typo_rate=typo_rate,
                )

            self.send_json(
                200 if valid else 400,
                {
                    "ok": valid,
                    "message": (
                        "Typing settings saved."
                        if valid
                        else "Invalid typing settings."
                    ),
                },
            )


def main():
    server = ThreadingHTTPServer(
        ("127.0.0.1", 8002),
        Handler,
    )

    print("HIDLab typing settings simulator")
    print("Open http://localhost:8002")
    print("No USB keyboard output is generated.")

    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        server.server_close()


if __name__ == "__main__":
    main()
