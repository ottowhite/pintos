#ifndef VM_SPT_H
#define VM_SPT_H

#include <stdint.h>
#include <stdbool.h>
#include <hash.h>

#include "filesys/off_t.h"
#include "filesys/file.h"
#include "filesys/inode.h"

/* Type of frame. */
enum frame_type
{
  STACK,
  EXECUTABLE_CODE,
  EXECUTABLE_DATA,
  ALL_ZERO,
  MMAP
};

/* Suplemental page table entry. */
struct spte
{
  void *uaddr;                 /* User address. */
  struct fte *fte_ptr;         /* Frame table entry pointer. */
  enum frame_type frame_type;  /* Type of frame. */
  struct inode *inode_ptr;     /* Pointer to indode. */
  off_t offset;                /* Inode offset. */
  int amount_occupied;         /* Amount of page not set to zero. */
  bool writable;               /* Page can be written to. */
  struct hash_elem hash_elem;  /* Hash element. */
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
