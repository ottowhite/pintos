#ifndef VM_SFT_H
#define VM_SFT_H

#include <hash.h>
#include "filesys/off_t.h"
#include "filesys/file.h"
#include "filesys/inode.h"

struct sfte
{
  struct inode *inode_ptr;
  off_t offset;
  int fid;
  struct hash_elem hash_elem;
};

#endif
