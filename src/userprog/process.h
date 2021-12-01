#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"
#include "vm/ft.h"

#define MAX_ARGS (50)
#define MAX_CHARS (512)

tid_t process_execute (const char *file_name);
int process_wait (tid_t);
void process_exit (void);
void process_activate (void);
bool install_page_unpin_frame (void *upage, 
                               void *kpage, 
                               bool *pinned,
                               bool writable);

#endif /* userprog/process.h */
