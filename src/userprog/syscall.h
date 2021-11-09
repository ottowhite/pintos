#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H
#include <lib/user/syscall.h>

typedef uint32_t (*function_0_args) (void);
typedef uint32_t (*function_1_args) (void *);
typedef uint32_t (*function_2_args) (void *, void *);
typedef uint32_t (*function_3_args) (void *, void *, void *);

struct function { void *function_ptr; int argc; };

void syscall_init (void);

#endif /* userprog/syscall.h */
