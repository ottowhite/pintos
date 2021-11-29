#ifndef VM_FT_H
#define VM_FT_H

/* Retrieval methods for frame table entries being evicted */
enum retrieval_method
{
  DELETE,
  WRITE_READ,
  SWAP
};

#endif
