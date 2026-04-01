#!/usr/bin/env python3
from __future__ import annotations

import os
import stat
import subprocess
import sys
import tempfile
from pathlib import Path


ROOT = Path(__file__).resolve().parent
BIN = ROOT / "w5"
ENV = {
    **os.environ,
    "PATH": "/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin",
}


def strip_prompts(text: str) -> str:
    return text.replace("$ ", "")


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


def run_shell(stdin: str, cwd: Path | None = None) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        [str(BIN)],
        input=stdin,
        cwd=cwd or ROOT,
        env=ENV,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        check=False,
    )


def assert_case(name: str, condition: bool, detail: str) -> None:
    if not condition:
        raise AssertionError(f"{name}: {detail}")


def main() -> int:
    build()

    proc = run_shell("exit\n")
    assert_case("exit default", proc.returncode == 0, f"expected 0, got {proc.returncode}")
    assert_case("exit default stderr", proc.stderr == "", f"unexpected stderr {proc.stderr!r}")

    proc = run_shell("exit 1\n")
    assert_case("exit numeric", proc.returncode == 1, f"expected 1, got {proc.returncode}")

    proc = run_shell("exit 999\n")
    assert_case("exit clamp", proc.returncode == 255, f"expected 255, got {proc.returncode}")

    proc = run_shell("exit abc\nexit\n")
    assert_case("exit invalid status", proc.returncode == 0, f"expected 0, got {proc.returncode}")
    assert_case(
        "exit invalid message",
        proc.stderr == "exit: abc: invalid integer\n",
        f"unexpected stderr {proc.stderr!r}",
    )

    proc = run_shell("exit 1 nope\nexit 5\n")
    assert_case("exit too many status", proc.returncode == 5, f"expected 5, got {proc.returncode}")
    assert_case(
        "exit too many message",
        proc.stderr == "exit: too many arguments\n",
        f"unexpected stderr {proc.stderr!r}",
    )

    proc = run_shell("echo hello\nexit\n")
    assert_case("path command status", proc.returncode == 0, f"expected 0, got {proc.returncode}")
    assert_case(
        "path command stdout",
        strip_prompts(proc.stdout) == "hello\n",
        f"unexpected stdout {proc.stdout!r}",
    )

    with tempfile.TemporaryDirectory() as tmpdir_str:
        tmpdir = Path(tmpdir_str)
        helper = tmpdir / "hello"
        helper.write_text("#!/bin/sh\necho current-dir\n", encoding="ascii")
        helper.chmod(helper.stat().st_mode | stat.S_IXUSR)

        proc = run_shell("hello\nexit\n", cwd=tmpdir)
        assert_case("cwd command status", proc.returncode == 0, f"expected 0, got {proc.returncode}")
        assert_case(
            "cwd command stdout",
            strip_prompts(proc.stdout) == "current-dir\n",
            f"unexpected stdout {proc.stdout!r}",
        )

    proc = run_shell("definitely_missing_command\nexit\n")
    assert_case("missing command status", proc.returncode == 0, f"expected 0, got {proc.returncode}")
    assert_case(
        "missing command stderr",
        proc.stderr == "definitely_missing_command: command not found\n",
        f"unexpected stderr {proc.stderr!r}",
    )

    too_many = "echo " + " ".join(f"arg{i}" for i in range(100)) + "\nexit\n"
    proc = run_shell(too_many)
    assert_case("parse args status", proc.returncode == 0, f"expected 0, got {proc.returncode}")
    assert_case(
        "parse args message",
        proc.stderr == "Error: Too many arguments\n",
        f"unexpected stderr {proc.stderr!r}",
    )

    print("PASS: week5 shell behavior matches the assignment cases")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except subprocess.CalledProcessError as exc:
        sys.stderr.write(exc.stderr or "")
        sys.stderr.write(exc.stdout or "")
        raise
