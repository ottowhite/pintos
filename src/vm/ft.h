#ifndef VM_FT_H
#define VM_FT_H

#include <hash.h>
#include "userprog/syscall.h"
#include "vm/spt.h"

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
  pid_t owner;
  void *frame_location;
  enum retrieval_method retrieval_method;
  int amount_occupied;
  struct hash_elem hash_elem;
};

void        ft_init         (void);
void        ft_destroy      (void);
void        ft_remove_frame (struct fte *fte_ptr);
struct fte *ft_get_frame    (pid_t owner, 
                             enum frame_type frame_type, 
                             struct inode *inode_ptr,
                             off_t offset,
                             int amount_occupied);
#endif
