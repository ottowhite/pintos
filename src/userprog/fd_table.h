/*
#ifndef USERPROG_FD_TABLE_H
#define USERPROG_FD_TABLE_H

#include <stdint.h>
#include "userprog/fd_table.h"
#include "threads/thread.h"
#include "lib/kernel/hash.h"



bool fd_table_init (void);
int get_free_fd (void);
struct file *get_file (int fd);
bool remove_file (int fd);

#endif /* userprog/fd_table.h */
