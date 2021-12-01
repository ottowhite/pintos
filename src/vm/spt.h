#ifndef VM_SPT_H
#define VM_SPT_H

#include <stdint.h>

#include "filesys/off_t.h"
#include "filesys/file.h"

enum frame_type
{
  STACK,
  EXECUTABLE_CODE,
  EXECUTABLE_DATA,
  ALL_ZERO,
  MMAP
};

struct spte
{
  void *uaddr;
  uint32_t fid;
  enum frame_type frame_type;
  struct inode *inode_ptr;
  off_t offset;
  int amount_occupied;
  struct hash_elem hash_elem;
};

bool         spt_init         (struct hash *spt_ptr);
void         spt_destroy      (struct hash *spt_ptr);
void         spt_remove_entry (void);
struct spte *spt_add_entry    (struct hash *spt_ptr,
                               void *uaddr,
                               enum frame_type frame_type,
                               struct inode *inode_ptr,
                               off_t offset,
                               int amount_occupied);

#endif
