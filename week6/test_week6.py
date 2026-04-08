#!/usr/bin/env python3
from __future__ import annotations

import os
import subprocess
import sys
import time
from pathlib import Path


ROOT = Path(__file__).resolve().parent
BIN = ROOT / "w6"
ENV = {
    **os.environ,
    "PATH": "/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin",
}


def build() -> None:
    subprocess.run(
        ["make", "clean"],
        cwd=ROOT,
        env=ENV,
        check=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
    )
    subprocess.run(
        ["make"],
        cwd=ROOT,
        env=ENV,
        check=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
    )


def run_program(count: int) -> tuple[subprocess.CompletedProcess[str], float]:
    start = time.monotonic()
    proc = subprocess.run(
        [str(BIN), str(count)],
        cwd=ROOT,
        env=ENV,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        check=False,
        timeout=20,
    )
    duration = time.monotonic() - start
    return proc, duration


def assert_case(name: str, condition: bool, detail: str) -> None:
    if not condition:
        raise AssertionError(f"{name}: {detail}")


def main() -> int:
    build()

    proc, duration = run_program(3)
    expected_lines = [
        "number of signals to send: 3",
        "sender: total remaining signal(s): 3",
        "receiver: received signal #1 and sending ack",
        "sender: total remaining signal(s): 2",
        "receiver: received signal #2 and sending ack",
        "sender: total remaining signal(s): 1",
        "receiver: received signal #3 and sending ack",
        "all signals have been sent!",
        "receiver: received 3 signals",
    ]
    stdout_lines = proc.stdout.splitlines()

    assert_case("exit status", proc.returncode == 0, f"expected 0, got {proc.returncode}")
    assert_case("stderr", proc.stderr == "", f"unexpected stderr {proc.stderr!r}")
    assert_case(
        "stdout lines",
        stdout_lines == expected_lines,
        f"unexpected stdout lines {stdout_lines!r}",
    )
    assert_case(
        "runtime lower bound",
        duration >= 2.5,
        f"expected runtime >= 2.5s, got {duration:.3f}s",
    )
    assert_case(
        "runtime upper bound",
        duration <= 6.5,
        f"expected runtime <= 6.5s, got {duration:.3f}s",
    )

    proc, duration = run_program(10)
    expected_lines = [
        "number of signals to send: 10",
        "sender: total remaining signal(s): 10",
        "receiver: received signal #1 and sending ack",
        "sender: total remaining signal(s): 9",
        "receiver: received signal #2 and sending ack",
        "sender: total remaining signal(s): 8",
        "receiver: received signal #3 and sending ack",
        "sender: total remaining signal(s): 7",
        "receiver: received signal #4 and sending ack",
        "sender: total remaining signal(s): 6",
        "receiver: received signal #5 and sending ack",
        "sender: total remaining signal(s): 5",
        "receiver: received signal #6 and sending ack",
        "sender: total remaining signal(s): 4",
        "receiver: received signal #7 and sending ack",
        "sender: total remaining signal(s): 3",
        "receiver: received signal #8 and sending ack",
        "sender: total remaining signal(s): 2",
        "receiver: received signal #9 and sending ack",
        "sender: total remaining signal(s): 1",
        "receiver: received signal #10 and sending ack",
        "all signals have been sent!",
        "receiver: received 10 signals",
    ]
    stdout_lines = proc.stdout.splitlines()

    assert_case("exit status 10", proc.returncode == 0, f"expected 0, got {proc.returncode}")
    assert_case("stderr 10", proc.stderr == "", f"unexpected stderr {proc.stderr!r}")
    assert_case(
        "stdout lines 10",
        stdout_lines == expected_lines,
        f"unexpected stdout lines {stdout_lines!r}",
    )
    assert_case(
        "runtime lower bound 10",
        duration >= 9.0,
        f"expected runtime >= 9.0s, got {duration:.3f}s",
    )
    assert_case(
        "runtime upper bound 10",
        duration <= 13.0,
        f"expected runtime <= 13.0s, got {duration:.3f}s",
    )

    print("PASS: week6 queued signal behavior matches the assignment cases")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except subprocess.CalledProcessError as exc:
        sys.stderr.write(exc.stderr or "")
        sys.stderr.write(exc.stdout or "")
        raise
