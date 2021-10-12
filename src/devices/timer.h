#ifndef DEVICES_TIMER_H
#define DEVICES_TIMER_H

#include <round.h>
#include <stdint.h>
#include "threads/synch.h"

/* Number of timer interrupts per second. */
#define TIMER_FREQ 100

/* For use in a priority-queue-like list that stores sleeping threads
 * and when to wake them. Elements are created in timer.c:time_sleep
 * and removed in timer.c:timer_interrupt if ticks >= wake_time_ticks
 */
struct sleeping_thread {
  struct thread *thread_ptr;
  int64_t wake_time_ticks;
  struct list_elem sleepelem;
  struct semaphore sleeping_sema;
};

/* Initialise a sleeping thread to wake at the specified time,
 * Must then be added to the sleeping_thread_list to be woken 
 * Also 0 initialises a semaphore in the sleeping thread struct
 */
void sleeping_thread_init(struct sleeping_thread *sleeping_thread_ptr,
                          struct thread *thread_ptr,
                          int64_t wake_time_ticks);

/* list_less_func for ordered sleeping thread insertions */
bool thread_wakes_before(const struct list_elem *a_ptr,
                         const struct list_elem *b_ptr,
                         void *aux);

void timer_init (void);
void timer_calibrate (void);

int64_t timer_ticks (void);
int64_t timer_elapsed (int64_t);

/* Sleep and yield the CPU to other threads. */
void timer_sleep (int64_t ticks);
void timer_msleep (int64_t milliseconds);
void timer_usleep (int64_t microseconds);
void timer_nsleep (int64_t nanoseconds);

/* Busy waits. */
void timer_mdelay (int64_t milliseconds);
void timer_udelay (int64_t microseconds);
void timer_ndelay (int64_t nanoseconds);

void timer_print_stats (void);

#endif /* devices/timer.h */
