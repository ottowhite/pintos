#include <hash.h>
#include <debug.h>
#include "vm/sft.h"
#include "threads/synch.h"
#include "threads/malloc.h"
#include "threads/vaddr.h"

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
  return (unsigned) sfte_ptr->inode_ptr + (sfte_ptr->offset / PGSIZE);
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

void 
sft_destroy (void)
{
  lock_acquire (&sft_lock);
  hash_destroy (&sft, &sfte_deallocate_func);
  lock_release (&sft_lock);
}

bool 
sft_insert (int fid, struct inode *inode_ptr, off_t offset)
{
  lock_acquire (&sft_lock);
  struct sfte *sfte_ptr = malloc (sizeof (struct sfte));
  if (sfte_ptr == NULL) 
    {
      lock_release (&sft_lock);
      return false;
    }

  sfte_ptr->fid       = fid;
  sfte_ptr->inode_ptr = inode_ptr;
  sfte_ptr->offset    = offset;

  /* Element was already present in the table */
  if (hash_insert (&sft, &sfte_ptr->hash_elem) != NULL) 
      free (sfte_ptr);

  lock_release (&sft_lock);
  return true;
}

bool 
sft_remove (struct sfte *sfte_ptr)
{
  lock_acquire (&sft_lock);
  if (hash_delete (&sft, &sfte_ptr->hash_elem) == NULL)
    {
      lock_release (&sft_lock);
      return false;
    }
  else
    {
      lock_release (&sft_lock);
      return true;
    }
}

struct sfte *
sft_search (struct inode *inode_ptr, off_t offset)
{
  struct sfte sfte;
  sfte.inode_ptr = inode_ptr;
  sfte.offset    = offset;

  lock_acquire (&sft_lock);
  struct hash_elem *e_ptr = hash_find (&sft, &sfte.hash_elem);
  if (e_ptr == NULL) 
    {
      lock_release (&sft_lock);
      return NULL;
    }
  else
    {
      struct sfte *sfte_ptr = hash_entry (e_ptr, struct sfte, hash_elem);
      lock_release (&sft_lock);
      return sfte_ptr;
    }

}
