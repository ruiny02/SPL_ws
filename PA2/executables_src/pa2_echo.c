#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>

enum { BUFFER_SIZE = 8192 };

char to_escape_char(char c) {
    switch (c) {
        case 'n':
            return '\n';
        case 't':
            return '\t';
        case '\\':
            return '\\';
        default:
            return c;
    }
}

void write_when_needed(char *buffer, size_t *buffer_i) {
    if (*buffer_i >= BUFFER_SIZE - 1) {
        write(STDOUT_FILENO, buffer, *buffer_i);
        *buffer_i = 0;
    }
}

int handle_options(int argc, char* argv[], bool* output_trailing_newline, bool* enable_backslash_escaping) {
    opterr = 0; // prevent printing opt errors.
    
    int opt;
    while ((opt = getopt(argc, argv, "+neEh")) != -1) {
        switch (opt) {
            case 'n':
                *output_trailing_newline = false;
                break;
            case 'e':
                *enable_backslash_escaping = true;
                break;
            case 'E': 
                *enable_backslash_escaping = false;
                break;
            case 'h':
                printf("Usage: %s [OPTION]... [STRING]...\n", argv[0]);
                printf("Echo the STRING(s) to standard output.\n");
                printf(" -n   do not output the trailing newline\n");
                printf(" -e   enable interpretation of backslash escapes\n");
                printf(" -E   disable interpretation of backslash escapes (default)\n");
                printf(" -h   display this help and exit\n");
                exit(0);
            case '?':
                return optind - 1; // treat unknown option as a non-option argument
        }
    }
    return optind; // optind is the index of the first non-option argument
}

int main(int argc, char* argv[]) {
    bool output_trailing_newline = true;
    bool enable_backslash_escaping = false;
    int i = handle_options(argc, argv, &output_trailing_newline, &enable_backslash_escaping);

    char buffer[BUFFER_SIZE];
    size_t buffer_i = 0;

    for (; i < argc; i++) {
        for (size_t j = 0; argv[i][j] != '\0'; j++) {
            write_when_needed(buffer, &buffer_i);
            buffer[buffer_i++] = (enable_backslash_escaping && argv[i][j] == '\\' && argv[i][j + 1] != '\0') ? to_escape_char(argv[i][++j]) : argv[i][j];
        }

        if (i < argc - 1) {
            write_when_needed(buffer, &buffer_i);
            buffer[buffer_i++] = ' ';
        }
    }

    if (buffer_i > 0)            write(STDOUT_FILENO, buffer, buffer_i);
    if (output_trailing_newline) write(STDOUT_FILENO, "\n", 1);

    return 0;
}