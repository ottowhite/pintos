#ifndef __LIB_KERNEL_FIXED_POINT_H
#define __LIB_KERNEL_FIXED_POINT_H

#include <stdint.h>

/* The types of the fixed_point representation */
#define INT32 int_32t
#define FP32  int_32t

/* We will use the 17.14 fixed_point representation*/
#define f (1 << 14)

#define convert_int_to_fp(n)                 \
        ((n) * (f))                          \

#define convert_fp_to_int_towards_zero(x)    \
        ((x) / (f))                          \

#define convert_fp_to_int_rounding(x)        \
        (x >= 0) ? (x + (f/2)) : (x - (f/2)) \

#define add_fp_and_fp(x, y)                  \
        (x) + (y)                            \

#define add_fp_and_int(x, y)                 \
        x + ((n) * (f))                      \

#define sub_fp_from_fp(x, y)                 \
        x - y                                \

#define sub_int_from_fp(x, n)                \
        x - ((n) * (f))                      \

#define mul_fp_by_fp(x, y)                   \
        ((int64_t) x) * (y) / (f)            \

#define mul_fp_by_int(x, n)                  \
        (x) * (n)                            \

#define div_fp_by_fp(x, y)                   \
        ((int64_t) x) * (f) / (y)            \

#define div_fp_by_int(x, n)                  \
        (x) / (n)                            \

#endif