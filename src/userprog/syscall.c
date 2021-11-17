#include <stdio.h>
#include <syscall-nr.h>
#include <string.h>
#include <hash.h>
#include "filesys/file.h"
#include "threads/vaddr.h"
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/synch.h"
#include "threads/malloc.h"
#include "userprog/syscall.h"
#include "userprog/pagedir.h"
#include "userprog/process.h"
#include "userprog/fd_table.h"
#include "devices/shutdown.h"

/* System call helper functions */
static void     syscall_halt     (void);
static void     syscall_exit     (int status);
static pid_t    syscall_exec     (const char *cmd_line);
static int      syscall_wait     (pid_t pid);
static bool     syscall_create   (const char *file, unsigned initial_size);
static bool     syscall_remove   (const char *file);
static int      syscall_open     (const char *file);
static int      syscall_filesize (int fd);
static int      syscall_read     (int fd, void *buffer, unsigned size);
static int      syscall_write    (int fd, const void *buffer, unsigned size);
static void     syscall_seek     (int fd, unsigned position);
static unsigned syscall_tell     (int fd);
static void     syscall_close    (int fd);

static void     syscall_handler (struct intr_frame *f);
static bool     verify_ptr      (const void *ptr);
static bool     verify_args     (int argc, const uint32_t *esp);
static uint32_t invoke_function (const void *syscall_ptr, 
                                 int argc, 
                                 const uint32_t *esp);
static int  read_from_console    (void *buffer, unsigned size);
static int  read_from_file       (int fd, void *buffer, unsigned size);
static int  write_to_console     (const char *buffer, unsigned size);
static int  write_to_file        (int fd, const char *buffer, unsigned size);

static struct lock filesys_lock;

static struct syscall 
syscall_func_map[] = 
  {
    {&syscall_halt,     .argc = 0},  /* SYS_HALT */      
    {&syscall_exit,     .argc = 1},  /* SYS_EXIT */      
    {&syscall_exec,     .argc = 1},  /* SYS_EXEC */      
    {&syscall_wait,     .argc = 1},  /* SYS_WAIT */      
    {&syscall_create,   .argc = 2},  /* SYS_CREATE */    
    {&syscall_remove,   .argc = 1},  /* SYS_REMOVE */    
    {&syscall_open,     .argc = 1},  /* SYS_OPEN */      
    {&syscall_filesize, .argc = 1},  /* SYS_FILESIZE */  
    {&syscall_read,     .argc = 3},  /* SYS_READ */      
    {&syscall_write,    .argc = 3},  /* SYS_WRITE */     
    {&syscall_seek,     .argc = 2},  /* SYS_SEEK */      
    {&syscall_tell,     .argc = 1},  /* SYS_TELL */      
    {&syscall_close,    .argc = 1},  /* SYS_CLOSE */     
  };

void
syscall_init (void) 
{
  lock_init (&filesys_lock);
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

/* Allows user processes to ask the kernel to execute operations
   that they don't have permission to execute themselves */
static void
syscall_handler (struct intr_frame *f) 
{
  /* Read the syscall number at the stack pointer (f->esp) */
  if (!verify_ptr (f->esp)) syscall_exit (-1);
  int syscall_no = *((int *) f->esp);

  /* Ensure our syscall_no refers to a defined system call */
  ASSERT (SYS_HALT <= syscall_no && syscall_no <= SYS_CLOSE);

  /* Read the argc and function_ptr values from our syscall_func_map */
  int   argc        = syscall_func_map[syscall_no].argc;
  void *syscall_ptr = syscall_func_map[syscall_no].syscall_ptr;

  /* Verify the necessary number of arguments before passing function
     invocation to invoke_function */
  if (!verify_args (argc, f->esp)) syscall_exit (-1);
  f->eax = invoke_function (syscall_ptr, argc, f->esp);
}

static bool 
verify_args (int argc, const uint32_t *esp) 
{
  for (int i = argc; i >= 1; i--) if (!verify_ptr (&esp[i])) return false;
  return true;
}

static bool
verify_ptr (const void *ptr)
{
  /* Verify address in user space and in page directory*/
  if (ptr != NULL && 
      is_user_vaddr (ptr) && 
      pagedir_get_page (active_pd (), ptr) != NULL) 
    return true;
  else 
    return false;
}

static uint32_t 
invoke_function (const void *syscall_ptr, int argc, const uint32_t *esp) 
{
  switch (argc) 
    {
      case 0: return ((syscall_0_args) syscall_ptr) ();
      case 1: return ((syscall_1_args) syscall_ptr) (esp[1]); 
      case 2: return ((syscall_2_args) syscall_ptr) (esp[1], esp[2]); 
      case 3: return ((syscall_3_args) syscall_ptr) (esp[1], esp[2], esp[3]); 
      default: NOT_REACHED (); 
    }
}

/* SYS_HALT */
static void
syscall_halt (void)
{
  shutdown_power_off ();
}

/* SYS_EXIT 
 * Set return status in the child struct for parent to access later on. 
 * Free current process resources and output process name and exit code. */
static void
syscall_exit (int status)
{
	struct thread *cur = thread_current ();

	/* Retrieve name of process. */
	char name[MAX_PROCESS_NAME_LENGTH];
	strlcpy (name, cur->name, strlen (thread_current ()->name) + 1);
	process_exit ();
	
	/* Generate output string. */
	uint8_t output_buffer_size = MAX_PROCESS_NAME_LENGTH + 15; 
	char output_buffer[output_buffer_size];
  int chars_written = snprintf (output_buffer, 
                                output_buffer_size, 
                                "%s: exit(%d)\n", name, status);
	ASSERT (chars_written != 0);
	syscall_write (1, output_buffer, strlen (output_buffer));

	/* Set exit status and call sema up. For process_wait. */

	/* Acquire lock to prevent race conditions between process writting to the 
	 * struct child, and its parent deallocating that struct when exiting. */
	lock_acquire (&cur->self_lock);
	struct child *child_ptr = cur->self_child_ptr;
	if (child_ptr != NULL) 
		{
			child_ptr->exit_status = status;
			lock_release (&cur->self_lock);
			sema_up (&child_ptr->sema);
		}
	else lock_release (&cur->self_lock);

  thread_exit ();
}

/* SYS_EXEC */
static pid_t
syscall_exec (const char *cmd_line)
{
  if (!verify_ptr (cmd_line)) syscall_exit (-1);
  tid_t pid = process_execute (cmd_line);
  return (pid_t) pid; 
}

/* SYS_WAIT */
static int
syscall_wait (pid_t pid)
{
  return process_wait ((tid_t) pid);
}

/* SYS_CREATE */
static bool
syscall_create (const char *file UNUSED, unsigned initial_size UNUSED)
{
  if (file == NULL || !verify_ptr (file)) syscall_exit (-1);
  /* Acquire the lock to access files */
  lock_acquire (&filesys_lock);
  
  bool result = filesys_create (file, initial_size);
  
  lock_release (&filesys_lock);
  
  return result;
}

/* SYS_REMOVE */
static bool
syscall_remove (const char *file UNUSED)
{
  /* Acquire the lock to access files */
  lock_acquire (&filesys_lock);

  bool remove = filesys_remove (file);
  
  lock_release (&filesys_lock);
  return remove;
}

/* SYS_OPEN */
static int
syscall_open (const char *file)
{
  /* Returns an error if the file name is invalid */
  if (file == NULL) return -1;

  if (!verify_ptr (file)) syscall_exit (-1);

  /* Acquires the lock to open the file, 
     and returns an error if the file is invalid */
  lock_acquire (&filesys_lock);

  struct file *file_to_open = filesys_open (file);

  lock_release (&filesys_lock);
  if (file_to_open == NULL) return -1;

  /* Create a new fd_item to pass into the hash table, and
     return an error if it failed to do so */
  struct fd_item *new_fd_item = malloc (sizeof (struct fd_item));
  if (new_fd_item == NULL) syscall_exit (-1);

  /* Acquires the lock to store the file_to_open in a new fd_item struct
     and push the struct into the current thread's hash table */
  lock_acquire (&filesys_lock);

  init_fd_item (new_fd_item, thread_current (), file_to_open);

  lock_release (&filesys_lock);

  return new_fd_item->fd;
}

/* SYS_FILESIZE */
static int
syscall_filesize (int fd UNUSED)
{
  /* Fetches the corresponding file */
  struct file *fp = get_file (&thread_current ()->hash_fd, fd);
  if (fp == NULL) syscall_exit (-1);

  /* Acquire the lock to access files */
  lock_acquire (&filesys_lock);
  
  int length = file_length (fp);
  
  lock_release (&filesys_lock);
  
  return length;
}

/* SYS_READ */
static int
syscall_read (int fd, void *buffer, unsigned size)
{
  if (buffer == NULL || 
      fd >= MAX_OPEN_FILES ||
      fd == STDOUT_FILENO ||
      !verify_ptr (buffer)) syscall_exit (-1);

  unsigned bytes_read;

  lock_acquire (&filesys_lock);

  if (fd == STDIN_FILENO)  bytes_read = read_from_console (buffer, size);
  else                     bytes_read = read_from_file (fd, buffer, size);

  lock_release (&filesys_lock);

  return bytes_read;
}

static int
read_from_console (void *buffer, unsigned size)
{
  unsigned cnt = 0;

  /* While the count does not exceed the buffer size, read input from console */
  while (cnt != size) *(uint8_t *) (buffer + cnt++) = input_getc ();
  
  return cnt;
}

static int
read_from_file (int fd, void *buffer, unsigned size)
{
  struct file *fp = get_file (&thread_current ()->hash_fd, fd);
  if (fp == NULL) return -1;
  int cnt = file_read (fp, buffer, size);
  return cnt;
}


/* SYS_WRITE */
static int
syscall_write (int fd, const void *buffer, unsigned size)
{
  if (buffer == NULL || fd >= MAX_OPEN_FILES) syscall_exit (-1);

  unsigned bytes_written;

  lock_acquire (&filesys_lock);

  if      (fd == STDIN_FILENO)  syscall_exit (-1);
  else if (fd == STDOUT_FILENO) bytes_written = write_to_console (buffer, size);
  else                          bytes_written = write_to_file (fd, buffer, size);

  lock_release (&filesys_lock);

  return bytes_written;
}

static int
write_to_console (const char *buffer, unsigned size)
{
  int bytes_written = 0;

  /* here we break the buffer into chunks of size MAX_CONSOLE_BUFFER_SIZE 
   * if necessary and write them to the console */
  for (int32_t offset   = 0, bytes_remaining = size, bytes_to_write; 
       bytes_remaining  > 0;
       bytes_remaining -= MAX_CONSOLE_BUFFER_SIZE,
       offset          += MAX_CONSOLE_BUFFER_SIZE) 
    {
      bytes_to_write = (bytes_remaining - offset <= MAX_CONSOLE_BUFFER_SIZE) ?
          bytes_remaining : MAX_CONSOLE_BUFFER_SIZE; 
      putbuf (&buffer[offset], bytes_to_write);
      bytes_written += bytes_to_write;
    }

  return bytes_written;
}

static int
write_to_file (int fd, const char *buffer, unsigned size)
{
  struct file *fp = get_file (&thread_current ()->hash_fd, fd);
  ASSERT (fp != NULL);
  return file_write (fp, buffer, size);
}

/* SYS_SEEK */
static void
syscall_seek (int fd UNUSED, unsigned position UNUSED)
{
  /* Fetches the corresponding file */
  struct file *fp = get_file (&thread_current ()->hash_fd, fd);
  if (fp == NULL) syscall_exit (-1);

  /* Acquires the lock to access files */
  lock_acquire (&filesys_lock);

  file_seek(fp, position);
  
  lock_release (&filesys_lock);
}

/* SYS_TELL */
static unsigned
syscall_tell (int fd UNUSED)
{
  /* Fetches the corresponding file */
  struct file *fp = get_file (&thread_current ()->hash_fd, fd);
  if (fp == NULL) syscall_exit (-1);
  
  /* Acquires the lock to access files */
  lock_acquire (&filesys_lock);
  
  int tell = file_tell (fp);
  
  lock_acquire (&filesys_lock);

  return tell;
}

/* SYS_CLOSE */
static void
syscall_close (int fd UNUSED)
{
  /* Fetches the corresponding file */
  struct fd_item *fd_item_ptr = get_fd_item (&thread_current ()->hash_fd, fd);
  if (fd_item_ptr == NULL) syscall_exit (-1);

  /* Acquires the lock to access files and free the fd_item struct allocated
     upon syscall_open */
  lock_acquire (&filesys_lock);
  
  hash_delete (&thread_current ()->hash_fd, &fd_item_ptr->hash_elem);
  file_close (fd_item_ptr->file_ptr);
  free (fd_item_ptr);
  
  lock_release (&filesys_lock);
}
