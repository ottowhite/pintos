#include "userprog/exception.h"
#include <inttypes.h>
#include <stdio.h>
#include "userprog/gdt.h"
#include "userprog/syscall.h"
#include "userprog/process.h"
#include "threads/vaddr.h"
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "filesys/filesys.h"
#include "vm/spt.h"
#include "vm/ft.h"

/* Number of page faults processed. */
static long long page_fault_cnt;

static void kill (struct intr_frame *);
static void page_fault (struct intr_frame *);

/* Page fault handler helpers */
static bool attempt_stack_growth (void *esp, const void *fault_addr);
static bool attempt_frame_load (struct spte *spte_ptr, bool left_pinned);

/* Registers handlers for interrupts that can be caused by user
   programs.

   In a real Unix-like OS, most of these interrupts would be
   passed along to the user process in the form of signals, as
   described in [SV-386] 3-24 and 3-25, but we don't implement
   signals.  Instead, we'll make them simply kill the user
   process.

   Page faults are an exception.  Here they are treated the same
   way as other exceptions, but this will need to change to
   implement virtual memory.

   Refer to [IA32-v3a] section 5.15 "Exception and Interrupt
   Reference" for a description of each of these exceptions. */
void
exception_init (void) 
{
  /* These exceptions can be raised explicitly by a user program,
     e.g. via the INT, INT3, INTO, and BOUND instructions.  Thus,
     we set DPL==3, meaning that user programs are allowed to
     invoke them via these instructions. */
  intr_register_int (3, 3, INTR_ON, kill, "#BP Breakpoint Exception");
  intr_register_int (4, 3, INTR_ON, kill, "#OF Overflow Exception");
  intr_register_int (5, 3, INTR_ON, kill, "#BR BOUND Range Exceeded Exception");

  /* These exceptions have DPL==0, preventing user processes from
     invoking them via the INT instruction.  They can still be
     caused indirectly, e.g. #DE can be caused by dividing by
     0.  */
  intr_register_int (0, 0, INTR_ON, kill, "#DE Divide Error");
  intr_register_int (1, 0, INTR_ON, kill, "#DB Debug Exception");
  intr_register_int (6, 0, INTR_ON, kill, "#UD Invalid Opcode Exception");
  intr_register_int (7, 0, INTR_ON, kill, "#NM Device Not Available Exception");
  intr_register_int (11, 0, INTR_ON, kill, "#NP Segment Not Present");
  intr_register_int (12, 0, INTR_ON, kill, "#SS Stack Fault Exception");
  intr_register_int (13, 0, INTR_ON, kill, "#GP General Protection Exception");
  intr_register_int (16, 0, INTR_ON, kill, "#MF x87 FPU Floating-Point Error");
  intr_register_int (19, 0, INTR_ON, kill, "#XF SIMD Floating-Point Exception");

  /* Most exceptions can be handled with interrupts turned on.
     We need to disable interrupts for page faults because the
     fault address is stored in CR2 and needs to be preserved. */
  intr_register_int (14, 0, INTR_OFF, page_fault, "#PF Page-Fault Exception");
}

/* Prints exception statistics. */
void
exception_print_stats (void) 
{
  printf ("Exception: %lld page faults\n", page_fault_cnt);
}

/* Handler for an exception (probably) caused by a user process. */
static void
kill (struct intr_frame *f) 
{
  /* This interrupt is one (probably) caused by a user process.
     For example, the process might have tried to access unmapped
     virtual memory (a page fault).  For now, we simply kill the
     user process.  Later, we'll want to handle page faults in
     the kernel.  Real Unix-like operating systems pass most
     exceptions back to the process via signals, but we don't
     implement them. */
     
  /* The interrupt frame's code segment value tells us where the
     exception originated. */
  switch (f->cs)
    {
    case SEL_UCSEG:
      /* User's code segment, so it's a user exception, as we
         expected.  Kill the user process.  */
      printf ("%s: dying due to interrupt %#04x (%s).\n",
              thread_name (), f->vec_no, intr_name (f->vec_no));
      intr_dump_frame (f);
      thread_exit (); 

    case SEL_KCSEG:
      /* Kernel's code segment, which indicates a kernel bug.
         Kernel code shouldn't throw exceptions.  (Page faults
         may cause kernel exceptions--but they shouldn't arrive
         here.)  Panic the kernel to make the point.  */
      intr_dump_frame (f);
      PANIC ("Kernel bug - unexpected interrupt in kernel"); 

    default:
      /* Some other code segment?  
         Shouldn't happen.  Panic the kernel. */
      printf ("Interrupt %#04x (%s) in unknown segment %04x\n",
             f->vec_no, intr_name (f->vec_no), f->cs);
      PANIC ("Kernel bug - this shouldn't be possible!");
    }
}

void
page_fault_trigger (const void *fault_addr, void *esp, bool not_present, 
                    bool write, bool user, bool left_pinned)
{
  /* Turn interrupts back on (they were only off so that we could
     be assured of reading CR2 before it changed). */
  intr_enable ();

  struct spte *spte_ptr = spt_find_entry (thread_current ()->spt_ptr, 
                                          pg_round_down (fault_addr));

  if (spte_ptr != NULL)
    {
      if ((write && !spte_ptr->writable) ||
          !attempt_frame_load (spte_ptr, left_pinned))
        goto fail;
    }
  else if (!attempt_stack_growth (esp, fault_addr)) 
      goto fail;

  return;

fail:
  /* Useful for debugging */
  // printf ("Page fault at %p: %s error %s page in %s context.\n",
  //     fault_addr,
  //     not_present ? "not present" : "rights violation",
  //     write ? "writing" : "reading",
  //     user ? "user" : "kernel");
  page_fault_cnt++;
  if (filesys_locked ()) release_filesys ();
  syscall_exit (-1);
}

/* Page fault handler.  This is a skeleton that must be filled in
   to implement virtual memory.  Some solutions to task 2 may
   also require modifying this code.

   At entry, the address that faulted is in CR2 (Control Register
   2) and information about the fault, formatted as described in
   the PF_* macros in exception.h, is in F's error_code member.  The
   example code here shows how to parse that information.  You
   can find more information about both of these in the
   description of "Interrupt 14--Page Fault Exception (#PF)" in
   [IA32-v3a] section 5.15 "Exception and Interrupt Reference". */
static void
page_fault (struct intr_frame *f_ptr) 
{

  /* Obtain faulting address, the virtual address that was
     accessed to cause the fault.  It may point to code or to
     data.  It is not necessarily the address of the instruction
     that caused the fault (that's f->eip).
     See [IA32-v2a] "MOV--Move to/from Control Registers" and
     [IA32-v3a] 5.15 "Interrupt 14--Page Fault Exception
     (#PF)". */
  void *fault_addr;  /* Fault address. */
  asm ("movl %%cr2, %0" : "=r" (fault_addr));

  /* Determine cause. */
  /* True: not-present page, false: writing r/o page. */
  bool not_present = (f_ptr->error_code & PF_P) == 0;
  /* True: access was write, false: access was read. */
  bool write       = (f_ptr->error_code & PF_W) != 0;
  /* True: access by user, false: access by kernel. */
  bool user        = (f_ptr->error_code & PF_U) != 0;

  page_fault_trigger (fault_addr, f_ptr->esp, not_present, write, user, false);
}

/* Attempts to load the given frame from an spte entry */
static bool
attempt_frame_load (struct spte *spte_ptr, bool left_pinned)
{
  /* Returns null if read failed, obtaining frame, or allocating fte failed */
  acquire_ft ();
  struct fte *fte_ptr = ft_get_frame (spte_ptr);
  release_ft ();
  if (fte_ptr == NULL) 
      goto fail_1;

  /* Returns false if installation failed, frame left pinned */
  acquire_ft ();
  if (!ft_install_frame (spte_ptr, fte_ptr)) 
      goto fail_2;

  /* Leave the frame pinned if left_pinned, for usage in syscall handlers */
  ASSERT (fte_ptr->pin_cnt >= 0);
  if (left_pinned) fte_ptr->pin_cnt++;
  release_ft ();

  return true;

  fail_2: spt_propagate_removal (thread_current ()->spt_ptr, spte_ptr->uaddr);
          release_ft ();
  fail_1: return false;
}

/* Grows stack if fault_addr was a valid stack access, fails otherwise */
static bool
attempt_stack_growth (void *esp, const void *fault_addr)
{
  /* Fault was a valid stack access, we need to bring in a new page */
  // DELETE: CERTAINLY WHERE FAILURE HAPPENS

  /* esp must be between STACK_LIMIT and PHYS_BASE */
  if (fault_addr <  STACK_LIMIT ||
      fault_addr >= PHYS_BASE) 
      goto fail;

  /* esp can only be 4, or 32 bytes above fault_addr, other amounts invalid */
  if (fault_addr <  esp &&
      fault_addr != esp - 4 && 
      fault_addr != esp - 32) 
      goto fail;

  struct spte *spte_ptr = spt_add_entry (thread_current ()->spt_ptr, 
      pg_round_down (fault_addr), STACK, NULL, 0, PGSIZE, true);

  if (spte_ptr != NULL && attempt_frame_load (spte_ptr, false)) 
      return true;

fail:
  // printf ("esp = %p, fault_addr = %p\n", esp, fault_addr);
  return false;
}
