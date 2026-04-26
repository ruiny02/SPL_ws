// Do not use exec*!
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

enum { BUFFER_SIZE = 8192 };

static bool ends_with_slash(const char* path) {
  size_t len = strlen(path);
  return len > 0 && path[len - 1] == '/';
}

static const char* pa2_basename(const char* path) {
  const char* slash = strrchr(path, '/');
  if (slash == NULL) {
    return path;
  }
  return slash[1] == '\0' ? slash : slash + 1;
}

static char* join_path(const char* dir, const char* base) {
  size_t dir_len = strlen(dir);
  size_t base_len = strlen(base);
  bool need_slash = dir_len > 0 && dir[dir_len - 1] != '/';

  char* joined = malloc(dir_len + need_slash + base_len + 1);
  if (joined == NULL) {
    perror("pa2_cp");
    return NULL;
  }

  memcpy(joined, dir, dir_len);
  if (need_slash) {
    joined[dir_len] = '/';
  }
  memcpy(joined + dir_len + need_slash, base, base_len + 1);
  return joined;
}

static bool same_file(const struct stat* a, const struct stat* b) {
  return a->st_dev == b->st_dev && a->st_ino == b->st_ino;
}

static int copy_file_contents(const char* source, const char* target) {
  int input = open(source, O_RDONLY);
  if (input == -1) {
    fprintf(stderr, "pa2_cp: cannot open '%s' for reading: %s\n", source,
            strerror(errno));
    return 1;
  }

  int output = open(target, O_WRONLY | O_CREAT | O_TRUNC, 0666);
  if (output == -1) {
    fprintf(stderr, "pa2_cp: cannot create regular file '%s': %s\n", target,
            strerror(errno));
    close(input);
    return 1;
  }

  char buffer[BUFFER_SIZE];
  int status = 0;
  while (1) {
    ssize_t bytes_read = read(input, buffer, sizeof(buffer));
    if (bytes_read == 0) {
      break;
    }
    if (bytes_read < 0) {
      fprintf(stderr, "pa2_cp: error reading '%s': %s\n", source,
              strerror(errno));
      status = 1;
      break;
    }

    ssize_t offset = 0;
    while (offset < bytes_read) {
      ssize_t written =
          write(output, buffer + offset, (size_t)(bytes_read - offset));
      if (written < 0) {
        fprintf(stderr, "pa2_cp: error writing '%s': %s\n", target,
                strerror(errno));
        status = 1;
        break;
      }
      offset += written;
    }
    if (status != 0) {
      break;
    }
  }

  if (close(input) == -1) {
    status = 1;
  }
  if (close(output) == -1) {
    fprintf(stderr, "pa2_cp: error writing '%s': %s\n", target,
            strerror(errno));
    status = 1;
  }
  return status;
}

static char* resolve_target(const char* source, const char* dest,
                            bool multiple_sources) {
  struct stat dest_st;

  if (stat(dest, &dest_st) == -1) {
    if (errno == ENOTDIR) {
      fprintf(stderr, "pa2_cp: cannot stat '%s': Not a directory\n", dest);
      return NULL;
    }
    if (multiple_sources || ends_with_slash(dest)) {
      fprintf(stderr,
              "pa2_cp: cannot create regular file '%s': Not a directory\n",
              dest);
      return NULL;
    }
    return strdup(dest);
  }

  if (S_ISDIR(dest_st.st_mode)) {
    return join_path(dest, pa2_basename(source));
  }

  if (multiple_sources || ends_with_slash(dest)) {
    fprintf(stderr, "pa2_cp: cannot stat '%s': Not a directory\n", dest);
    return NULL;
  }

  return strdup(dest);
}

static int copy_one(const char* source, const char* dest,
                    bool multiple_sources) {
  struct stat source_st;
  if (stat(source, &source_st) == -1) {
    fprintf(stderr, "pa2_cp: cannot stat '%s': %s\n", source, strerror(errno));
    return 1;
  }

  char* target = resolve_target(source, dest, multiple_sources);
  if (target == NULL) {
    return 1;
  }

  struct stat target_st;
  if (stat(target, &target_st) == 0 && same_file(&source_st, &target_st)) {
    fprintf(stderr, "pa2_cp: '%s' and '%s' are the same file\n", source,
            target);
    free(target);
    return 1;
  }

  int status = copy_file_contents(source, target);
  free(target);
  return status;
}

/** 
# Synopsis
`pa2_cp SOURCE DEST`
`pa2_cp SOURCE… DIRECTORY`

# Description 
- Copy SOURCE to DEST. 
- If DEST exists and it is a file, overwrite DEST completely.
- If it is an existing DIRECTORY, copy multiple SOURCE(s) to DIRECTORY.
- Ignore cases when SOURCE is a directory. SOURCE is guaranteed to be a file.

# Errors

- When no arguments are passed, print “pa2_cp: missing file operand” 
  - Input: `pa2_cp`
  - Output: `pa2_cp: missing file operand`
- When only one argument is passed, print “pa2_cp: missing destination file operand after ‘SOURCE’” 
  - Input: `pa2_cp foo`
  - Output: `pa2_cp: missing file operand after ‘foo’`
- When a file does not exist, print “pa2_cp: cannot stat ‘FILE’: No such file or directory”
  - Input: `rm -f foo && pa2_cp foo foo1`
  - Output: `pa2_cp: cannot stat ‘foo’: No such file or directory`
- When target directory does not exist, print “pa2_cp: cannot create regular file ‘DIRECTORY’: Not a directory”
  - Input: `touch foo && pa2_cp foo bar/`
  - Output: `pa2_cp: cannot create regular file ‘bar/’: Not a directory`
**/
int main(int argc, char* argv[]) {
  if (argc == 1) {
    fprintf(stderr, "pa2_cp: missing file operand\n");
    return EXIT_FAILURE;
  }
  if (argc == 2) {
    fprintf(stderr,
            "pa2_cp: missing destination file operand after '%s'\n",
            argv[1]);
    return EXIT_FAILURE;
  }

  const char* dest = argv[argc - 1];
  bool multiple_sources = argc > 3;
  int status = 0;

  for (int i = 1; i < argc - 1; i++) {
    if (copy_one(argv[i], dest, multiple_sources) != 0) {
      status = 1;
    }
  }

  return status == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
