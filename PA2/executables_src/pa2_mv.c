// Do not use exec*!
#include <errno.h>
#include <libgen.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

// Retrieved from https://sourceware.org/git/?p=glibc.git;a=blob_plain;f=string/basename.c
static char* pa2_basename(const char* path) {
  char* path_p = strrchr(path, '/');
  return path_p ? path_p + 1 : (char*) path;
}

static bool ends_with_slash(const char* path) {
  size_t len = strlen(path);
  return len > 0 && path[len - 1] == '/';
}

static char* join_path(const char* dir, const char* base) {
  size_t dir_len = strlen(dir);
  size_t base_len = strlen(base);
  bool need_slash = dir_len > 0 && dir[dir_len - 1] != '/';

  char* joined = malloc(dir_len + need_slash + base_len + 1);
  if (joined == NULL) {
    perror("pa2_mv");
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

static char* parent_path(const char* path) {
  char* copy = strdup(path);
  if (copy == NULL) {
    perror("pa2_mv");
    return NULL;
  }
  char* dir = dirname(copy);
  char* result = strdup(dir);
  free(copy);
  if (result == NULL) {
    perror("pa2_mv");
  }
  return result;
}

static bool is_subdirectory_target(const char* source, const char* target) {
  char* target_parent = parent_path(target);
  if (target_parent == NULL) {
    return false;
  }

  char* real_source = realpath(source, NULL);
  char* real_parent = realpath(target_parent, NULL);
  free(target_parent);

  if (real_source == NULL || real_parent == NULL) {
    free(real_source);
    free(real_parent);
    return false;
  }

  size_t source_len = strlen(real_source);
  bool result = strcmp(real_source, real_parent) == 0 ||
                (strncmp(real_source, real_parent, source_len) == 0 &&
                 real_parent[source_len] == '/');

  free(real_source);
  free(real_parent);
  return result;
}

static char* resolve_target(const char* source, const char* dest,
                            bool multiple_sources) {
  struct stat dest_st;

  if (stat(dest, &dest_st) == -1) {
    if (errno == ENOTDIR) {
      fprintf(stderr, "pa2_mv: cannot stat '%s': Not a directory\n", dest);
      return NULL;
    }
    if (multiple_sources || ends_with_slash(dest)) {
      fprintf(stderr, "pa2_mv: cannot move '%s' to '%s': Not a directory\n",
              pa2_basename(source), dest);
      return NULL;
    }
    return strdup(dest);
  }

  if (S_ISDIR(dest_st.st_mode)) {
    return join_path(dest, pa2_basename(source));
  }

  if (multiple_sources || ends_with_slash(dest)) {
    fprintf(stderr, "pa2_mv: cannot stat '%s': Not a directory\n", dest);
    return NULL;
  }

  return strdup(dest);
}

static int move_one(const char* source, const char* dest,
                    bool multiple_sources) {
  struct stat source_st;
  if (stat(source, &source_st) == -1) {
    fprintf(stderr, "pa2_mv: cannot stat '%s': %s\n", pa2_basename(source),
            strerror(errno));
    return 1;
  }

  char* target = resolve_target(source, dest, multiple_sources);
  if (target == NULL) {
    return 1;
  }

  struct stat target_st;
  if (stat(target, &target_st) == 0 && same_file(&source_st, &target_st)) {
    fprintf(stderr, "pa2_mv: '%s' and '%s' are the same file\n",
            pa2_basename(source), target);
    free(target);
    return 1;
  }

  if (S_ISDIR(source_st.st_mode) && is_subdirectory_target(source, target)) {
    fprintf(stderr,
            "pa2_mv: cannot move '%s' to a subdirectory of itself, '%s'\n",
            pa2_basename(source), target);
    free(target);
    return 1;
  }

  if (rename(source, target) == -1) {
    if (errno == EINVAL) {
      fprintf(stderr,
              "pa2_mv: cannot move '%s' to a subdirectory of itself, '%s'\n",
              pa2_basename(source), target);
    } else if (errno == ENOTDIR && ends_with_slash(dest)) {
      fprintf(stderr, "pa2_mv: cannot stat '%s': Not a directory\n", dest);
    } else {
      fprintf(stderr, "pa2_mv: cannot move '%s' to '%s': %s\n",
              pa2_basename(source), target, strerror(errno));
    }
    free(target);
    return 1;
  }

  free(target);
  return 0;
}

/** 
# Synopsis
`pa2_mv SOURCE DEST`
`pa2_mv SOURCE… DIRECTORY`

# Description
- Rename SOURCE to DEST. 
- If DEST exists and it is a file, overwrite DEST completely.
- If it is an existing DIRECTORY, move multiple SOURCE(s) to DIRECTORY.
- SOURCE can be a directory as well.
- When handling errors, print just the basename ( ‘foo.txt’ instead of ‘/…/…/foo.txt’ or ‘bar/foo.txt’)
- TARGET: DEST or DIRECTORY/basename(SOURCE)

# Errors

- When no arguments are passed, print “pa2_mv: missing file operand” 
  - Input: `pa2_mv`
  - Output: `pa2_mv: missing file operand`
- When only one argument is passed, print “pa2_mv: missing destination file operand after ‘SOURCE’” 
  - Input: `pa2_mv foo`
  - Output: `pa2_mv: missing file operand after ‘foo’`
- When a file does not exist, print “pa2_mv: cannot stat ‘SOURCE’: No such file or directory”
  - Input: `rm -f foo; pa2_mv foo`
  - Output: `pa2_mv: cannot stat ‘foo’: No such file or directory`
- When target directory does not exist, print “pa2_mv: cannot move ‘SOURCE’ to ‘DIRECTORY’: Not a directory”
  - Input: `touch foo; pa2_mv foo bar/; rm foo`
  - Output: `pa2_mv: cannot move ‘foo’ to ‘bar/’: Not a directory`
- When target directory is not a directory, print “pa2_mv: cannot stat ‘DIRECTORY’: Not a directory”
  - Input: `touch foo; touch bar; pa2_mv foo bar/; rm foo bar`
  - Output: `pa2_mv: cannot stat ‘bar/’: Not a directory`
- When a directory cannot be accessed, print “pa2_mv: cannot move ‘SOURCE’ to ‘TARGET’: Permission denied”
  - Input: `mkdir bar; chmod u-w bar; touch foo; pa2_mv foo bar/; rm foo; rmdir bar`
  - Output: `pa2_mv: cannot move ‘foo’ to ‘bar/foo’: Permission denied`
- When the SOURCE and TARGET are the same, print “pa2_mv: ‘SOURCE' and ‘TARGET’ are the same file”
  - Input: `touch foo; pa2_mv foo foo`
  - Output: `pa2_mv: ‘foo’ and ‘foo’ are the same file`
- When the DIRECTORY is a subdirectory of SOURCE, print “pa2_mv: cannot move ‘SOURCE’ to a subdirectory of itself, ‘TARGET’”
  - Input: `mkdir -p bar/foo; pa2_mv bar bar/foo; rm -r bar`
  - Output: `pa2_mv: cannot move ‘bar’ to a subdirectory of itself, ‘bar/foo/bar’`
**/
int main(int argc, char* argv[]) {
  if (argc == 1) {
    fprintf(stderr, "pa2_mv: missing file operand\n");
    return EXIT_FAILURE;
  }
  if (argc == 2) {
    fprintf(stderr,
            "pa2_mv: missing destination file operand after '%s'\n",
            pa2_basename(argv[1]));
    return EXIT_FAILURE;
  }

  const char* dest = argv[argc - 1];
  bool multiple_sources = argc > 3;
  int status = 0;

  for (int i = 1; i < argc - 1; i++) {
    if (move_one(argv[i], dest, multiple_sources) != 0) {
      status = 1;
    }
  }

  return status == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
