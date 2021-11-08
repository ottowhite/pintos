#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"

static void syscall_handler (struct intr_frame *);



/* System call helper functions */
static syscall_func halt_helper NO_RETURN;
static syscall_func exit_helper NO_RETURN;
static syscall_func exec_helper;
static syscall_func wait_helper;
static syscall_func create_helper;
static syscall_func remove_helper;
static syscall_func open_helper;
static syscall_func filesize_helper;
static syscall_func read_helper;
static syscall_func write_helper;
static syscall_func seek_helper;
static syscall_func tell_helper;
static syscall_func close_helper;


static syscall_func *syscall_func_map[] = {
  /* SYS_HALT */      &halt_helper,
  /* SYS_EXIT */      &exit_helper,
  /* SYS_EXEC */      &exec_helper, 
  /* SYS_WAIT */      &wait_helper,
  /* SYS_CREATE */    &create_helper,
  /* SYS_REMOVE */    &remove_helper,
  /* SYS_OPEN */      &open_helper,
  /* SYS_FILESIZE */  &filesize_helper,
  /* SYS_READ */      &read_helper,
  /* SYS_WRITE */     &write_helper,
  /* SYS_SEEK */      &seek_helper,
  /* SYS_TELL */      &tell_helper,
  /* SYS_CLOSE */     &close_helper,
};


void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  printf ("system call!\n");
  thread_exit ();
}
