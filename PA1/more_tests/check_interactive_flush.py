#!/usr/bin/env python3
from __future__ import annotations

import select
import subprocess
import sys
from pathlib import Path


PA1_DIR = Path(__file__).resolve().parents[1]
BIN = PA1_DIR / "pa1"
INPUT = PA1_DIR / "sample_input.txt"


def main() -> int:
    subprocess.run(["make", "-C", str(PA1_DIR)], check=True)

    proc = subprocess.Popen(
        [str(BIN), str(INPUT)],
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        bufsize=0,
    )

    assert proc.stdin is not None
    assert proc.stdout is not None

    proc.stdin.write("the\n")
    proc.stdin.flush()

    ready, _, _ = select.select([proc.stdout], [], [], 0.5)
    if not ready:
        proc.kill()
        proc.wait()
        print("FAIL: no output became visible before PA1EXIT")
        return 1

    output = proc.stdout.readline()
    proc.stdin.write("PA1EXIT\n")
    proc.stdin.flush()
    proc.wait(timeout=2)

    expected = "1:0 1:31 \n"
    if output != expected:
        print(f"FAIL: expected {expected!r}, got {output!r}")
        return 1

    print("PASS: interactive output was flushed per query")
    return 0


if __name__ == "__main__":
    sys.exit(main())
