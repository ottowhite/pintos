#ifndef THREADS_FIXED_POINT_H
#define THREADS_FIXED_POINT_H

#include <stdint.h>

/* The types of the fixed_point representation */
typedef int32_t fp32_t;

#define FRACTION_SEPARATOR 14

/* We will use the 17.14 fixed_point representation*/
#define F (1 << FRACTION_SEPARATOR)

#define convert_int_to_fp(n)                          \
        ((n) * (F))                                   \

#define convert_fp_to_int_towards_zero(x)             \
        ((x) / (F))                                   \

#define convert_fp_to_int_rounding(x)                 \
        ((x >= 0) ? (x + (F/2))/F : (F - (F/2))/F)    \

#define add_fp_and_fp(x, y)                           \
        ((x) + (y))                                   \

#define add_fp_and_int(x, n)                          \
        (x + ((n) * (F)))                             \

#define sub_fp_from_fp(x, y)                          \
        (x - y)                                       \

#define sub_int_from_fp(x, n)                         \
        (x - ((n) * (F)))                             \

#define mul_fp_by_fp(x, y)                            \
        (((int64_t) x) * (y) / (F))                   \

#define mul_fp_by_int(x, n)                           \
        ((x) * (n))                                   \

#define div_fp_by_fp(x, y)                            \
        (((int64_t) x) * (F) / (y))                   \

#define div_fp_by_int(x, n)                           \
        ((x) / (n))                                   \

#endif
