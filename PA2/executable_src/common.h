#ifndef PA2_COMMON_H
#define PA2_COMMON_H

#include <stdbool.h>
#include <stddef.h>

enum { PA2_BUFFER_SIZE = 8192 };

bool is_stdin_path(const char *path);
bool path_has_trailing_slash(const char *path);
int parse_nonnegative(const char *text, long *value);
int write_all(int fd, const void *buffer, size_t count);
int copy_fd(int input_fd, int output_fd);
bool same_file(const char *left, const char *right);
char *join_path(const char *directory, const char *base);
const char *base_name(const char *path);
char *realpath_missing_ok(const char *path);

#endif
