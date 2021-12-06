#include <hash.h>
#include <debug.h>
#include <string.h>
#include "threads/malloc.h"
#include "threads/synch.h"
#include "threads/vaddr.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "userprog/syscall.h" 
#include "filesys/inode.h"
#include "filesys/filesys.h"
#include "vm/ft.h"
#include "vm/spt.h"
#include "vm/sft.h"

/* Frame table globals */
static struct hash ft;
static struct lock ft_lock;

/* Frame table hashmap helpers */
static unsigned fte_hash_func       (const struct hash_elem *e_ptr, 
                                     void *aux UNUSED);
static bool     fte_less_func       (const struct hash_elem *a_ptr,
                                     const struct hash_elem *b_ptr,
                                     void *aux UNUSED);
static void     fte_deallocate_func (struct hash_elem *e_ptr, 
                                     void *aux UNUSED);

/* Frame table entry helpers */
static void        fte_insert      (struct fte *fte_ptr);
static void        fte_remove      (struct fte *fte_ptr);
static struct fte *construct_fte   (void *frame_location,
                                    enum retrieval_method retrieval_method,
                                    struct inode *inode_ptr,
                                    off_t offset,
                                    int amount_occupied);
static struct fte *construct_frame (enum frame_type frame_type, 
                                    struct inode *inode_ptr,
                                    off_t offset, 
                                    int amount_occupied);

static struct fte *ft_find_frame   (struct inode *inode_ptr, off_t offset);

/* Helper to obtain retrieval methods by frame type */
static enum retrieval_method get_retrieval_method (enum frame_type frame_type);

/* Helper for reading from inode when creating frame */
static off_t read_from_inode (void *frame_ptr, 
                              struct inode *inode_ptr, 
                              off_t offset,
                              off_t bytes_to_read);


/* Initilizes the frame table as a hash map of struct ftes */
bool 
ft_init (void)
{
  if (!hash_init (&ft, &fte_hash_func, &fte_less_func, NULL)) return false;
  lock_init (&ft_lock);
  return true;
}

/* Deallocate the frame / swap table and all entries. 
   There should be no entries at this point.
   All threads must have terminated before this so the frame table content 
   is consistent and hash clear doesn't yield undefined behaviour. */
void 
ft_destroy (void)
{
  lock_acquire (&ft_lock);
  hash_destroy (&ft, &fte_deallocate_func);
  lock_release (&ft_lock);
}

/* Obtains a user pool page and constructs a pinned frame table entry
   to go with it. Returns NULL if either failed.
   Returned frames must be unpinned after they have been installed to a page
   table */
struct fte *
ft_get_frame (struct spte *spte_ptr)
{
  struct fte *fte_ptr = ft_get_frame_preemptive (spte_ptr->frame_type,
                                                 spte_ptr->inode_ptr,
                                                 spte_ptr->offset,
                                                 spte_ptr->amount_occupied);
  spte_ptr->fte_ptr = fte_ptr;
  return fte_ptr;
}

/* Generalised version of ft_get_frame.
   Useful for getting frames before their spt entry exists.
   These frames should be registered in an spt table after acquired. */
struct fte *
ft_get_frame_preemptive (enum frame_type frame_type, 
                         struct inode *inode_ptr,
                         off_t offset, 
                         int amount_occupied)
{
  // TODO: Failure handling
  struct fte *fte_ptr;
  if (frame_type == EXECUTABLE_CODE ||
      frame_type == MMAP)
    {
      struct sfte *sfte_ptr = sft_search (inode_ptr, offset);
      if (sfte_ptr == NULL)
        {
          /* Add new shareable frame */
          fte_ptr = construct_frame (frame_type, inode_ptr, offset, 
              amount_occupied);
        }
      else
        {
          /* Found shared frame */

          fte_ptr = ft_find_frame (inode_ptr, offset);
          if (fte_ptr->shared)
            {
              /* Frame already is shared with other processes */

              // TODO: Add to the list of shared frames
            }
          else
            {
              /* Frame not yet shared between processes */
              fte_ptr->shared = true;

              // TODO: Initialise a singleton list with the
              //       current fid, store back in same location
              
              // TODO: Add the current owner to the list of owners
            }
        }
    }
  else
    {
      /* Not a shareable frame */
      fte_ptr = construct_frame (frame_type, inode_ptr, offset, 
          amount_occupied);
    }

  return fte_ptr;
}

static struct fte *
ft_find_frame (struct inode *inode_ptr, off_t offset)
{
  struct fte fte;
  fte.inode_ptr = inode_ptr;
  fte.offset = offset;

  struct hash_elem *e_ptr = hash_find (&ft, &fte.hash_elem);

  if (e_ptr == NULL) return NULL;
  return hash_entry (e_ptr, struct fte, hash_elem);
}

struct fte *
construct_frame (enum frame_type frame_type, 
                 struct inode *inode_ptr,
                 off_t offset, 
                 int amount_occupied)
{
  /* Gets a page from the user pool, zeroed if stack page */
  void *frame_ptr = palloc_get_page (frame_type == STACK
                                        ? PAL_USER | PAL_ZERO 
                                        : PAL_USER);
  if (frame_ptr == NULL) goto fail_1;

  enum retrieval_method retrieval_method = get_retrieval_method (frame_type);

  /* Constructs a pinned frame (unpinned when installed in page table) */
  struct fte *fte_ptr = construct_fte (frame_ptr, retrieval_method, inode_ptr, 
      offset, amount_occupied);
  
  if (fte_ptr == NULL) goto fail_2;
  
  /* Read in the necessary data from the filesystem if frame type requires */
  if (frame_type == EXECUTABLE_CODE  || 
      frame_type == EXECUTABLE_DATA  ||
      frame_type == MMAP)
    {
      if (read_from_inode (frame_ptr, inode_ptr, offset, amount_occupied) 
              != amount_occupied)
          goto fail_2;
    }

  /* Zero pad the remaining bits */
  memset (frame_ptr + amount_occupied, 0, PGSIZE - amount_occupied);

  /* Coarse grained insertion to the frame / swap table */
  fte_insert (fte_ptr);
  return fte_ptr;

  fail_2: palloc_free_page (frame_ptr);
  fail_1: return NULL;
}

/* Locks the filesystem whilst reading bytes_to_read bytes from the inode 
   at the given offset into the frame_ptr, returns bytes read */
static off_t
read_from_inode (void *frame_ptr, 
                 struct inode *inode_ptr, 
                 off_t offset,
                 off_t bytes_to_read)
{
  acquire_filesys ();
  off_t bytes_read 
      = inode_read_at (inode_ptr, frame_ptr, bytes_to_read, offset);
  release_filesys ();
  return bytes_read;
}

/* Helper to obtain retrieval methods by frame type */
static enum retrieval_method
get_retrieval_method (enum frame_type frame_type)
{
  switch (frame_type) 
    {
      case     STACK:           return SWAP;
      case     EXECUTABLE_CODE: return DELETE;
      case     EXECUTABLE_DATA: return DELETE;
      case     ALL_ZERO:        return DELETE;
      case     MMAP:            return WRITE_READ;
      default: NOT_REACHED ();
    }
}

/* Frees a frame, deallocates and removes the assocated ft entry. */
void 
ft_remove_frame (struct fte *fte_ptr)
{
  palloc_free_page (fte_ptr->frame_location);
  fte_remove (fte_ptr);
}

/* Constructs a pinned frame table entry stored in the kernel pool
   returns NULL if memory allocation failed */
static struct fte * 
construct_fte (void *frame_location,
               enum retrieval_method retrieval_method,
               struct inode *inode_ptr,
               off_t offset,
               int amount_occupied)
{
  struct fte *fte_ptr = malloc (sizeof (struct fte));
  if (fte_ptr == NULL) return NULL;

  fte_ptr->swapped          = false;
  fte_ptr->shared           = false;
  fte_ptr->pinned           = true;
  fte_ptr->pde_ptrs         = NULL;
  fte_ptr->frame_location   = frame_location;
  fte_ptr->inode_ptr        = inode_ptr;
  fte_ptr->offset           = offset;
  fte_ptr->retrieval_method = retrieval_method;
  fte_ptr->amount_occupied  = amount_occupied;
    
  return fte_ptr;
}

static void
fte_insert (struct fte *fte_ptr)
{
  hash_insert (&ft, &fte_ptr->hash_elem);
}

/* Removes a frame table entry from the frame table and frees the 
   space used to store it. Doesn't free the user page. */
static void
fte_remove (struct fte *fte_ptr)
{
  hash_delete (&ft, &fte_ptr->hash_elem);
  free (fte_ptr);
}

static unsigned
fte_hash_func (const struct hash_elem *e_ptr, void *aux UNUSED)
{
  struct fte *fte_ptr = hash_entry (e_ptr, struct fte, hash_elem);
  return (unsigned) fte_ptr->inode_ptr + (fte_ptr->offset / PGSIZE);
}

static bool 
fte_less_func (const struct hash_elem *a_ptr,
               const struct hash_elem *b_ptr,
               void *aux UNUSED) 
{
  return hash_entry (a_ptr, struct fte, hash_elem)->frame_location <
         hash_entry (b_ptr, struct fte, hash_elem)->frame_location;
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

