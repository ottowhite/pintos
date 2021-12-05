#include <hash.h>
#include <debug.h>
#include "vm/sft.h"
#include "threads/synch.h"
#include "threads/malloc.h"

static struct hash sft;
static struct lock sft_lock;

/* Shared frame table hashmap helpers */
static unsigned sfte_hash_func       (const struct hash_elem *e_ptr, 
                                      void *aux UNUSED);
static bool     sfte_less_func       (const struct hash_elem *a_ptr,
                                      const struct hash_elem *b_ptr,
                                      void *aux UNUSED);
static void     sfte_deallocate_func (struct hash_elem *e_ptr, 
                                      void *aux UNUSED);


static unsigned 
sfte_hash_func (const struct hash_elem *e_ptr, 
                void *aux UNUSED)
{
  struct sfte *sfte_ptr = hash_entry (e_ptr, struct sfte, hash_elem);
  return (unsigned) sfte_ptr->inode_ptr + sfte_ptr->offset;
}

static bool 
sfte_less_func (const struct hash_elem *a_ptr,
                const struct hash_elem *b_ptr,
                void *aux UNUSED)
{
  return hash_entry (a_ptr, struct sfte, hash_elem)->fid <
         hash_entry (b_ptr, struct sfte, hash_elem)->fid;
}

static void 
sfte_deallocate_func (struct hash_elem *e_ptr, 
                      void *aux UNUSED)
{
  free (hash_entry (e_ptr, struct sfte, hash_elem));
}

bool
sft_init (void)
{
  if (!hash_init (&sft, &sfte_hash_func, &sfte_less_func, NULL)) return false;
  lock_init (&sft_lock);
  return true;
}
