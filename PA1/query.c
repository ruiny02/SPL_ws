#include "pa1.h"

// Generic callback type used while scanning occurrence records.
typedef int (*OccScanFn)(const OccRecord *record, void *arg);

typedef struct {
    OutBuf *out;
} WordStream;

typedef struct {
    Index *index;
    OutBuf *out;
    ByteVec *line_buf;
    const char *phrase;
    size_t phrase_len;
    u32 cached_line_no;
} PhraseMatch;

typedef struct {
    OutBuf out;
    u32 count;
} CacheWriter;

typedef struct {
    int fd;
    OccRecord records[1024];
    size_t pos;
    size_t len;
    OccRecord current;
    int has_record;
} OccurrenceReader;

typedef struct {
    int fd;
    u32 values[2048];
    size_t pos;
    size_t len;
    u32 current;
    int has_value;
} LineNumberReader;

typedef enum {
    LINE_SOURCE_WORD_CACHE,
    LINE_SOURCE_LINE_FILE
} LineSourceKind;

typedef struct {
    LineSourceKind kind;
    OccurrenceReader occurrence_reader;
    LineNumberReader line_reader;
    u32 current;
    int has_value;
} LineNumberSource;

// Print an empty result line for a query with no matches.
static int emit_blank_line(OutBuf *out) {
    return outbuf_write_byte(out, '\n');
}

// Print one "line:start" match entry followed by a space.
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

// Print one matching line number followed by a space.
static int emit_line_no(OutBuf *out, u32 line_no) {
    if (outbuf_write_u32(out, line_no) < 0) {
        return -1;
    }
    return outbuf_write_byte(out, ' ');
}

// Scan one bucket file and call the consumer for matching word records.
static int scan_bucket_for_word(Index *index, u32 bucket, u32 word_id, OccScanFn fn, void *arg) {
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
            if (chunk.records[i].word_id == word_id && fn(&chunk.records[i], arg) < 0) {
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

// Append one occurrence record into a per-word cache file.
static int cache_record_consumer(const OccRecord *record, void *arg) {
    CacheWriter *writer = (CacheWriter *) arg;
    writer->count += 1;
    return outbuf_write_data(&writer->out, record, sizeof(*record));
}

// Materialize one word's postings into a dedicated temp file on demand.
static int ensure_word_cache(Index *index, u32 word_id, u32 bucket) {
    LexEntry *entry = &index->lexicon.entries[word_id];
    CacheWriter writer;
    char path[64];
    char *stored_path;
    size_t path_len;
    int fd;

    if (entry->cache_path != NULL) {
        return 0;
    }

    fd = create_named_temp_file("wcache", path, sizeof(path));
    if (fd < 0) {
        return -1;
    }

    outbuf_init(&writer.out, fd);
    writer.count = 0;
    if (scan_bucket_for_word(index, bucket, word_id, cache_record_consumer, &writer) < 0 ||
        outbuf_flush(&writer.out) < 0) {
        unlink(path);
        close(fd);
        return -1;
    }
    if (close(fd) < 0) {
        unlink(path);
        return -1;
    }

    path_len = cstr_len(path);
    stored_path = (char *) malloc(path_len + 1);
    if (stored_path == NULL) {
        unlink(path);
        return -1;
    }

    copy_bytes(stored_path, path, path_len + 1);
    entry->cache_path = stored_path;
    entry->cache_count = writer.count;
    return 0;
}

// Scan the dedicated cache file for one word.
static int scan_cached_word(Index *index, u32 word_id, OccScanFn fn, void *arg) {
    LexEntry *entry = &index->lexicon.entries[word_id];
    union {
        OccRecord records[1024];
        char bytes[1024 * sizeof(OccRecord)];
    } chunk;
    size_t carry = 0;
    int status = 0;
    int fd;

    (void) index;

    if (entry->cache_path == NULL) {
        return -1;
    }

    fd = open(entry->cache_path, O_RDONLY);
    if (fd < 0) {
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
            status = -1;
            break;
        }
        if (got == 0) {
            break;
        }

        total = carry + (size_t) got;
        usable = (total / sizeof(OccRecord)) * sizeof(OccRecord);
        count = usable / sizeof(OccRecord);

        for (i = 0; i < count; ++i) {
            if (fn(&chunk.records[i], arg) < 0) {
                status = -1;
                break;
            }
        }
        if (status < 0) {
            break;
        }

        carry = total - usable;
        if (carry > 0) {
            move_bytes(chunk.bytes, chunk.bytes + usable, carry);
        }
    }

    if (status == 0 && carry != 0) {
        status = -1;
    }
    if (close(fd) < 0) {
        status = -1;
    }
    return status;
}

// Read occurrence records sequentially from one cached postings file.
static int occurrence_reader_open_path(OccurrenceReader *reader, const char *path) {
    reader->fd = open(path, O_RDONLY);
    if (reader->fd < 0) {
        return -1;
    }
    reader->pos = 0;
    reader->len = 0;
    reader->has_record = 0;
    return 0;
}

// Advance to the next occurrence record in a cached postings file.
static int occurrence_reader_next(OccurrenceReader *reader) {
    if (reader->pos == reader->len) {
        ssize_t got;

        do {
            got = read(reader->fd, reader->records, sizeof(reader->records));
        } while (got < 0 && errno == EINTR);

        if (got < 0) {
            return -1;
        }
        if (got == 0) {
            reader->has_record = 0;
            return 0;
        }
        if (((size_t) got % sizeof(OccRecord)) != 0) {
            reader->has_record = 0;
            errno = EINVAL;
            return -1;
        }

        reader->pos = 0;
        reader->len = (size_t) got / sizeof(OccRecord);
    }

    reader->current = reader->records[reader->pos];
    reader->pos += 1;
    reader->has_record = 1;
    return 1;
}

// Close one occurrence reader.
static int occurrence_reader_close(OccurrenceReader *reader) {
    int status = 0;

    if (reader->fd >= 0 && close(reader->fd) < 0) {
        status = -1;
    }
    reader->fd = -1;
    reader->pos = 0;
    reader->len = 0;
    reader->has_record = 0;
    return status;
}

// Read line numbers sequentially from a temporary line-set file.
static int line_number_reader_open_fd(LineNumberReader *reader, int fd) {
    if (lseek(fd, 0, SEEK_SET) < 0) {
        return -1;
    }

    reader->fd = fd;
    reader->pos = 0;
    reader->len = 0;
    reader->has_value = 0;
    return 0;
}

// Advance to the next line number in a temporary line-set file.
static int line_number_reader_next(LineNumberReader *reader) {
    if (reader->pos == reader->len) {
        ssize_t got;

        do {
            got = read(reader->fd, reader->values, sizeof(reader->values));
        } while (got < 0 && errno == EINTR);

        if (got < 0) {
            return -1;
        }
        if (got == 0) {
            reader->has_value = 0;
            return 0;
        }
        if (((size_t) got % sizeof(u32)) != 0) {
            reader->has_value = 0;
            errno = EINVAL;
            return -1;
        }

        reader->pos = 0;
        reader->len = (size_t) got / sizeof(u32);
    }

    reader->current = reader->values[reader->pos];
    reader->pos += 1;
    reader->has_value = 1;
    return 1;
}

// Open a line-number source over one cached word postings file.
static int line_number_source_open_word(Index *index, u32 word_id, u32 bucket, LineNumberSource *source) {
    LexEntry *entry = &index->lexicon.entries[word_id];

    if (ensure_word_cache(index, word_id, bucket) < 0) {
        return -1;
    }
    if (entry->cache_path == NULL) {
        return -1;
    }
    if (occurrence_reader_open_path(&source->occurrence_reader, entry->cache_path) < 0) {
        return -1;
    }

    source->kind = LINE_SOURCE_WORD_CACHE;
    source->has_value = 0;
    return 0;
}

// Open a line-number source over one temporary line-set file.
static int line_number_source_open_file(LineNumberSource *source, int fd) {
    if (line_number_reader_open_fd(&source->line_reader, fd) < 0) {
        return -1;
    }

    source->kind = LINE_SOURCE_LINE_FILE;
    source->has_value = 0;
    return 0;
}

// Advance to the next unique line number from a line-number source.
static int line_number_source_next(LineNumberSource *source) {
    if (source->kind == LINE_SOURCE_WORD_CACHE) {
        u32 line_no;

        if (!source->occurrence_reader.has_record) {
            int status = occurrence_reader_next(&source->occurrence_reader);
            if (status <= 0) {
                source->has_value = 0;
                return status;
            }
        }

        line_no = source->occurrence_reader.current.line_no;
        source->current = line_no;
        source->has_value = 1;

        while (1) {
            int status = occurrence_reader_next(&source->occurrence_reader);
            if (status < 0) {
                return -1;
            }
            if (status == 0) {
                break;
            }
            if (source->occurrence_reader.current.line_no != line_no) {
                break;
            }
        }

        return 1;
    }

    {
        int status = line_number_reader_next(&source->line_reader);
        if (status <= 0) {
            source->has_value = 0;
            return status;
        }
    }

    source->current = source->line_reader.current;
    source->has_value = 1;
    return 1;
}

// Close one line-number source.
static int line_number_source_close(LineNumberSource *source) {
    if (source->kind == LINE_SOURCE_WORD_CACHE) {
        return occurrence_reader_close(&source->occurrence_reader);
    }

    source->line_reader.fd = -1;
    source->line_reader.pos = 0;
    source->line_reader.len = 0;
    source->line_reader.has_value = 0;
    source->has_value = 0;
    return 0;
}

// Stream all single-word matches directly to the output buffer.
static int stream_word_consumer(const OccRecord *record, void *arg) {
    WordStream *stream = (WordStream *) arg;
    return emit_line_ref(stream->out, record->line_no, record->start_idx);
}

// Check whether a matched phrase ends at whitespace or line end.
static int phrase_has_valid_end(const ByteVec *line_buf, u32 start_idx, size_t phrase_len) {
    size_t end = (size_t) start_idx + phrase_len;

    if (end == line_buf->len) {
        return 1;
    }

    return is_word_sep(line_buf->data[end]);
}

// Verify a phrase candidate against the original line text.
static int phrase_consumer(const OccRecord *record, void *arg) {
    PhraseMatch *match = (PhraseMatch *) arg;

    if (match->cached_line_no != record->line_no) {
        if (index_read_line(match->index, record->line_no, match->line_buf) < 0) {
            return -1;
        }
        match->cached_line_no = record->line_no;
    }

    if ((size_t) record->start_idx + match->phrase_len > match->line_buf->len) {
        return 0;
    }

    if (!phrase_equal_bytes(match->line_buf->data + record->start_idx, match->phrase, match->phrase_len)) {
        return 0;
    }
    if (!phrase_has_valid_end(match->line_buf, record->start_idx, match->phrase_len)) {
        return 0;
    }

    return emit_line_ref(match->out, record->line_no, record->start_idx);
}

// Initialize reusable scratch buffers for query processing.
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

// Reset scratch buffers between queries without always discarding small allocations.
void query_scratch_reset(QueryScratch *scratch) {
    if (scratch->line_buf.cap > (size_t) (1u << 20)) {
        bytevec_free(&scratch->line_buf);
    } else {
        bytevec_reset(&scratch->line_buf);
    }

    matchvec_reset(&scratch->matches_a);
    matchvec_reset(&scratch->matches_b);
    u32vec_reset(&scratch->lines_a);
    u32vec_reset(&scratch->lines_b);
    u32vec_reset(&scratch->lines_tmp);
}

// Free all scratch buffers used during query processing.
void query_scratch_free(QueryScratch *scratch) {
    bytevec_free(&scratch->line_buf);
    matchvec_free(&scratch->matches_a);
    matchvec_free(&scratch->matches_b);
    u32vec_free(&scratch->lines_a);
    u32vec_free(&scratch->lines_b);
    u32vec_free(&scratch->lines_tmp);
}

// Emit every unique line number from one line-number source.
static int output_line_source(LineNumberSource *line_src, OutBuf *out) {
    while (1) {
        int status = line_number_source_next(line_src);
        if (status < 0) {
            return -1;
        }
        if (status == 0) {
            break;
        }
        if (emit_line_no(out, line_src->current) < 0) {
            return -1;
        }
    }

    return emit_blank_line(out);
}

// Intersect two sorted line-number streams and append the result to a line-set file.
static int intersect_line_sources_to_file(LineNumberSource *left, LineNumberSource *right, int output_fd, u32 *match_count) {
    OutBuf out;
    int left_status;
    int right_status;

    if (ftruncate(output_fd, 0) < 0) {
        return -1;
    }
    if (lseek(output_fd, 0, SEEK_SET) < 0) {
        return -1;
    }

    outbuf_init(&out, output_fd);
    *match_count = 0;

    left_status = line_number_source_next(left);
    if (left_status < 0) {
        return -1;
    }
    right_status = line_number_source_next(right);
    if (right_status < 0) {
        return -1;
    }

    while (left_status > 0 && right_status > 0) {
        if (left->current == right->current) {
            if (outbuf_write_data(&out, &left->current, sizeof(left->current)) < 0) {
                return -1;
            }
            *match_count += 1;
            left_status = line_number_source_next(left);
            if (left_status < 0) {
                return -1;
            }
            right_status = line_number_source_next(right);
            if (right_status < 0) {
                return -1;
            }
        } else if (left->current < right->current) {
            left_status = line_number_source_next(left);
            if (left_status < 0) {
                return -1;
            }
        } else {
            right_status = line_number_source_next(right);
            if (right_status < 0) {
                return -1;
            }
        }
    }

    return outbuf_flush(&out);
}

// Intersect two sorted line-number streams and print the result.
static int intersect_line_sources_to_output(LineNumberSource *left, LineNumberSource *right, OutBuf *out, u32 *match_count) {
    int left_status;
    int right_status;

    *match_count = 0;

    left_status = line_number_source_next(left);
    if (left_status < 0) {
        return -1;
    }
    right_status = line_number_source_next(right);
    if (right_status < 0) {
        return -1;
    }

    while (left_status > 0 && right_status > 0) {
        if (left->current == right->current) {
            if (emit_line_no(out, left->current) < 0) {
                return -1;
            }
            *match_count += 1;
            left_status = line_number_source_next(left);
            if (left_status < 0) {
                return -1;
            }
            right_status = line_number_source_next(right);
            if (right_status < 0) {
                return -1;
            }
        } else if (left->current < right->current) {
            left_status = line_number_source_next(left);
            if (left_status < 0) {
                return -1;
            }
        } else {
            right_status = line_number_source_next(right);
            if (right_status < 0) {
                return -1;
            }
        }
    }

    return emit_blank_line(out);
}

// Sort query words by ascending cached posting count to shrink intersections early.
static void sort_query_words_by_cache_size(Index *index, u32 *word_ids, u32 *buckets, size_t count) {
    size_t i;

    for (i = 1; i < count; ++i) {
        u32 word_id = word_ids[i];
        u32 bucket = buckets[i];
        u32 posting_count = index->lexicon.entries[word_id].cache_count;
        size_t j = i;

        while (j > 0) {
            u32 prev_word_id = word_ids[j - 1];
            u32 prev_count = index->lexicon.entries[prev_word_id].cache_count;

            if (prev_count <= posting_count) {
                break;
            }

            word_ids[j] = word_ids[j - 1];
            buckets[j] = buckets[j - 1];
            j -= 1;
        }

        word_ids[j] = word_id;
        buckets[j] = bucket;
    }
}

// Handle a single-word query and print every occurrence.
static int handle_single(Index *index, const char *query, size_t len, OutBuf *out) {
    WordStream stream;
    u32 word_id;
    u32 bucket;

    if (len == 0) {
        return emit_blank_line(out);
    }
    if (!lexicon_find_existing(&index->lexicon, query, len, &word_id, &bucket)) {
        return emit_blank_line(out);
    }

    stream.out = out;
    if (ensure_word_cache(index, word_id, bucket) < 0) {
        return -1;
    }
    if (scan_cached_word(index, word_id, stream_word_consumer, &stream) < 0) {
        return -1;
    }

    return emit_blank_line(out);
}

// Handle a quoted phrase query with exact spacing and order.
static int handle_phrase(Index *index, const char *query, size_t len, OutBuf *out, QueryScratch *scratch) {
    PhraseMatch match;
    size_t word_end = 0;
    u32 word_id;
    u32 bucket;

    if (len < 2 || query[0] != '"' || query[len - 1] != '"') {
        return emit_blank_line(out);
    }

    query += 1;
    len -= 2;

    if (len == 0 || is_word_sep(query[0]) || is_word_sep(query[len - 1]) || contains_char(query, len, '"')) {
        return emit_blank_line(out);
    }

    while (word_end < len && !is_word_sep(query[word_end])) {
        word_end += 1;
    }
    if (word_end == 0) {
        return emit_blank_line(out);
    }

    if (!lexicon_find_existing(&index->lexicon, query, word_end, &word_id, &bucket)) {
        return emit_blank_line(out);
    }

    match.index = index;
    match.out = out;
    match.line_buf = &scratch->line_buf;
    match.phrase = query;
    match.phrase_len = len;
    match.cached_line_no = 0;

    if (ensure_word_cache(index, word_id, bucket) < 0) {
        return -1;
    }
    if (scan_cached_word(index, word_id, phrase_consumer, &match) < 0) {
        return -1;
    }

    return emit_blank_line(out);
}

// Parse a multi-word query into unique lexicon word ids.
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

// Handle a multi-word query by intersecting sorted unique line streams.
static int handle_multi(Index *index, const char *query, size_t len, OutBuf *out) {
    u32 word_ids[(QUERY_LIMIT / 2) + 1];
    u32 buckets[(QUERY_LIMIT / 2) + 1];
    size_t word_count = 0;
    size_t i;
    int current_lines_fd = -1;
    u32 match_count = 0;

    if (!collect_query_words(index, query, len, word_ids, buckets, &word_count)) {
        return emit_blank_line(out);
    }
    if (word_count == 0) {
        return emit_blank_line(out);
    }

    for (i = 0; i < word_count; ++i) {
        if (ensure_word_cache(index, word_ids[i], buckets[i]) < 0) {
            return -1;
        }
    }
    sort_query_words_by_cache_size(index, word_ids, buckets, word_count);

    if (word_count == 1) {
        LineNumberSource source;
        int status;

        if (line_number_source_open_word(index, word_ids[0], buckets[0], &source) < 0) {
            return -1;
        }
        status = output_line_source(&source, out);
        if (line_number_source_close(&source) < 0) {
            return -1;
        }
        return status;
    }

    if (word_count == 2) {
        LineNumberSource left_source;
        LineNumberSource right_source;
        int status;

        if (line_number_source_open_word(index, word_ids[0], buckets[0], &left_source) < 0) {
            return -1;
        }
        if (line_number_source_open_word(index, word_ids[1], buckets[1], &right_source) < 0) {
            line_number_source_close(&left_source);
            return -1;
        }

        status = intersect_line_sources_to_output(&left_source, &right_source, out, &match_count);
        if (line_number_source_close(&left_source) < 0 || line_number_source_close(&right_source) < 0) {
            return -1;
        }
        return status;
    }

    current_lines_fd = create_temp_file("lineset");
    if (current_lines_fd < 0) {
        return -1;
    }

    {
        LineNumberSource left_source;
        LineNumberSource right_source;
        int status;

        if (line_number_source_open_word(index, word_ids[0], buckets[0], &left_source) < 0) {
            close(current_lines_fd);
            return -1;
        }
        if (line_number_source_open_word(index, word_ids[1], buckets[1], &right_source) < 0) {
            line_number_source_close(&left_source);
            close(current_lines_fd);
            return -1;
        }

        status = intersect_line_sources_to_file(&left_source, &right_source, current_lines_fd, &match_count);
        if (line_number_source_close(&left_source) < 0 || line_number_source_close(&right_source) < 0) {
            close(current_lines_fd);
            return -1;
        }
        if (status < 0) {
            close(current_lines_fd);
            return -1;
        }
    }

    if (match_count == 0) {
        close(current_lines_fd);
        return emit_blank_line(out);
    }

    for (i = 2; i < word_count; ++i) {
        LineNumberSource left_source;
        LineNumberSource right_source;
        int last_stage = (i + 1 == word_count);
        int status;

        if (line_number_source_open_file(&left_source, current_lines_fd) < 0) {
            close(current_lines_fd);
            return -1;
        }
        if (line_number_source_open_word(index, word_ids[i], buckets[i], &right_source) < 0) {
            line_number_source_close(&left_source);
            close(current_lines_fd);
            return -1;
        }

        if (last_stage) {
            status = intersect_line_sources_to_output(&left_source, &right_source, out, &match_count);
        } else {
            int next_lines_fd = create_temp_file("lineset");
            if (next_lines_fd < 0) {
                line_number_source_close(&left_source);
                line_number_source_close(&right_source);
                close(current_lines_fd);
                return -1;
            }

            status = intersect_line_sources_to_file(&left_source, &right_source, next_lines_fd, &match_count);
            if (status == 0) {
                close(current_lines_fd);
                current_lines_fd = next_lines_fd;
            } else {
                close(next_lines_fd);
            }
        }

        if (line_number_source_close(&left_source) < 0 || line_number_source_close(&right_source) < 0) {
            close(current_lines_fd);
            return -1;
        }
        if (status < 0) {
            close(current_lines_fd);
            return -1;
        }
        if (match_count == 0) {
            close(current_lines_fd);
            return last_stage ? 0 : emit_blank_line(out);
        }
    }

    close(current_lines_fd);
    return 0;
}

// Check whether a pattern query illegally contains spaces or tabs.
static int has_space_or_tab(const char *data, size_t len) {
    size_t i;

    for (i = 0; i < len; ++i) {
        if (is_word_sep(data[i])) {
            return 1;
        }
    }

    return 0;
}

// Advance one occurrence reader past all records on the current line.
static int skip_occurrence_line_group(OccurrenceReader *reader) {
    u32 line_no;

    if (!reader->has_record) {
        return 0;
    }

    line_no = reader->current.line_no;
    while (reader->has_record && reader->current.line_no == line_no) {
        int status = occurrence_reader_next(reader);
        if (status < 0) {
            return -1;
        }
        if (status == 0) {
            break;
        }
    }

    return 0;
}

// Handle a word1*word2 pattern query on the same line in order.
static int handle_pattern(Index *index, const char *query, size_t len, OutBuf *out) {
    size_t star = 0;
    u32 left_id;
    u32 left_bucket;
    u32 right_id;
    u32 right_bucket;
    OccurrenceReader left_reader;
    OccurrenceReader right_reader;
    int left_status;
    int right_status;
    int result = 0;

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

    if (ensure_word_cache(index, left_id, left_bucket) < 0 || ensure_word_cache(index, right_id, right_bucket) < 0) {
        return -1;
    }
    if (occurrence_reader_open_path(&left_reader, index->lexicon.entries[left_id].cache_path) < 0) {
        return -1;
    }
    if (occurrence_reader_open_path(&right_reader, index->lexicon.entries[right_id].cache_path) < 0) {
        occurrence_reader_close(&left_reader);
        return -1;
    }

    left_status = occurrence_reader_next(&left_reader);
    if (left_status < 0) {
        occurrence_reader_close(&left_reader);
        occurrence_reader_close(&right_reader);
        return -1;
    }
    right_status = occurrence_reader_next(&right_reader);
    if (right_status < 0) {
        occurrence_reader_close(&left_reader);
        occurrence_reader_close(&right_reader);
        return -1;
    }

    while (left_reader.has_record && right_reader.has_record) {
        if (left_reader.current.line_no < right_reader.current.line_no) {
            if (skip_occurrence_line_group(&left_reader) < 0) {
                result = -1;
                break;
            }
            continue;
        }
        if (left_reader.current.line_no > right_reader.current.line_no) {
            if (skip_occurrence_line_group(&right_reader) < 0) {
                result = -1;
                break;
            }
            continue;
        }

        {
            u32 line_no = left_reader.current.line_no;
            u32 first_left = left_reader.current.start_idx;
            u32 last_right = right_reader.current.start_idx;

            while (left_reader.has_record && left_reader.current.line_no == line_no) {
                int status = occurrence_reader_next(&left_reader);
                if (status < 0) {
                    result = -1;
                    break;
                }
                if (status == 0) {
                    break;
                }
            }
            if (result < 0) {
                break;
            }

            while (right_reader.has_record && right_reader.current.line_no == line_no) {
                last_right = right_reader.current.start_idx;
                {
                    int status = occurrence_reader_next(&right_reader);
                    if (status < 0) {
                        result = -1;
                        break;
                    }
                    if (status == 0) {
                        break;
                    }
                }
            }
            if (result < 0) {
                break;
            }

            if (first_left < last_right && emit_line_no(out, line_no) < 0) {
                result = -1;
                break;
            }
        }
    }

    if (occurrence_reader_close(&left_reader) < 0 || occurrence_reader_close(&right_reader) < 0) {
        return -1;
    }
    if (result < 0) {
        return -1;
    }

    return emit_blank_line(out);
}

// Count how many words are present after trimming space and tab separators.
static size_t count_query_words(const char *query, size_t len, size_t *single_start, size_t *single_len) {
    size_t pos = 0;
    size_t count = 0;

    while (pos < len) {
        size_t start;

        while (pos < len && is_word_sep(query[pos])) {
            pos += 1;
        }
        if (pos >= len) {
            break;
        }

        start = pos;
        while (pos < len && !is_word_sep(query[pos])) {
            pos += 1;
        }

        if (count == 0 && single_start != NULL && single_len != NULL) {
            *single_start = start;
            *single_len = pos - start;
        }
        count += 1;
    }

    return count;
}

// Dispatch one raw query to the correct search mode.
int handle_query(Index *index, const char *query, size_t len, OutBuf *out, QueryScratch *scratch) {
    size_t single_start = 0;
    size_t single_len = 0;
    size_t word_count;

    query_scratch_reset(scratch);

    if (contains_char(query, len, '"')) {
        return handle_phrase(index, query, len, out, scratch);
    }
    if (contains_char(query, len, '*')) {
        return handle_pattern(index, query, len, out);
    }

    word_count = count_query_words(query, len, &single_start, &single_len);
    if (word_count == 0) {
        return emit_blank_line(out);
    }
    if (word_count == 1) {
        return handle_single(index, query + single_start, single_len, out);
    }
    return handle_multi(index, query, len, out);
}
