#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H
#include <lib/user/syscall.h>

void syscall_init (void);

static struct function { void *function_ptr; int argc; };

typedef uint32_t (*function_0_args) ();
typedef uint32_t (*function_1_args) (void *);
typedef uint32_t (*function_2_args) (void *, void *);
typedef uint32_t (*function_3_args) (void *, void *, void *);

static void     verify_ptr      (void *ptr);
static void     verify_args     (int argc, void *esp);
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

#endif /* userprog/syscall.h */
