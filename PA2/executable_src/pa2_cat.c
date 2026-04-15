#include "common.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

/* Stream one named input, or stdin when the path is "-". */
static int cat_one(const char *path) {
    int fd;
    struct stat st;

    if (is_stdin_path(path)) {
        return copy_fd(STDIN_FILENO, STDOUT_FILENO);
    }

    if (stat(path, &st) == 0 && S_ISDIR(st.st_mode)) {
        fprintf(stderr, "pa2_cat: %s: %s\n", path, strerror(EISDIR));
        return -1;
    }

    fd = open(path, O_RDONLY);
    if (fd < 0) {
        fprintf(stderr, "pa2_cat: %s: %s\n", path, strerror(errno));
        return -1;
    }
    if (copy_fd(fd, STDOUT_FILENO) < 0) {
        fprintf(stderr, "pa2_cat: %s: %s\n", path, strerror(errno));
        close(fd);
        return -1;
    }
    close(fd);
    return 0;
}

/* Concatenate zero or more inputs onto standard output. */
int main(int argc, char *argv[]) {
    int status = EXIT_SUCCESS;

    if (argc == 1) {
        return cat_one("-") == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
    }

    for (int i = 1; i < argc; i++) {
        if (cat_one(argv[i]) < 0) {
            status = EXIT_FAILURE;
        }
    }

    return status;
}
