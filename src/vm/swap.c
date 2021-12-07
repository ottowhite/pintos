#include "vm/swap.h"
#include "vm/ft.h"
#include "vm/spt.h"
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
swap_in (block_sector_t sector, struct fte *fte_ptr)
{
  void *kpage = fte_ptr->loc.frame_ptr;

  for (int i = 0; i < SECTORS_PER_PAGE; i++, sector++, kpage += BLOCK_SECTOR_SIZE)
    block_read (swap_device, sector, kpage);

  fte_ptr->swapped = false;
  
  bitmap_reset (swap_bitmap, fte_ptr->loc.swap_index);
}

/* Swaps out he evicted page from the frame and copy into the swap disk */
void
swap_out (struct fte *fte_ptr)
{
   block_sector_t sector;
   void *kpage = fte_ptr->loc.frame_ptr;
  
  if (!find_free_slot (&sector))
   PANIC ("No swap space available");
  
  fte_ptr->swapped = true;
  fte_ptr->loc.swap_index = sector / SECTORS_PER_PAGE;
 
  for (int i = 0; i < SECTORS_PER_PAGE; i++, sector++, kpage += BLOCK_SECTOR_SIZE)
    block_write (swap_device, sector, kpage);
}

static bool
find_free_slot (block_sector_t *sector_ptr)
{
    lock_acquire (&swap_lock);
    block_sector_t sector = bitmap_scan_and_flip (swap_bitmap, 0, 1, false);
    lock_release (&swap_lock);

    sector_ptr = sector * SECTORS_PER_PAGE;

    return (sector == BITMAP_ERROR);
}
