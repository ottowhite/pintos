#include <syscall.h>
#include <stdio.h>

int
main (int argc, char **argv)
{
  printf ("%d\n", argc);
  printf ("%s\n", argv[1]);
  printf ("%s\n", argv[2]);
  return EXIT_SUCCESS;
}
