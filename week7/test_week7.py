#!/usr/bin/env python3
from __future__ import annotations

import os
import subprocess
import sys
import tempfile
from pathlib import Path


ROOT = Path(__file__).resolve().parent
BIN = ROOT / "w7"
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

    proc = run_shell("echo hello\nexit\n")
    assert_case("simple command status", proc.returncode == 0, f"expected 0, got {proc.returncode}")
    assert_case("simple command stderr", proc.stderr == "", f"unexpected stderr {proc.stderr!r}")
    assert_case(
        "simple command stdout",
        strip_prompts(proc.stdout) == "hello\n",
        f"unexpected stdout {proc.stdout!r}",
    )

    with tempfile.TemporaryDirectory() as tmpdir_str:
        tmpdir = Path(tmpdir_str)
        input_file = tmpdir / "input.txt"
        output_file = tmpdir / "output.txt"
        append_file = tmpdir / "append.txt"
        input_file.write_text("alpha\nbeta\n", encoding="ascii")

        proc = run_shell("cat < input.txt\nexit\n", cwd=tmpdir)
        assert_case("input redirect status", proc.returncode == 0, f"expected 0, got {proc.returncode}")
        assert_case("input redirect stderr", proc.stderr == "", f"unexpected stderr {proc.stderr!r}")
        assert_case(
            "input redirect stdout",
            strip_prompts(proc.stdout) == "alpha\nbeta\n",
            f"unexpected stdout {proc.stdout!r}",
        )

        proc = run_shell("echo hello > output.txt\nexit\n", cwd=tmpdir)
        assert_case("output redirect status", proc.returncode == 0, f"expected 0, got {proc.returncode}")
        assert_case("output redirect stderr", proc.stderr == "", f"unexpected stderr {proc.stderr!r}")
        assert_case(
            "output redirect stdout",
            strip_prompts(proc.stdout) == "",
            f"unexpected stdout {proc.stdout!r}",
        )
        assert_case(
            "output redirect file",
            output_file.read_text(encoding="ascii") == "hello\n",
            f"unexpected file contents {output_file.read_text(encoding='ascii')!r}",
        )

        proc = run_shell("echo one > append.txt\necho two >> append.txt\nexit\n", cwd=tmpdir)
        assert_case("append redirect status", proc.returncode == 0, f"expected 0, got {proc.returncode}")
        assert_case("append redirect stderr", proc.stderr == "", f"unexpected stderr {proc.stderr!r}")
        assert_case(
            "append redirect file",
            append_file.read_text(encoding="ascii") == "one\ntwo\n",
            f"unexpected file contents {append_file.read_text(encoding='ascii')!r}",
        )

        proc = run_shell("echo hi>tight.txt\ncat<tight.txt\nexit\n", cwd=tmpdir)
        assert_case("tight redirect status", proc.returncode == 0, f"expected 0, got {proc.returncode}")
        assert_case("tight redirect stderr", proc.stderr == "", f"unexpected stderr {proc.stderr!r}")
        assert_case(
            "tight redirect stdout",
            strip_prompts(proc.stdout) == "hi\n",
            f"unexpected stdout {proc.stdout!r}",
        )

        proc = run_shell("echo hello | tr a-z A-Z\nexit\n", cwd=tmpdir)
        assert_case("pipe status", proc.returncode == 0, f"expected 0, got {proc.returncode}")
        assert_case("pipe stderr", proc.stderr == "", f"unexpected stderr {proc.stderr!r}")
        assert_case(
            "pipe stdout",
            strip_prompts(proc.stdout) == "HELLO\n",
            f"unexpected stdout {proc.stdout!r}",
        )

        proc = run_shell("echo hello|tr a-z A-Z\nexit\n", cwd=tmpdir)
        assert_case("tight pipe status", proc.returncode == 0, f"expected 0, got {proc.returncode}")
        assert_case("tight pipe stderr", proc.stderr == "", f"unexpected stderr {proc.stderr!r}")
        assert_case(
            "tight pipe stdout",
            strip_prompts(proc.stdout) == "HELLO\n",
            f"unexpected stdout {proc.stdout!r}",
        )

    print("PASS: week7 shell supports single redirection and single pipe")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except subprocess.CalledProcessError as exc:
        sys.stderr.write(exc.stderr or "")
        sys.stderr.write(exc.stdout or "")
        raise
