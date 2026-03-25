# PA1 Search Engine Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Build a Linux C program in `PA1/` that indexes a large ASCII text file with Unix file I/O and answers the four PA1 query modes until `PA1EXIT`.

**Architecture:** Stream the input file once to build two temporary on-disk indexes: a line-offset table for random line access and hash-partitioned occurrence files for word postings. Keep only the lexicon and small query-time working sets in memory so the source file is never fully loaded. Phrase queries will verify exact matches against the original line text using the line-offset table; the other query types will be answered from postings alone.

**Tech Stack:** C, POSIX file I/O (`open`, `read`, `write`, `lseek`, `close`, `unlink`), custom hash table, Makefile

---

### Task 1: Skeleton and low-level utilities

**Files:**
- Create: `PA1/pa1.h`
- Create: `PA1/main.c`
- Create: `PA1/io.c`
- Create: `PA1/util.c`
- Create: `PA1/Makefile`

**Step 1: Write the failing build**

Run: `make -C PA1`
Expected: FAIL because source files do not exist yet

**Step 2: Add minimal program skeleton**

Define:
- custom integer typedefs / structs in `PA1/pa1.h`
- basic string, ASCII-lowercase, buffered output, and line-reader helpers
- `main()` argument validation and `PA1EXIT` loop

**Step 3: Re-run build**

Run: `make -C PA1`
Expected: PASS and produce `PA1/pa1`

### Task 2: File indexing

**Files:**
- Modify: `PA1/pa1.h`
- Modify: `PA1/main.c`
- Create: `PA1/index.c`

**Step 1: Write a failing execution check**

Run: `printf 'PA1EXIT\n' | PA1/pa1 instructions/sample.txt`
Expected: FAIL until indexing and program flow are wired

**Step 2: Build the indexer**

Implement:
- streaming read of the input text file
- per-line bookkeeping with start offsets stored in a temp file
- word parsing on space/tab-delimited tokens
- lowercase lexicon entries with bucket selection
- append-only occurrence records `(word_id, line_no, start_idx)` into partition files

**Step 3: Re-run the execution check**

Run: `printf 'PA1EXIT\n' | PA1/pa1 instructions/sample.txt`
Expected: PASS with no output

### Task 3: Query execution

**Files:**
- Modify: `PA1/pa1.h`
- Modify: `PA1/main.c`
- Create: `PA1/query.c`

**Step 1: Write failing end-to-end checks for each query class**

Run commands against a small fixture that cover:
- single word positions
- multi-word line intersection
- phrase exact matching with repeated hits
- `word1*word2` order-sensitive line matching

Expected: FAIL before query code exists

**Step 2: Implement the minimal query handlers**

Implement:
- query classification
- postings scan for a single word
- postings-based line-set intersection for multi-word queries
- phrase verification against fetched lines
- ordered same-line pattern detection
- exact output formatting including blank-result lines

**Step 3: Re-run the checks**

Run the same commands
Expected: PASS

### Task 4: Verification assets

**Files:**
- Create: `PA1/sample_input.txt`
- Create: `PA1/sample_queries.txt`
- Create: `PA1/sample_expected.txt`

**Step 1: Add a compact fixture**

Include cases for:
- punctuation-sensitive words
- case-insensitive word matching
- empty-result lines
- overlapping phrase matches
- pattern order sensitivity

**Step 2: Add a reproducible verification command**

Run: `PA1/pa1 PA1/sample_input.txt < PA1/sample_queries.txt | diff -u PA1/sample_expected.txt -`
Expected: PASS

### Task 5: Final build verification

**Files:**
- Modify: `PA1/Makefile` (if needed)

**Step 1: Clean rebuild**

Run: `make -C PA1 clean && make -C PA1`
Expected: PASS

**Step 2: End-to-end verification**

Run:
- `PA1/pa1 PA1/sample_input.txt < PA1/sample_queries.txt`
- one additional ad hoc query batch for whitespace / punctuation edge cases

Expected: Outputs match expected behavior exactly

**Step 3: Cleanup review**

Check that:
- temp files are removed on exit
- only allowed system headers plus at least one hand-written header are used
- output formatting still prints one line per query even for no matches
