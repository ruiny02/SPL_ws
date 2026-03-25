#include "pa1.h"

typedef int (*OccConsumer)(const OccRecord *record, void *ctx);

typedef struct {
    OutBuf *out;
} StreamWordCtx;

typedef struct {
    MatchVec *matches;
} CollectMatchesCtx;

typedef struct {
    U32Vec *lines;
} CollectLinesCtx;

typedef struct {
    Index *index;
    OutBuf *out;
    ByteVec *line_buf;
    const char *phrase;
    size_t phrase_len;
    u32 cached_line_no;
} PhraseCtx;

static int emit_blank_line(OutBuf *out) {
    return outbuf_write_byte(out, '\n');
}

static int emit_line_ref(OutBuf *out, u32 line_no, u32 start_idx) {
    if (outbuf_write_u32(out, line_no) < 0) {
        return -1;
    }
    if (outbuf_write_byte(out, ':') < 0) {
        return -1;
    }
    if (outbuf_write_u32(out, start_idx) < 0) {
        return -1;
    }
    return outbuf_write_byte(out, ' ');
}

static int emit_line_no(OutBuf *out, u32 line_no) {
    if (outbuf_write_u32(out, line_no) < 0) {
        return -1;
    }
    return outbuf_write_byte(out, ' ');
}

static int scan_bucket_for_word(Index *index, u32 bucket, u32 word_id, OccConsumer consumer, void *ctx) {
    int fd = index->occ_fds[bucket];
    union {
        OccRecord records[1024];
        char bytes[1024 * sizeof(OccRecord)];
    } chunk;
    size_t carry = 0;

    if (lseek(fd, 0, SEEK_SET) < 0) {
        return -1;
    }

    while (1) {
        size_t total;
        size_t usable;
        size_t count;
        size_t i;
        ssize_t got;

        do {
            got = read(fd, chunk.bytes + carry, sizeof(chunk.bytes) - carry);
        } while (got < 0 && errno == EINTR);

        if (got < 0) {
            return -1;
        }
        if (got == 0) {
            break;
        }

        total = carry + (size_t) got;
        usable = (total / sizeof(OccRecord)) * sizeof(OccRecord);
        count = usable / sizeof(OccRecord);

        for (i = 0; i < count; ++i) {
            if (chunk.records[i].word_id == word_id && consumer(&chunk.records[i], ctx) < 0) {
                return -1;
            }
        }

        carry = total - usable;
        if (carry > 0) {
            move_bytes(chunk.bytes, chunk.bytes + usable, carry);
        }
    }

    return carry == 0 ? 0 : -1;
}

static int stream_word_consumer(const OccRecord *record, void *ctx) {
    StreamWordCtx *state = (StreamWordCtx *) ctx;
    return emit_line_ref(state->out, record->line_no, record->start_idx);
}

static int collect_matches_consumer(const OccRecord *record, void *ctx) {
    CollectMatchesCtx *state = (CollectMatchesCtx *) ctx;
    return matchvec_push(state->matches, record->line_no, record->start_idx);
}

static int collect_lines_consumer(const OccRecord *record, void *ctx) {
    CollectLinesCtx *state = (CollectLinesCtx *) ctx;
    if (state->lines->len == 0 || state->lines->items[state->lines->len - 1] != record->line_no) {
        return u32vec_push(state->lines, record->line_no);
    }
    return 0;
}

static int phrase_consumer(const OccRecord *record, void *ctx) {
    PhraseCtx *state = (PhraseCtx *) ctx;

    if (state->cached_line_no != record->line_no) {
        if (index_read_line(state->index, record->line_no, state->line_buf) < 0) {
            return -1;
        }
        state->cached_line_no = record->line_no;
    }

    if ((size_t) record->start_idx + state->phrase_len > state->line_buf->len) {
        return 0;
    }

    if (!phrase_equal_bytes(state->line_buf->data + record->start_idx, state->phrase, state->phrase_len)) {
        return 0;
    }

    return emit_line_ref(state->out, record->line_no, record->start_idx);
}

void query_scratch_init(QueryScratch *scratch) {
    scratch->line_buf.data = NULL;
    scratch->line_buf.len = 0;
    scratch->line_buf.cap = 0;
    scratch->matches_a.items = NULL;
    scratch->matches_a.len = 0;
    scratch->matches_a.cap = 0;
    scratch->matches_b.items = NULL;
    scratch->matches_b.len = 0;
    scratch->matches_b.cap = 0;
    scratch->lines_a.items = NULL;
    scratch->lines_a.len = 0;
    scratch->lines_a.cap = 0;
    scratch->lines_b.items = NULL;
    scratch->lines_b.len = 0;
    scratch->lines_b.cap = 0;
    scratch->lines_tmp.items = NULL;
    scratch->lines_tmp.len = 0;
    scratch->lines_tmp.cap = 0;
}

void query_scratch_free(QueryScratch *scratch) {
    bytevec_free(&scratch->line_buf);
    matchvec_free(&scratch->matches_a);
    matchvec_free(&scratch->matches_b);
    u32vec_free(&scratch->lines_a);
    u32vec_free(&scratch->lines_b);
    u32vec_free(&scratch->lines_tmp);
}

static int output_line_list(const U32Vec *lines, OutBuf *out) {
    size_t i;

    for (i = 0; i < lines->len; ++i) {
        if (emit_line_no(out, lines->items[i]) < 0) {
            return -1;
        }
    }

    return emit_blank_line(out);
}

static int load_unique_lines(Index *index, u32 word_id, u32 bucket, U32Vec *out) {
    CollectLinesCtx ctx;

    u32vec_reset(out);
    ctx.lines = out;
    return scan_bucket_for_word(index, bucket, word_id, collect_lines_consumer, &ctx);
}

static int load_matches(Index *index, u32 word_id, u32 bucket, MatchVec *out) {
    CollectMatchesCtx ctx;

    matchvec_reset(out);
    ctx.matches = out;
    return scan_bucket_for_word(index, bucket, word_id, collect_matches_consumer, &ctx);
}

static int intersect_line_lists(const U32Vec *left, const U32Vec *right, U32Vec *out) {
    size_t i = 0;
    size_t j = 0;

    u32vec_reset(out);

    while (i < left->len && j < right->len) {
        if (left->items[i] == right->items[j]) {
            if (u32vec_push(out, left->items[i]) < 0) {
                return -1;
            }
            i += 1;
            j += 1;
        } else if (left->items[i] < right->items[j]) {
            i += 1;
        } else {
            j += 1;
        }
    }

    return 0;
}

static int handle_single(Index *index, const char *query, size_t len, OutBuf *out) {
    StreamWordCtx ctx;
    u32 word_id;
    u32 bucket;

    if (len == 0) {
        return emit_blank_line(out);
    }
    if (!lexicon_find_existing(&index->lexicon, query, len, &word_id, &bucket)) {
        return emit_blank_line(out);
    }

    ctx.out = out;
    if (scan_bucket_for_word(index, bucket, word_id, stream_word_consumer, &ctx) < 0) {
        return -1;
    }

    return emit_blank_line(out);
}

static int handle_phrase(Index *index, const char *query, size_t len, OutBuf *out, QueryScratch *scratch) {
    PhraseCtx ctx;
    size_t word_end = 0;
    u32 word_id;
    u32 bucket;

    if (len < 2 || query[0] != '"' || query[len - 1] != '"') {
        return emit_blank_line(out);
    }

    query += 1;
    len -= 2;

    while (word_end < len && !is_word_sep(query[word_end])) {
        word_end += 1;
    }
    if (word_end == 0) {
        return emit_blank_line(out);
    }

    if (!lexicon_find_existing(&index->lexicon, query, word_end, &word_id, &bucket)) {
        return emit_blank_line(out);
    }

    ctx.index = index;
    ctx.out = out;
    ctx.line_buf = &scratch->line_buf;
    ctx.phrase = query;
    ctx.phrase_len = len;
    ctx.cached_line_no = 0;

    if (scan_bucket_for_word(index, bucket, word_id, phrase_consumer, &ctx) < 0) {
        return -1;
    }

    return emit_blank_line(out);
}

static int collect_query_words(Index *index, const char *query, size_t len, u32 *word_ids, u32 *buckets, size_t *count) {
    size_t pos = 0;
    size_t out_count = 0;

    while (pos < len) {
        size_t start = pos;
        size_t end;
        size_t i;
        int duplicate = 0;
        u32 word_id;
        u32 bucket;

        while (start < len && is_word_sep(query[start])) {
            start += 1;
        }
        if (start >= len) {
            break;
        }

        end = start;
        while (end < len && !is_word_sep(query[end])) {
            end += 1;
        }

        if (!lexicon_find_existing(&index->lexicon, query + start, end - start, &word_id, &bucket)) {
            return 0;
        }

        for (i = 0; i < out_count; ++i) {
            if (word_ids[i] == word_id) {
                duplicate = 1;
                break;
            }
        }

        if (!duplicate) {
            word_ids[out_count] = word_id;
            buckets[out_count] = bucket;
            out_count += 1;
        }

        pos = end;
    }

    *count = out_count;
    return 1;
}

static int handle_multi(Index *index, const char *query, size_t len, OutBuf *out, QueryScratch *scratch) {
    u32 word_ids[(PA1_QUERY_LIMIT / 2) + 1];
    u32 buckets[(PA1_QUERY_LIMIT / 2) + 1];
    size_t word_count = 0;
    size_t i;
    U32Vec *current = &scratch->lines_a;
    U32Vec *next = &scratch->lines_b;
    U32Vec *tmp = &scratch->lines_tmp;

    if (!collect_query_words(index, query, len, word_ids, buckets, &word_count)) {
        return emit_blank_line(out);
    }
    if (word_count == 0) {
        return emit_blank_line(out);
    }

    if (load_unique_lines(index, word_ids[0], buckets[0], current) < 0) {
        return -1;
    }

    for (i = 1; i < word_count; ++i) {
        if (load_unique_lines(index, word_ids[i], buckets[i], next) < 0) {
            return -1;
        }
        if (intersect_line_lists(current, next, tmp) < 0) {
            return -1;
        }

        {
            U32Vec *swap = current;
            current = tmp;
            tmp = swap;
        }
    }

    return output_line_list(current, out);
}

static int has_space_or_tab(const char *data, size_t len) {
    size_t i;

    for (i = 0; i < len; ++i) {
        if (is_word_sep(data[i])) {
            return 1;
        }
    }

    return 0;
}

static int handle_pattern(Index *index, const char *query, size_t len, OutBuf *out, QueryScratch *scratch) {
    size_t star = 0;
    u32 left_id;
    u32 left_bucket;
    u32 right_id;
    u32 right_bucket;
    size_t i = 0;
    size_t j = 0;

    while (star < len && query[star] != '*') {
        star += 1;
    }

    if (star == 0 || star + 1 >= len || star == len || contains_char(query + star + 1, len - star - 1, '*')) {
        return emit_blank_line(out);
    }
    if (has_space_or_tab(query, len)) {
        return emit_blank_line(out);
    }

    if (!lexicon_find_existing(&index->lexicon, query, star, &left_id, &left_bucket)) {
        return emit_blank_line(out);
    }
    if (!lexicon_find_existing(&index->lexicon, query + star + 1, len - star - 1, &right_id, &right_bucket)) {
        return emit_blank_line(out);
    }

    if (load_matches(index, left_id, left_bucket, &scratch->matches_a) < 0) {
        return -1;
    }
    if (load_matches(index, right_id, right_bucket, &scratch->matches_b) < 0) {
        return -1;
    }

    while (i < scratch->matches_a.len && j < scratch->matches_b.len) {
        Match *a = &scratch->matches_a.items[i];
        Match *b = &scratch->matches_b.items[j];

        if (a->line_no < b->line_no) {
            u32 line = a->line_no;
            while (i < scratch->matches_a.len && scratch->matches_a.items[i].line_no == line) {
                i += 1;
            }
            continue;
        }
        if (a->line_no > b->line_no) {
            u32 line = b->line_no;
            while (j < scratch->matches_b.len && scratch->matches_b.items[j].line_no == line) {
                j += 1;
            }
            continue;
        }

        {
            u32 line = a->line_no;
            u32 first_left = a->start_idx;
            u32 last_right = b->start_idx;

            while (i < scratch->matches_a.len && scratch->matches_a.items[i].line_no == line) {
                i += 1;
            }
            while (j < scratch->matches_b.len && scratch->matches_b.items[j].line_no == line) {
                last_right = scratch->matches_b.items[j].start_idx;
                j += 1;
            }

            if (first_left < last_right && emit_line_no(out, line) < 0) {
                return -1;
            }
        }
    }

    return emit_blank_line(out);
}

int handle_query(Index *index, const char *query, size_t len, OutBuf *out, QueryScratch *scratch) {
    size_t spaces = 0;
    size_t i;

    matchvec_reset(&scratch->matches_a);
    matchvec_reset(&scratch->matches_b);
    u32vec_reset(&scratch->lines_a);
    u32vec_reset(&scratch->lines_b);
    u32vec_reset(&scratch->lines_tmp);

    if (contains_char(query, len, '"')) {
        return handle_phrase(index, query, len, out, scratch);
    }
    if (contains_char(query, len, '*')) {
        return handle_pattern(index, query, len, out, scratch);
    }

    for (i = 0; i < len; ++i) {
        if (is_word_sep(query[i])) {
            spaces += 1;
        }
    }

    if (spaces == 0) {
        return handle_single(index, query, len, out);
    }
    return handle_multi(index, query, len, out, scratch);
}
