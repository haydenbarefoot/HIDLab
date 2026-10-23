
"""Local HTTP server and simulated keyboard backend for HIDLab."""

import json
import threading
import time
from collections import deque
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path

HOST = "127.0.0.1"
PORT = 8000
WEB_FILE = Path(__file__).resolve().parent.parent / "web" / "index.html"

MAX_PASSAGES = 2
MAX_CHARACTERS = 12000

lock = threading.Lock()
queue = deque()
current_text = None
position = 0
state = "idle"


def snapshot():
    """Return a thread-safe snapshot of the simulator state."""
    with lock:
        return {
            "status": state,
            "queued": len(queue),
            "passages": list(queue),
        }


def valid_index(index):
    """Check whether an index identifies a waiting passage."""
    return type(index) is int and 0 <= index < len(queue)


def valid_text(text):
    """Check whether a passage meets the input requirements."""
    return (
        isinstance(text, str)
        and bool(text.strip())
        and len(text) <= MAX_CHARACTERS
    )


def perform(action, text="", index=None, direction=None):
    """Execute a queue or playback command and return (success, message)."""
    global current_text, position, state

    with lock:
        if action == "add":
            if not valid_text(text):
                return False, "Enter text between 1 and 12,000 characters."

            # Include the active passage when enforcing queue capacity.
            active_count = int(current_text is not None)

            if len(queue) + active_count >= MAX_PASSAGES:
                return False, "Queue is full."

            queue.append(text)
            return True, "Passage added."

        if action == "remove":
            if not valid_index(index):
                return False, "Invalid queue position."

            del queue[index]
            return True, "Passage removed."

        if action == "edit":
            if not valid_index(index):
                return False, "Invalid queue position."

            if not valid_text(text):
                return False, "Enter text between 1 and 12,000 characters."

            queue[index] = text
            return True, "Passage updated."

        if action == "move":
            if not valid_index(index):
                return False, "Invalid queue position."

            if direction not in ("up", "down"):
                return False, "Invalid move direction."

            offset = -1 if direction == "up" else 1
            destination = index + offset

            if not valid_index(destination):
                return False, "Passage cannot move in that direction."

            # Swap adjacent passages without altering either passage.
            queue[index], queue[destination] = (
                queue[destination],
                queue[index],
            )

            return True, "Queue order updated."

        if action == "start":
            if state != "idle":
                return False, "Already active."

            if not queue:
                return False, "Queue is empty."

            current_text = queue.popleft()
            position = 0
            state = "typing"
            return True, "Started."

        if action == "pause":
            if state != "typing":
                return False, "Not currently typing."

            state = "paused"
            return True, "Paused."

        if action == "resume":
            if state != "paused":
                return False, "Not paused."

            state = "typing"
            return True, "Resumed."

        if action == "stop":
            if state == "idle":
                return False, "Already idle."

            # Restore the full passage so the next Start begins at index zero.
            if current_text is not None:
                queue.appendleft(current_text)

            current_text = None
            position = 0
            state = "idle"
            return True, "Stopped. Passage returned to queue."

        return False, "Unknown action."


def keyboard_worker():
    """Log simulated keyboard events independently of HTTP requests."""
    global current_text, position, state

    while True:
        output = None
        finished = False

        with lock:
            if state == "typing" and current_text is not None:
                if position < len(current_text):
                    output = current_text[position]
                    position += 1

                if position >= len(current_text):
                    finished = True
                    current_text = None
                    position = 0
                    state = "idle"

        if output is not None:
            print(f"[SIMULATED KEY] {output!r}", flush=True)

        if finished:
            print("[SIMULATOR] Passage completed.", flush=True)

        # Fixed delay keeps playback observable during simulator testing.
        time.sleep(0.15)


class Handler(BaseHTTPRequestHandler):
    """Serve the HIDLab webpage and simulator API."""

    def respond(self, status, data, content_type="application/json"):
        """Serialize and send an HTTP response."""
        body = (
            data.encode("utf-8")
            if isinstance(data, str)
            else json.dumps(data).encode("utf-8")
        )

        self.send_response(status)
        self.send_header("Content-Type", f"{content_type}; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Cache-Control", "no-store")
        self.end_headers()
        self.wfile.write(body)

    def do_GET(self):
        """Serve the control page or return simulator status."""
        if self.path == "/":
            if not WEB_FILE.is_file():
                self.respond(
                    404,
                    {"error": "web/index.html not found"},
                )
                return

            self.respond(
                200,
                WEB_FILE.read_text(encoding="utf-8"),
                "text/html",
            )

        elif self.path == "/api/status":
            self.respond(200, snapshot())

        else:
            self.respond(404, {"error": "Not found"})

    def do_POST(self):
        """Validate and execute a command sent to /api/action."""
        if self.path != "/api/action":
            self.respond(404, {"error": "Not found"})
            return

        try:
            length = int(self.headers.get("Content-Length", "0"))

            if not 1 <= length <= 100000:
                self.respond(413, {"error": "Invalid request size"})
                return

            request = json.loads(self.rfile.read(length))

            if not isinstance(request, dict):
                raise ValueError("Expected a JSON object.")

            ok, message = perform(
                action=request.get("action"),
                text=request.get("text", ""),
                index=request.get("index"),
                direction=request.get("direction"),
            )

            self.respond(
                200 if ok else 400,
                {"ok": ok, "message": message},
            )

        except (ValueError, json.JSONDecodeError):
            self.respond(400, {"error": "Invalid JSON request"})


def main():
    """Start the simulated keyboard worker and local HTTP server."""
    worker = threading.Thread(
        target=keyboard_worker,
        daemon=True,
    )
    worker.start()

    server = ThreadingHTTPServer((HOST, PORT), Handler)

    print(f"HIDLab ready: http://localhost:{PORT}")
    print("Keyboard output is simulated only.")

    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        server.server_close()


if __name__ == "__main__":
    main()
