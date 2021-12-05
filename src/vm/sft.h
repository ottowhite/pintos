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

bool         sft_init    (void);
void         sft_destroy (void);
bool         sft_insert  (int fid, struct inode *inode_ptr, off_t offset);
bool         sft_remove  (struct sfte *sfte_ptr);
struct sfte *sft_search  (struct inode *inode_ptr, off_t offset);

#endif
