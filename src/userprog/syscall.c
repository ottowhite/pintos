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
       void     syscall_exit     (int status);
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

/* A jump table that contains a function pointer and the number of arguments */
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

/* Initialisation of the syscall handler */
void
syscall_init (void) 
{
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

/* Verifies the arguments set upon the stack */
static bool 
verify_args (int argc, const uint32_t *esp) 
{
  /* The argc value from the jump table is passed in
     for the number of iterations */
  for (int i = argc; i >= 1; i--) if (!verify_ptr (&esp[i])) return false;
  return true;
}

/* Checks whether a given pointer is valid for use */
static bool
verify_ptr (const void *ptr)
{
  /* Verifies the address in user space and page directory */
  if (ptr != NULL && 
      is_user_vaddr (ptr) && 
      pagedir_get_page (active_pd (), ptr) != NULL) 
    return true;
  else 
    return false;
}

/* Invokes the corresponding function of the syscall number
   by using the jump table. The args must be verified in prior */
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
void
syscall_exit (int status)
{
	struct thread *cur = thread_current ();

	/* Retrieve name of process. */
	char name[MAX_PROCESS_NAME_LENGTH];
	strlcpy (name, cur->name, strlen (thread_current ()->name) + 1);
	
	/* Generate output string. */
	uint8_t output_buffer_size = MAX_PROCESS_NAME_LENGTH + 15; 
	char output_buffer[output_buffer_size];
  int chars_written = snprintf (output_buffer, 
                                output_buffer_size, 
                                "%s: exit(%d)\n", name, status);
	ASSERT (chars_written != 0);
	write_to_console (output_buffer, strlen (output_buffer));

	/* Set exit status and call sema up. For process_wait. */

	/* Acquire lock to prevent race conditions between process writing to the 
	 * struct child, and its parent deallocating that struct when exiting. */
	lock_acquire (&cur->self_lock);
	struct child *child_ptr = cur->self_child_ptr;
	if (child_ptr != NULL) 
		{
			child_ptr->exit_status = status;
			lock_release (&cur->self_lock);
			sema_up (&child_ptr->sema);
      sema_up (&child_ptr->load_sema);
		}
	else lock_release (&cur->self_lock);

  /* Allow writes back to the executable */
  if (cur->executable)
    file_allow_write (thread_current ()->executable);

  thread_exit ();
}

/* SYS_EXEC */
static pid_t
syscall_exec (const char *cmd_line)
{
  if (!verify_ptr (cmd_line)) syscall_exit (-1);
  pid_t pid = process_execute (cmd_line);
  return pid; 
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
  /* Sanity check */
  if (file == NULL || !verify_ptr (file)) syscall_exit (-1);

  /* Acquires the lock to create the file, 
     and returns an error if the file is invalid */
  acquire_filesys ();
  
  bool result = filesys_create (file, initial_size);
  
  release_filesys ();
  
  return result;
}

/* SYS_REMOVE */
static bool
syscall_remove (const char *file UNUSED)
{
  /* Acquires the lock to remove the file, 
     and returns an error if the file is invalid */
  acquire_filesys ();

  bool remove = filesys_remove (file);
  
  release_filesys ();

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
  acquire_filesys ();

  struct file *file_to_open = filesys_open (file);

  release_filesys ();
  if (file_to_open == NULL) return -1;

  /* Create a new fd_item to pass into the hash table, and
     return an error if it failed to do so */
  struct fd_item *new_fd_item = malloc (sizeof (struct fd_item));
  if (new_fd_item == NULL) syscall_exit (-1);

  /* Acquires the lock to store the file_to_open in a new fd_item struct
     and push the struct into the current thread's hash table */
  acquire_filesys ();

  init_fd_item (new_fd_item, thread_current (), file_to_open);

  release_filesys ();

  return new_fd_item->fd;
}

/* SYS_FILESIZE */
static int
syscall_filesize (int fd UNUSED)
{
  /* Fetches the corresponding file mapped from the fd value
     Exit if the file is invalid */
  struct file *fp = get_file (&thread_current ()->hash_fd, fd);
  if (fp == NULL) syscall_exit (-1);

  /* Acquire the lock to fetch the file size */
  acquire_filesys ();
  
  int length = file_length (fp);
  
  release_filesys ();
  
  return length;
}

/* SYS_READ */
static int
syscall_read (int fd, void *buffer, unsigned size)
{
  /* Additional sanity check to verify the buffer, check if the fd value
     is below the maximum range of open files, and check if the fd value
     is not equal to 1 (STDOUT_FILENO) for syscall_read */
  if (buffer == NULL || 
      fd >= MAX_OPEN_FILES ||
      fd == STDOUT_FILENO ||
      !verify_ptr (buffer)) syscall_exit (-1);

  unsigned bytes_read;

  /* Acquires the lock to first check the fd value to either read from the
     console, or read from a specific file */
  acquire_filesys ();

  if (fd == STDIN_FILENO)  bytes_read = read_from_console (buffer, size);
  else                     bytes_read = read_from_file (fd, buffer, size);

  release_filesys ();

  /* Returns the total count of bytes read */
  return bytes_read;
}

/* Helper function for syscall_read to return the bytes read from the console */
static int
read_from_console (void *buffer, unsigned size)
{
  unsigned cnt = 0;

  /* While the count does not exceed the buffer size, read input from console */
  while (cnt != size) *(uint8_t *) (buffer + cnt++) = input_getc ();
  
  return cnt;
}

/* Helper function for syscall_read to return the bytes read from the file */
static int
read_from_file (int fd, void *buffer, unsigned size)
{
  /* Fetch the corresponding file mapped with the value of fd
     Read from the file if the file fetched is valid, otherwise return -1 */
  struct file *fp = get_file (&thread_current ()->hash_fd, fd);
  if (fp == NULL) return -1;
  int cnt = file_read (fp, buffer, size);

  return cnt;
}


/* SYS_WRITE */
static int
syscall_write (int fd, const void *buffer, unsigned size)
{
  /* Additional sanity check to verify the buffer, check if the fd value
     is below the maximum range of open files, and check if the fd value
     is not equal to 0 (STDIN_FILENO) for syscall_write */
  if (buffer == NULL       || 
      fd >= MAX_OPEN_FILES || 
      fd == STDIN_FILENO   ||
      !verify_ptr (buffer)) syscall_exit (-1);

  unsigned bytes_written;

  /* Acquires the lock to first check the fd value to either write to the
     console, or write to a specific file */
  acquire_filesys ();

  if (fd == STDOUT_FILENO) bytes_written = write_to_console (buffer, size);
  else                     bytes_written = write_to_file (fd, buffer, size);

  release_filesys ();


  /* Returns the total count of bytes written */
  return bytes_written;
}

/* Helper function for syscall_write to return the bytes written to the console */
static int
write_to_console (const char *buffer, unsigned size)
{
  int bytes_written = 0;

  /* Break the buffer into chunks of size MAX_CONSOLE_BUFFER_SIZE 
     if necessary and write them to the console */
  for (int32_t offset   = 0, bytes_remaining = size, bytes_to_write; 
       bytes_remaining  > 0;
       bytes_remaining -= MAX_CONSOLE_BUFFER_SIZE,
       offset          += MAX_CONSOLE_BUFFER_SIZE) 
    {
      /* Buffer into chunks */
      bytes_to_write = (bytes_remaining - offset <= MAX_CONSOLE_BUFFER_SIZE) ?
          bytes_remaining : MAX_CONSOLE_BUFFER_SIZE; 
      
      /* Writes to the console and increment the byte counts */
      putbuf (&buffer[offset], bytes_to_write);
      bytes_written += bytes_to_write;
    }

  return bytes_written;
}

/* Helper function for syscall_write to return the bytes written to the file */
static int
write_to_file (int fd, const char *buffer, unsigned size)
{
  /* Fetch the corresponding file mapped with the value of fd
     Write to the file if the file fetched is valid, otherwise return -1 */
  struct file *fp = get_file (&thread_current ()->hash_fd, fd);
  if (fp == NULL) return -1;
  return file_write (fp, buffer, size);
}

/* SYS_SEEK */
static void
syscall_seek (int fd UNUSED, unsigned position UNUSED)
{
  /* Fetch the corresponding file mapped with the value of fd
     Exit with -1 if fetch fails */
  struct file *fp = get_file (&thread_current ()->hash_fd, fd);
  if (fp == NULL) syscall_exit (-1);

  /* Acquires the lock to seek the given position of the file */
  acquire_filesys ();

  file_seek(fp, position);
  
  release_filesys ();
}

/* SYS_TELL */
static unsigned
syscall_tell (int fd UNUSED)
{
  /* Fetch the corresponding file mapped with the value of fd
     Exit with -1 if fetch fails */
  struct file *fp = get_file (&thread_current ()->hash_fd, fd);
  if (fp == NULL) syscall_exit (-1);
  
  /* Acquires the lock to tell the current position in the file */
  acquire_filesys ();
  
  int tell = file_tell (fp);
  
  release_filesys ();

  return tell;
}

/* SYS_CLOSE */
static void
syscall_close (int fd UNUSED)
{
  /* Fetch the corresponding file mapped with the value of fd
     Exit with -1 if fetch fails */
  struct fd_item *fd_item_ptr = get_fd_item (&thread_current ()->hash_fd, fd);
  if (fd_item_ptr == NULL) syscall_exit (-1);

  /* Acquires the lock to access files and free the fd_item struct allocated
     upon syscall_open before closing */
  acquire_filesys ();
  
  hash_delete (&thread_current ()->hash_fd, &fd_item_ptr->hash_elem);
  file_close (fd_item_ptr->file_ptr);
  free (fd_item_ptr);
  
  release_filesys ();
}
