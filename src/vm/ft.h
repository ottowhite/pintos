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

void ft_init    (struct hash *ft_ptr);
void ft_free    (struct hash *ft_ptr);
void fte_insert (struct hash *ft_ptr,
                 void *frame_location,
                 enum retrieval_method retrieval_method,
                 int amount_occupied);
#endif
