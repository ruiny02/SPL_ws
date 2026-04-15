#define _GNU_SOURCE

#include "common.h"

#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

/* Treat a missing path or "-" as standard input. */
bool is_stdin_path(const char *path) {
    return path == NULL || strcmp(path, "-") == 0;
}

/* Report whether a path explicitly names a directory target. */
bool path_has_trailing_slash(const char *path) {
    size_t len = strlen(path);
    return len > 0 && path[len - 1] == '/';
}

/* Parse a nonnegative decimal count from user input. */
int parse_nonnegative(const char *text, long *value) {
    char *end_ptr = NULL;
    long parsed;

    errno = 0;
    parsed = strtol(text, &end_ptr, 10);
    if (errno != 0 || end_ptr == text || *end_ptr != '\0' || parsed < 0) {
        return -1;
    }

    *value = parsed;
    return 0;
}

/* Keep writing until the whole buffer reaches the descriptor. */
int write_all(int fd, const void *buffer, size_t count) {
    const char *cursor = buffer;
    size_t written = 0;

    while (written < count) {
        ssize_t rv = write(fd, cursor + written, count - written);

        if (rv < 0) {
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }
        written += (size_t)rv;
    }

    return 0;
}

/* Copy a byte stream from one file descriptor to another. */
int copy_fd(int input_fd, int output_fd) {
    char buffer[PA2_BUFFER_SIZE];

    while (1) {
        ssize_t bytes_read = read(input_fd, buffer, sizeof(buffer));

        if (bytes_read < 0) {
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }
        if (bytes_read == 0) {
            return 0;
        }
        if (write_all(output_fd, buffer, (size_t)bytes_read) < 0) {
            return -1;
        }
    }
}

/* Compare inode identity to detect self-copies and self-moves. */
bool same_file(const char *left, const char *right) {
    struct stat left_stat;
    struct stat right_stat;

    if (stat(left, &left_stat) < 0 || stat(right, &right_stat) < 0) {
        return false;
    }

    return left_stat.st_dev == right_stat.st_dev && left_stat.st_ino == right_stat.st_ino;
}

/* Build DIRECTORY/BASE and allocate the resulting path. */
char *join_path(const char *directory, const char *base) {
    size_t dir_len = strlen(directory);
    size_t base_len = strlen(base);
    bool needs_slash = dir_len > 0 && directory[dir_len - 1] != '/';
    size_t total = dir_len + (needs_slash ? 1 : 0) + base_len + 1;
    char *result = malloc(total);

    if (result == NULL) {
        return NULL;
    }

    memcpy(result, directory, dir_len);
    if (needs_slash) {
        result[dir_len++] = '/';
    }
    memcpy(result + dir_len, base, base_len);
    result[dir_len + base_len] = '\0';
    return result;
}

/* Return the final path component without allocating new storage. */
const char *base_name(const char *path) {
    const char *last_slash = strrchr(path, '/');
    return last_slash == NULL ? path : last_slash + 1;
}

/* Resolve a path even when the final component does not exist yet. */
char *realpath_missing_ok(const char *path) {
    char *resolved = realpath(path, NULL);
    char *copy;
    char *slash;
    char *parent_resolved;
    char cwd[PATH_MAX];
    size_t parent_len;
    size_t base_len;
    char *result;

    if (resolved != NULL) {
        return resolved;
    }

    copy = strdup(path);
    if (copy == NULL) {
        return NULL;
    }

    slash = strrchr(copy, '/');
    if (slash == NULL) {
        if (getcwd(cwd, sizeof(cwd)) == NULL) {
            free(copy);
            return NULL;
        }
        result = join_path(cwd, copy);
        free(copy);
        return result;
    }

    *slash = '\0';
    parent_resolved = (*copy == '\0') ? strdup("/") : realpath(copy, NULL);
    if (parent_resolved == NULL) {
        free(copy);
        return NULL;
    }

    parent_len = strlen(parent_resolved);
    base_len = strlen(slash + 1);
    result = malloc(parent_len + 1 + base_len + 1);
    if (result == NULL) {
        free(copy);
        free(parent_resolved);
        return NULL;
    }

    memcpy(result, parent_resolved, parent_len);
    result[parent_len] = '/';
    memcpy(result + parent_len + 1, slash + 1, base_len);
    result[parent_len + 1 + base_len] = '\0';

    free(copy);
    free(parent_resolved);
    return result;
}
