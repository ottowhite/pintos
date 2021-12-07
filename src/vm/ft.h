#ifndef VM_FT_H
#define VM_FT_H

#include <hash.h>
#include "userprog/syscall.h"
#include "vm/spt.h"

/* Retrieval methods for frame table entries being evicted */
enum eviction_method {
  DELETE,
  SWAP,
  SWAP_IF_DIRTY,
  WRITE_IF_DIRTY
};

/* Uniquely references the thread and upage that reference a frame */
struct owner
{
  struct thread *owner_ptr;
  void *upage_ptr;
};

/* used to store a single owner when multiple processes own a frame */
struct owner_list_elem
{
  struct owner owner;
  struct list_elem elem;
};

/* When swapped is false, use frame_ptr, otherwise use swap_index */
union Frame_location
{
  void *frame_ptr;
  int swap_index;
};

/* When shared is false, use owner_single, otherwise use owner_list_ptr */
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
  union Frame_location loc;
  enum eviction_method eviction_method;
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
