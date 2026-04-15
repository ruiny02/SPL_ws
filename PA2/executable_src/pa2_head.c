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
        fprintf(stderr, "pa2_head: error reading '%s': %s\n", path, strerror(EISDIR));
        return NULL;
    }

    stream = fopen(path, "r");
    if (stream == NULL) {
        fprintf(stderr, "pa2_head: cannot open '%s' for reading: %s\n", path, strerror(errno));
    }
    return stream;
}

/* Print the first N lines from a file or standard input. */
int main(int argc, char *argv[]) {
    long max_lines = 10;
    const char *path = NULL;
    FILE *stream;
    char *line = NULL;
    size_t cap = 0;
    ssize_t line_len;
    int opt;
    int status = EXIT_SUCCESS;

    opterr = 0;
    while ((opt = getopt(argc, argv, "+n:")) != -1) {
        switch (opt) {
            case 'n':
                if (parse_nonnegative(optarg, &max_lines) < 0) {
                    fprintf(stderr, "pa2_head: invalid number of lines: '%s'\n", optarg);
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

    while (max_lines > 0 && (line_len = getline(&line, &cap, stream)) != -1) {
        if (write_all(STDOUT_FILENO, line, (size_t)line_len) < 0) {
            status = EXIT_FAILURE;
            break;
        }
        max_lines--;
    }

    free(line);
    if (stream != stdin) {
        fclose(stream);
    }
    return status;
}
