/*
#include "userprog/fd_table.h"

uint32_t fd_counter;

static struct hash fd_hash_table;

hash_hash_func *fd_hash_func_ptr;
hash_less_func *fd_hash_less_func_ptr;


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
*/