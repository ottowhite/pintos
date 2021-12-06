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

/* Uniquely references the page table and upage that reference a frame */
struct owner
{
  uint32_t *pd_ptr;
  void *upage_ptr;
};

/* used to store a single owner when multiple processes own a frame */
struct owner_list_elem
{
  struct owner owner;
  struct list_elem elem;
};

union Frame_location
{
  void *frame_ptr;
  int swap_index;
};

/* list element for the owners list */
union Owner
{
  struct owner owner_single;
  struct list *owner_list_ptr;
};

/* Frame / swap table entry */
struct fte 
{
  bool swapped;
  bool shared;
  int pin_cnt;
  struct inode *inode_ptr;
  off_t offset;
  union Owner owners;
  union Frame_location frame_location;
  enum retrieval_method retrieval_method;
  int amount_occupied;
  struct hash_elem hash_elem;
};


bool        ft_init                 (void);
void        ft_destroy              (void);
void        ft_remove_frame         (struct fte *fte_ptr);
struct fte *ft_get_frame            (struct spte *spte_ptr);
struct fte *ft_get_frame_preemptive (enum frame_type frame_type, 
                                     struct inode *inode_ptr,
                                     off_t offset,
                                     int amount_occupied);
bool        ft_install_frame        (struct spte *spte_ptr, 
                                     struct fte *fte_ptr);
#endif
