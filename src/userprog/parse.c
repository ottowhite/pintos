#include "parse.h"
#include <string.h>

// should be using strlcpy
void
parse (char *input, int *argc, char **argv, char *argv_str) 
{
  // note: side affects input, should be discarded after use
  *argc = 0;
  char *save_ptr;
  input = strtok_r (input, " ", &save_ptr);
  // We don't have access to strdup
  for (int offset = 0; input != NULL; ) {
    int arg_length = strlen (input) + 1;

    strncpy (argv_str + offset, input, arg_length);

    argv[*argc]  = argv_str + offset;
    offset      += arg_length;

    input        = strtok_r (NULL, " ", &save_ptr);
    (*argc)++;
  }
	// null terminator for argv
  argv[*argc] = 0;
}

