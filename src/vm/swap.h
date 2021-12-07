#ifndef VM_SWAP_H
#define VM_SWAP_H

#include "src/devices/block.h"

#define SECTORS_PER_PAGE (PGSIZE / BLOCK_SECTOR_SIZE)

void swap_in (block_sector_t sector, struct fte *fte_ptr);
void swap_out (struct fte *fte_ptr);

#endif
