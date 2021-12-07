#ifndef VM_SWAP_H
#define VM_SWAP_H

#include "devices/block.h"
#include "vm/ft.h"

#define SECTORS_PER_PAGE (PGSIZE / BLOCK_SECTOR_SIZE)

void swap_init  (void);
void swap_in    (struct fte *fte_ptr);
void swap_out   (struct fte *fte_ptr);

#endif
