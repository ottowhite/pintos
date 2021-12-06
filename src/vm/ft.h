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

/* used to store a single owner when multiple processes own a frame */
struct pde_list_elem
{
  uint32_t *pde_ptr;
  struct list_elem elem;
};

/* list element for the owners list */
union Pde
{
  uint32_t *pde_ptr;
  struct list *pde_list_ptr;
};

/* Frame / swap table entry */
struct fte 
{
  bool swapped;
  bool shared;
  int pin_cnt;
  struct inode *inode_ptr;
  off_t offset;
  union Pde pdes;
  void *frame_location;
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
