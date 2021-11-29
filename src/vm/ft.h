#ifndef VM_FT_H
#define VM_FT_H

/* Retrieval methods for frame table entries being evicted */
enum retrieval_method
{
  DELETE,
  WRITE_READ,
  SWAP
};

struct fte 
{
  int fid;
  bool swapped;
  bool shared;
  bool pinned;
  void *frame_location;
  int frame_index;
  enum retrieval_method retrieval_method;
  int amount_occupied;
};

#endif
