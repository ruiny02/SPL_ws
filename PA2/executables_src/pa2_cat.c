// Do not use exec*!
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>


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
    return EXIT_SUCCESS;
}