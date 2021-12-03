#include "threads/malloc.h"
#include "vm/mmap.h"

void mmap_init (struct list *list_ptr)
{
  list_init (list_ptr);
}

bool 
mmap_add_entry (struct list *list_ptr, 
                mapid_t mid, 
                void *uaddr, 
                size_t filesize)
{
  struct mmape *mmape_ptr = malloc (sizeof (struct mmape));
  mmape_ptr->mid          = mid;
  mmape_ptr->uaddr        = uaddr;
  mmape_ptr->filesize     = filesize;
  mmap_init (list_ptr);
}
