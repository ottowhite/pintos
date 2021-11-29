#include <hash.h>
#include <debug.h>
#include "vm/ft.h"

void initialize_ft (struct hash *ft);
void deallocate_ft (struct hash *ft);

static unsigned fte_hash_func (const struct hash_elem *e, void *aux);
static bool fte_less_func (const struct hash_elem *a,
                           const struct hash_elem *b,
                           void *aux UNUSED);
static void fte_deallocate_func (struct hash_elem *e, void *aux UNUSED);


/* Initilizes the frame table as a hash map of struct ftes */
void 
initialize_ft (struct hash *ft)
{
  hash_init (ft, &fte_hash_func, &fte_less_func, NULL);
}

/* Deallocate the frame / swap table and all entries. 
   There should be no entries at this point.
   All threads must have terminated before this so the frame table content 
   is consistent and hash clear doesn't yield undefined behaviour. */
void 
deallocate_ft (struct hash *ft UNUSED)
{
  hash_destroy (ft, &fte_deallocate_func);
}

static unsigned
fte_hash_func (const struct hash_elem *e, void *aux)
{
  return 0;
}

static bool 
fte_less_func (const struct hash_elem *a,
               const struct hash_elem *b,
               void *aux UNUSED) 
{
  return false;
}

static void
fte_deallocate_func (struct hash_elem *e, void *aux UNUSED)
{
  return;
}
