#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"

static void syscall_handler (struct intr_frame *);

typedef uint32_t syscall_func (void *args[]);

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


static syscall_func *syscall_func_map[] = {
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
  verify_ptr (f->esp);

  /* Read the syscall number at the stack pointer (f->esp) */
  uint32_t *esp = f->esp;
  int syscall_no = *esp;

  // NUM_SYSCALL = number of system calls
  ASSERT (0 <= syscall_no && syscall_no < NUM_SYSCALL);

  // TODO
  int num_of_args;
  // magic number MAX_NUM_OF_ARGS = 3
  uint32_t args[3];

  /* Read the arguments above the stack pointer */
  switch (num_of_args)
  {
    case 3 :
      verify_ptr (*(esp + 3));
      args[2] = *(esp + 3);
    case 2 :
      verify_ptr (*(esp + 2));
      args[1] = *(esp + 2);
    case 1 :
      verify_ptr (*(esp + 1));
      args[0] = *(esp + 1);
    default :
      break;
  }
  
  /* Pass these to the appropriate function */
  uint32_t result = syscall_func_map[syscall_no] (args);

  /* Return the result to f->eax */
  f->eax = result;
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

/* SYS_HALT */
static uint32_t
syscall_halt (void *args[])
{
  /* TODO implementation */
  return;
}

/* SYS_EXIT */
static uint32_t
syscall_exit (void *args[])
{
  /* TODO implementation */
  return;
}

/* SYS_EXEC */
static uint32_t
syscall_exec (void *args[])
{
  /* TODO implementation */
  return;
}

/* SYS_WAIT */
static uint32_t
syscall_wait (void *args[])
{
  /* TODO implementation */
  return;
}

/* SYS_CREATE */
static uint32_t
syscall_create (void *args[])
{
  /* TODO implementation */
  return;
}

/* SYS_REMOVE */
static uint32_t
syscall_remove (void *args[])
{
  /* TODO implementation */
  return;
}

/* SYS_OPEN */
static uint32_t
syscall_open (void *args[])
{
  /* TODO implementation */
  return;
}

/* SYS_FILESIZE */
static uint32_t
syscall_filesize (void *args[])
{
  /* TODO implementation */
  return;
}

/* SYS_READ */
static uint32_t
syscall_read (void *args[])
{
  /* TODO implementation */
  return;
}

/* SYS_WRITE */
static uint32_t
syscall_write (void *args[])
{
  /* TODO implementation */
  return;
}

/* SYS_SEEK */
static uint32_t
syscall_seek (void *args[])
{
  /* TODO implementation */
  return;
}

/* SYS_TELL */
static uint32_t
syscall_tell (void *args[])
{
  /* TODO implementation */
  return;
}

/* SYS_CLOSE */
static uint32_t
syscall_close (void *args[])
{
  /* TODO implementation */
  return;
}