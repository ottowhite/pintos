#ifndef VM_SFT_H
#define VM_SFT_H

#include "filesys/off_t.h"
#include "filesys/file.h"
#include "filesys/inode.h"

struct public_frame
{
  struct inode *inode_ptr;
  off_t offset;
  int fid;
};

#endif
