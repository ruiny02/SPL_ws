#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* Remove every operand, reporting errors without stopping early. */
int main(int argc, char *argv[]) {
    int status = EXIT_SUCCESS;

    if (argc == 1) {
        fprintf(stderr, "pa2_rm: missing operand\n");
        return EXIT_FAILURE;
    }

    for (int i = 1; i < argc; i++) {
        if (unlink(argv[i]) < 0) {
            fprintf(stderr, "pa2_rm: cannot remove '%s': %s\n", argv[i], strerror(errno));
            status = EXIT_FAILURE;
        }
    }

    return status;
}
