#include "threads/malloc.h"
#include "vm/mmap.h"

/* mmap_remove_entry helper functions */
static struct mmape *mmap_locate_entry (struct list *list_ptr, mapid_t mid);
static void          mmap_delete_entry (struct mmape *mmape_ptr);

void mmap_init (struct list *list_ptr)
{
  list_init (list_ptr);
}

/* Allocates space for a new mmap entry, returns false if malloc fails */
bool 
mmap_add_entry (struct list *list_ptr, 
                mapid_t mid, 
                void *uaddr, 
                size_t filesize)
{
  struct mmape *mmape_ptr = malloc (sizeof (struct mmape));
  if (mmape_ptr == NULL) return false;

  mmape_ptr->mid          = mid;
  mmape_ptr->uaddr        = uaddr;
  mmape_ptr->filesize     = filesize;
  list_push_front (list_ptr, &mmape_ptr->list_elem);

  return true;
}

/* Locates and deletes an mmap entry, returns NULL if not located */
struct mmape *
mmap_remove_entry (struct list *list_ptr, mapid_t mid)
{
  struct mmape *mmape_ptr = mmap_locate_entry (list_ptr, mid);
  if (mmape_ptr != NULL) mmap_delete_entry (mmape_ptr);
  return mmape_ptr;
}

/* Iterates over the given list and returns the mmape pointer if found,
   returns NULL otherwise. */
static struct mmape * 
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
mmap_delete_all (struct list *list_ptr)
{
  /* loops through each mmap entry removing it from the list and freeing it */
  for (struct list_elem *e = list_begin (list_ptr); 
       e != list_end (list_end (list_ptr));
       e  = list_next (e))
    {
      list_remove (e);
      free (list_entry (e, struct mmape, list_elem));
    }
}

static void
mmap_delete_entry (struct mmape *mmape_ptr)
{
  list_remove (&mmape_ptr->list_elem);
  free (mmape_ptr);
}
