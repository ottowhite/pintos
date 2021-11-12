#include "parse.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

// TODO remove main and unnecessary includes

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

int
main (void)
{
  char input[] = "   hello   there  slime   ";

  int argc;
  char **argv = calloc (100, sizeof (char *));

  void *esp = calloc (300, sizeof (void *));


  parse (input, &argc, argv);

  void *writing_pointer = &esp[100];
  printf ("argc: %d\n", argc);
  for (int i = argc - 1; i >= 0; i--) {
    printf("writing argument %d: %s\n", i, argv[i]);
    writing_pointer -= strlen(argv[i]) + 1;
    memcpy(writing_pointer, argv[i], strlen(argv[i]) + 1);
    argv[i] = writing_pointer;
  }

  /* round the pointer down to a multiple of 4 */
  uint32_t padding = ((uint32_t) writing_pointer) % 4;
  printf("writing_pointer: %x\n", writing_pointer);
  printf("padding: %d\n", padding);
  writing_pointer = ((uint32_t) writing_pointer) & ~((uint32_t) 3);
  //writing_pointer -= 2;
  printf("writing_pointer: %x\n", writing_pointer);
  memset(writing_pointer, 0, padding);
  

  printf("%s\n", (&esp[100] - 6));
  printf("%s\n", (&esp[100] - 12));
  printf("%s\n", (&esp[100] - 18));

  writing_pointer -= sizeof(char *);
  *((char **) writing_pointer) = NULL;

  for (int i = argc - 1; i >= 0; i--) {
    writing_pointer -= sizeof(char *);
    *((char **) writing_pointer) = argv[i];
  }

  writing_pointer -= sizeof(char **);
  *((char **) writing_pointer) = writing_pointer + sizeof(char **);

  writing_pointer -= sizeof(int);
  *((int *) writing_pointer) = argc;

  writing_pointer -= sizeof(void *);
  *((void **) writing_pointer) = NULL;

  for (int i = argc - 1; i >= 0; i--) {
    free (argv[i]);
  }
  free (argv);
}

