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
  list_push_front (list_ptr, &mmape_ptr->list_elem);
}

/* Iterates over the given list and returns the mmape pointer if found,
   returns NULL otherwise. */
struct mmape * 
mmap_locate_entry (struct list *list_ptr, mapid_t mid)
{
  bool found = false;
  struct mmape *mmape_ptr;

  for (struct list_elem *e = list_begin (list_ptr); e != list_end (list_ptr);
       e = list_next (e))
    {
      mmape_ptr = list_entry (e, struct mmape, list_elem);
      if (mmape_ptr->mid == mid) 
        {
          found = true;
          break;
        } 
    }

  return (found) ? mmape_ptr : NULL;
}

void
mmap_delete_entry (struct mmape *mmape_ptr)
{
  list_remove (&mmape_ptr->list_elem);
}
