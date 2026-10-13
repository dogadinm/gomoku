#!/usr/bin/env python3
"""Exercise the real ncurses executable in an isolated pseudo-terminal."""
import fcntl
import os
from pathlib import Path
import pty
import select
import signal
import struct
import subprocess
import tempfile
import termios
import time

binary = Path(__file__).resolve().parents[1] / "Gomoku"


def exercise():
    master, slave = pty.openpty()
    fcntl.ioctl(slave, termios.TIOCSWINSZ, struct.pack("HHHH", 30, 90, 0, 0))
    with tempfile.TemporaryDirectory(prefix="gomoku-ui-") as directory:
        process = subprocess.Popen(
            [str(binary)], stdin=slave, stdout=slave, stderr=slave,
            cwd=directory, env={**os.environ, "TERM": "xterm-256color"},
        )
        os.close(slave)
        output = bytearray()

        def wait_for(text, seconds=3):
            end = time.monotonic() + seconds
            while time.monotonic() < end:
                if text in output:
                    return
                if select.select([master], [], [], 0.05)[0]:
                    try:
                        output.extend(os.read(master, 65536))
                    except OSError:
                        break
            raise AssertionError(f"Missing UI text: {text!r}; tail: {output[-400:]!r}")

        def send(keys):
            output.clear()
            os.write(master, keys)

        try:
            wait_for(b"Two humans")
            send(b"2")
            wait_for(b"Game started")
            send(b" ")
            wait_for(b"Move played")
            send(b"\x1bOC s")
            wait_for(b"Saved")
            save = Path(directory) / "gomoku.save"
            assert save.read_text().splitlines() == ["GOMOKU 2", "1 1 0 2", "M 9 9", "M 10 9"]
            send(b"u")
            wait_for(b"undone")
            send(b"l")
            wait_for(b"Loaded")
            send(b"h")
            wait_for(b"Suggested move", 5)
            send(b"[")
            time.sleep(0.1)
            send(b"]")
            time.sleep(0.1)
            send(b"n")
            wait_for(b"Two humans")
            send(b"3")
            wait_for(b"AI moved", 5)
            send(b"s")
            wait_for(b"Saved")
            assert save.read_text().splitlines()[1] == "1 1 0 1"
            send(b"n")
            wait_for(b"Two humans")
            send(b"ooo2")
            wait_for(b"Game started")
            send(b" \x1bOC \x1bOB e\x1bOD \x1bOB ws")
            wait_for(b"Saved")
            assert save.read_text().splitlines() == [
                "GOMOKU 2", "1 1 3 7", "M 9 9", "M 10 9", "M 10 10",
                "C 3", "M 9 10", "M 9 11", "C 2",
            ]
            send(b"us")
            wait_for(b"Saved")
            assert save.read_text().splitlines()[1] == "1 1 3 6"
            send(b"lbs")
            wait_for(b"Saved")
            assert save.read_text().splitlines()[-1] == "C 1"
            fcntl.ioctl(master, termios.TIOCSWINSZ, struct.pack("HHHH", 20, 60, 0, 0))
            process.send_signal(signal.SIGWINCH)
            wait_for(b"Please resize")
            send(b"q")
            deadline = time.monotonic() + 3
            while process.poll() is None and time.monotonic() < deadline:
                if select.select([master], [], [], 0.05)[0]:
                    try:
                        os.read(master, 65536)
                    except OSError:
                        break
            assert process.wait(timeout=1) == 0
            print("UI smoke passed: hotseat, move, undo, save/load, hint, replay, AI, resize, quit")
        finally:
            if process.poll() is None:
                process.kill()
                process.wait()
            os.close(master)


if __name__ == "__main__":
    exercise()
