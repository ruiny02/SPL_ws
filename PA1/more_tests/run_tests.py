#!/usr/bin/env python3
from __future__ import annotations

import random
import shutil
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path


PA1_DIR = Path(__file__).resolve().parents[1]
BIN = PA1_DIR / "pa1"
OUT_DIR = PA1_DIR / "more_tests" / "generated"
SEED = 20260325
RANDOM_CASES = 36
RANDOM_QUERIES_PER_CASE = 96


@dataclass
class Case:
    name: str
    text: str
    queries: list[str]


def normalize(word: str) -> str:
    return word.lower()


def is_sep(ch: str) -> bool:
    return ch in (" ", "\t")


def tokenize_line(line: str) -> list[tuple[str, int]]:
    tokens: list[tuple[str, int]] = []
    i = 0
    while i < len(line):
        while i < len(line) and is_sep(line[i]):
            i += 1
        if i >= len(line):
            break
        start = i
        while i < len(line) and not is_sep(line[i]):
            i += 1
        tokens.append((line[start:i], start))
    return tokens


def classify_query(query: str) -> str:
    if '"' in query:
        return "phrase"
    if "*" in query:
        return "pattern"
    parts = split_query_words(query)
    if len(parts) > 1:
        return "multi"
    return "single"


def split_query_words(query: str) -> list[str]:
    words: list[str] = []
    pos = 0
    while pos < len(query):
        while pos < len(query) and is_sep(query[pos]):
            pos += 1
        if pos >= len(query):
            break
        start = pos
        while pos < len(query) and not is_sep(query[pos]):
            pos += 1
        words.append(query[start:pos])
    return words


def format_result(items: list[str]) -> str:
    if not items:
        return "\n"
    return " ".join(items) + " \n"


def solve_single(lines: list[str], query: str) -> str:
    words = split_query_words(query)
    if len(words) != 1:
        return "\n"
    needle = normalize(words[0])
    hits: list[str] = []
    for line_no, line in enumerate(lines, start=1):
        for token, start in tokenize_line(line):
            if normalize(token) == needle:
                hits.append(f"{line_no}:{start}")
    return format_result(hits)


def solve_multi(lines: list[str], query: str) -> str:
    wanted = {normalize(part) for part in split_query_words(query)}
    hits: list[str] = []
    for line_no, line in enumerate(lines, start=1):
        present = {normalize(token) for token, _ in tokenize_line(line)}
        if wanted.issubset(present):
            hits.append(str(line_no))
    return format_result(hits)


def phrase_valid_end(line: str, end: int) -> bool:
    return end == len(line) or is_sep(line[end])


def solve_phrase(lines: list[str], query: str) -> str:
    phrase = query[1:-1]
    if not phrase or is_sep(phrase[0]) or is_sep(phrase[-1]) or '"' in phrase:
        return "\n"
    first_word_end = 0
    while first_word_end < len(phrase) and not is_sep(phrase[first_word_end]):
        first_word_end += 1
    if first_word_end == 0:
        return "\n"
    first_word = normalize(phrase[:first_word_end])

    hits: list[str] = []
    for line_no, line in enumerate(lines, start=1):
        for token, start in tokenize_line(line):
            end = start + len(phrase)
            if normalize(token) != first_word:
                continue
            if end > len(line):
                continue
            if line[start:end].lower() != phrase.lower():
                continue
            if not phrase_valid_end(line, end):
                continue
            hits.append(f"{line_no}:{start}")
    return format_result(hits)


def solve_pattern(lines: list[str], query: str) -> str:
    if " " in query or "\t" in query:
        return "\n"
    if query.count("*") != 1:
        return "\n"
    left, right = query.split("*", 1)
    if not left or not right:
        return "\n"
    left = normalize(left)
    right = normalize(right)
    hits: list[str] = []
    for line_no, line in enumerate(lines, start=1):
        words = [(normalize(token), start) for token, start in tokenize_line(line)]
        matched = False
        first_left_start: int | None = None
        last_right_start: int | None = None

        for word, start in words:
            if word == left and first_left_start is None:
                first_left_start = start
            if word == right:
                last_right_start = start

        if first_left_start is not None and last_right_start is not None and first_left_start < last_right_start:
            matched = True
        if matched:
            hits.append(str(line_no))
    return format_result(hits)


def oracle(text: str, queries: list[str]) -> str:
    lines = text.splitlines()
    out_parts: list[str] = []
    for query in queries:
        qtype = classify_query(query)
        if qtype == "single":
            out_parts.append(solve_single(lines, query))
        elif qtype == "multi":
            out_parts.append(solve_multi(lines, query))
        elif qtype == "phrase":
            out_parts.append(solve_phrase(lines, query))
        else:
            out_parts.append(solve_pattern(lines, query))
    return "".join(out_parts)


def random_case(rng: random.Random, index: int) -> Case:
    vocab = [
        "aladdin", "jasmine", "genie", "lamp", "wish", "magic", "carpet",
        "castle", "market", "cave", "guard", "friend", "can't", "score:",
        "wide-awake", "priests'", "sons'", "hello!", "world?", "i",
        "i'm", "ice", "brother's", "dream", "moon", "sun", "river",
    ]
    separators = [" ", " ", " ", "  ", "\t", "\t ", " \t"]

    def style(word: str) -> str:
        mode = rng.randrange(4)
        if mode == 0:
            return word.lower()
        if mode == 1:
            return word.upper()
        if mode == 2:
            return word.title()
        return "".join(ch.upper() if i % 2 else ch.lower() for i, ch in enumerate(word))

    lines: list[str] = []
    for _ in range(rng.randint(35, 70)):
        if rng.random() < 0.12:
            lines.append("")
            continue
        token_count = rng.randint(1, 8)
        parts = [style(rng.choice(vocab)) for _ in range(token_count)]
        line = parts[0]
        for token in parts[1:]:
            line += rng.choice(separators) + token
        lines.append(line)

    tokenized = [tokenize_line(line) for line in lines]
    all_words = sorted({normalize(token) for tokens in tokenized for token, _ in tokens})
    present_multi_sources = [tokens for tokens in tokenized if len(tokens) >= 2]
    present_phrase_sources = [line for line, tokens in zip(lines, tokenized) if len(tokens) >= 2]

    queries: list[str] = []

    while len(queries) < RANDOM_QUERIES_PER_CASE:
        kind = rng.choice(["single", "multi", "phrase", "pattern"])

        if kind == "single":
            if rng.random() < 0.7 and all_words:
                queries.append(rng.choice(all_words))
            else:
                queries.append(f"absent_{index}_{len(queries)}")
            continue

        if kind == "multi":
            if rng.random() < 0.7 and present_multi_sources:
                tokens = rng.choice(present_multi_sources)
                picks = rng.sample(tokens, k=min(len(tokens), rng.randint(2, 3)))
                queries.append(" ".join(normalize(token) for token, _ in picks))
            else:
                queries.append(f"absent_{index}_{len(queries)} {rng.choice(all_words) if all_words else 'ghost'}")
            continue

        if kind == "phrase":
            if rng.random() < 0.7 and present_phrase_sources:
                line = rng.choice(present_phrase_sources)
                tokens = tokenize_line(line)
                start_idx = rng.randrange(0, len(tokens) - 1)
                end_idx = rng.randrange(start_idx + 1, len(tokens))
                start = tokens[start_idx][1]
                end = tokens[end_idx][1] + len(tokens[end_idx][0])
                queries.append(f"\"{line[start:end]}\"")
            else:
                queries.append("\"absent phrase\"")
            continue

        if rng.random() < 0.7 and present_multi_sources:
            tokens = rng.choice(present_multi_sources)
            if len(tokens) >= 2:
                left_i = rng.randrange(0, len(tokens) - 1)
                right_i = rng.randrange(left_i + 1, len(tokens))
                queries.append(f"{normalize(tokens[left_i][0])}*{normalize(tokens[right_i][0])}")
            else:
                queries.append(f"{normalize(tokens[0][0])}*ghost")
        else:
            queries.append(f"{rng.choice(all_words) if all_words else 'ghost'}*absent_{index}_{len(queries)}")

    return Case(
        name=f"random_{index:02d}",
        text="\n".join(lines) + "\n",
        queries=queries,
    )


def edge_cases() -> list[Case]:
    return [
        Case(
            name="edge_phrase_boundary",
            text="magic carpet!\nMAGIC CARPET and\nmagic carpet.\nmagic carpet ride\n",
            queries=["\"magic carpet\"", "magic", "magic carpet", "magic*carpet"],
        ),
        Case(
            name="edge_tabs_and_spaces",
            text="first\tlast first  last\nfirst last\n",
            queries=["\"first\tlast\"", "\"first  last\"", "\"first last\"", "first last"],
        ),
        Case(
            name="edge_empty_lines",
            text="Alpha\n\nbeta Beta\n\tbeta\n",
            queries=["alpha", "beta", "alpha beta", "\"beta Beta\"", "beta*beta"],
        ),
        Case(
            name="edge_overlap",
            text="HA HA HA HA\nha\tha\n",
            queries=["\"HA HA\"", "\"ha\tha\"", "ha", "ha*ha"],
        ),
        Case(
            name="edge_punctuation",
            text="I I'm I! Ice\nword word! word?\n",
            queries=["I", "\"word!\"", "word", "\"word\"", "word*word?"],
        ),
        Case(
            name="edge_query_spacing",
            text="alpha\tbeta gamma\nalpha beta\ngamma\talpha\n",
            queries=[" alpha", "alpha ", "\talpha\tbeta", "alpha\tbeta", "alpha  beta", "\" alpha\"", "\"alpha \""],
        ),
        Case(
            name="edge_same_word_pattern",
            text="echo echo\necho\necho\tmiddle\techo\nECHO echo\n",
            queries=["echo*echo", "echo", "echo echo", "\"echo echo\""],
        ),
        Case(
            name="edge_duplicate_words",
            text="lamp lamp genie\nlamp genie\nlamp\n",
            queries=["lamp lamp", "lamp genie genie", "lamp", "\"lamp lamp\"", "lamp*lamp"],
        ),
        Case(
            name="edge_no_final_newline",
            text="last line only\nmagic carpet\nfinal token",
            queries=["final", "\"final token\"", "magic*carpet", "\"magic carpet\""],
        ),
        Case(
            name="edge_long_line",
            text=((" ".join(["alpha", "beta", "gamma", "delta", "alpha", "omega", "beta", "gamma"]) + " ") * 6).strip() + "\nshort line\n",
            queries=["alpha", "alpha beta", "\"alpha beta gamma\"", "alpha*omega", "omega*gamma"],
        ),
    ]


def generate_cases() -> list[Case]:
    rng = random.Random(SEED)
    cases = edge_cases()
    cases.extend(random_case(rng, idx) for idx in range(RANDOM_CASES))
    return cases


def write_cases(cases: list[Case]) -> None:
    if OUT_DIR.exists():
        shutil.rmtree(OUT_DIR)
    OUT_DIR.mkdir(parents=True, exist_ok=True)

    for case in cases:
        (OUT_DIR / f"{case.name}_input.txt").write_text(case.text, encoding="ascii")
        (OUT_DIR / f"{case.name}_queries.txt").write_text("\n".join(case.queries + ["PA1EXIT"]) + "\n", encoding="ascii")
        expected = oracle(case.text, case.queries)
        (OUT_DIR / f"{case.name}_expected.txt").write_text(expected, encoding="ascii")


def build_binary() -> None:
    subprocess.run(["make", "-C", str(PA1_DIR)], check=True)


def run_case(case: Case) -> tuple[bool, str]:
    input_path = OUT_DIR / f"{case.name}_input.txt"
    query_path = OUT_DIR / f"{case.name}_queries.txt"
    expected_path = OUT_DIR / f"{case.name}_expected.txt"
    actual_path = OUT_DIR / f"{case.name}_actual.txt"

    with query_path.open("rb") as query_file:
        result = subprocess.run(
            [str(BIN), str(input_path)],
            stdin=query_file,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            check=False,
        )

    actual_path.write_bytes(result.stdout)
    expected = expected_path.read_bytes()
    if result.returncode != 0:
        return False, f"{case.name}: process exited with {result.returncode}\n{result.stderr.decode('utf-8', 'replace')}"
    if result.stdout != expected:
        return False, (
            f"{case.name}: output mismatch\n"
            f"input={input_path}\nqueries={query_path}\nexpected={expected_path}\nactual={actual_path}"
        )
    actual_path.unlink(missing_ok=True)
    return True, f"{case.name}: ok"


def main() -> int:
    cases = generate_cases()
    write_cases(cases)
    build_binary()

    failures: list[str] = []
    for case in cases:
        ok, message = run_case(case)
        print(message)
        if not ok:
            failures.append(message)

    print(f"\nsummary: {len(cases) - len(failures)}/{len(cases)} cases passed")
    if failures:
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
