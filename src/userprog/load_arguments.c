#include "userprog/load_arguments.h"
#include <string.h>
#include <stdint.h>

static void *
push_element (void *write_dest, void *write_src, int size)
{
  write_dest -= size;
  memcpy (write_dest, write_src, size);
  return write_dest;
}

static void *
push_zero_element (void *write_dest, int size)
{
  write_dest -= size;
  memset (write_dest, 0, size);
  return write_dest;
}

void load_arguments (int argc, char **argv, void **esp)
{
  for (int i = argc - 1; i >= 0; i--) {
    *esp = push_element (*esp, argv[i], strlen (argv[i]) + 1); // args
    argv[i] = *esp; // save arg pointer in old arg pos
  }

  int padding = ((uint64_t) *esp) % 4;
  *esp = push_zero_element (*esp, padding * sizeof (char));   // padding
  *esp = push_zero_element (*esp, sizeof (char *));           // null address

  for (int i = argc - 1; i >= 0; i--)
      *esp = push_element  (*esp, &argv[i], sizeof (char *)); // arg pointers

  *esp = push_element      (*esp, &esp, sizeof (char **));    // argv
  *esp = push_element      (*esp, &argc, sizeof (int));       // argc
  *esp = push_zero_element (*esp, sizeof (void *));           // null return func
}
