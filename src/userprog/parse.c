#include "userprog/parse.h"
#include <string.h>

void
parse (const char *input, int *argc, char **buffer) 
{
  *argc            = 0;
  int input_length = strlen (input) + 1;

  char curr[input_length];
  char *curr_ptr = &curr[0];

	// should be using strlcpy
  strncpy (curr, input, input_length);

  char *save_ptr;
  curr_ptr = strtok_r (curr, " ", &save_ptr);

  while (curr_ptr != NULL) 
		{
			// We don't have access to strdup
  	  buffer[*argc] = strdup (curr_ptr);
  	  // printf ("%s\n", curr_ptr);
  	  // printf ("%d\n", strlen (curr_ptr));
  	  // printf ("%s\n", buffer[*argc]);
  	  // strncpy (buffer[*argc], curr_ptr, strlen (curr_ptr) + 1);
  	  curr_ptr = strtok_r (NULL, " ", &save_ptr);
  	  (*argc)++;
  	}
}

// int
// main (void)
// {
//   char input[] = "   hello   there  slime   ";
// 
//   int argc;
//   char **argv = calloc (100, sizeof (char *));
// 
//   void *esp = calloc (100, sizeof (void *));
// 
//   parse (input, &argc, argv);
// 
//   printf ("argc: %d\n", argc);
//   for (int i = 0; i < argc; i++) {
//     printf("argument %d: %s\n", i, argv[i]);
//     free (argv[i]);
//   }
//   free (argv);
// }
// 
