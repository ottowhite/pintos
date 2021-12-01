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
};

#endif
