#include <debug.h>
#include <stdio.h>
#include <random.h>
#include "vm/evict.h"
#include "vm/ft.h"

static int evict_find_victim_random (void);
static int evict_find_victim_sca    (void);

static int sca_victim_candidate = 0;

int
evict (void)
{
  /* Obtain a random int from 0 to frame_index_size (exclusive) */
  /* Keep trying until we find a frame that is not pinned to evict */

  int victim_index    = evict_find_victim_random ();
  struct fte *fte_ptr = frame_index_arr[victim_index];

  switch (fte_ptr->eviction_method)
    {
      case SWAP:
        {
          debugf("Eviction by swap. \n");
          frame_swap (fte_ptr);
          break;
        }
      case DELETE: 
        {
          debugf("Eviction by deletion. \n");
          frame_remove_owners (fte_ptr, true);
          frame_delete (fte_ptr); 
          break;
        }
      case SWAP_IF_DIRTY:
        {
          bool dirty = frame_dirty (fte_ptr);

          if (dirty) debugf("Eviction by swapping as dirty. \n");
          else       debugf("Eviction by deletion as not dirty. \n");

          if (dirty) frame_swap (fte_ptr);
          else 
            {
              frame_remove_owners (fte_ptr, !dirty);
              frame_delete (fte_ptr);
            }

          break;
        }
      case WRITE_IF_DIRTY:
        {
          bool dirty = frame_dirty (fte_ptr);
          if (dirty) frame_write (fte_ptr);

          if (dirty) debugf("Eviction by writing as dirty. \n");
          else       debugf("Eviction by deletion as not dirty.");

          frame_remove_owners (fte_ptr, true);
          frame_delete        (fte_ptr);
          break;
        }
      default: NOT_REACHED ();
    }

  frame_index_arr[victim_index] = NULL;
  return victim_index;
}

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

  return 0;
}
