#include "pa1.h"

int write_full(int fd, const void *data, size_t len) {
    const char *ptr = (const char *) data;

    while (len > 0) {
        ssize_t written = write(fd, ptr, len);
        if (written < 0) {
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }
        ptr += (size_t) written;
        len -= (size_t) written;
    }

    return 0;
}

int pread_full(int fd, void *data, size_t len, off_t offset) {
    char *ptr = (char *) data;

    while (len > 0) {
        ssize_t got = pread(fd, ptr, len, offset);
        if (got < 0) {
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }
        if (got == 0) {
            return -1;
        }
        ptr += (size_t) got;
        offset += (off_t) got;
        len -= (size_t) got;
    }

    return 0;
}

void line_reader_init(LineReader *reader, int fd) {
    reader->fd = fd;
    reader->pos = 0;
    reader->len = 0;
}

int line_reader_read_line(LineReader *reader, ByteVec *out, int *had_newline) {
    bytevec_reset(out);
    *had_newline = 0;

    while (1) {
        size_t start;

        if (reader->pos == reader->len) {
            ssize_t got;

            do {
                got = read(reader->fd, reader->buf, sizeof(reader->buf));
            } while (got < 0 && errno == EINTR);

            if (got < 0) {
                return -1;
            }
            if (got == 0) {
                return out->len > 0 ? 1 : 0;
            }

            reader->pos = 0;
            reader->len = (size_t) got;
        }

        start = reader->pos;
        while (reader->pos < reader->len && reader->buf[reader->pos] != '\n') {
            reader->pos += 1;
        }

        if (bytevec_append(out, reader->buf + start, reader->pos - start) < 0) {
            return -1;
        }

        if (reader->pos < reader->len && reader->buf[reader->pos] == '\n') {
            reader->pos += 1;
            *had_newline = 1;
            return 1;
        }
    }
}

void outbuf_init(OutBuf *out, int fd) {
    out->fd = fd;
    out->len = 0;
}

int outbuf_flush(OutBuf *out) {
    if (out->len == 0) {
        return 0;
    }

    if (write_full(out->fd, out->buf, out->len) < 0) {
        return -1;
    }

    out->len = 0;
    return 0;
}

int outbuf_write_data(OutBuf *out, const void *data, size_t len) {
    const char *ptr = (const char *) data;

    while (len > 0) {
        size_t avail = sizeof(out->buf) - out->len;
        size_t chunk = len;

        if (avail == 0) {
            if (outbuf_flush(out) < 0) {
                return -1;
            }
            avail = sizeof(out->buf);
        }

        if (chunk > avail) {
            chunk = avail;
        }

        copy_bytes(out->buf + out->len, ptr, chunk);
        out->len += chunk;
        ptr += chunk;
        len -= chunk;
    }

    return 0;
}

int outbuf_write_byte(OutBuf *out, char ch) {
    return outbuf_write_data(out, &ch, 1);
}

int outbuf_write_u32(OutBuf *out, u32 value) {
    char digits[16];
    size_t len = 0;

    if (value == 0) {
        digits[len++] = '0';
    } else {
        while (value > 0) {
            digits[len++] = (char) ('0' + (value % 10u));
            value /= 10u;
        }
    }

    while (len > 0) {
        len -= 1;
        if (outbuf_write_byte(out, digits[len]) < 0) {
            return -1;
        }
    }

    return 0;
}

int create_temp_file(const char *tag) {
    char path[64];
    const char *prefix = "/tmp/pa1-";
    const char *suffix = "-XXXXXX";
    size_t pos = 0;
    size_t i;
    int fd;

    for (i = 0; prefix[i] != '\0'; ++i) {
        path[pos++] = prefix[i];
    }
    for (i = 0; tag[i] != '\0'; ++i) {
        path[pos++] = tag[i];
    }
    for (i = 0; suffix[i] != '\0'; ++i) {
        path[pos++] = suffix[i];
    }
    path[pos] = '\0';

    fd = mkstemp(path);
    if (fd < 0) {
        return -1;
    }

    if (unlink(path) < 0) {
        close(fd);
        return -1;
    }

    return fd;
}
