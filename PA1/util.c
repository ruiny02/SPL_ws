#include "pa1.h"

void copy_bytes(void *dst, const void *src, size_t len) {
    char *d = (char *) dst;
    const char *s = (const char *) src;
    size_t i;

    for (i = 0; i < len; ++i) {
        d[i] = s[i];
    }
}

void move_bytes(void *dst, const void *src, size_t len) {
    char *d = (char *) dst;
    const char *s = (const char *) src;
    size_t i;

    if (d == s || len == 0) {
        return;
    }

    if (d < s) {
        for (i = 0; i < len; ++i) {
            d[i] = s[i];
        }
        return;
    }

    i = len;
    while (i > 0) {
        --i;
        d[i] = s[i];
    }
}

size_t cstr_len(const char *s) {
    size_t len = 0;

    while (s[len] != '\0') {
        ++len;
    }

    return len;
}

int slice_equals_cstr(const char *data, size_t len, const char *literal) {
    size_t i = 0;

    while (i < len && literal[i] != '\0') {
        if (data[i] != literal[i]) {
            return 0;
        }
        ++i;
    }

    return i == len && literal[i] == '\0';
}

int contains_char(const char *data, size_t len, char needle) {
    size_t i;

    for (i = 0; i < len; ++i) {
        if (data[i] == needle) {
            return 1;
        }
    }

    return 0;
}

char ascii_lower(char c) {
    if (c >= 'A' && c <= 'Z') {
        return (char) (c - 'A' + 'a');
    }
    return c;
}

int is_word_sep(char c) {
    return c == ' ' || c == '\t';
}

u32 hash_lower_bytes(const char *data, size_t len) {
    u32 hash = 2166136261u;
    size_t i;

    for (i = 0; i < len; ++i) {
        hash ^= (u32) (unsigned char) ascii_lower(data[i]);
        hash *= 16777619u;
    }

    return hash;
}

int case_equal_bytes(const char *a, const char *b, size_t len) {
    size_t i;

    for (i = 0; i < len; ++i) {
        if (ascii_lower(a[i]) != ascii_lower(b[i])) {
            return 0;
        }
    }

    return 1;
}

int phrase_equal_bytes(const char *line, const char *phrase, size_t len) {
    size_t i;

    for (i = 0; i < len; ++i) {
        if (ascii_lower(line[i]) != ascii_lower(phrase[i])) {
            return 0;
        }
    }

    return 1;
}

static int grow_capacity(size_t *cap, size_t needed) {
    size_t next = *cap == 0 ? 64 : *cap;

    while (next < needed) {
        if (next > (SIZE_MAX / 2)) {
            return -1;
        }
        next *= 2;
    }

    *cap = next;
    return 0;
}

int bytevec_reserve(ByteVec *vec, size_t needed) {
    char *next_data;
    size_t next_cap = vec->cap;

    if (needed <= vec->cap) {
        return 0;
    }

    if (grow_capacity(&next_cap, needed) < 0) {
        return -1;
    }

    next_data = (char *) realloc(vec->data, next_cap);
    if (next_data == NULL) {
        return -1;
    }

    vec->data = next_data;
    vec->cap = next_cap;
    return 0;
}

int bytevec_append(ByteVec *vec, const char *data, size_t len) {
    if (len == 0) {
        return 0;
    }

    if (bytevec_reserve(vec, vec->len + len) < 0) {
        return -1;
    }

    copy_bytes(vec->data + vec->len, data, len);
    vec->len += len;
    return 0;
}

void bytevec_reset(ByteVec *vec) {
    vec->len = 0;
}

void bytevec_free(ByteVec *vec) {
    free(vec->data);
    vec->data = NULL;
    vec->len = 0;
    vec->cap = 0;
}

static int matchvec_reserve(MatchVec *vec, size_t needed) {
    Match *next_data;
    size_t next_cap = vec->cap;

    if (needed <= vec->cap) {
        return 0;
    }

    if (grow_capacity(&next_cap, needed) < 0) {
        return -1;
    }

    next_data = (Match *) realloc(vec->items, next_cap * sizeof(Match));
    if (next_data == NULL) {
        return -1;
    }

    vec->items = next_data;
    vec->cap = next_cap;
    return 0;
}

int matchvec_push(MatchVec *vec, u32 line_no, u32 start_idx) {
    if (matchvec_reserve(vec, vec->len + 1) < 0) {
        return -1;
    }

    vec->items[vec->len].line_no = line_no;
    vec->items[vec->len].start_idx = start_idx;
    vec->len += 1;
    return 0;
}

void matchvec_reset(MatchVec *vec) {
    vec->len = 0;
}

void matchvec_free(MatchVec *vec) {
    free(vec->items);
    vec->items = NULL;
    vec->len = 0;
    vec->cap = 0;
}

static int u32vec_reserve(U32Vec *vec, size_t needed) {
    u32 *next_data;
    size_t next_cap = vec->cap;

    if (needed <= vec->cap) {
        return 0;
    }

    if (grow_capacity(&next_cap, needed) < 0) {
        return -1;
    }

    next_data = (u32 *) realloc(vec->items, next_cap * sizeof(u32));
    if (next_data == NULL) {
        return -1;
    }

    vec->items = next_data;
    vec->cap = next_cap;
    return 0;
}

int u32vec_push(U32Vec *vec, u32 value) {
    if (u32vec_reserve(vec, vec->len + 1) < 0) {
        return -1;
    }

    vec->items[vec->len] = value;
    vec->len += 1;
    return 0;
}

void u32vec_reset(U32Vec *vec) {
    vec->len = 0;
}

void u32vec_free(U32Vec *vec) {
    free(vec->items);
    vec->items = NULL;
    vec->len = 0;
    vec->cap = 0;
}
