# PA1/more_tests

Stress-test harness for `PA1/pa1`.

Usage:

```bash
python3 PA1/more_tests/run_tests.py
```

What it does:

- creates deterministic edge cases and random cases under `PA1/more_tests/generated/`
- writes matching query files and oracle answers
- builds `PA1/pa1`
- runs the binary on every generated case
- reports pass/fail and leaves `*_actual.txt` only for failures
