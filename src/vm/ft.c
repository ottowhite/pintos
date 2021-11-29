#include <hash.h>
#include <debug.h>
#include "vm/ft.h"

void initialize_ft (struct hash *ft_ptr);
void deallocate_ft (struct hash *ft_ptr);

static unsigned fte_hash_func       (const struct hash_elem *e_ptr, void *aux);
static bool     fte_less_func       (const struct hash_elem *a_ptr,
                                     const struct hash_elem *b_ptr,
                                     void *aux UNUSED);
static void     fte_deallocate_func (struct hash_elem *e_ptr, void *aux UNUSED);


/* Initilizes the frame table as a hash map of struct ftes */
void 
initialize_ft (struct hash *ft_ptr)
{
  hash_init (ft_ptr, &fte_hash_func, &fte_less_func, NULL);
}

/* Deallocate the frame / swap table and all entries. 
   There should be no entries at this point.
   All threads must have terminated before this so the frame table content 
   is consistent and hash clear doesn't yield undefined behaviour. */
void 
deallocate_ft (struct hash *ft_ptr)
{
  hash_destroy (ft_ptr, &fte_deallocate_func);
}

static unsigned
fte_hash_func (const struct hash_elem *e_ptr, void *aux UNUSED)
{
  return hash_entry (e_ptr, struct fte, elem)->fid;
}

static bool 
fte_less_func (const struct hash_elem *a_ptr,
               const struct hash_elem *b_ptr,
               void *aux UNUSED) 
{
  return hash_entry (a_ptr, struct fte, elem)->fid <
         hash_entry (b_ptr, struct fte, elem)->fid;
}

static void
fte_deallocate_func (struct hash_elem *e_ptr, void *aux UNUSED)
{
  // TODO (complete once allocation is done)
  // Free all resources associated with a particular frame at the end of 
  // execution. This should in theory have to run on no FTEs
  return;
}
