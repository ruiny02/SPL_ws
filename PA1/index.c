#include "pa1.h"

// Rebuild the lexicon hash slots when the table grows.
static int lexicon_rehash(Lexicon *lexicon, size_t next_slots) {
    u32 *slots;
    size_t i;

    slots = (u32 *) calloc(next_slots, sizeof(u32));
    if (slots == NULL) {
        return -1;
    }

    for (i = 0; i < lexicon->count; ++i) {
        LexEntry *entry = &lexicon->entries[i];
        size_t slot = entry->hash % next_slots;

        entry->next = slots[slot];
        slots[slot] = (u32) (i + 1);
    }

    free(lexicon->slots);
    lexicon->slots = slots;
    lexicon->slot_count = next_slots;
    return 0;
}

// Initialize the in-memory lexicon table.
int lexicon_init(Lexicon *lexicon) {
    lexicon->entries = NULL;
    lexicon->count = 0;
    lexicon->cap = 0;
    lexicon->slot_count = PA1_LEXICON_SLOTS_INIT;
    lexicon->slots = (u32 *) calloc(lexicon->slot_count, sizeof(u32));
    return lexicon->slots == NULL ? -1 : 0;
}

// Free all lexicon memory and cached posting files.
void lexicon_free(Lexicon *lexicon) {
    size_t i;

    for (i = 0; i < lexicon->count; ++i) {
        if (lexicon->entries[i].cache_fd >= 0) {
            close(lexicon->entries[i].cache_fd);
        }
        free(lexicon->entries[i].text);
    }
    free(lexicon->entries);
    free(lexicon->slots);

    lexicon->entries = NULL;
    lexicon->slots = NULL;
    lexicon->count = 0;
    lexicon->cap = 0;
    lexicon->slot_count = 0;
}

// Grow the lexicon entry array when more words are added.
static int lexicon_reserve_entries(Lexicon *lexicon, size_t needed) {
    size_t next_cap = lexicon->cap == 0 ? 256 : lexicon->cap;
    LexEntry *next_entries;

    if (needed <= lexicon->cap) {
        return 0;
    }

    while (next_cap < needed) {
        if (next_cap > (SIZE_MAX / 2)) {
            return -1;
        }
        next_cap *= 2;
    }

    next_entries = (LexEntry *) realloc(lexicon->entries, next_cap * sizeof(LexEntry));
    if (next_entries == NULL) {
        return -1;
    }

    lexicon->entries = next_entries;
    lexicon->cap = next_cap;
    return 0;
}

// Look up a word in the lexicon hash table.
static int lexicon_lookup(Lexicon *lexicon, const char *data, size_t len, u32 hash, u32 *entry_index) {
    u32 index = lexicon->slots[hash % lexicon->slot_count];

    while (index != 0) {
        LexEntry *entry = &lexicon->entries[index - 1];
        if (entry->hash == hash && entry->len == len && case_equal_bytes(entry->text, data, len)) {
            *entry_index = index - 1;
            return 1;
        }
        index = entry->next;
    }

    return 0;
}

// Find a word or add it to the lexicon if it is new.
int lexicon_find_or_add(Lexicon *lexicon, const char *data, size_t len, u32 *word_id, u32 *bucket) {
    u32 hash = hash_lower_bytes(data, len);
    u32 entry_index;
    size_t slot;
    char *stored;
    size_t i;
    LexEntry *entry;

    if (lexicon_lookup(lexicon, data, len, hash, &entry_index)) {
        entry = &lexicon->entries[entry_index];
        *word_id = entry->id;
        *bucket = entry->bucket;
        return 0;
    }

    if ((lexicon->count + 1) * 4 >= lexicon->slot_count * 3) {
        if (lexicon_rehash(lexicon, lexicon->slot_count * 2) < 0) {
            return -1;
        }
    }

    if (lexicon_reserve_entries(lexicon, lexicon->count + 1) < 0) {
        return -1;
    }

    stored = (char *) malloc(len + 1);
    if (stored == NULL) {
        return -1;
    }

    for (i = 0; i < len; ++i) {
        stored[i] = ascii_lower(data[i]);
    }
    stored[len] = '\0';

    entry = &lexicon->entries[lexicon->count];
    entry->text = stored;
    entry->len = (u32) len;
    entry->id = (u32) lexicon->count;
    entry->hash = hash;
    entry->bucket = hash % PA1_OCC_BUCKETS;
    entry->cache_fd = -1;
    entry->cache_count = 0;
    slot = hash % lexicon->slot_count;
    entry->next = lexicon->slots[slot];
    lexicon->slots[slot] = (u32) (lexicon->count + 1);
    lexicon->count += 1;

    *word_id = entry->id;
    *bucket = entry->bucket;
    return 0;
}

// Look up an existing word without creating a new lexicon entry.
int lexicon_find_existing(Lexicon *lexicon, const char *data, size_t len, u32 *word_id, u32 *bucket) {
    u32 hash = hash_lower_bytes(data, len);
    u32 entry_index;

    if (!lexicon_lookup(lexicon, data, len, hash, &entry_index)) {
        return 0;
    }

    *word_id = lexicon->entries[entry_index].id;
    *bucket = lexicon->entries[entry_index].bucket;
    return 1;
}

// Tokenize one line and append its word occurrences to bucket files.
static int process_line(Index *index, const char *line, size_t len, u32 line_no) {
    size_t pos = 0;

    while (pos < len) {
        size_t start = pos;
        size_t end;
        u32 word_id;
        u32 bucket;
        OccRecord record;

        while (start < len && is_word_sep(line[start])) {
            start += 1;
        }
        if (start >= len) {
            break;
        }

        end = start;
        while (end < len && !is_word_sep(line[end])) {
            end += 1;
        }

        if (lexicon_find_or_add(&index->lexicon, line + start, end - start, &word_id, &bucket) < 0) {
            return -1;
        }

        record.word_id = word_id;
        record.line_no = line_no;
        record.start_idx = (u32) start;
        if (outbuf_write_data(&index->occ_out[bucket], &record, sizeof(record)) < 0) {
            return -1;
        }

        pos = end;
    }

    return 0;
}

// Open the temporary files needed for line offsets and postings.
int index_init(Index *index, int input_fd) {
    size_t i;
    u64 zero = 0;

    index->input_fd = input_fd;
    index->line_count = 0;

    if (lexicon_init(&index->lexicon) < 0) {
        return -1;
    }

    index->line_fd = create_temp_file("lines");
    if (index->line_fd < 0) {
        lexicon_free(&index->lexicon);
        return -1;
    }
    outbuf_init(&index->line_out, index->line_fd);

    for (i = 0; i < PA1_OCC_BUCKETS; ++i) {
        index->occ_fds[i] = create_temp_file("occ");
        if (index->occ_fds[i] < 0) {
            while (i > 0) {
                --i;
                close(index->occ_fds[i]);
            }
            close(index->line_fd);
            lexicon_free(&index->lexicon);
            return -1;
        }
        outbuf_init(&index->occ_out[i], index->occ_fds[i]);
    }

    if (outbuf_write_data(&index->line_out, &zero, sizeof(zero)) < 0) {
        index_free(index);
        return -1;
    }

    return 0;
}

// Stream the input file once and build the temporary indexes.
int index_build(Index *index) {
    LineReader reader;
    ByteVec line;
    int read_status;
    u64 offset = 0;

    line.data = NULL;
    line.len = 0;
    line.cap = 0;
    line_reader_init(&reader, index->input_fd);

    while (1) {
        int had_newline = 0;
        read_status = line_reader_read_line(&reader, &line, &had_newline);
        if (read_status <= 0) {
            break;
        }

        offset += (u64) line.len + (u64) had_newline;
        index->line_count += 1;

        if (process_line(index, line.data, line.len, index->line_count) < 0) {
            bytevec_free(&line);
            return -1;
        }
        if (outbuf_write_data(&index->line_out, &offset, sizeof(offset)) < 0) {
            bytevec_free(&line);
            return -1;
        }
    }

    if (read_status < 0) {
        bytevec_free(&line);
        return -1;
    }

    bytevec_free(&line);

    if (outbuf_flush(&index->line_out) < 0) {
        return -1;
    }
    for (read_status = 0; read_status < PA1_OCC_BUCKETS; ++read_status) {
        if (outbuf_flush(&index->occ_out[read_status]) < 0) {
            return -1;
        }
    }

    return 0;
}

// Read one stored line offset from the line-offset table.
static int read_line_offset(Index *index, u32 slot, u64 *value) {
    return pread_full(index->line_fd, value, sizeof(*value), (off_t) slot * (off_t) sizeof(*value));
}

// Fetch a specific line back from the original input file.
int index_read_line(Index *index, u32 line_no, ByteVec *line) {
    u64 start;
    u64 end;
    size_t size;

    if (line_no == 0 || line_no > index->line_count) {
        return -1;
    }

    if (read_line_offset(index, line_no - 1, &start) < 0 || read_line_offset(index, line_no, &end) < 0) {
        return -1;
    }

    if (end < start) {
        return -1;
    }

    size = (size_t) (end - start);
    bytevec_reset(line);

    if (size == 0) {
        return 0;
    }

    if (bytevec_reserve(line, size) < 0) {
        return -1;
    }
    if (pread_full(index->input_fd, line->data, size, (off_t) start) < 0) {
        return -1;
    }

    line->len = size;
    if (line->len > 0 && line->data[line->len - 1] == '\n') {
        line->len -= 1;
    }

    return 0;
}

// Close files and release all index-owned resources.
void index_free(Index *index) {
    size_t i;

    outbuf_flush(&index->line_out);
    if (index->line_fd >= 0) {
        close(index->line_fd);
        index->line_fd = -1;
    }

    for (i = 0; i < PA1_OCC_BUCKETS; ++i) {
        outbuf_flush(&index->occ_out[i]);
        if (index->occ_fds[i] >= 0) {
            close(index->occ_fds[i]);
            index->occ_fds[i] = -1;
        }
    }

    if (index->input_fd >= 0) {
        close(index->input_fd);
        index->input_fd = -1;
    }

    lexicon_free(&index->lexicon);
}
