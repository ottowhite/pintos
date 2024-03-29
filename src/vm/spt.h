#ifndef VM_SPT_H
#define VM_SPT_H

#include <stdint.h>
#include <stdbool.h>
#include <hash.h>

#include "filesys/off_t.h"
#include "filesys/file.h"
#include "filesys/inode.h"

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
  struct fte *fte_ptr;
  enum frame_type frame_type;
  struct inode *inode_ptr;
  off_t offset;
  int amount_occupied;
  bool writable;
  struct hash_elem hash_elem;
};

#include "vm/ft.h"

bool         spt_init              (struct hash **spt_ptr_ptr);
void         spt_destroy           (struct hash *spt_ptr);
bool         spt_propagate_removal (struct hash *spt_ptr, void *uaddr);
struct spte *spt_find_entry        (struct hash *spt_ptr, void *uaddr);
struct spte *spt_add_entry         (struct hash *spt_ptr,
                                    void *uaddr,
                                    enum frame_type frame_type,
                                    struct inode *inode_ptr,
                                    off_t offset,
                                    int amount_occupied,
                                    bool writable);

#endif
