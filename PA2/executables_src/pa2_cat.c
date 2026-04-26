// Do not use exec*!
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

enum { BUFFER_SIZE = 8192 };

static int copy_fd(int fd, const char* label) {
  char buffer[BUFFER_SIZE];

  while (1) {
    ssize_t bytes_read = read(fd, buffer, sizeof(buffer));
    if (bytes_read == 0) {
      return 0;
    }
    if (bytes_read < 0) {
      fprintf(stderr, "pa2_cat: %s: %s\n", label, strerror(errno));
      return 1;
    }

    ssize_t offset = 0;
    while (offset < bytes_read) {
      ssize_t bytes_written =
          write(STDOUT_FILENO, buffer + offset, (size_t)(bytes_read - offset));
      if (bytes_written < 0) {
        fprintf(stderr, "pa2_cat: write error: %s\n", strerror(errno));
        return 1;
      }
      offset += bytes_written;
    }
  }
}

static int cat_path(const char* path) {
  if (strcmp(path, "-") == 0) {
    return copy_fd(STDIN_FILENO, "-");
  }

  struct stat st;
  if (stat(path, &st) == 0 && S_ISDIR(st.st_mode)) {
    fprintf(stderr, "pa2_cat: %s: Is a directory\n", path);
    return 1;
  }

  int fd = open(path, O_RDONLY);
  if (fd == -1) {
    fprintf(stderr, "pa2_cat: %s: %s\n", path, strerror(errno));
    return 1;
  }

  int status = copy_fd(fd, path);
  close(fd);
  return status;
}

/** 
# Synopsis
`pa2_cat [FILE]…`

# Description
- Concatenate FILE(s) to standard output.
- Note for concatenation: Do not add a newline or space when concatenating. Just print the contents of the files as is.
- You do not need to consider the maximum size of the files. 
- If no FILE is provided, or FILE is -, read standard input.

# Errors

- When a file does not exist, print "pa2_cat: FILE: No such file or directory". 
  - Input: `rm –f foo; pa2_cat foo`
  - Output: `pa2_cat: cannot open ‘foo' for reading: No such file or directory`
- When the file is a directory, print "pa2_cat: FILE: Is a directory".
  - Input: `mkdir foo; pa2_cat foo; rmdir foo`
  - Output: `pa2_cat: foo: Is a directory`
- When you don’t have permissions to open a file, print "pa2_cat: FILE: Permission denied".
  - Input: `touch foo; chmod u-r foo; pa2_cat foo`
  - Output: `pa2_cat: foo: Permission denied`
**/
int main(int argc, char* argv[]) {
  if (argc == 1) {
    return copy_fd(STDIN_FILENO, "-") == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
  }

  int status = 0;
  for (int i = 1; i < argc; i++) {
    if (cat_path(argv[i]) != 0) {
      status = 1;
    }
  }

  return status == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
