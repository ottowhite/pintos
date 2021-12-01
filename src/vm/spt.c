#include <debug.h>
#include <hash.h>
#include "threads/malloc.h"
#include "vm/spt.h"

static unsigned     spte_hash_func       (const struct hash_elem *e_ptr, 
                                          void *aux UNUSED);
static bool         spte_less_func       (const struct hash_elem *a_ptr,
                                          const struct hash_elem *b_ptr,
                                          void *aux UNUSED);
static void         spte_deallocate_func (struct hash_elem *e_ptr, 
                                          void *aux UNUSED);
static struct spte *spte_construct       (void *uaddr,
                                          enum frame_type frame_type,
                                          struct inode *inode_ptr,
                                          off_t offset,
                                          int amount_occupied);

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

/* Attempts to initialise the supplementary page table 
   Returns false if failed and true if succeeeded. */
bool 
spt_init (struct hash **spt_ptr_ptr)
{
  *spt_ptr_ptr = malloc (sizeof (struct hash));
  if (*spt_ptr_ptr == NULL) return false;

  if (!hash_init (*spt_ptr_ptr, &spte_hash_func, &spte_less_func, NULL)) {
    free (*spt_ptr_ptr);
    return false;
  }
  return true;
}

void 
spt_destroy (struct hash *spt_ptr)
{
  /* TODO: Close associated files if necesary (and if not shared) */
  hash_destroy (spt_ptr, &spte_deallocate_func);
  free (spt_ptr);
}

/* Constructs supplementary page table entry and inserts it to given 
   supplementary page table hashmap, returns NULL if spte memory allocation
   fail */
struct spte *
spt_add_entry (struct hash *spt_ptr,
               void *uaddr,
               enum frame_type frame_type,
               struct inode *inode_ptr,
               off_t offset,
               int amount_occupied)
{
  struct spte *spte_ptr = spte_construct (uaddr, frame_type, 
      inode_ptr, offset, amount_occupied);

  if (spte_ptr == NULL) return NULL;

  hash_insert (spt_ptr, &spte_ptr->hash_elem);

  return spte_ptr;
}

void
spt_remove_entry (void)
{
  // TODO when I know exactly what will be the most convienient interface
  return;
}

/* Constructs a supplmental page table entry, returns NULL
   if memory allocation fails */
static struct spte *
spte_construct (void *uaddr,
                enum frame_type frame_type,
                struct inode *inode_ptr,
                off_t offset,
                int amount_occupied)
{
  struct spte *spte_ptr = malloc (sizeof (struct spte));
  if (spte_ptr == NULL) return NULL;

  spte_ptr->uaddr           = uaddr;
  spte_ptr->frame_type      = frame_type;
  spte_ptr->inode_ptr       = inode_ptr;
  spte_ptr->offset          = offset;
  spte_ptr->amount_occupied = amount_occupied;

  return spte_ptr;
}
