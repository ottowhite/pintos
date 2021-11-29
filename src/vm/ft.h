#ifndef VM_FT_H
#define VM_FT_H

#include <hash.h>

/* Retrieval methods for frame table entries being evicted */
enum retrieval_method
{
  DELETE,
  WRITE_READ,
  SWAP
};

enum page_type
{
  STACK
};

/* Frame / swap table entry */
struct fte 
{
  int fid;
  bool swapped;
  bool shared;
  bool pinned;
  void *frame_location;
  enum retrieval_method retrieval_method;
  int amount_occupied;
  struct hash_elem hash_elem;
};

void ft_init       (void);
void ft_free       (void);
void *ft_get_frame (enum page_type type);
#endif
