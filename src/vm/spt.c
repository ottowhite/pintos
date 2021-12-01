#include <debug.h>
#include <hash.h>
#include "vm/spt.h"

static void *spte_hash_func (const struct hash_elem *e_ptr, void *aux UNUSED);
static void *spte_less_func (const struct hash_elem *a_ptr,
                             const struct hash_elem *b_ptr,
                             void *aux UNUSED);

static void * 
spte_hash_func (const struct hash_elem *e_ptr, void *aux UNUSED)
{
  return hash_entry (e_ptr, struct spte, hash_elem)->uaddr;
}

static bool 
fte_less_func (const struct hash_elem *a_ptr,
               const struct hash_elem *b_ptr,
               void *aux UNUSED) 
{
  return hash_entry (a_ptr, struct spte, hash_elem)->uaddr <
         hash_entry (b_ptr, struct spte, hash_elem)->uaddr;
}
