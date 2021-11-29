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

void initialize_ft (struct hash *ft_ptr);
void deallocate_ft (struct hash *ft_ptr);
void insert_fte    (struct hash *ft_ptr,
                    void *frame_location,
                    enum retrieval_method retrieval_method,
                    int amount_occupied);
#endif
