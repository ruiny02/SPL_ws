#define _GNU_SOURCE

#include "common.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

/* Open a file for line-based reading, or reuse stdin when requested. */
static FILE *open_input(const char *path) {
    struct stat st;
    FILE *stream;

    if (is_stdin_path(path)) {
        return stdin;
    }

    if (stat(path, &st) == 0 && S_ISDIR(st.st_mode)) {
        fprintf(stderr, "pa2_tail: error reading '%s': %s\n", path, strerror(EISDIR));
        return NULL;
    }

    stream = fopen(path, "r");
    if (stream == NULL) {
        fprintf(stderr, "pa2_tail: cannot open '%s' for reading: %s\n", path, strerror(errno));
    }
    return stream;
}

/* Print the last N lines from a file or standard input. */
int main(int argc, char *argv[]) {
    long max_lines = 10;
    const char *path = NULL;
    FILE *stream;
    char **ring = NULL;
    char *line = NULL;
    size_t cap = 0;
    ssize_t line_len;
    long total_lines = 0;
    int opt;
    int status = EXIT_SUCCESS;

    opterr = 0;
    while ((opt = getopt(argc, argv, "+n:")) != -1) {
        switch (opt) {
            case 'n':
                if (parse_nonnegative(optarg, &max_lines) < 0) {
                    fprintf(stderr, "pa2_tail: invalid number of lines: '%s'\n", optarg);
                    return EXIT_FAILURE;
                }
                break;
            default:
                return EXIT_FAILURE;
        }
    }

    if (optind < argc) {
        path = argv[optind];
    }

    stream = open_input(path);
    if (stream == NULL) {
        return EXIT_FAILURE;
    }

    if (max_lines > 0) {
        ring = calloc((size_t)max_lines, sizeof(*ring));
        if (ring == NULL) {
            if (stream != stdin) {
                fclose(stream);
            }
            return EXIT_FAILURE;
        }
    }

    while ((line_len = getline(&line, &cap, stream)) != -1) {
        char *copy;
        size_t slot;

        if (max_lines == 0) {
            continue;
        }

        copy = malloc((size_t)line_len + 1);
        if (copy == NULL) {
            status = EXIT_FAILURE;
            goto cleanup;
        }
        memcpy(copy, line, (size_t)line_len + 1);
        slot = (size_t)(total_lines % max_lines);
        free(ring[slot]);
        ring[slot] = copy;
        total_lines++;
    }

    if (max_lines > 0) {
        long start = total_lines > max_lines ? total_lines - max_lines : 0;

        for (long i = start; i < total_lines; i++) {
            char *out = ring[(size_t)(i % max_lines)];
            size_t len = strlen(out);

            if (write_all(STDOUT_FILENO, out, len) < 0) {
                status = EXIT_FAILURE;
                break;
            }
        }
    }

cleanup:
    if (ring != NULL) {
        for (long i = 0; i < max_lines; i++) {
            free(ring[i]);
        }
    }
    free(ring);
    free(line);
    if (stream != stdin) {
        fclose(stream);
    }
    return status;
}
