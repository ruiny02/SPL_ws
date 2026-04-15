#define _GNU_SOURCE

#include "common.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

/* Detect moves that would place a directory inside itself. */
static bool is_subdir_move(const char *source, const char *target) {
    struct stat source_stat;
    char *source_real;
    char *target_real;
    bool result = false;
    size_t source_len;

    if (stat(source, &source_stat) < 0 || !S_ISDIR(source_stat.st_mode)) {
        return false;
    }

    source_real = realpath(source, NULL);
    target_real = realpath_missing_ok(target);
    if (source_real == NULL || target_real == NULL) {
        free(source_real);
        free(target_real);
        return false;
    }

    source_len = strlen(source_real);
    if (strncmp(source_real, target_real, source_len) == 0 &&
        (target_real[source_len] == '/' || target_real[source_len] == '\0')) {
        result = true;
    }

    free(source_real);
    free(target_real);
    return result;
}

/* Rename one source operand into its destination path. */
static int move_path(const char *source, const char *target) {
    const char *source_name = base_name(source);

    if (access(source, F_OK) < 0) {
        fprintf(stderr, "pa2_mv: cannot stat '%s': %s\n", source_name, strerror(errno));
        return -1;
    }

    if (same_file(source, target)) {
        fprintf(stderr, "pa2_mv: '%s' and '%s' are the same file\n", source_name, base_name(target));
        return -1;
    }

    if (is_subdir_move(source, target)) {
        fprintf(stderr, "pa2_mv: cannot move '%s' to a subdirectory of itself, '%s'\n", source_name, target);
        return -1;
    }

    if (rename(source, target) < 0) {
        fprintf(stderr, "pa2_mv: cannot move '%s' to '%s': %s\n", source_name, target, strerror(errno));
        return -1;
    }

    return 0;
}

/* Implement the simplified mv command from the PA2 spec. */
int main(int argc, char *argv[]) {
    const char *destination;
    struct stat dest_stat;
    bool destination_is_directory = false;
    int source_count;
    int status = EXIT_SUCCESS;

    if (argc == 1) {
        fprintf(stderr, "pa2_mv: missing file operand\n");
        return EXIT_FAILURE;
    }
    if (argc == 2) {
        fprintf(stderr, "pa2_mv: missing destination file operand after '%s'\n", argv[1]);
        return EXIT_FAILURE;
    }

    source_count = argc - 2;
    destination = argv[argc - 1];

    if (source_count > 1) {
        if (stat(destination, &dest_stat) < 0 || !S_ISDIR(dest_stat.st_mode)) {
            fprintf(stderr, "pa2_mv: cannot stat '%s': %s\n", destination, strerror(ENOTDIR));
            return EXIT_FAILURE;
        }
        destination_is_directory = true;
    } else if (path_has_trailing_slash(destination)) {
        if (stat(destination, &dest_stat) < 0) {
            fprintf(stderr, "pa2_mv: cannot move '%s' to '%s': %s\n", base_name(argv[1]), destination, strerror(ENOTDIR));
            return EXIT_FAILURE;
        }
        if (!S_ISDIR(dest_stat.st_mode)) {
            fprintf(stderr, "pa2_mv: cannot stat '%s': %s\n", destination, strerror(ENOTDIR));
            return EXIT_FAILURE;
        }
        destination_is_directory = true;
    } else if (stat(destination, &dest_stat) == 0 && S_ISDIR(dest_stat.st_mode)) {
        destination_is_directory = true;
    }

    for (int i = 1; i < argc - 1; i++) {
        char *target = NULL;
        int result;

        if (destination_is_directory) {
            target = join_path(destination, base_name(argv[i]));
            if (target == NULL) {
                status = EXIT_FAILURE;
                continue;
            }
        }

        result = move_path(argv[i], target == NULL ? destination : target);
        if (result < 0) {
            status = EXIT_FAILURE;
        }
        free(target);
    }

    return status;
}
