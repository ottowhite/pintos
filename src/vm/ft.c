#include <hash.h>
#include <debug.h>
#include "threads/malloc.h"
#include "threads/synch.h"
#include "threads/vaddr.h"
#include "userprog/syscall.h"
#include "vm/ft.h"

void initialize_ft (struct hash *ft_ptr);
void deallocate_ft (struct hash *ft_ptr);

static unsigned fte_hash_func       (const struct hash_elem *e_ptr, void *aux);
static bool     fte_less_func       (const struct hash_elem *a_ptr,
                                     const struct hash_elem *b_ptr,
                                     void *aux UNUSED);
static void     fte_deallocate_func (struct hash_elem *e_ptr, void *aux UNUSED);

static struct lock ft_lock;
static int fid_cnt;

/* Initilizes the frame table as a hash map of struct ftes */
void 
initialize_ft (struct hash *ft_ptr)
{
  hash_init (ft_ptr, &fte_hash_func, &fte_less_func, NULL);
  lock_init (&ft_lock);
  fid_cnt = 0;
}

/* Deallocate the frame / swap table and all entries. 
   There should be no entries at this point.
   All threads must have terminated before this so the frame table content 
   is consistent and hash clear doesn't yield undefined behaviour. */
void 
deallocate_ft (struct hash *ft_ptr)
{
  lock_acquire (&ft_lock);
  hash_destroy (ft_ptr, &fte_deallocate_func);
  lock_release (&ft_lock);
}

void 
insert_fte (struct hash *ft_ptr,
            void *frame_location,
            enum retrieval_method retrieval_method,
            int amount_occupied)
{
  struct fte *fte_ptr = malloc (sizeof (struct fte));
  if (fte_ptr == NULL) syscall_exit (-1);

  lock_acquire (&ft_lock);

  fte_ptr->fid              = fid_cnt++;
  fte_ptr->swapped          = false;
  fte_ptr->shared           = false;
  fte_ptr->pinned           = false;
  fte_ptr->frame_location   = frame_location;
  fte_ptr->retrieval_method = retrieval_method;
  fte_ptr->amount_occupied  = amount_occupied;

  hash_insert (ft_ptr, &fte_ptr->hash_elem);
    
  // TODO: Set the frame table presence bit in bitmap
  int frame_index = (frame_location - PHYS_BASE) / PGSIZE;

  lock_release (&ft_lock);
}

static unsigned
fte_hash_func (const struct hash_elem *e_ptr, void *aux UNUSED)
{
  return hash_entry (e_ptr, struct fte, hash_elem)->fid;
}

static bool 
fte_less_func (const struct hash_elem *a_ptr,
               const struct hash_elem *b_ptr,
               void *aux UNUSED) 
{
  return hash_entry (a_ptr, struct fte, hash_elem)->fid <
         hash_entry (b_ptr, struct fte, hash_elem)->fid;
}

static void
fte_deallocate_func (struct hash_elem *e_ptr, void *aux UNUSED)
{
  // TODO (complete once allocation is done)
  // Free all resources associated with a particular frame at the end of 
  // execution. This should in theory have to run on no FTEs

  // ft_lock held by caller
  free (hash_entry (e_ptr, struct fte, hash_elem));
}
