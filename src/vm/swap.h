#include "src/devices/block.h"

void swap_in (block_sector_t sector, void *kpage);
void swap_out (void *kpage);
static bool find_free_slot (block_sector_t *sector_ptr);