#include <hash.h>
#include <debug.h>
#include <string.h>
#include "threads/malloc.h"
#include "threads/synch.h"
#include "threads/vaddr.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "userprog/syscall.h" 
#include "userprog/process.h" 
#include "userprog/pagedir.h" 
#include "filesys/inode.h"
#include "filesys/filesys.h"
#include "vm/ft.h"
#include "vm/spt.h"

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

static bool fte_add_pde_shared (struct fte *fte_ptr, uint32_t *pde_ptr);
static bool fte_add_pde_newly_shared (struct fte *fte_ptr, uint32_t *pde_ptr);

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

bool
ft_install_frame (struct spte *spte_ptr, struct fte *fte_ptr)
{
  void *upage   = spte_ptr->uaddr;
  void *kpage   = fte_ptr->frame_location;
  bool writable = spte_ptr->writable;

  bool success = install_page (upage, kpage, writable);
  if (success)
    {
      /* Get the existing page directory entry */
      uint32_t *pde_ptr 
          = lookup_page (thread_current ()->pagedir, upage, false);

      /* If the frame already shared, try and add the new pde to it's
         pde list, otherwise, either add the new non-list pde,
         or make a newly shared pde list with the new and old pde. */
      if (fte_ptr->shared && 
          !fte_add_pde_shared (fte_ptr, pde_ptr))
          return false;
      else
        {
          if (fte_ptr->pdes.pde_ptr != NULL && 
              !fte_add_pde_newly_shared (fte_ptr, pde_ptr))
              return false;
          else
              fte_ptr->pdes.pde_ptr = pde_ptr;
        }

      /* Unpin the frame and associate SPTE with FTE */
      spte_ptr->fte_ptr = fte_ptr;
      fte_ptr->pin_cnt--;
      return true;
    }
  
  return false;
}

/* Add a PDE to a frame table entry that was previously being shared 
   Return false on any allocation failure */
static bool
fte_add_pde_shared (struct fte *fte_ptr, uint32_t *pde_ptr)
{
  /* Add the pde to the existing list of pdes on the fte */
  struct pde_list_elem *e_ptr = malloc (sizeof (struct pde_list_elem));
  if (e_ptr == NULL) return false;
  e_ptr->pde_ptr = pde_ptr;
  list_push_front (fte_ptr->pdes.pde_list_ptr, &e_ptr->elem);
  return true;
}

/* Add a PDE to a frame table entry that was not previously being shared 
   Return false on any allocation failure*/
static bool
fte_add_pde_newly_shared (struct fte *fte_ptr, uint32_t *pde_ptr)
{
  /* Allocate a new list for storage of multiple pdes that 
     reference the frame */
  struct list *pde_list_ptr = malloc (sizeof (struct list));
  if (pde_list_ptr == NULL) 
      goto fail_1;
  list_init (pde_list_ptr);

  /* Set up two new pde_list_elems, one for the old pde and one
     for the one we are adding to the referencers of the frame*/
  struct pde_list_elem *pde_initial_elem_ptr 
      = malloc (sizeof (struct pde_list_elem));
  if (pde_initial_elem_ptr == NULL)
      goto fail_2;

  struct pde_list_elem *pde_new_elem_ptr 
      = malloc (sizeof (struct pde_list_elem));
  if (pde_new_elem_ptr == NULL)
      goto fail_3;

  pde_initial_elem_ptr->pde_ptr = fte_ptr->pdes.pde_ptr;
  pde_new_elem_ptr->pde_ptr     = pde_ptr;

  list_push_front (fte_ptr->pdes.pde_list_ptr, 
                   &pde_initial_elem_ptr->elem);
  list_push_front (fte_ptr->pdes.pde_list_ptr, 
                   &pde_new_elem_ptr->elem);

  /* If all was successful, associate the frame with the newly created
     list of pdes */
  fte_ptr->pdes.pde_list_ptr = pde_list_ptr;
  return true;

  fail_3: free (pde_initial_elem_ptr);
  fail_2: free (pde_list_ptr);
  fail_1: return false;
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
  struct fte *fte_ptr;
  if ((frame_type == EXECUTABLE_CODE ||
       frame_type == MMAP)           &&
      (fte_ptr = ft_find_frame (inode_ptr, offset)) != NULL)
    {
      /* Found shared frame, pin it until installation. */
      fte_ptr->pin_cnt++;
    }
  else
    {
      /* Construct a new pinned frame */
      fte_ptr = construct_frame (frame_type, inode_ptr, offset, 
          amount_occupied);
      if (fte_ptr == NULL) return NULL;
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
  if (frame_ptr == NULL) 
      goto fail_1;

  enum retrieval_method retrieval_method = get_retrieval_method (frame_type);

  /* Constructs a pinned frame (unpinned when installed in page table) */
  struct fte *fte_ptr = construct_fte (frame_ptr, retrieval_method, inode_ptr, 
      offset, amount_occupied);
  
  if (fte_ptr == NULL) 
      goto fail_2;
  
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
  fte_ptr->pin_cnt          = 1;
  fte_ptr->pdes.pde_ptr     = NULL;
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

