#include "vm/swap.h"
#include "vm/spt.h"
#include "threads/synch.h"
#include "threads/vaddr.h"
#include "lib/kernel/bitmap.h"

static bool find_free_slot (block_sector_t *sector_ptr);

struct block *swap_device;
static struct bitmap *swap_bitmap;  
static struct lock swap_lock;

/* Initializes the swap disk */
void
swap_init (void)
{
  swap_device = block_get_role (BLOCK_SWAP);
  swap_bitmap = bitmap_create (block_size (swap_device) / SECTORS_PER_PAGE);

  if (swap_bitmap == NULL)
    PANIC ("Memory allocation for swap bitmap failed--Swap device is too large");

  lock_init (&swap_lock);
}

/* Swaps in the page from the swap table into the memory */
void
swap_in (struct fte *fte_ptr, void *kpage)
{
  ASSERT (fte_ptr != NULL);
  ASSERT (fte_ptr->swapped);

  block_sector_t sector  = fte_ptr->loc.swap_index * SECTORS_PER_PAGE;
  void *kpage_write_head = kpage;

  if (sector < 0)
    PANIC ("Data is not stored in the swap table");

  for (int i = 0; i < SECTORS_PER_PAGE; i++, sector++, 
                                        kpage_write_head += BLOCK_SECTOR_SIZE)
    block_read (swap_device, sector, kpage_write_head);

  bitmap_reset (swap_bitmap, fte_ptr->loc.swap_index);
  
  fte_ptr->swapped = false;
  fte_ptr->loc.frame_ptr = kpage;
}

/* Swaps out the evicted page from the frame and copy into the swap disk */
void
swap_out (struct fte *fte_ptr)
{
  ASSERT (fte_ptr != NULL);
  
  block_sector_t sector;
  void *kpage = fte_ptr->loc.frame_ptr;
  
  if (!find_free_slot (&sector))
   PANIC ("No swap space available");
  
  fte_ptr->swapped = true;
  fte_ptr->loc.swap_index = sector / SECTORS_PER_PAGE;
 
  for (int i = 0; i < SECTORS_PER_PAGE; i++, sector++, kpage += BLOCK_SECTOR_SIZE)
    block_write (swap_device, sector, kpage);
}

/* Resets the bit corresponding to the fte in the swap bitmap. */
void
swap_remove (struct fte *fte_ptr)
{
	ASSERT (fte_ptr != NULL);
	lock_acquire (&swap_lock);
	bitmap_reset (swap_bitmap, fte_ptr->loc.swap_index);
	lock_release (&swap_lock);
}

static bool
find_free_slot (block_sector_t *sector_ptr)
{
    lock_acquire (&swap_lock);
    block_sector_t sector = bitmap_scan_and_flip (swap_bitmap, 0, 1, false);
    lock_release (&swap_lock);

    *sector_ptr = sector * SECTORS_PER_PAGE;

    return (sector == BITMAP_ERROR);
}
