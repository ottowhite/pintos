#include "userprog/fd_table.h"
#include "userprog/syscall.h"
#include <hash.h>

uint32_t fd_counter;

hash_hash_func *fd_hash_func_ptr;
hash_less_func *fd_hash_less_func_ptr;

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

  struct fd_item fake_fd_item = create_fake_fd_item (fd, thread_current ()->tid);

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
