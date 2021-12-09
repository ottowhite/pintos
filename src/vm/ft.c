#include <hash.h>
#include <debug.h>
#include <stdio.h>
#include <string.h>
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
#include "vm/evict.h"

/* Frame table globals */
static struct hash  ft;
static struct lock  ft_lock;

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
static void convert_fte_to_non_shared  (struct fte *fte_ptr);

/* Helper to obtain eviction methods by frame type */
static enum eviction_method get_eviction_method (enum frame_type frame_type);
static void *palloc_get_page_with_eviction (enum palloc_flags flags);

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

static void *user_pool_top;
static void *user_pool_bottom;

bool debug;
struct fte** frame_index_arr;
size_t       frame_index_size;

/* Initilizes the frame table as a hash map of struct ftes */
bool 
ft_init (void)
{
  debug = false;

  if (!hash_init (&ft, &fte_hash_func, &fte_less_func, NULL)) 
      goto fail_1;
  lock_init (&ft_lock);
  /* Frame index is used for eviction to store which ftes correspond to
     which frames */
  user_pool_top    = get_user_pool_top ();
  user_pool_bottom = get_user_pool_bottom ();
  frame_index_size = ((user_pool_top - user_pool_bottom) / PGSIZE) - 1;

  frame_index_arr  = malloc (frame_index_size * sizeof (struct fte *));
  if (frame_index_arr == NULL)
      goto fail_2;

  /* Zero initialize the frame index as the user pool is initially empty */
  memset (frame_index_arr, 0, frame_index_size * sizeof (struct fte *));

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
  ASSERT (!fte_ptr->swapped);
  void *upage   = spte_ptr->uaddr;
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
  ASSERT (fte_ptr->pin_cnt > 0);
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
  switch (spte_ptr->frame_type)
  {
    case EXECUTABLE_DATA: debugf("Getting EXECUTABLE_DATA page. \n"); break;
    case EXECUTABLE_CODE: debugf("Getting EXECUTABLE_CODE page. \n"); break;
    case STACK:           debugf("Getting STACK page. \n"); break;
    case ALL_ZERO:        debugf("Getting ALL_ZERO page. \n"); break;
    case MMAP:            debugf("Getting MMAP page. \n"); break;
  }

  enum frame_type frame_type = spte_ptr->frame_type;
  struct inode *inode_ptr    = spte_ptr->inode_ptr;
  off_t offset               = spte_ptr->offset;
  int amount_occupied        = spte_ptr->amount_occupied;

  /* Set the fte_ptr to what the SPT entry refers to, null if no frame yet. */
  struct fte *fte_ptr = spte_ptr->fte_ptr;

  /* If this failed look for a shared frame. */
  if ((fte_ptr == NULL) &&
      (frame_type == EXECUTABLE_CODE || frame_type == MMAP))
      fte_ptr = ft_find_frame (inode_ptr, offset);

  /* If we found a frame, bring it in from swap if necessary. */
  if (fte_ptr != NULL)
    {
      debugf("------------------Swapping back in. \n");

      if (fte_ptr->swapped) 
        {
          void *frame_ptr = palloc_get_page_with_eviction (PAL_USER);
          swap_in (fte_ptr, frame_ptr);
        }
    }
  else
    {
      /* Otherwise construct a new frame */
      fte_ptr = construct_frame (frame_type, inode_ptr, offset, 
          amount_occupied);
      if (fte_ptr == NULL) return NULL;
    }

  ASSERT (fte_ptr->pin_cnt >= 0);
  fte_ptr->pin_cnt++;

  /* Associate the new frame location in the user pool with the fte */
  int frame_index = index_from_frame_ptr (fte_ptr->loc.frame_ptr);
  debugf("Setting frame index %d to %p. \n", 
      frame_index, fte_ptr->loc.frame_ptr);
  frame_index_arr[frame_index] = fte_ptr;

  /* Associate the supplemental page table entry with the frame */
  spte_ptr->fte_ptr = fte_ptr;
  return fte_ptr;
}

/* Obtains the index in the frame_index_arr from a frame_ptr */
static int
index_from_frame_ptr (void *frame_ptr)
{
  return ((frame_ptr - user_pool_bottom) / PGSIZE) - 1;
}

/* Obtains the frame_ptr from an index used by the frame_index_arr */
static void *
frame_ptr_from_index (int frame_index)
{
  return ((frame_index - 1) * PGSIZE) + user_pool_bottom;
}

static void *
palloc_get_page_with_eviction (enum palloc_flags flags)
{
  void *frame_ptr = palloc_get_page (flags);
  if (frame_ptr == NULL) 
    {
      evict ();
      frame_ptr = palloc_get_page (flags);
    }
  return frame_ptr;
}

struct fte *
construct_frame (enum frame_type frame_type, 
                 struct inode *inode_ptr,
                 off_t offset, 
                 int amount_occupied)
{
  enum palloc_flags flags = frame_type == STACK 
                                ? PAL_USER | PAL_ZERO 
                                : PAL_USER;

  /* Gets a page from the user pool, zeroed if stack page */
  void *frame_ptr = palloc_get_page_with_eviction (flags);

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

/* Remove a single owner from an FTE, converting the FTE to non-shared if
   necessary. Remove the referencing page table entry. Then return the 
   owner. */
struct owner 
ft_remove_owner (struct fte *fte_ptr)
{
  struct owner owner;
  if (fte_ptr->shared) 
    {
      /* loop through the list of owners in the fte until the 
         list entry correspoding to the current thread is found
         and then call list_remove on this element and save the 
         owner to owner */
      struct list *owner_list_ptr = fte_ptr->owners.owner_list_ptr;
      struct thread *t_ptr = thread_current ();
      for (struct list_elem *e = list_begin (owner_list_ptr); 
            e != list_end (owner_list_ptr);
            e  = list_next (e))
        {
          struct owner_list_elem *owner_e_ptr 
              = list_entry (e, struct owner_list_elem, elem);
          owner = owner_e_ptr->owner;

          if (owner.owner_ptr == t_ptr)
            {
              list_remove (e);
              free (owner_e_ptr);
              break;
            }
        }

      frame_remove_pte (owner);

      /* If the owner list became a singleton, convert the FTE to non shared */
      if (list_front (owner_list_ptr) == list_back (owner_list_ptr))
          convert_fte_to_non_shared (fte_ptr);
    } 
  else 
    {
      /* Non-shared case: remove the page table entry and NULL the owner */
      owner = fte_ptr->owners.owner_single;
      frame_remove_pte (owner);
      fte_ptr->owners.owner_single = (struct owner) { NULL, NULL };
    }
  return owner;
}

/* Converts a shared frame table entry to a non-shared entry
   in the case that it's owner list becomes a singleton after owner removal */
static void
convert_fte_to_non_shared (struct fte *fte_ptr)
{
  struct list *owner_list_ptr = fte_ptr->owners.owner_list_ptr;

  /* Defensive assertions */
  ASSERT (fte_ptr->shared);
  ASSERT (list_front (owner_list_ptr) == list_back (owner_list_ptr));

  struct owner_list_elem *owner_e_ptr 
      = list_entry (list_front (owner_list_ptr), 
                    struct owner_list_elem, 
                    elem);
  
  struct owner new_owner = owner_e_ptr->owner;
  
  free (owner_e_ptr);
  free (owner_list_ptr);
  
  fte_ptr->shared              = false;
  fte_ptr->owners.owner_single = new_owner;
}

/* Remove a frame if the last owner was removed. In the swapped case
   remove the entry from the swap bitmap with swap_remove, in the 
   non-swapped case write back if the frame is dirty. */
void 
ft_remove_frame_if_necessary (struct fte *fte_ptr, struct owner original_owner)
{
  /* if the last owner removed was not the last owner
     we dont need to do anything */
  if (fte_ptr->shared || fte_ptr->owners.owner_single.owner_ptr != NULL) 
      return;

  /* Otherwise we just removed the last owner. */
  if (fte_ptr->swapped) 
			swap_remove (fte_ptr);
  else if (fte_ptr->eviction_method == WRITE_IF_DIRTY && 
           pagedir_is_dirty (original_owner.owner_ptr->pagedir,
                             original_owner.upage_ptr)) 
      frame_write (fte_ptr);

  /* NULL the slot in the frame_index_arr that the frame occupied */
  frame_index_arr[index_from_frame_ptr(fte_ptr->loc.frame_ptr)] = NULL;
  frame_delete (fte_ptr);
}

/* Constructs a frame table entry stored in the kernel pool
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
  fte_ptr->pin_cnt             = 0;
  fte_ptr->owners.owner_single = (struct owner) { NULL, NULL };
  fte_ptr->loc                 = loc;
  fte_ptr->inode_ptr           = inode_ptr;
  fte_ptr->offset              = offset;
  fte_ptr->eviction_method     = eviction_method;
  fte_ptr->amount_occupied     = amount_occupied;
    
  return fte_ptr;
}

/* Attempt to write a frames contents to the filesystem, exit the process
   on failure to avoid undefined behaviour in the program whose memory 
   has been lost. */
void
frame_write (struct fte *fte_ptr)
{
  ASSERT (!fte_ptr->swapped);
  if (write_to_inode (fte_ptr->loc.frame_ptr, fte_ptr->inode_ptr,
      fte_ptr->offset, fte_ptr->amount_occupied) != fte_ptr->amount_occupied)
      syscall_exit (-1);
}

/* Deletes a frame and frees the associated frame table entry */
void
frame_delete (struct fte *fte_ptr)
{
  /* Free the page in memory and the frame table entry */
  ASSERT (!fte_ptr->swapped);
  palloc_free_page (fte_ptr->loc.frame_ptr);
  hash_delete (&ft, &fte_ptr->hash_elem);
  free (fte_ptr);
}

/* Swaps a into the swap partition and frees the page in memory */
void
frame_swap (struct fte *fte_ptr)
{
  void *frame_ptr = fte_ptr->loc.frame_ptr;
  swap_out (fte_ptr);
  palloc_free_page (frame_ptr);
}

bool
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

void
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

void
frame_remove_owner (struct owner owner, bool remove_spte_reference)
{
  if (remove_spte_reference)
      frame_remove_spte_reference (owner);

  frame_remove_pte (owner);
}

void
frame_remove_spte_reference (struct owner owner)
{
  /* Remove reference to the frame from the owners spte */
  struct spte *spte_ptr = spt_find_entry (owner.owner_ptr->spt_ptr, 
                                          owner.upage_ptr);
  ASSERT (spte_ptr != NULL);
  spte_ptr->fte_ptr = NULL;
}

void
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
  return hash_entry (a_ptr, struct fte, hash_elem)->loc.swap_index <
         hash_entry (b_ptr, struct fte, hash_elem)->loc.swap_index;
}

static void
fte_deallocate_func (struct hash_elem *e_ptr, void *aux UNUSED)
{
  /* Only need to deallocate the frame structs themselves as our
     SPT deallocation for each thread will deal with cleaning up all resources 
     indirectly. This should in theory run on no FTEs */
  free (hash_entry (e_ptr, struct fte, hash_elem));
}

void
acquire_ft (void)
{
  lock_acquire (&ft_lock);
}

void
release_ft (void)
{
  lock_release (&ft_lock);
}
