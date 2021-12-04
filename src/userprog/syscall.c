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
#include "devices/input.h"

static void     syscall_handler (struct intr_frame *f);
static uint32_t invoke_function (const void *syscall_ptr, 
                                 int argc, 
                                 const uint32_t *esp);

/* Pointer verification helpers */
static bool verify_ptr            (const void *ptr);
static bool verify_ptr_privileged (const void *ptr, bool write);
static bool verify_buffer         (const void *buffer, int size, bool write);
static bool verify_args           (int argc, const uint32_t *esp);

/* System calls */
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
static mapid_t  syscall_mmap     (int fd, void *addr);
static void     syscall_munmap   (mapid_t mapping);

/* Filesystem interaction helpers */
static int  read_from_console    (void *buffer, unsigned size);
static int  read_from_file       (int fd, void *buffer, unsigned size);
static int  write_to_console     (const char *buffer, unsigned size);
static int  write_to_file        (int fd, const char *buffer, unsigned size);

/* A jump table that contains a function pointer and the number of arguments */
static struct syscall 
syscall_func_map[] = 
  {
    {&syscall_halt,     .argc = 0}, 
    {&syscall_exit,     .argc = 1}, 
    {&syscall_exec,     .argc = 1}, 
    {&syscall_wait,     .argc = 1}, 
    {&syscall_create,   .argc = 2}, 
    {&syscall_remove,   .argc = 1}, 
    {&syscall_open,     .argc = 1}, 
    {&syscall_filesize, .argc = 1}, 
    {&syscall_read,     .argc = 3}, 
    {&syscall_write,    .argc = 3}, 
    {&syscall_seek,     .argc = 2}, 
    {&syscall_tell,     .argc = 1}, 
    {&syscall_close,    .argc = 1}, 
    {&syscall_mmap,     .argc = 2}, 
    {&syscall_munmap,   .argc = 1}, 
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
  ASSERT (SYS_HALT <= syscall_no && syscall_no <= SYS_MUNMAP);

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

/* Verifies the given pointer, additionally if write is true but the page is 
   not writable we will return false. */
static bool 
verify_ptr_privileged (const void *ptr, bool write)
{
  /* Verifies the address in user space and page directory */
  if (ptr != NULL && 
      is_user_vaddr (ptr))
    {
      if (pagedir_get_page (active_pd (), ptr) == NULL) 
        {

          struct spte *spte_ptr = spt_find_entry (thread_current ()->spt_ptr, 
                                                  pg_round_down (ptr));

          if (spte_ptr == NULL || (write && !spte_ptr->writable)) 
              return false;
          else
            {
              // TODO: Pin this frame until syscall finished
              // TODO: Get around compiler optimisation to force 
              //       page fault, or make an artificial page fault
              /* Compiler will optimise out this de-reference unless it
                 is stored in a variable marked volatile. Intentionally
                 triggers a page fault to bring in the page. */
              volatile int garbage; 
              garbage = *((int *) ptr);
            }
        }
    }
  else 
      return false;

  return true;
}

/* Checks whether a given pointer is valid for use */
static bool
verify_ptr (const void *ptr)
{
  return verify_ptr_privileged (ptr, false);
}

/* Checks that the entire buffer (up to size) is in valid memory and returns 
   false if not */
static bool
verify_buffer (const void *buffer, int size, bool write) 
{
  /* Check that the first memory location in the buffer is valid, and that 
     every memory location on the end of a page + 1 (i.e. the next page over) 
     is also valid */
  void *buffer_top = buffer + size * sizeof (char);
  for (void *loc = buffer; 
       loc <= buffer_top; 
       loc = pg_round_up(loc) + 1) 
  {
    if (!verify_ptr_privileged (loc, write)) {
      return false;
    }
  }
  return true;
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

  /* Stores the file_to_open in a new fd_item struct and pushes
     the struct into the current thread's hash table */
  init_fd_item (new_fd_item, thread_current (), file_to_open);

  return new_fd_item->fd;
}

/* SYS_FILESIZE */
static int
syscall_filesize (int fd UNUSED)
{
  /* Fetches the corresponding file mapped from the fd value
     Exit if the file is invalid */
  struct file *fp = get_file (thread_current ()->hash_fd_ptr, fd);
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
  if (buffer == NULL       || 
      fd >= MAX_OPEN_FILES ||
      fd == STDOUT_FILENO  ||
      !verify_buffer (buffer, size, false)) syscall_exit (-1);

  unsigned bytes_read;

  /* Acquires the lock to first check the fd value to either read from the
     console, or read from a specific file */
  acquire_filesys ();

  if (fd == STDIN_FILENO) bytes_read = read_from_console (buffer, size);
  else                    bytes_read = read_from_file (fd, buffer, size);

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
  struct file *fp = get_file (thread_current ()->hash_fd_ptr, fd);
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
      !verify_buffer (buffer, strlen (buffer), true)) syscall_exit (-1);


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
  struct file *fp = get_file (thread_current ()->hash_fd_ptr, fd);
  if (fp == NULL) return -1;
  return file_write (fp, buffer, size);
}

/* SYS_SEEK */
static void
syscall_seek (int fd UNUSED, unsigned position UNUSED)
{
  /* Fetch the corresponding file mapped with the value of fd
     Exit with -1 if fetch fails */
  struct file *fp = get_file (thread_current ()->hash_fd_ptr, fd);
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
  struct file *fp = get_file (thread_current ()->hash_fd_ptr, fd);
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
  struct fd_item *fd_item_ptr = get_fd_item (thread_current ()->hash_fd_ptr, fd);
  if (fd_item_ptr == NULL) syscall_exit (-1);

  /* deletes the entry associated with the file from the hash table */
  hash_delete (thread_current ()->hash_fd_ptr, &fd_item_ptr->hash_elem);

  /* Acquires the lock to close file */
  acquire_filesys ();
  
  file_close (fd_item_ptr->file_ptr);
  
  release_filesys ();

  free (fd_item_ptr);
}

static mapid_t 
syscall_mmap (int fd, void *addr)
{
  /* Fail if addr is 0, we are attempting to write to reserved fds,
     or the addr is not page aligned */
  if (addr == 0           ||
      fd == STDIN_FILENO  ||
      fd == STDOUT_FILENO || 
      addr != pg_round_down (addr))
    return -1;

  struct thread *t_ptr = thread_current ();

  /* Fail if the file is not mapped to a file descriptor */
  struct file *file_ptr = get_file (t_ptr->hash_fd_ptr, fd);
  if (file_ptr == NULL) return -1;

  /* Fail if the file has length equal to 0 */
  off_t filesize = file_length (file_ptr);
  if (filesize == 0) return -1;

  /* Fail if the mmapped file will overflow the stack */
  void *mmap_top = addr + filesize;
  if ((uint32_t) mmap_top >= (uint32_t) STACK_LIMIT) return -1;

  /* Fail if mmapped file will overwrite any supplemental pages */
  for (void *loc = addr; 
       loc <= mmap_top; 
       loc = pg_round_up(loc) + 1) 
  {
    if (spt_find_entry (t_ptr->spt_ptr, loc) != NULL)
        return -1;
  }

  /* Add the mmapped file pages to our SPT */
  off_t bytes_remaining = filesize;
  off_t offset = 0;
  void *loc = addr;
  for (loc = addr; loc <= mmap_top; 
       loc              = pg_round_up(loc) + 1, 
       bytes_remaining -= PGSIZE, 
       offset          += PGSIZE) 
  {
    if (spt_add_entry (t_ptr->spt_ptr,
                       0,               /* No fid yet */
                       loc,
                       MMAP, 
                       file_ptr->inode, 
                       offset, 
                       (bytes_remaining > PGSIZE) ? PGSIZE : bytes_remaining, 
                       true)            /* Writable */
        == NULL) goto fail_1;
  }

  return 0;

fail_1:

  /* Remove all allocated spt entries associated with the mmapped file. */
  while (loc > mmap_top) 
      spt_remove_entry (t_ptr->spt_ptr, loc = pg_round_down (--loc));

  return -1;

}
static void 
syscall_munmap (mapid_t mapping UNUSED)
{
  // TODO: Implement
}
