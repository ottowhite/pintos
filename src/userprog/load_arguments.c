#include "userprog/load_arguments.h"
#include <string.h>
#include <stdint.h>

/* Decrements the write_dest by size, and then writes the item at write_src
   into the memory location at the new write_dest, then returns write_dest */
static void *
push_element (void *write_dest, void *write_src, int size)
{
  write_dest -= size;
  memcpy (write_dest, write_src, size);
  return write_dest;
}

/* Decrements the write_dest by size, and then writes size number of zeros into
   the memory location at the new write_dest, then returns write_dest */
static void *
push_zero_element (void *write_dest, int size)
{
  write_dest -= size;
  memset (write_dest, 0, size);
  return write_dest;
}

/* Sets up the stack starting at the pointer at esp, and moving downward.
   This side effects argv and decrements the pointer at esp to the start of the 
   stack. */
void load_arguments (int argc, char **argv, void **esp)
{
  /* pushes each of the arguments to the stack */
  for (int i = argc - 1; i >= 0; i--) {
    *esp = push_element (*esp, argv[i], strlen (argv[i]) + 1); // args
    argv[i] = *esp; // save arg pointer in old arg pos
  }

  int padding = ((uint32_t) *esp) % 4;
  *esp = push_zero_element (*esp, padding * sizeof (char));   // padding
  *esp = push_zero_element (*esp, sizeof (char *));           // null address

  /* pushes each of the pointers to the arguments to the stack */
  for (int i = argc - 1; i >= 0; i--)
      *esp = push_element  (*esp, &argv[i], sizeof (char *)); // arg pointers

  *esp = push_element      (*esp, esp, sizeof (char **));     // argv
  *esp = push_element      (*esp, &argc, sizeof (int));       // argc
  *esp = push_zero_element (*esp, sizeof (void *));           // null return
}
