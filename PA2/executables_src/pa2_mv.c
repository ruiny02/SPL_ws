// Do not use exec*!
#include <ctype.h>
#include <stdio.h> // rename()
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// Retrieved from https://sourceware.org/git/?p=glibc.git;a=blob_plain;f=string/basename.c
char* pa2_basename(const char* path) {
  char* path_p = strrchr(path, '/');
  return path_p ? path_p + 1 : (char*) path;
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
    return EXIT_SUCCESS;
}