#include "threads/malloc.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "vm/mmap.h"
#include "spt.h"

static struct mmape *mmap_locate_entry (struct list *list_ptr, mapid_t mid);

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

/* Locates and deletes an mmap entry in the current thread, returns NULL 
   if not located and does not free the mmape_ptr. */
void
mmap_remove_entry (mapid_t mid)
{
	struct thread *t_ptr  = thread_current ();
	struct list *list_ptr = &t_ptr->mmap_list;

  struct mmape *mmape_ptr = mmap_locate_entry (list_ptr, mid);
/* If list mmap entry not found return. */
	if (mmape_ptr == NULL) return;
	
	list_remove (&mmape_ptr->list_elem);
	/* Remove all allocated spt entries associated with the mmapped file. */
  void *loc = mmape_ptr->uaddr + mmape_ptr->filesize;
  acquire_ft ();
  while (loc >= mmape_ptr->uaddr) 
    spt_remove_entry (t_ptr->spt_ptr, loc = pg_round_down (--loc));
  release_ft ();
  free (mmape_ptr);
}

/* Takes a pointer to the mmap_list of thread and removes and frees all of the
   mmap entries. Called when a process is terminated. */
void
mmap_remove_all (struct list *list_ptr)
{
  struct thread *t_ptr = thread_current ();
  /* loops through each mmap entry removing it from the list and freeing it
     as well as calling spt_remove on each of its user addresses */
	struct list_elem *e = list_begin (list_ptr);
	struct list_elem *e_nxt;
	while (e != list_end (list_ptr))
    {
			e_nxt = list_next (e);
      struct mmape *mmape_ptr = list_entry (e, struct mmape, list_elem);
      /* Remove all allocated spt entries associated with the mmapped file. */
      void *loc = mmape_ptr->uaddr + mmape_ptr->filesize;
      acquire_ft ();
      while (loc >= mmape_ptr->uaddr) 
          spt_remove_entry (t_ptr->spt_ptr, loc -= PGSIZE); 
      release_ft ();

			/* Deallocate the list entry. */
			free (mmape_ptr);
			e = e_nxt;
    }
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

