#include "userprog/fd_table.h"
#include "userprog/syscall.h"
#include "threads/malloc.h"
#include <hash.h>

hash_hash_func *fd_hash_func_ptr;
hash_less_func *fd_hash_less_func_ptr;

void
init_fd_item (struct fd_item *fd_item_ptr, struct thread *t, struct file *fp)
{
  fd_item_ptr->fd = t->fd_cnt++;
  fd_item_ptr->pid = (pid_t) t->tid;
  fd_item_ptr->file_ptr = fp;
  hash_insert (t->hash_fd_ptr, &(fd_item_ptr->hash_elem));
}

static struct fd_item
create_fake_fd_item (int fd, pid_t tid)
{
  struct fd_item fake_fd_item;
  fake_fd_item.fd = fd;
  fake_fd_item.pid = (pid_t) thread_current ()->tid;
  return fake_fd_item;
}

struct fd_item *
get_fd_item (struct hash *fd_hash_table, int fd)
{
  struct fd_item fake_fd_item 
    = create_fake_fd_item (fd, thread_current ()->tid);

  struct hash_elem *elem = hash_find (fd_hash_table, 
                                      &fake_fd_item.hash_elem);
  if (elem == NULL) return NULL;
  return hash_entry (elem, struct fd_item, hash_elem);
}

struct file *
get_file (struct hash *fd_hash_table, int fd) 
{
  if (get_fd_item (fd_hash_table, fd) == NULL)
    return NULL;
  return get_fd_item (fd_hash_table, fd)->file_ptr;
}

bool 
remove_file (struct hash *fd_hash_table, int fd) 
{
  struct fd_item fake_fd_item 
      = create_fake_fd_item (fd, thread_current ()->tid);
  struct hash_elem *found_item 
      = hash_delete (fd_hash_table, &fake_fd_item.hash_elem);
  return found_item != NULL;
}

/* An action function to apply hash_destroy to the fd_item hash map */
void
fd_hash_free (struct hash_elem *e, void *aux UNUSED)
{
  struct fd_item *fd_item_ptr = hash_entry (e, struct fd_item, hash_elem);

  /* Acquires the file system to close any opened files before freeing */
  acquire_filesys ();
  file_close (fd_item_ptr->file_ptr);
  release_filesys ();

  free (fd_item_ptr);
}

