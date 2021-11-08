#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"

static void syscall_handler (struct intr_frame *);

/* System call helper functions */
static syscall_func syscall_halt NO_RETURN;
static syscall_func syscall_exit NO_RETURN;
static syscall_func syscall_exec;
static syscall_func syscall_wait;
static syscall_func syscall_create;
static syscall_func syscall_remove;
static syscall_func syscall_open;
static syscall_func syscall_filesize;
static syscall_func syscall_read;
static syscall_func syscall_write;
static syscall_func syscall_seek;
static syscall_func syscall_tell;
static syscall_func syscall_close;


static void *syscall_func_map[] = {
  /* SYS_HALT */      &syscall_halt,
  /* SYS_EXIT */      &syscall_exit,
  /* SYS_EXEC */      &syscall_exec, 
  /* SYS_WAIT */      &syscall_wait,
  /* SYS_CREATE */    &syscall_create,
  /* SYS_REMOVE */    &syscall_remove,
  /* SYS_OPEN */      &syscall_open,
  /* SYS_FILESIZE */  &syscall_filesize,
  /* SYS_READ */      &syscall_read,
  /* SYS_WRITE */     &syscall_write,
  /* SYS_SEEK */      &syscall_seek,
  /* SYS_TELL */      &syscall_tell,
  /* SYS_CLOSE */     &syscall_close,
};


void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

/* Allows user processes to ask the kernel to execute operations
   that they don't have permission to execute themselves */
static void
syscall_handler (struct intr_frame *f UNUSED) 

{
  /* Verification before dereference */
   uint32_t *esp = (uint32_t *)verify_ptr (f->esp);

  /* Read the syscall number at the stack pointer (f->esp) */
  int syscall_no = *esp;
  ASSERT (0 <= syscall_no && syscall_no < NUM_SYSCALL);

  /* Read the arguments above the stack pointer */

  /* Pass these to the appropriate function */
  syscall_func *helper = syscall_func_map[syscall_no];

  ASSERT (helper != NULL);

  /* Return the result to f->eax */
  f->eax = helper (esp);

  printf ("system call!\n");
  thread_exit ();
}

static void
verify_ptr (void *ptr)
{
  /* Check that the pointer address is in the user space */
  if (ptr != NULL && is_user_vaddr (ptr))
    {
      /* Check that the address is mapped */
      if (pagedir_get_page (uint32_t *pd, const void *uaddr) != NULL)
        {
          return;
        }
    }

  /* If this point is reached the pointer is not valid. Exit with -1 */
  exit_process_in_syscall (-1);
  NOT_REACHED ();
}
