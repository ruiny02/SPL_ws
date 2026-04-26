// Do not use exec*!
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>


/** 
# Synopsis
`pa2_head [OPTION] [FILE]`

# Description
- Prints the first 10 lines of a FILE to standard output.
- If the file has fewer than 10 lines, it will print the whole file without any padding.
- If no FILE is provided, or FILE is -, read standard input.
- -n NUM	prints up to NUM lines instead of 10
- It is guaranteed that NUM is not negative.

# Errors

- When FILE does not exist, print "pa2_head: cannot open 'FILE' for reading: No such file or directory".  
  - Note that "No such file or directory" comes from errno. 
  - Input: `rm –f foo; pa2_head foo`
  - Output: `pa2_head: cannot open ‘foo' for reading: No such file or directory`
- When FILE is a directory, print "pa2_head: error reading ‘FILE’: Is a directory".
  - Input: `mkdir bar; pa2_head bar ; rmdir bar`
  - Output: `pa2_head: error reading ‘bar’ : Is a directory`
- When NUM is not a number, print "pa2_head: invalid number of lines: ‘NUM’"
  - Input: `touch foo; pa2_head foo –n foo; rm foo`
  - Output: `pa2_head: invalid number of lines: ‘foo’`
**/

int main(int argc, char* argv[]) {
    return EXIT_SUCCESS;
}