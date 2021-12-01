#include <debug.h>
#include <hash.h>
#include "threads/malloc.h"
#include "vm/spt.h"

static unsigned spte_hash_func       (const struct hash_elem *e_ptr, 
                                      void *aux UNUSED);
static bool     spte_less_func       (const struct hash_elem *a_ptr,
                                      const struct hash_elem *b_ptr,
                                      void *aux UNUSED);
static void  spte_deallocate_func (struct hash_elem *e_ptr, void *aux UNUSED);

static unsigned
spte_hash_func (const struct hash_elem *e_ptr, void *aux UNUSED)
{
  return hash_entry (e_ptr, struct spte, hash_elem)->uaddr;
}

static bool 
spte_less_func (const struct hash_elem *a_ptr,
                const struct hash_elem *b_ptr,
                void *aux UNUSED) 
{
  return hash_entry (a_ptr, struct spte, hash_elem)->uaddr <
         hash_entry (b_ptr, struct spte, hash_elem)->uaddr;
}

static void
spte_deallocate_func (struct hash_elem *e_ptr, void *aux UNUSED)
{
  free (hash_entry (e_ptr, struct spte, hash_elem));
}

void 
spt_init (struct hash *spt_ptr)
{
  hash_init (spt_ptr, &spte_hash_func, &spte_less_func, NULL);
}

void 
spt_destroy (struct hash *spt_ptr)
{
  hash_destroy (spt_ptr, &spte_deallocate_func);
}
