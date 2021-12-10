#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

#include <lib/user/syscall.h>

#define MAX_CONSOLE_BUFFER_SIZE (400)
#define MAX_OPEN_FILES (32)
#define MAX_PROCESS_NAME_LENGTH (16)

typedef uint32_t (*syscall_0_args) (void);
typedef uint32_t (*syscall_1_args) (uint32_t);
typedef uint32_t (*syscall_2_args) (uint32_t, uint32_t);
typedef uint32_t (*syscall_3_args) (uint32_t, uint32_t, uint32_t);

void syscall_exit (int status);

struct syscall { void *syscall_ptr; int argc; };

void syscall_init (void);

#endif /* userprog/syscall.h */
