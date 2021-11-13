#include <syscall.h>
#include <stdio.h>

int
main (int argc, char **argv)
{
  printf ("%d\n", argc);
  printf ("%s\n", argv[0]);
  return EXIT_SUCCESS;
}
