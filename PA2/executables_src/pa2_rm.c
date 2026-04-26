// Do not use exec*!
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static int remove_path(const char* path) {
  if (unlink(path) == 0) {
    return 0;
  }

  if (errno == EISDIR || errno == EPERM) {
    struct stat st;
    if (stat(path, &st) == 0 && S_ISDIR(st.st_mode)) {
      fprintf(stderr, "pa2_rm: cannot remove '%s': Is a directory\n", path);
      return 1;
    }
  }

  fprintf(stderr, "pa2_rm: cannot remove '%s': %s\n", path, strerror(errno));
  return 1;
}

/** 
# Synopsis
`pa2_rm FILE…`

# Description
- Remove FILE(s)
- If an errors occur for one file, the other files should still be removed.
- No need to consider users not having sufficient permission to delete the file(s).

---

# Errors

- When no arguments are passed, print “pa2_rm: missing operand”
  - Input: `pa2_rm`
  - Output: `pa2_rm: missing operand`
- When a file does not exist, print  “pa2_rm: cannot remove ‘FILE': No such file or directory”
  - Input: `pa2_rm foo`
  - Output: `pa2_rm: cannot remove ‘foo': No such file or directory`
- When a file is a directory, print  “pa2_rm: cannot remove ‘FILE’: Is a directory”
  - Input: `mkdir foo; pa2_rm foo; rmdir foo`
  - Output: `pa2_rm: cannot remove ‘foo’: Is a directory`
**/
int main(int argc, char* argv[]) {
  if (argc == 1) {
    fprintf(stderr, "pa2_rm: missing operand\n");
    return EXIT_FAILURE;
  }

  int status = 0;
  for (int i = 1; i < argc; i++) {
    if (remove_path(argv[i]) != 0) {
      status = 1;
    }
  }
  return status == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
