#include <stdint.h>
#include "userprog/fd_table.h"
#include "threads/thread.h"
#include "lib/kernel/hash.h"
#include "lib/user/syscall.h"

struct fd_item 
  {
    int fd;
    pid_t pid;
    struct file *file_ptr;
    struct hash_elem hash_elem;
  };

uint32_t fd_counter;

static struct hash fd_hash_table;

hash_hash_func *fd_hash_func_ptr;
hash_less_func *fd_hash_less_func_ptr;

uint32_t fd_hash_func (const struct hash_elem*, void*);
bool fd_hash_less_func (const struct hash_elem*, 
                        const struct hash_elem*, 
                        void*);

bool 
fd_table_init (void) 
{
  fd_counter = 2;
  return hash_init (&fd_hash_table, &fd_hash_func, &fd_hash_less_func, NULL);
}

int 
get_free_fd (void) 
{
  return fd_counter++;
}

struct 
file *get_file (int fd) 
{
  struct fd_item fake_fd_item;
  fake_fd_item.fd = fd;
  fake_fd_item.pid = (pid_t) thread_current ()->tid;
  struct hash_elem *elem = hash_find (&fd_hash_table, 
                                      &fake_fd_item.hash_elem);
  if (elem == NULL) 
    {
      return NULL;
    }
  
  return hash_entry (elem, struct fd_item, hash_elem)->file_ptr;
}

bool 
remove_file (int fd) 
{
  struct fd_item fake_fd_item;
  fake_fd_item.fd = fd;
  fake_fd_item.pid = (pid_t) thread_current ()->tid;
  struct hash_elem *found_item = hash_delete (&fd_hash_table,
                                              &fake_fd_item.hash_elem);
  return found_item != NULL;
}

uint32_t 
fd_hash_func (const struct hash_elem *elem, void *aux UNUSED) 
{
  return (uint32_t) hash_entry (elem, struct fd_item, hash_elem)->fd;
}

bool fd_hash_less_func 
(const struct hash_elem* a, const struct hash_elem* b, void* aux UNUSED) 
{
  struct fd_item *item_a = hash_entry (a, struct fd_item, hash_elem);
  struct fd_item *item_b = hash_entry (b, struct fd_item, hash_elem);

  if (item_a->fd == item_b->fd) 
    return item_a->pid < item_b->pid;
  return item_a->fd < item_b->fd;
} 
