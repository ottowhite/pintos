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

void mmap_init      (struct list *list_ptr);
bool mmap_add_entry (struct list *list_ptr, 
                     mapid_t mid, 
                     void *uaddr, 
                     size_t filesize);

void          mmap_remove_entry (mapid_t mid);
void          mmap_remove_all   (struct list *list_ptr);

#endif
