#include <debug.h>
#include <stdio.h>
#include <random.h>
#include "threads/thread.h"
#include "userprog/pagedir.h"
#include "vm/evict.h"
#include "vm/ft.h"

static int evict_find_victim_random (void) UNUSED;

static int evict_find_victim_linear (void) UNUSED;
static int linear_victim_candidate_index = 0;

static int  evict_find_victim_sca      (void);
static bool frame_unset_accessed_ptes  (struct fte *fte_ptr);
static bool pagedir_unset_accessed_pte (struct owner owner);
static int  sca_victim_candidate_index = 0;

/* Returns index of frame to be evicted. */
int
evict (void)
{
	/* Finds victim index using the evict_find_victim_sca. Different functions
	 * can be used here to apply different eviction algorithms. */
  int victim_index    = evict_find_victim_sca ();
  struct fte *fte_ptr = frame_index_arr[victim_index];

	/* Case statement to determine correct behaviour of eviction depending on 
	 * specified frame eviction method. */
  switch (fte_ptr->eviction_method)
    {
      case SWAP:
        {
          frame_swap (fte_ptr);
          /* Remove PTE references */
          frame_remove_owners (fte_ptr, false, true);
          break;
        }
      case DELETE: 
        {
          /* Remove PTE and SPTE references */
          frame_remove_owners (fte_ptr, true, true);
          frame_delete (fte_ptr); 
          break;
        }
      case SWAP_IF_DIRTY:
        {
          if (!fte_ptr->dirty) fte_ptr->dirty = frame_dirty (fte_ptr);
          if (fte_ptr->dirty) 
            {
              frame_remove_owners (fte_ptr, false, true);
              frame_swap (fte_ptr);
            }
          else 
            {
              /* Remove SPTE references if not dirty and PTE references */
              frame_remove_owners (fte_ptr, !fte_ptr->dirty, true);
              frame_delete (fte_ptr);
            }

          break;
        }
      case WRITE_IF_DIRTY:
        {
          if (!fte_ptr->dirty) fte_ptr->dirty = frame_dirty (fte_ptr);
          if (fte_ptr->dirty) frame_write (fte_ptr);

          /* Remove PTE and SPTE references */
          frame_remove_owners (fte_ptr, true, true);
          frame_delete        (fte_ptr);
          break;
        }
      default: NOT_REACHED ();
    }

  frame_index_arr[victim_index] = NULL;
  return victim_index;
}

/* Linear eviction algorithm. */
static int
evict_find_victim_linear (void)
{
  while (frame_index_arr[linear_victim_candidate_index]->pin_cnt > 0)
      linear_victim_candidate_index 
          = (linear_victim_candidate_index + 1) % frame_index_size;

  int victim_index = linear_victim_candidate_index;
  linear_victim_candidate_index 
      = (linear_victim_candidate_index + 1) % frame_index_size;
  return victim_index;
}

/* Random eviction. */
static int
evict_find_victim_random (void)
{
  int i = (int) (random_ulong () % frame_index_size);

  while (true)
    {
      if (frame_index_arr[i]->pin_cnt == 0) break;
      i = (int) (random_ulong () % frame_index_size);
    }

  ASSERT (frame_index_arr[i]->pin_cnt == 0);
  return i;
}

/* Second chance eviction. */
static int
evict_find_victim_sca (void)
{
  struct fte *sca_victim_candidate_ptr;
  for (int i = sca_victim_candidate_index; true;
       i = (i + 1) % frame_index_size)
    {
      sca_victim_candidate_ptr = frame_index_arr[i];
      if (sca_victim_candidate_ptr->pin_cnt > 0)
          continue;
      else if (frame_unset_accessed_ptes (sca_victim_candidate_ptr))
          continue;
      else
        {
          sca_victim_candidate_index = (i + 1) % frame_index_size;
          return i;
        }
    }
}

/* Returns true if the frame was accessed, all access bits of owners will
   be set to 0 in this case. Otherwise returns false. */
static bool
frame_unset_accessed_ptes (struct fte *fte_ptr)
{
  bool accessed = false;
  if (fte_ptr->shared)
    {
      struct list *owner_list_ptr = fte_ptr->owners.owner_list_ptr;

      for (struct list_elem *e = list_begin (owner_list_ptr); 
           e != list_end (owner_list_ptr);
           e  = list_next (e))
        {
          accessed |= pagedir_unset_accessed_pte (
              list_entry (e, struct owner_list_elem, elem)->owner);
        }
    }
  else
    {
      accessed = pagedir_unset_accessed_pte (fte_ptr->owners.owner_single);
    }

  return accessed;
}

/* Returns true if a PTE is accessed and unsets it's access bit. 
   Returns false otherwise */
static bool
pagedir_unset_accessed_pte (struct owner owner)
{
  if (pagedir_is_accessed (owner.owner_ptr->pagedir,
                           owner.upage_ptr))
    {
      pagedir_set_accessed (owner.owner_ptr->pagedir, 
                            owner.upage_ptr,
                            false);
      return true;
    }

  return false;
}
