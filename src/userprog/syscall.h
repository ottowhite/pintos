#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H
#include <lib/user/syscall.h>

#define MAX_CONSOLE_BUFFER_SIZE (400)
#define MAX_OPEN_FILES (16)
#define FALLTHROUGH do { __attribute__ ((fallthrough)); } while (0)

typedef uint32_t (*function_0_args) (void);
typedef uint32_t (*function_1_args) (uint32_t);
typedef uint32_t (*function_2_args) (uint32_t, uint32_t);
typedef uint32_t (*function_3_args) (uint32_t, uint32_t, uint32_t);

struct function { void *function_ptr; int argc; };

void syscall_init (void);

#endif /* userprog/syscall.h */
