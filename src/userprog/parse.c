#include "userprog/parse.h"
#include <string.h>

void
parse (char *input, int *argc, char **argv, char *argv_store) 
{
  // note: side affects input, should be discarded after use
  *argc = 0;
  char *save_ptr;
  input = strtok_r (input, " ", &save_ptr);
  // loop through arg tokens, adding them to argv_store and pointers to argv
  for (int offset = 0; input != NULL; ) 
    {
      int arg_length = strlen (input) + 1;

      strlcpy (argv_store + offset, input, arg_length);

      argv[*argc]  = argv_store + offset;
      offset      += arg_length;

      input        = strtok_r (NULL, " ", &save_ptr);
      (*argc)++;
    }
	// null terminator for argv
  argv[*argc] = 0;
}
