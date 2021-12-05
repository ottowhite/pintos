#include <hash.h>
#include <debug.h>
#include "vm/sft.h"
#include "threads/synch.h"

static struct hash sft;
static struct lock sft_lock;

/* Shared frame table hashmap helpers */
static unsigned sfte_hash_func       (const struct hash_elem *e_ptr, 
                                      void *aux);
static bool     sfte_less_func       (const struct hash_elem *a_ptr,
                                      const struct hash_elem *b_ptr,
                                      void *aux);
static void     sfte_deallocate_func (struct hash_elem *e_ptr, 
                                      void *aux);


