#ifndef __LIB_SYSCALL_NR_H
#define __LIB_SYSCALL_NR_H
#include <lib/user/syscall.h>

static struct function { void *function_ptr; int argc; };

typedef uint32_t (*function_0_args) ();
typedef uint32_t (*function_1_args) (void *);
typedef uint32_t (*function_2_args) (void *, void *);
typedef uint32_t (*function_3_args) (void *, void *, void *);

static void verify_ptr (void *ptr);
static void verify_args (int argc, void *esp);
static uint32_t invoke_function (void *function_ptr, int argc, void *esp);

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

/* System call numbers. */
enum 
  {
    /* Tasks 2 and later. */
    SYS_HALT      = 0,     /* Halt the operating system. */
    SYS_EXIT      = 1,     /* Terminate this process. */
    SYS_EXEC      = 2,     /* Start another process. */
    SYS_WAIT      = 3,     /* Wait for a child process to die. */
    SYS_CREATE    = 4,     /* Create a file. */
    SYS_REMOVE    = 5,     /* Delete a file. */
    SYS_OPEN      = 6,     /* Open a file. */
    SYS_FILESIZE  = 7,     /* Obtain a file's size. */
    SYS_READ      = 8,     /* Read from a file. */
    SYS_WRITE     = 9,     /* Write to a file. */
    SYS_SEEK      = 10,    /* Change position in a file. */
    SYS_TELL      = 11,    /* Report current position in a file. */
    SYS_CLOSE     = 12,    /* Close a file. */

    /* Task 3 and optionally task 4. */
    SYS_MMAP,                   /* Map a file into memory. */
    SYS_MUNMAP,                 /* Remove a memory mapping. */

    /* Task 4 only. */
    SYS_CHDIR,                  /* Change the current directory. */
    SYS_MKDIR,                  /* Create a directory. */
    SYS_READDIR,                /* Reads a directory entry. */
    SYS_ISDIR,                  /* Tests if a fd represents a directory. */
    SYS_INUMBER                 /* Returns the inode number for a fd. */
  };

#endif /* lib/syscall-nr.h */
