#ifndef USERPROG_FD_TABLE_H
#define USERPROG_FD_TABLE_H

#include <stdint.h>
#include "userprog/fd_table.h"
#include "threads/thread.h"
#include "lib/kernel/hash.h"

struct fd_item 
{
  int fd;
  tid_t pid;
  struct file *file_ptr;
  struct hash_elem hash_elem;
};

void init_fd_item (struct fd_item *fd_item_ptr,
                   struct thread *t,
                   struct file *fp);
int get_free_fd (void);
struct fd_item *get_fd_item (struct hash *fd_hash_table, int fd);
struct file *get_file (struct hash *fd_hash_table, int fd);
bool remove_file (struct hash *fd_hash_table, int fd);
void fd_hash_free (struct hash_elem *e, void *aux UNUSED);

#endif 
