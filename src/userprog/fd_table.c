#include "userprog/fd_table.h"
#include "userprog/syscall.h"
#include <hash.h>

/* Initialises a fd_item and inserts it into the given thread's hash map */
void
init_fd_item (struct fd_item *fd_item_ptr, struct thread *t, struct file *fp)
{
  /* Increments the thread's fd_cnt to ensure that each file is mapped upon
     a unique fd value. Initially initialised to 2 in thread_create */
  fd_item_ptr->fd = t->fd_cnt++;
  fd_item_ptr->pid = (pid_t) t->tid;
  fd_item_ptr->file_ptr = fp;
  hash_insert (&t->hash_fd, &(fd_item_ptr->hash_elem));
}

/* Creates a fake_fd_item to pass in for hash functions
   This fake fd_item will have the same fd and pid value of the fd_item
   which is for sought in get_fd_item and remove_item */
static struct fd_item
create_fake_fd_item (int fd, pid_t tid)
{
  struct fd_item fake_fd_item;
  fake_fd_item.fd = fd;
  fake_fd_item.pid = (pid_t) thread_current ()->tid;
  return fake_fd_item;
}

/* Fetches the fd_item, given the fd value */
struct fd_item *
get_fd_item (struct hash *fd_hash_table, int fd)
{
  /* Create a fake fd_item to pass in hash_find */
  struct fd_item fake_fd_item 
    = create_fake_fd_item (fd, thread_current ()->tid);

  /* Searches the hash_elem with the same fd value of the fake fd_item.
     returns null if the hash_elem could not be found */
  struct hash_elem *elem = hash_find (fd_hash_table, 
                                      &fake_fd_item.hash_elem);
  if (elem == NULL) return NULL;
  return hash_entry (elem, struct fd_item, hash_elem);
}

/* Fetches the file, given the fd value */ 
struct file *
get_file (struct hash *fd_hash_table, int fd) 
{
  if (get_fd_item (fd_hash_table, fd) == NULL)
    return NULL;
  return get_fd_item (fd_hash_table, fd)->file_ptr;
}

/* Removes the file, given the fd value */
bool 
remove_file (struct hash *fd_hash_table, int fd) 
{
  /* Create a fake fd_item to pass in hash_delete */
  struct fd_item fake_fd_item 
      = create_fake_fd_item (fd, thread_current ()->tid);

  /* Searches to delete the hash_elem with the same fd value of the fake fd_item.
     returns null if the hash_elem could not be found */
  struct hash_elem *found_item 
      = hash_delete (fd_hash_table, &fake_fd_item.hash_elem);
  return found_item != NULL;
}
