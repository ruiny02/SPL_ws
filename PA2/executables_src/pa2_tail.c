// Do not use exec*!
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static int parse_line_count(const char* value, size_t* count) {
  char* end = NULL;
  errno = 0;
  unsigned long long parsed = strtoull(value, &end, 10);
  if (errno != 0 || value[0] == '\0' || *end != '\0') {
    fprintf(stderr, "pa2_tail: invalid number of lines: '%s'\n", value);
    return 1;
  }
  *count = (size_t) parsed;
  return 0;
}

static FILE* open_input(const char* path) {
  if (path == NULL || strcmp(path, "-") == 0) {
    return stdin;
  }

  struct stat st;
  if (stat(path, &st) == -1) {
    fprintf(stderr, "pa2_tail: cannot open '%s' for reading: %s\n", path,
            strerror(errno));
    return NULL;
  }

  if (S_ISDIR(st.st_mode)) {
    fprintf(stderr, "pa2_tail: error reading '%s': Is a directory\n", path);
    return NULL;
  }

  FILE* fp = fopen(path, "r");
  if (fp == NULL) {
    fprintf(stderr, "pa2_tail: cannot open '%s' for reading: %s\n", path,
            strerror(errno));
  }
  return fp;
}

static void free_lines(char** lines, size_t* lengths, size_t count) {
  (void) lengths;
  for (size_t i = 0; i < count; i++) {
    free(lines[i]);
  }
  free(lines);
  free(lengths);
}

static int print_tail(FILE* fp, const char* label, size_t line_count) {
  if (line_count == 0) {
    return 0;
  }

  char** lines = calloc(line_count, sizeof(*lines));
  size_t* lengths = calloc(line_count, sizeof(*lengths));
  if (lines == NULL || lengths == NULL) {
    perror("pa2_tail");
    free(lines);
    free(lengths);
    return 1;
  }

  char* line = NULL;
  size_t capacity = 0;
  ssize_t length;
  size_t total_lines = 0;

  while ((length = getline(&line, &capacity, fp)) != -1) {
    size_t slot = total_lines % line_count;
    free(lines[slot]);
    lines[slot] = line;
    lengths[slot] = (size_t) length;
    line = NULL;
    capacity = 0;
    total_lines++;
  }
  free(line);

  if (ferror(fp)) {
    fprintf(stderr, "pa2_tail: error reading '%s': %s\n", label,
            strerror(errno));
    free_lines(lines, lengths, line_count);
    return 1;
  }

  size_t used = total_lines < line_count ? total_lines : line_count;
  size_t start = total_lines < line_count ? 0 : total_lines % line_count;

  for (size_t i = 0; i < used; i++) {
    size_t slot = (start + i) % line_count;
    if (fwrite(lines[slot], 1, lengths[slot], stdout) != lengths[slot]) {
      fprintf(stderr, "pa2_tail: write error: %s\n", strerror(errno));
      free_lines(lines, lengths, line_count);
      return 1;
    }
  }

  free_lines(lines, lengths, line_count);
  return 0;
}

/** 
# Synopsis
`pa2_tail [OPTION] [FILE]`

# Description
- Prints the last 10 lines of a FILE to standard output.
- If the file has fewer than 10 lines, it will print the whole file without any padding.
- If no FILE is provided, or FILE is -, read standard input.
- -n NUM	prints up to NUM lines instead of 10
- It is guaranteed that NUM is not negative.

# Errors

- When FILE does not exist, print “pa2_tail: cannot open 'FILE' for reading: No such file or directory”.  
  - Note that “No such file or directory” comes from errno. 
  - Input: `rm –f foo; pa2_tail foo`
  - Output: `pa2_tail: cannot open ‘foo' for reading: No such file or directory`
- When FILE is a directory, print “pa2_tail: error reading ‘FILE’: Is a directory”.
  - Input: `mkdir bar; pa2_tail bar; rmdir bar`
  - Output: `pa2_tail: error reading ‘bar’ : Is a directory`
- When NUM is not a number, print “pa2_tail: invalid number of lines: ‘NUM’”
  - Input: `touch foo; pa2_tail foo –n foo; rm foo`
  - Output: `pa2_tail: invalid number of lines: ‘foo’`
**/
int main(int argc, char* argv[]) {
  size_t line_count = 10;
  int opt;

  opterr = 0;
  while ((opt = getopt(argc, argv, "n:")) != -1) {
    switch (opt) {
      case 'n':
        if (parse_line_count(optarg, &line_count) != 0) {
          return EXIT_FAILURE;
        }
        break;
      default:
        fprintf(stderr, "pa2_tail: invalid option -- '%c'\n", optopt);
        return EXIT_FAILURE;
    }
  }

  const char* path = (optind < argc) ? argv[optind] : NULL;
  FILE* input = open_input(path);
  if (input == NULL) {
    return EXIT_FAILURE;
  }

  int status = print_tail(input, path == NULL ? "-" : path, line_count);
  if (input != stdin) {
    fclose(input);
  }
  return status == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
