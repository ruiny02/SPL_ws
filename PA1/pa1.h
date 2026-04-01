#ifndef PA1_H
#define PA1_H

#define _POSIX_C_SOURCE 200809L

#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <stdint.h>

typedef uint32_t u32;
typedef uint64_t u64;

// Shared constants
enum {
    PA1_READ_BUF_SIZE = 65536,
    PA1_WRITE_BUF_SIZE = 65536,
    PA1_OCC_BUCKETS = 256,
    PA1_QUERY_LIMIT = 4096,
    PA1_LEXICON_SLOTS_INIT = 4096
};

// Shared dynamic buffers
typedef struct {
    char *data;
    size_t len;
    size_t cap;
} ByteVec;

typedef struct {
    u32 line_no;
    u32 start_idx;
} Match;

typedef struct {
    Match *items;
    size_t len;
    size_t cap;
} MatchVec;

typedef struct {
    u32 *items;
    size_t len;
    size_t cap;
} U32Vec;

// Buffered I/O helpers
typedef struct {
    int fd;
    char buf[PA1_READ_BUF_SIZE];
    size_t pos;
    size_t len;
} LineReader;

typedef struct {
    int fd;
    char buf[PA1_WRITE_BUF_SIZE];
    size_t len;
} OutBuf;

// Search index records
typedef struct {
    u32 word_id;
    u32 line_no;
    u32 start_idx;
} OccRecord;

typedef struct {
    char *text;
    u32 len;
    u32 id;
    u32 hash;
    u32 bucket;
    char *cache_path;
    u32 next;
} LexEntry;

typedef struct {
    LexEntry *entries;
    size_t count;
    size_t cap;
    u32 *slots;
    size_t slot_count;
} Lexicon;

// Top-level runtime state
typedef struct {
    int input_fd;
    int line_fd;
    int occ_fds[PA1_OCC_BUCKETS];
    OutBuf line_out;
    OutBuf occ_out[PA1_OCC_BUCKETS];
    Lexicon lexicon;
    u32 line_count;
} Index;

typedef struct {
    ByteVec line_buf;
    MatchVec matches_a;
    MatchVec matches_b;
    U32Vec lines_a;
    U32Vec lines_b;
    U32Vec lines_tmp;
} QueryScratch;

// util.c: byte helpers, ASCII helpers, and dynamic array utilities
void copy_bytes(void *dst, const void *src, size_t len);
void move_bytes(void *dst, const void *src, size_t len);
size_t cstr_len(const char *s);
int slice_equals_cstr(const char *data, size_t len, const char *literal);
int contains_char(const char *data, size_t len, char needle);
char ascii_lower(char c);
int is_word_sep(char c);
u32 hash_lower_bytes(const char *data, size_t len);
int case_equal_bytes(const char *a, const char *b, size_t len);
int phrase_equal_bytes(const char *line, const char *phrase, size_t len);

int bytevec_reserve(ByteVec *vec, size_t needed);
int bytevec_append(ByteVec *vec, const char *data, size_t len);
void bytevec_reset(ByteVec *vec);
void bytevec_free(ByteVec *vec);

int matchvec_push(MatchVec *vec, u32 line_no, u32 start_idx);
void matchvec_reset(MatchVec *vec);
void matchvec_free(MatchVec *vec);

int u32vec_push(U32Vec *vec, u32 value);
void u32vec_reset(U32Vec *vec);
void u32vec_free(U32Vec *vec);

// io.c: buffered input/output and temporary file helpers
void line_reader_init(LineReader *reader, int fd);
int line_reader_read_line(LineReader *reader, ByteVec *out, int *had_newline);

void outbuf_init(OutBuf *out, int fd);
int outbuf_flush(OutBuf *out);
int outbuf_write_data(OutBuf *out, const void *data, size_t len);
int outbuf_write_byte(OutBuf *out, char ch);
int outbuf_write_u32(OutBuf *out, u32 value);
int write_full(int fd, const void *data, size_t len);
int pread_full(int fd, void *data, size_t len, off_t offset);
int create_named_temp_file(const char *tag, char *path, size_t path_cap);
int create_temp_file(const char *tag);

// index.c: lexicon management and on-disk index construction
int lexicon_init(Lexicon *lexicon);
void lexicon_free(Lexicon *lexicon);
int lexicon_find_or_add(Lexicon *lexicon, const char *data, size_t len, u32 *word_id, u32 *bucket);
int lexicon_find_existing(Lexicon *lexicon, const char *data, size_t len, u32 *word_id, u32 *bucket);

int index_init(Index *index, int input_fd);
int index_build(Index *index);
int index_read_line(Index *index, u32 line_no, ByteVec *line);
void index_free(Index *index);

// query.c: query scratch state and search dispatch
void query_scratch_init(QueryScratch *scratch);
void query_scratch_free(QueryScratch *scratch);
int handle_query(Index *index, const char *query, size_t len, OutBuf *out, QueryScratch *scratch);

#endif
