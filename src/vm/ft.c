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

void ft_init (void);
void ft_destroy (void);

static unsigned    fte_hash_func       (const struct hash_elem *e_ptr, 
                                        void *aux UNUSED);
static bool        fte_less_func       (const struct hash_elem *a_ptr,
                                        const struct hash_elem *b_ptr,
                                        void *aux UNUSED);
static void        fte_deallocate_func (struct hash_elem *e_ptr, 
                                        void *aux UNUSED);
static void        fte_insert          (struct fte *fte_ptr);
static struct fte *fte_construct       (pid_t owner,
                                        void *frame_location,
                                        enum retrieval_method retrieval_method,
                                        int amount_occupied);
static void        fte_remove          (struct fte *fte_ptr);

static enum retrieval_method 
    get_retrieval_method (enum frame_type frame_type);

static struct lock ft_lock;
static struct lock fid_lock;
static int         fid_cnt;
static struct hash ft;

/* Initilizes the frame table as a hash map of struct ftes */
void 
ft_init (void)
{
  hash_init (&ft, &fte_hash_func, &fte_less_func, NULL);
  lock_init (&ft_lock);
  lock_init (&fid_lock);
  fid_cnt = 0;
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
ft_get_frame (pid_t owner, 
              enum frame_type frame_type, 
              struct inode *inode_ptr,
              off_t offset, 
              int amount_occupied)
{
  /* I expect this interface will change over time */
  /* Gets a page from the user pool, zeroed if stack page */

  void *frame_ptr = palloc_get_page (frame_type == STACK
                                        ? PAL_USER | PAL_ZERO 
                                        : PAL_USER);
  if (frame_ptr == NULL) return NULL;

  enum retrieval_method retrieval_method = get_retrieval_method (frame_type);

  /* Constructs a pinned frame (unpinned when installed in page table) */
  struct fte *fte_ptr 
      = fte_construct (owner, frame_ptr, retrieval_method, amount_occupied);

  if (fte_ptr == NULL) 
    {
      palloc_free_page (frame_ptr);
      return NULL;
    }
  
  /* Read in the necessary data from the filesystem, zero out unallocated 
     space */
  if (frame_type == EXECUTABLE_CODE || 
      frame_type == EXECUTABLE_DATA ||
      frame_type == MMAP) 
    {
      acquire_filesys ();
      // TODO: free the kpage if file read doesn't return correct no bytes
      inode_read_at (inode_ptr, frame_ptr, amount_occupied, offset);
      release_filesys ();
      memset (frame_ptr + amount_occupied, 0, PGSIZE - amount_occupied);
    }

  /* Coarse grained insertion to the frame / swap table */
  fte_insert (fte_ptr);

  return fte_ptr;
}

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

void 
ft_remove_frame (struct fte *fte_ptr)
{
  palloc_free_page (fte_ptr->frame_location);
  fte_remove (fte_ptr);
}
/* Constructs a pinned frame table entry stored in the kernel pool
   returns NULL if memory allocation failed */
static struct fte * 
fte_construct (pid_t owner,
               void *frame_location,
               enum retrieval_method retrieval_method,
               int amount_occupied)
{
  struct fte *fte_ptr = malloc (sizeof (struct fte));
  if (fte_ptr == NULL) return NULL;

  lock_acquire (&fid_lock);
  fte_ptr->fid              = fid_cnt++;
  lock_release (&fid_lock);
  fte_ptr->swapped          = false;
  fte_ptr->shared           = false;
  fte_ptr->pinned           = true;
  fte_ptr->owner            = owner;
  fte_ptr->frame_location   = frame_location;
  fte_ptr->retrieval_method = retrieval_method;
  fte_ptr->amount_occupied  = amount_occupied;
    
  return fte_ptr;
}

static void
fte_insert (struct fte *fte_ptr)
{
  lock_acquire (&ft_lock);
  hash_insert (&ft, &fte_ptr->hash_elem);
  lock_release (&ft_lock);
}

/* Removes a frame table entry from the frame table and frees the 
   space used to store it. Doesn't free the user page. */
static void
fte_remove (struct fte *fte_ptr)
{
  lock_acquire (&ft_lock);

  hash_delete (&ft, &fte_ptr->hash_elem);
  free (fte_ptr);

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

