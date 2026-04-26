// Do not use exec*!
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>


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
    return EXIT_SUCCESS;
}