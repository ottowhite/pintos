#include <hash.h>
#include <debug.h>
#include <string.h>
#include <random.h>
#include <list.h>
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
#include "vm/swap.h"

/* Frame table globals */
static struct hash  ft;
static struct lock  ft_lock;
static struct fte** frame_index_arr;
static size_t       frame_index_size;

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
static struct fte *construct_fte   (union Frame_location loc,
                                    enum eviction_method eviction_method,
                                    struct inode *inode_ptr,
                                    off_t offset,
                                    int amount_occupied);
static struct fte *construct_frame (enum frame_type frame_type, 
                                    struct inode *inode_ptr,
                                    off_t offset, 
                                    int amount_occupied);

static struct fte *ft_find_frame   (struct inode *inode_ptr, off_t offset);

static bool fte_add_owner_shared       (struct fte *fte_ptr, 
                                        struct thread *t_ptr, 
                                        void *upage);
static bool fte_add_owner_newly_shared (struct fte *fte_ptr, 
                                        struct thread *t_ptr, 
                                        void *upage);

/* Helper to obtain eviction methods by frame type */
static enum eviction_method get_eviction_method (enum frame_type frame_type);

/* Eviction and eviction helpers */
static int  evict        (void);
static bool frame_dirty  (struct fte *fte_ptr);
static void frame_delete (struct fte *fte_ptr);
static void frame_swap   (struct fte *fte_ptr);
static void frame_write  (struct fte *fte_ptr);

static void frame_remove_spte_reference (struct owner owner);
static void frame_remove_pte            (struct owner owner);
static void frame_remove_owner          (struct owner owner, 
                                         bool remove_spte_reference);
static void frame_remove_owners         (struct fte *fte_ptr, 
                                         bool remove_spte_reference);


/* Helper for reading from inode when creating frame */
static off_t read_from_inode (void *frame_ptr, 
                              struct inode *inode_ptr, 
                              off_t offset,
                              off_t bytes_to_read);

static off_t write_to_inode  (void *frame_ptr, 
                              struct inode *inode_ptr, 
                              off_t offset,
                              off_t bytes_to_write);

/* Helpers for converting between frame indices and frame_ptrs */
static int   index_from_frame_ptr (void *frame_ptr);
static void *frame_ptr_from_index (int frame_index);


/* Initilizes the frame table as a hash map of struct ftes */
bool 
ft_init (void)
{
  if (!hash_init (&ft, &fte_hash_func, &fte_less_func, NULL)) 
      goto fail_1;
  lock_init (&ft_lock);
  /* Frame index is used for eviction to store which ftes correspond to
     which frames */
  frame_index_size = (get_user_pool_start () - PHYS_BASE) / PGSIZE;
  frame_index_arr  = malloc (frame_index_size * sizeof (struct fte *));
  if (frame_index_arr == NULL)
      goto fail_2;

  /* Zero initialize the frame index as the user pool is initially empty */
  memset (frame_index_arr, 0, frame_index_size);

  return true;

  fail_2: ft_destroy ();
  fail_1: return false;
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
  free (frame_index_arr);
}

bool
ft_install_frame (struct spte *spte_ptr, struct fte *fte_ptr)
{
  void *upage   = spte_ptr->uaddr;
  /* TODO: handle case in which the frame is swapped*/
  void *kpage   = fte_ptr->loc.frame_ptr;
  bool writable = spte_ptr->writable;

  if (!install_page (upage, kpage, writable))
      goto fail_install_page;

  /* Get the existing page directory entry */
  struct thread *t_ptr = thread_current ();

  /* If the frame already shared, try and add the new pde to it's
     pde list, otherwise, either add the new non-list pde,
     or make a newly shared pde list with the new and old pde. */

  if (fte_ptr->shared)
    {
      /* If the frame is already shared, attempt to add another owner */
      if (!fte_add_owner_shared (fte_ptr, t_ptr, upage))
          goto fail_add_pde;
    }
  else
    {
      /* Not shared case */
      if (fte_ptr->owners.owner_single.owner_ptr != NULL)
        {
          /* If the frame has a single current owner, attempt to add the
             new owner and make the frame shared */
          if (fte_add_owner_newly_shared (fte_ptr, t_ptr, upage))
              fte_ptr->shared = true;
          else
              goto fail_add_pde;
        }
      else
          /* If the frame has no current owner, add the owner */
          fte_ptr->owners.owner_single = (struct owner) { t_ptr, upage };
    }

  /* Unpin the frame and associate SPTE with FTE */
  spte_ptr->fte_ptr = fte_ptr;
  fte_ptr->pin_cnt--;
  return true;

  fail_add_pde:      pagedir_clear_page (thread_current ()->pagedir, upage);
  fail_install_page: return false;
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
      if (fte_ptr->swapped)
        {
          // TODO: If there is no memory left, evict and swap in
          //       Otherwise get the free slot and swap in
        }
      fte_ptr->pin_cnt++;
    }
  else
    {
      /* Construct a new pinned frame */
      fte_ptr = construct_frame (frame_type, inode_ptr, offset, 
          amount_occupied);
      if (fte_ptr == NULL) return NULL;
    }

  /* Associate the new frame location in the user pool with the fte */
  int frame_index = index_from_frame_ptr (fte_ptr->loc.frame_ptr);
  frame_index_arr[frame_index] = fte_ptr;

  return fte_ptr;
}

/* Obtains the index in the frame_index_arr from a frame_ptr */
static int
index_from_frame_ptr (void *frame_ptr)
{
  return (frame_ptr - PHYS_BASE) / PGSIZE;
}

/* Obtains the frame_ptr from an index used by the frame_index_arr */
static void *
frame_ptr_from_index (int frame_index)
{
  return (frame_index * PGSIZE) + PHYS_BASE;
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
      evict ();

  enum eviction_method eviction_method = get_eviction_method (frame_type);

  /* Constructs a pinned frame (unpinned when installed in page table) */
  struct fte *fte_ptr = construct_fte (
      (union Frame_location) { .frame_ptr = frame_ptr }, eviction_method, 
      inode_ptr, offset, amount_occupied);
  
  if (fte_ptr == NULL) 
      goto fail;
  
  /* Read in the necessary data from the filesystem if frame type requires */
  if (frame_type == EXECUTABLE_CODE  || 
      frame_type == EXECUTABLE_DATA  ||
      frame_type == MMAP)
    {
      if (read_from_inode (frame_ptr, inode_ptr, offset, amount_occupied) 
              != amount_occupied)
          goto fail;
    }

  /* Zero pad the remaining bits */
  memset (frame_ptr + amount_occupied, 0, PGSIZE - amount_occupied);

  /* Coarse grained insertion to the frame / swap table */
  fte_insert (fte_ptr);
  return fte_ptr;

  fail: palloc_free_page (frame_ptr);
        return NULL;
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

/* Add a PDE to a frame table entry that was previously being shared 
   Return false on any allocation failure */
static bool
fte_add_owner_shared (struct fte *fte_ptr, struct thread *t_ptr, void *upage)
{
  /* Add the pde to the existing list of pdes on the fte */
  struct owner_list_elem *e_ptr = malloc (sizeof (struct owner_list_elem));
  if (e_ptr == NULL) return false;
  e_ptr->owner = (struct owner) { t_ptr, upage };
  list_push_front (fte_ptr->owners.owner_list_ptr, &e_ptr->elem);
  return true;
}

/* Add a PDE to a frame table entry that was not previously being shared 
   Return false on any allocation failure*/
static bool
fte_add_owner_newly_shared (struct fte *fte_ptr, 
                            struct thread *t_ptr, 
                            void *upage)
{
  /* Allocate a new list for storage of multiple owners that 
     reference the frame */
  struct list *owner_list_ptr = malloc (sizeof (struct list));
  if (owner_list_ptr == NULL) 
      goto fail_1;
  list_init (owner_list_ptr);

  /* Set up two new owner_list_elems, one for the old owner and one
     for the one we are adding to the referencers of the frame*/
  struct owner_list_elem *owner_initial_elem_ptr 
      = malloc (sizeof (struct owner_list_elem));
  if (owner_initial_elem_ptr == NULL)
      goto fail_2;

  struct owner_list_elem *owner_new_elem_ptr 
      = malloc (sizeof (struct owner_list_elem));
  if (owner_new_elem_ptr == NULL)
      goto fail_3;

  owner_initial_elem_ptr->owner = fte_ptr->owners.owner_single;
  owner_new_elem_ptr->owner     = (struct owner) { t_ptr, upage };

  list_push_front (fte_ptr->owners.owner_list_ptr, 
                   &owner_initial_elem_ptr->elem);
  list_push_front (fte_ptr->owners.owner_list_ptr, 
                   &owner_new_elem_ptr->elem);

  /* If all was successful, associate the frame with the newly created
     list of owners */
  fte_ptr->owners.owner_list_ptr = owner_list_ptr;
  return true;

  fail_3: free (owner_initial_elem_ptr);
  fail_2: free (owner_list_ptr);
  fail_1: return false;
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

/* Locks the filesystem whilst writing bytes_to_write bytes to the inode 
   at the given offset from the frame_ptr, returns bytes written */
static off_t
write_to_inode (void *frame_ptr, 
                struct inode *inode_ptr, 
                off_t offset,
                off_t bytes_to_write)
{
  acquire_filesys ();
  off_t bytes_written 
      = inode_write_at (inode_ptr, frame_ptr, bytes_to_write, offset);
  release_filesys ();
  return bytes_written;
}

/* Helper to obtain eviction methods by frame type */
static enum eviction_method
get_eviction_method (enum frame_type frame_type)
{
  switch (frame_type) 
    {
      case     STACK:           return SWAP;
      case     EXECUTABLE_DATA: return SWAP_IF_DIRTY;
      case     MMAP:            return WRITE_IF_DIRTY;
      case     EXECUTABLE_CODE: return DELETE;
      case     ALL_ZERO:        return DELETE;
      default: NOT_REACHED ();
    }
}

/* Frees a frame, deallocates and removes the assocated ft entry. */
void 
ft_remove_frame (struct fte *fte_ptr)
{
  palloc_free_page (fte_ptr->loc.frame_ptr);
  fte_remove (fte_ptr);
}

/* Constructs a pinned frame table entry stored in the kernel pool
   returns NULL if memory allocation failed */
static struct fte * 
construct_fte (union Frame_location loc,
               enum eviction_method eviction_method,
               struct inode *inode_ptr,
               off_t offset,
               int amount_occupied)
{
  struct fte *fte_ptr = malloc (sizeof (struct fte));
  if (fte_ptr == NULL) return NULL;

  fte_ptr->swapped             = false;
  fte_ptr->shared              = false;
  fte_ptr->pin_cnt             = 1;
  fte_ptr->owners.owner_single = (struct owner) { NULL, NULL };
  fte_ptr->loc                 = loc;
  fte_ptr->inode_ptr           = inode_ptr;
  fte_ptr->offset              = offset;
  fte_ptr->eviction_method     = eviction_method;
  fte_ptr->amount_occupied     = amount_occupied;
    
  return fte_ptr;
}

static int
evict (void)
{
  /* Obtain a random int from 0 to frame_index_size (exclusive) */
  int i = (int) (random_ulong () % frame_index_size);
  /* Keep trying until we find a frame that is not pinned to evict */
  for (; frame_index_arr[i]->pin_cnt != 0; 
         i = (int) (random_ulong () % frame_index_size));

  struct fte *fte_ptr = frame_index_arr[i];

  switch (fte_ptr->eviction_method)
    {
      case SWAP:
        {
          frame_swap (fte_ptr);
          break;
        }
      case DELETE: 
        {
          frame_remove_owners (fte_ptr, true);
          frame_delete (fte_ptr); 
          break;
        }
      case SWAP_IF_DIRTY:
        {
          bool dirty = frame_dirty (fte_ptr);
          frame_remove_owners (fte_ptr, !dirty);
          if (dirty) frame_swap   (fte_ptr);
          else       frame_delete (fte_ptr);

          break;
        }
      case WRITE_IF_DIRTY:
        {
          if (frame_dirty (fte_ptr)) frame_write (fte_ptr);
          frame_remove_owners (fte_ptr, true);
          frame_delete        (fte_ptr);
          break;
        }
      default: NOT_REACHED ();
    }

  frame_index_arr[i] = NULL;
  return i;
}

/* Attempt to write a frames contents to the filesystem, exit the process
   on failure to avoid undefined behaviour in the program whose memory 
   has been lost. */
static void
frame_write (struct fte *fte_ptr)
{
  ASSERT (!fte_ptr->swapped);
  if (write_to_inode (fte_ptr->loc.frame_ptr, fte_ptr->inode_ptr,
      fte_ptr->offset, PGSIZE) != PGSIZE)
      syscall_exit (-1);
}

/* Deletes a frame and frees the associated frame table entry */
static void
frame_delete (struct fte *fte_ptr)
{
  /* Free the page in memory and the frame table entry */
  ASSERT (!fte_ptr->swapped);
  palloc_free_page (fte_ptr->loc.frame_ptr);
  free (fte_ptr);
}

/* Swaps a into the swap partition and frees the page in memory */
static void
frame_swap (struct fte *fte_ptr)
{
  void *frame_ptr = fte_ptr->loc.frame_ptr;
  swap_out (fte_ptr);
  palloc_free_page (frame_ptr);
}

static bool
frame_dirty (struct fte *fte_ptr)
{
  if (fte_ptr->shared)
    {
      struct list *owner_list_ptr = fte_ptr->owners.owner_list_ptr;

      for (struct list_elem *e = list_begin (owner_list_ptr); 
           e != list_end (owner_list_ptr);
           e  = list_next (e))
        {
          struct owner owner 
              = list_entry (e, struct owner_list_elem, elem)->owner;
          if (pagedir_is_dirty (owner.owner_ptr->pagedir, 
                                owner.upage_ptr))
              return true;
        }
    }
  else
    {
      struct owner owner = fte_ptr->owners.owner_single;
      return pagedir_is_dirty (owner.owner_ptr->pagedir,
                               owner.upage_ptr);
    }

  return false;
}

static void
frame_remove_owners (struct fte *fte_ptr, bool remove_spte_reference)
{
  if (fte_ptr->shared)
    {
      struct list *owner_list_ptr = fte_ptr->owners.owner_list_ptr;

      for (struct list_elem *e = list_begin (owner_list_ptr); 
           e != list_end (owner_list_ptr);
           e  = list_next (e))
        {
          struct owner_list_elem *owner_e_ptr
              = list_entry (e, struct owner_list_elem, elem);
          frame_remove_owner (
                owner_e_ptr->owner, 
                remove_spte_reference);
          list_remove (&owner_e_ptr->elem);
        }
    }
  else
    {
      frame_remove_owner (fte_ptr->owners.owner_single, 
                          remove_spte_reference);
      fte_ptr->owners.owner_single = (struct owner) { NULL, NULL };
    }
}

static void
frame_remove_owner (struct owner owner, bool remove_spte_reference)
{
  if (remove_spte_reference)
      frame_remove_spte_reference (owner);

  frame_remove_pte (owner);
}

static void
frame_remove_spte_reference (struct owner owner)
{
  /* Remove reference to the frame from the owners spte */
  struct spte *spte_ptr = spt_find_entry (owner.owner_ptr->spt_ptr, 
                                          owner.upage_ptr);
  ASSERT (spte_ptr != NULL);
  spte_ptr->fte_ptr = NULL;
}

static void
frame_remove_pte (struct owner owner)
{
  /* Clear the page in the owners page directory */
  pagedir_clear_page (owner.owner_ptr->pagedir, owner.upage_ptr);
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
  // TODO: Is this comparison operation safe with the union type?
  return hash_entry (a_ptr, struct fte, hash_elem)->loc.swap_index <
         hash_entry (b_ptr, struct fte, hash_elem)->loc.swap_index;
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

