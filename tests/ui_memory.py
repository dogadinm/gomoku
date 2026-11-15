#!/usr/bin/env python3
"""Reject C++ allocations in the real UI, then recover without losing input."""
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

binary = Path(__file__).resolve().parents[1] / "build" / "Gomoku-oom"
with tempfile.TemporaryDirectory(prefix="gomoku-memory-") as directory:
    master, slave = pty.openpty()
    fcntl.ioctl(slave, termios.TIOCSWINSZ, struct.pack("HHHH", 30, 90, 0, 0))
    process = subprocess.Popen([str(binary)], stdin=slave, stdout=slave, stderr=slave,
                               cwd=directory, env={**os.environ, "TERM": "xterm-256color"})
    os.close(slave)
    output = bytearray()

    def wait_for(text):
        deadline = time.monotonic() + 5
        while time.monotonic() < deadline:
            if text in output:
                return
            if select.select([master], [], [], 0.05)[0]:
                try:
                    output.extend(os.read(master, 65536))
                except OSError:
                    break
        raise AssertionError(f"Missing {text!r}: {output[-500:]!r}")

    def send(keys):
        output.clear()
        os.write(master, keys)

    try:
        wait_for(b"Two humans")
        send(b"2")
        wait_for(b"Game started")
        send(b" ")
        wait_for(b"Move played")
        process.send_signal(signal.SIGUSR1)  # all subsequent C++ allocations fail
        send(b"h")  # copying a nonempty history must fail
        wait_for(b"Not enough memory")
        assert process.poll() is None
        send(b"n")  # menu and reset must remain operable while failures persist
        wait_for(b"Two humans")
        send(b"2")
        wait_for(b"Game started")
        process.send_signal(signal.SIGUSR2)
        send(b" ")
        wait_for(b"Move played")
        send(b"h")
        wait_for(b"Suggested move")
        process.send_signal(signal.SIGUSR1)
        send(b"q")  # clean exit must work even while allocations are rejected
        deadline = time.monotonic() + 3
        while process.poll() is None and time.monotonic() < deadline:
            if select.select([master], [], [], 0.05)[0]:
                try:
                    os.read(master, 65536)
                except OSError:
                    break
        assert process.wait(timeout=1) == 0
        print("UI memory test passed: persistent failure, menu/reset, recovery, hint, quit")
    finally:
        if process.poll() is None:
            process.kill()
            process.wait()
        os.close(master)
