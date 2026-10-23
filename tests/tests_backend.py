
"""Unit tests for HIDLab's queue and playback state management."""

import sys
import unittest
from pathlib import Path

# Allow tests to import the simulator from the project root.
PROJECT_ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(PROJECT_ROOT))

from simulator import main as backend


class TestQueueManagement(unittest.TestCase):
    """Verify passage creation, modification, and queue capacity."""

    def setUp(self):
        """Reset shared simulator state before each test."""
        with backend.lock:
            backend.queue.clear()
            backend.current_text = None
            backend.position = 0
            backend.state = "idle"

    def test_add_passage(self):
        success, _ = backend.perform("add", text="Hello world")

        self.assertTrue(success)
        self.assertEqual(list(backend.queue), ["Hello world"])

    def test_reject_empty_passage(self):
        success, _ = backend.perform("add", text="   ")

        self.assertFalse(success)
        self.assertEqual(len(backend.queue), 0)

    def test_reject_oversized_passage(self):
        text = "A" * (backend.MAX_CHARACTERS + 1)

        success, _ = backend.perform("add", text=text)

        self.assertFalse(success)
        self.assertEqual(len(backend.queue), 0)

    def test_queue_capacity(self):
        backend.perform("add", text="First")
        backend.perform("add", text="Second")

        success, _ = backend.perform("add", text="Third")

        self.assertFalse(success)
        self.assertEqual(len(backend.queue), backend.MAX_PASSAGES)

    def test_active_passage_counts_toward_capacity(self):
        backend.perform("add", text="First")
        backend.perform("add", text="Second")
        backend.perform("start")

        success, _ = backend.perform("add", text="Third")

        self.assertFalse(success)

    def test_remove_passage(self):
        backend.perform("add", text="First")
        backend.perform("add", text="Second")

        success, _ = backend.perform("remove", index=0)

        self.assertTrue(success)
        self.assertEqual(list(backend.queue), ["Second"])

    def test_edit_passage(self):
        backend.perform("add", text="Original")

        success, _ = backend.perform(
            "edit",
            index=0,
            text="Updated",
        )

        self.assertTrue(success)
        self.assertEqual(list(backend.queue), ["Updated"])

    def test_move_passage_up(self):
        backend.perform("add", text="First")
        backend.perform("add", text="Second")

        success, _ = backend.perform(
            "move",
            index=1,
            direction="up",
        )

        self.assertTrue(success)
        self.assertEqual(
            list(backend.queue),
            ["Second", "First"],
        )

    def test_move_passage_down(self):
        backend.perform("add", text="First")
        backend.perform("add", text="Second")

        success, _ = backend.perform(
            "move",
            index=0,
            direction="down",
        )

        self.assertTrue(success)
        self.assertEqual(
            list(backend.queue),
            ["Second", "First"],
        )

    def test_reject_invalid_index(self):
        backend.perform("add", text="First")

        success, _ = backend.perform("remove", index=10)

        self.assertFalse(success)
        self.assertEqual(list(backend.queue), ["First"])

    def test_reject_boolean_index(self):
        backend.perform("add", text="First")

        success, _ = backend.perform("remove", index=True)

        self.assertFalse(success)
        self.assertEqual(list(backend.queue), ["First"])


class TestPlaybackControls(unittest.TestCase):
    """Verify playback state transitions and passage recovery."""

    def setUp(self):
        """Reset shared simulator state before each test."""
        with backend.lock:
            backend.queue.clear()
            backend.current_text = None
            backend.position = 0
            backend.state = "idle"

    def test_start_passage(self):
        backend.perform("add", text="Hello world")

        success, _ = backend.perform("start")

        self.assertTrue(success)
        self.assertEqual(backend.state, "typing")
        self.assertEqual(backend.current_text, "Hello world")
        self.assertEqual(backend.position, 0)

    def test_start_empty_queue(self):
        success, _ = backend.perform("start")

        self.assertFalse(success)
        self.assertEqual(backend.state, "idle")

    def test_pause_passage(self):
        backend.perform("add", text="Hello world")
        backend.perform("start")

        success, _ = backend.perform("pause")

        self.assertTrue(success)
        self.assertEqual(backend.state, "paused")

    def test_resume_passage(self):
        backend.perform("add", text="Hello world")
        backend.perform("start")
        backend.perform("pause")

        success, _ = backend.perform("resume")

        self.assertTrue(success)
        self.assertEqual(backend.state, "typing")

    def test_resume_preserves_position(self):
        backend.perform("add", text="Hello world")
        backend.perform("start")

        # Simulate characters already processed before pausing.
        backend.position = 5

        backend.perform("pause")
        backend.perform("resume")

        self.assertEqual(backend.position, 5)

    def test_stop_returns_passage_to_queue(self):
        backend.perform("add", text="Hello world")
        backend.perform("start")

        backend.position = 5

        success, _ = backend.perform("stop")

        self.assertTrue(success)
        self.assertEqual(backend.state, "idle")
        self.assertEqual(backend.position, 0)
        self.assertIsNone(backend.current_text)
        self.assertEqual(list(backend.queue), ["Hello world"])

    def test_stop_preserves_queue_order(self):
        backend.perform("add", text="First")
        backend.perform("add", text="Second")
        backend.perform("start")
        backend.perform("stop")

        self.assertEqual(
            list(backend.queue),
            ["First", "Second"],
        )

    def test_reject_pause_while_idle(self):
        success, _ = backend.perform("pause")

        self.assertFalse(success)
        self.assertEqual(backend.state, "idle")

    def test_reject_unknown_command(self):
        success, _ = backend.perform("invalid_command")

        self.assertFalse(success)


if __name__ == "__main__":
    unittest.main(verbosity=2)
