#include "userprog/fd_table.h"
#include "userprog/syscall.h"
#include <hash.h>

uint32_t fd_counter;

hash_hash_func *fd_hash_func_ptr;
hash_less_func *fd_hash_less_func_ptr;

void
init_fd_item (struct fd_item *fd_item_ptr, struct thread *t, struct file *f)
{
  fd_item_ptr->fd = t->fd_cnt++;
  fd_item_ptr->pid = (pid_t) t->tid;
  fd_item_ptr->file_ptr = f;
  hash_insert (&t->hash_fd, &(fd_item_ptr->hash_elem));
}

static struct fd_item
create_fake_fd_item (int fd, pid_t tid)
{
  struct fd_item fake_fd_item;
  fake_fd_item.fd = fd;
  fake_fd_item.pid = (pid_t) thread_current ()->tid;
  return fake_fd_item;
}

struct file *
get_file (struct hash *fd_hash_table, int fd) 
{

  struct fd_item fake_fd_item 
    = create_fake_fd_item (fd, thread_current ()->tid);

  struct hash_elem *elem = hash_find (fd_hash_table, 
                                      &fake_fd_item.hash_elem);
  if (elem == NULL) return NULL;
  else return hash_entry (elem, struct fd_item, hash_elem)->file_ptr;
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
