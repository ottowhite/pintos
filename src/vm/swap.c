#include "vm/swap.h"
#include "vm/ft.h"
#include "vm/spt.h"
#include "threads/vaddr.h"
#include "lib/kernel/bitmap.h"


#define SECTORS_PER_PAGE (PGSIZE / BLOCK_SECTOR_SIZE)


struct block *swap_device;
static struct bitmap *swap_bitmap;  
static struct lock swap_lock;

/* Initializes the swap disk */
void
swap_init()
{
  
}

/* Swaps in the page from the swap table into the memory */
void
swap_in (block_sector_t sector, void *kpage)
{
  for (int i = 0; i < SECTORS_PER_PAGE; i++, sector++, kpage += BLOCK_SECTOR_SIZE)
    block_read (swap_device, sector, kpage);
  
  // reset the bitmap for reuse
  bitmap_reset (swap_bitmap, //number of bits);
}

/* Swaps out he evicted page from the frame and copy into the swap disk */
void
swap_out (void *kpage)
{
   block_sector_t sector;
  
  if (!find_free_slot (&sector))
   PANIC ("No swap space available");
 
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
