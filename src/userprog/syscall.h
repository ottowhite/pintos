#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H
#include <lib/user/syscall.h>

#define MAX_CONSOLE_BUFFER_SIZE (400)
#define MAX_OPEN_FILES (16)

/* code was taken from stackoverflow.com/questions/3437404/min-and-max-in-c */
#define min(a,b) \
   ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
     _a < _b ? _a : _b; })

typedef uint32_t (*function_0_args) (void);
typedef uint32_t (*function_1_args) (void *);
typedef uint32_t (*function_2_args) (void *, void *);
typedef uint32_t (*function_3_args) (void *, void *, void *);

struct function { void *function_ptr; int argc; };

void syscall_init (void);

#endif /* userprog/syscall.h */
