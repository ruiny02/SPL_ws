#define _GNU_SOURCE

#include "common.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

/* Copy one regular file onto the chosen target path. */
static int copy_file(const char *source, const char *target) {
    int input_fd;
    int output_fd;

    if (same_file(source, target)) {
        fprintf(stderr, "pa2_cp: '%s' and '%s' are the same file\n", source, target);
        return -1;
    }

    input_fd = open(source, O_RDONLY);
    if (input_fd < 0) {
        fprintf(stderr, "pa2_cp: cannot stat '%s': %s\n", source, strerror(errno));
        return -1;
    }

    output_fd = open(target, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (output_fd < 0) {
        fprintf(stderr, "pa2_cp: cannot create regular file '%s': %s\n", target, strerror(errno));
        close(input_fd);
        return -1;
    }

    if (copy_fd(input_fd, output_fd) < 0) {
        fprintf(stderr, "pa2_cp: %s\n", strerror(errno));
        close(input_fd);
        close(output_fd);
        return -1;
    }

    close(input_fd);
    close(output_fd);
    return 0;
}

/* Map one source operand onto its final copy destination. */
static int copy_operand(const char *source, const char *destination, bool destination_is_directory) {
    struct stat source_stat;
    char *target = NULL;
    int result;

    if (stat(source, &source_stat) < 0) {
        fprintf(stderr, "pa2_cp: cannot stat '%s': %s\n", source, strerror(errno));
        return -1;
    }

    if (destination_is_directory) {
        target = join_path(destination, base_name(source));
        if (target == NULL) {
            return -1;
        }
    }

    result = copy_file(source, target == NULL ? destination : target);
    free(target);
    return result;
}

/* Implement the simplified cp command from the PA2 spec. */
int main(int argc, char *argv[]) {
    const char *destination;
    bool destination_is_directory = false;
    struct stat dest_stat;
    int status = EXIT_SUCCESS;
    int source_count;

    if (argc == 1) {
        fprintf(stderr, "pa2_cp: missing file operand\n");
        return EXIT_FAILURE;
    }
    if (argc == 2) {
        fprintf(stderr, "pa2_cp: missing destination file operand after '%s'\n", argv[1]);
        return EXIT_FAILURE;
    }

    source_count = argc - 2;
    destination = argv[argc - 1];

    if (source_count > 1) {
        if (stat(destination, &dest_stat) < 0 || !S_ISDIR(dest_stat.st_mode)) {
            fprintf(stderr, "pa2_cp: cannot stat '%s': %s\n", destination, strerror(ENOTDIR));
            return EXIT_FAILURE;
        }
        destination_is_directory = true;
    } else if (path_has_trailing_slash(destination)) {
        if (stat(destination, &dest_stat) < 0) {
            fprintf(stderr, "pa2_cp: cannot create regular file '%s': %s\n", destination, strerror(ENOTDIR));
            return EXIT_FAILURE;
        }
        if (!S_ISDIR(dest_stat.st_mode)) {
            fprintf(stderr, "pa2_cp: cannot stat '%s': %s\n", destination, strerror(ENOTDIR));
            return EXIT_FAILURE;
        }
        destination_is_directory = true;
    } else if (stat(destination, &dest_stat) == 0 && S_ISDIR(dest_stat.st_mode)) {
        destination_is_directory = true;
    }

    for (int i = 1; i < argc - 1; i++) {
        if (copy_operand(argv[i], destination, destination_is_directory) < 0) {
            status = EXIT_FAILURE;
            if (source_count == 1) {
                break;
            }
        }
    }

    return status;
}
