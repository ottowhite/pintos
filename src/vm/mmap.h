#ifndef VM_MMAP_H
#define VM_MMAP_H

#include <list.h>
#include "lib/user/syscall.h"

struct mmape
{
  mapid_t mid;
  void *uaddr;
  size_t filesize;
  struct list_elem list_elem;
};

void mmap_init (struct list *list_ptr);

#endif
