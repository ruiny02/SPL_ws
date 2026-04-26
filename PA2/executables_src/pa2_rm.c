// Do not use exec*!
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h> // unlink()

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
    return EXIT_SUCCESS;
}