#include "threads/thread.h"
#include <debug.h>
#include <stddef.h>
#include <random.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include "threads/flags.h"
#include "threads/fixed-point.h"
#include "threads/interrupt.h"
#include "threads/intr-stubs.h"
#include "threads/palloc.h"
#include "threads/switch.h"
#include "threads/synch.h"
#include "threads/vaddr.h"
#include "devices/timer.h"
#ifdef USERPROG
#include "userprog/process.h"
#endif

/* Random value for struct thread's `magic' member.
   Used to detect stack overflow.  See the big comment at the top
   of thread.h for details. */
#define THREAD_MAGIC 0xcd6abf4b

/* List of processes in THREAD_READY state, that is, processes
   that are ready to run but not actually running. */
static struct list ready_list;

/* List of all processes.  Processes are added to this list
   when they are first scheduled and removed when they exit. */
static struct list all_list;

/* Task 1 */
static struct list sleep_list;

/* Idle thread. */
static struct thread *idle_thread;

/* Initial thread, the thread running init.c:main(). */
static struct thread *initial_thread;

/* Lock used by allocate_tid(). */
static struct lock tid_lock;

/* Stack frame for kernel_thread(). */
struct kernel_thread_frame 
  {
    void *eip;                  /* Return address. */
    thread_func *function;      /* Function to call. */
    void *aux;                  /* Auxiliary data for function. */
  };

/* Statistics. */
static long long idle_ticks;    /* # of timer ticks spent idle. */
static long long kernel_ticks;  /* # of timer ticks in kernel threads. */
static long long user_ticks;    /* # of timer ticks in user programs. */

/* Scheduling. */
#define TIME_SLICE 4            /* # of timer ticks to give each thread. */
static unsigned thread_ticks;   /* # of timer ticks since last yield. */

/* If false (default), use round-robin scheduler.
   If true, use multi-level feedback queue scheduler.
   Controlled by kernel command-line option "-o mlfqs". */
bool thread_mlfqs;

static void kernel_thread (thread_func *, void *aux);

static void idle (void *aux UNUSED);
static struct thread *running_thread (void);
static struct thread *next_thread_to_run (void);
static void init_thread (struct thread *, const char *name, int priority);
static bool is_thread (struct thread *) UNUSED;
static void *alloc_frame (struct thread *, size_t size);
static void schedule (void);
void thread_schedule_tail (struct thread *prev);
static tid_t allocate_tid (void);
static void update_recent_cpu (struct thread *t); //updates thread's recent cpu mlfqs
static void update_load_avg (void);               //updates system load average mlfqs
static void thread_update_priority_mlfqs (struct thread *t); //updates a threads priority mlfqs
static void thread_set_thread_priority (struct thread *, int);  //manually set priority mlfqs

static fixed_point load_avg; 			/* current system load average, in fixed point form */

/* Initializes the threading system by transforming the code
   that's currently running into a thread.  This can't work in
   general and it is possible in this case only because loader.S
   was careful to put the bottom of the stack at a page boundary.

   Also initializes the run queue and the tid lock.

   After calling this function, be sure to initialize the page
   allocator before trying to create any threads with
   thread_create().

   It is not safe to call thread_current() until this function
   finishes. */
void
thread_init (void) 
{
  ASSERT (intr_get_level () == INTR_OFF);

  lock_init (&tid_lock);
  list_init (&ready_list);
  list_init (&all_list);
  list_init (&sleep_list);

  /* Set up a thread structure for the running thread. */
  initial_thread = running_thread ();
  init_thread (initial_thread, "main", PRI_DEFAULT);
  initial_thread->status = THREAD_RUNNING;
  initial_thread->tid = allocate_tid ();

  /* Task 1: initialise time (in ticks) when to wake the thread with sentinel
     value to indicate that it is not asleep.*/
  initial_thread->wake_ticks = 0;

  if (thread_mlfqs) {
    /* Set the inital values of thread to 0 */
    initial_thread->nice = 0;
    initial_thread->recent_cpu.value = 0;
    load_avg.value = 0;
    thread_update_priority_mlfqs (initial_thread);
  }

}

/* Starts preemptive thread scheduling by enabling interrupts.
   Also creates the idle thread. */
void
thread_start (void) 
{
  /* Create the idle thread. */
  struct semaphore idle_started;
  sema_init (&idle_started, 0);
  thread_create ("idle", PRI_MIN, idle, &idle_started);

  /* Start preemptive thread scheduling. */
  intr_enable ();

  /* Wait for the idle thread to initialize idle_thread. */
  sema_down (&idle_started);
}

/* Called by the timer interrupt handler at each timer tick.
   Thus, this function runs in an external interrupt context.
   N.B. Interrupts are disabled, minimise computation. */
void
thread_tick (void) 
{
  struct thread *t = thread_current ();

  /* Update statistics. */
  if (t == idle_thread)
    idle_ticks++;
#ifdef USERPROG
  else if (t->pagedir != NULL)
    user_ticks++;
#endif
  else
  {
	  if (thread_mlfqs)
    //increment recent_cpu on running thread
    add_int (&(t->recent_cpu), 1);
    kernel_ticks++;
  }
  /* Enforce preemption. */
  if (++thread_ticks >= TIME_SLICE)
    intr_yield_on_return ();

  if (thread_mlfqs)
  {
    bool hundred_ticks = timer_ticks() % TIMER_FREQ == 0;
    if (hundred_ticks)
      update_load_avg ();
    struct list_elem *each_thread;
	  bool updated_priority = (timer_ticks() % TIME_SLICE == 0);
    for (each_thread = list_begin (&all_list) ; each_thread != list_end (&all_list) ;
         each_thread = list_next (each_thread))
    {
      if (updated_priority)
      {
        struct thread *this_thread = list_entry (each_thread, struct thread, elem);
        //updates recent_cpu every hundred ticks
        if (hundred_ticks)
        {
          update_recent_cpu (this_thread);
        }
        thread_update_priority_mlfqs (this_thread);
      }
    }
	  if (updated_priority)
      //if priorities have been altered, resort the ready list queue
        intr_yield_on_return();
  }

  // Check the list of sleeping threads to see if any are ready to wake up.
  while(!list_empty (&sleep_list))
  {
    /* Find out which thread is next to wake up. */
    struct thread *next_to_wake = list_entry (list_front (&sleep_list),
                                              struct thread,
                                              elem);

    if (timer_ticks () >= next_to_wake->wake_ticks)
    {
      list_pop_front (&sleep_list);

      // Reset time when the thread should be woken to n/a
      next_to_wake->wake_ticks = 0;

      thread_unblock(next_to_wake);
    }
    else
    {
      break;
    }
  }
}

/* Used to order list of sleeping threads,
   order lexicographically by (wake_ticks increasing, priority decreasing)
   returns true if a comes before b */
static bool
compare_wake_ticks (const struct list_elem *a,
                    const struct list_elem *b,
                    void *aux UNUSED)
{
  struct thread *t1 = list_entry (a, struct thread, elem);
  struct thread *t2 = list_entry (b, struct thread, elem);
  if(t1->wake_ticks < t2->wake_ticks)
  {
    return true;
  }
  else if(t1->wake_ticks == t2->wake_ticks)
  {
    // What if priority changes while on the list?
    return t1->priority > t2->priority;
  }
  return false;
}

/* Sets the current thread to sleep until timer_ticks() >= wake_ticks */
void
thread_sleep (int64_t wake_ticks)
{
  struct thread *t = thread_current ();

  // Record the time (in ticks) when the thread should wake.
  t->wake_ticks = wake_ticks;

  // Add to the list of sleeping threads, threads which will wake soonest should
  // be at the front of the list.
  list_insert_ordered (&sleep_list, &t->elem,
                       compare_wake_ticks, NULL);

  // Interrupts must be disabled to block a thread.
  enum intr_level old_level = intr_disable ();

  // Block the calling thread.
  thread_block ();

  intr_set_level (old_level);

}

/* Prints thread statistics. */
void
thread_print_stats (void) 
{
  printf ("Thread: %lld idle ticks, %lld kernel ticks, %lld user ticks\n",
          idle_ticks, kernel_ticks, user_ticks);
}

/* Creates a new kernel thread named NAME with the given initial
   PRIORITY, which executes FUNCTION passing AUX as the argument,
   and adds it to the ready queue.  Returns the thread identifier
   for the new thread, or TID_ERROR if creation fails.

   If thread_start() has been called, then the new thread may be
   scheduled before thread_create() returns.  It could even exit
   before thread_create() returns.  Contrariwise, the original
   thread may run for any amount of time before the new thread is
   scheduled.  Use a semaphore or some other form of
   synchronization if you need to ensure ordering.

   The code provided sets the new thread's `priority' member to
   PRIORITY, but no actual priority scheduling is implemented.
   Priority scheduling is the goal of Problem 1-3. */
tid_t
thread_create (const char *name, int priority,
               thread_func *function, void *aux) 
{
  struct thread *t;
  struct kernel_thread_frame *kf;
  struct switch_entry_frame *ef;
  struct switch_threads_frame *sf;
  tid_t tid;
  enum intr_level old_level;

  ASSERT (function != NULL);

  /* Allocate thread. */
  t = palloc_get_page (PAL_ZERO);
  if (t == NULL)
    return TID_ERROR;

  /* Initialize thread. */
  init_thread (t, name, priority);
  tid = t->tid = allocate_tid ();

  /* Prepare thread for first run by initializing its stack.
     Do this atomically so intermediate values for the 'stack' 
     member cannot be observed. */
  old_level = intr_disable ();

  /* Stack frame for kernel_thread(). */
  kf = alloc_frame (t, sizeof *kf);
  kf->eip = NULL;
  kf->function = function;
  kf->aux = aux;

  /* Stack frame for switch_entry(). */
  ef = alloc_frame (t, sizeof *ef);
  ef->eip = (void (*) (void)) kernel_thread;

  /* Stack frame for switch_threads(). */
  sf = alloc_frame (t, sizeof *sf);
  sf->eip = switch_entry;
  sf->ebp = 0;

  /* Task 1: initialise time (in ticks) when to wake the thread with sentinel
     value to indicate that it is not asleep.*/
  t->wake_ticks = 0;

  if (thread_mlfqs) {
    /* Set the inital mlfqs values of thread */
    t->nice = 0;
    t->recent_cpu.value = 0;
    thread_update_priority_mlfqs (t); 
  }
  
  struct thread *curr = thread_current();
  if (curr != NULL ) {
    t->nice = curr->nice;
  }
  else
    t->nice = 0;

  intr_set_level (old_level);

  /* Add to run queue. */
  thread_unblock (t);

  return tid;
}

/* Puts the current thread to sleep.  It will not be scheduled
   again until awoken by thread_unblock().

   This function must be called with interrupts turned off.  It
   is usually a better idea to use one of the synchronization
   primitives in synch.h. */
void
thread_block (void) 
{
  ASSERT (!intr_context ());
  ASSERT (intr_get_level () == INTR_OFF);

  thread_current ()->status = THREAD_BLOCKED;
  schedule ();
}

/* Transitions a blocked thread T to the ready-to-run state.
   This is an error if T is not blocked.  (Use thread_yield() to
   make the running thread ready.)

   This function does not preempt the running thread.  This can
   be important: if the caller had disabled interrupts itself,
   it may expect that it can atomically unblock a thread and
   update other data. */
void
thread_unblock (struct thread *t) 
{
  enum intr_level old_level;

  ASSERT (is_thread (t));

  old_level = intr_disable ();
  ASSERT (t->status == THREAD_BLOCKED);
  t->status = THREAD_READY; 
  list_insert_ordered (&ready_list, &t->elem, compare_priority_less, NULL);
  t->status = THREAD_READY;
  intr_set_level (old_level);
  if (t->priority > thread_current ()->priority && thread_current () != idle_thread)
  {
    if (intr_context ())
      intr_yield_on_return ();
    else
      thread_yield ();
  }
}

/* Returns the name of the running thread. */
const char *
thread_name (void) 
{
  return thread_current ()->name;
}

/* Returns the running thread.
   This is running_thread() plus a couple of sanity checks.
   See the big comment at the top of thread.h for details. */
struct thread *
thread_current (void) 
{
  struct thread *t = running_thread ();
  
  /* Make sure T is really a thread.
     If either of these assertions fire, then your thread may
     have overflowed its stack.  Each thread has less than 4 kB
     of stack, so a few big automatic arrays or moderate
     recursion can cause stack overflow. */
  ASSERT (is_thread (t));
  ASSERT (t->status == THREAD_RUNNING);

  return t;
}

/* Returns the running thread's tid. */
tid_t
thread_tid (void) 
{
  return thread_current ()->tid;
}

/* Deschedules the current thread and destroys it.  Never
   returns to the caller. */
void
thread_exit (void) 
{
  ASSERT (!intr_context ());

#ifdef USERPROG
  process_exit ();
#endif

  /* Remove thread from all threads list, set our status to dying,
     and schedule another process.  That process will destroy us
     when it calls thread_schedule_tail(). */
  intr_disable ();
  list_remove (&thread_current()->allelem);
  thread_current ()->status = THREAD_DYING;
  schedule ();
  NOT_REACHED ();
}

/* Yields the CPU.  The current thread is not put to sleep and
   may be scheduled again immediately at the scheduler's whim. */
void
thread_yield (void) 
{
  struct thread *cur = thread_current ();
  enum intr_level old_level;
  
  ASSERT (!intr_context ());

  old_level = intr_disable ();
  if (cur != idle_thread)
    list_insert_ordered (&ready_list, &cur->elem, compare_priority_less, NULL);
  cur->status = THREAD_READY;
  schedule ();
  intr_set_level (old_level);
}

/* Invoke function 'func' on all threads, passing along 'aux'.
   This function must be called with interrupts off. */
void
thread_foreach (thread_action_func *func, void *aux)
{
  struct list_elem *e;

  ASSERT (intr_get_level () == INTR_OFF);

  for (e = list_begin (&all_list); e != list_end (&all_list);
       e = list_next (e))
    {
      struct thread *t = list_entry (e, struct thread, allelem);
      func (t, aux);
    }
}

/* Sets the current thread's priority to NEW_PRIORITY. */
void
thread_set_priority (int new_priority) 
{
  thread_current ()->priority = new_priority;

  //Order the ready list.
  //check if the current thread has the highest priority.
  //if not, yield to the next thread.
  //add current thread to the list and order the list.

  if (!list_empty (&ready_list) && list_entry (list_back (&ready_list), struct thread, elem)->priority > new_priority)
    thread_yield();
}

/* Returns the current thread's priority. */
int
thread_get_priority (void) 
{
  return thread_current ()->priority;
}

void
thread_set_thread_priority (struct thread *t, int new_priority )
{
  t->priority = new_priority;
}

/* Sets the current thread's nice value to NICE. */
void
thread_set_nice (int new_nice) 
{
  struct thread *curr = thread_current ();
  curr->nice = new_nice;
  thread_update_priority_mlfqs (curr);
  
}

/* Returns the current thread's nice value. */
int
thread_get_nice (void) 
{
  return thread_current ()->nice;
}

/* Returns 100 times the system load average. */
int
thread_get_load_avg (void) 
{
  return 100*convert_to_int (&load_avg);
}

/* Returns 100 times the current thread's recent_cpu value. */
int
thread_get_recent_cpu (void) 
{
  return 100*(convert_to_int (&(thread_current()->recent_cpu)));
}

/* Idle thread.  Executes when no other thread is ready to run.

   The idle thread is initially put on the ready list by
   thread_start().  It will be scheduled once initially, at which
   point it initializes idle_thread, "up"s the semaphore passed
   to it to enable thread_start() to continue, and immediately
   blocks.  After that, the idle thread never appears in the
   ready list.  It is returned by next_thread_to_run() as a
   special case when the ready list is empty. */
static void
idle (void *idle_started_ UNUSED) 
{
  struct semaphore *idle_started = idle_started_;
  idle_thread = thread_current ();
  sema_up (idle_started);

  for (;;) 
    {
      /* Let someone else run. */
      intr_disable ();
      thread_block ();

      /* Re-enable interrupts and wait for the next one.

         The `sti' instruction disables interrupts until the
         completion of the next instruction, so these two
         instructions are executed atomically.  This atomicity is
         important; otherwise, an interrupt could be handled
         between re-enabling interrupts and waiting for the next
         one to occur, wasting as much as one clock tick worth of
         time.

         See [IA32-v2a] "HLT", [IA32-v2b] "STI", and [IA32-v3a]
         7.11.1 "HLT Instruction". */
      asm volatile ("sti; hlt" : : : "memory");
    }
}

/* Function used as the basis for a kernel thread. */
static void
kernel_thread (thread_func *function, void *aux) 
{
  ASSERT (function != NULL);

  intr_enable ();       /* The scheduler runs with interrupts off. */
  function (aux);       /* Execute the thread function. */
  thread_exit ();       /* If function() returns, kill the thread. */
}

/* Returns the running thread. */
struct thread *
running_thread (void) 
{
  uint32_t *esp;

  /* Copy the CPU's stack pointer into `esp', and then round that
     down to the start of a page.  Because `struct thread' is
     always at the beginning of a page and the stack pointer is
     somewhere in the middle, this locates the curent thread. */
  asm ("mov %%esp, %0" : "=g" (esp));
  return pg_round_down (esp);
}

/* Returns true if T appears to point to a valid thread. */
static bool
is_thread (struct thread *t)
{
  return t != NULL && t->magic == THREAD_MAGIC;
}

/* Does basic initialization of T as a blocked thread named
   NAME. */
static void
init_thread (struct thread *t, const char *name, int priority)
{
  enum intr_level old_level;

  ASSERT (t != NULL);
  ASSERT (PRI_MIN <= priority && priority <= PRI_MAX);
  ASSERT (name != NULL);

  memset (t, 0, sizeof *t);
  t->status = THREAD_BLOCKED;
  strlcpy (t->name, name, sizeof t->name);
  t->stack = (uint8_t *) t + PGSIZE;
  t->priority = priority;
  t->magic = THREAD_MAGIC;

  old_level = intr_disable ();
  list_push_back (&all_list, &t->allelem);
  intr_set_level (old_level);
}

/* Allocates a SIZE-byte frame at the top of thread T's stack and
   returns a pointer to the frame's base. */
static void *
alloc_frame (struct thread *t, size_t size) 
{
  /* Stack data is always allocated in word-size units. */
  ASSERT (is_thread (t));
  ASSERT (size % sizeof (uint32_t) == 0);

  t->stack -= size;
  return t->stack;
}

/* Chooses and returns the next thread to be scheduled.  Should
   return a thread from the run queue, unless the run queue is
   empty.  (If the running thread can continue running, then it
   will be in the run queue.)  If the run queue is empty, return
   idle_thread. */
static struct thread *
next_thread_to_run (void) 
{
  if (list_empty (&ready_list))
    return idle_thread;
  else
    return list_entry (list_pop_back (&ready_list), struct thread, elem);
}

/* Completes a thread switch by activating the new thread's page
   tables, and, if the previous thread is dying, destroying it.

   At this function's invocation, we just switched from thread
   PREV, the new thread is already running, and interrupts are
   still disabled.  This function is normally invoked by
   thread_schedule() as its final action before returning, but
   the first time a thread is scheduled it is called by
   switch_entry() (see switch.S).

   It's not safe to call printf() until the thread switch is
   complete.  In practice that means that printf()s should be
   added at the end of the function.

   After this function and its caller returns, the thread switch
   is complete. */
void
thread_schedule_tail (struct thread *prev)
{
  struct thread *cur = running_thread ();
  
  ASSERT (intr_get_level () == INTR_OFF);

  /* Mark us as running. */
  cur->status = THREAD_RUNNING;

  /* Start new time slice. */
  thread_ticks = 0;

#ifdef USERPROG
  /* Activate the new address space. */
  process_activate ();
#endif

  /* If the thread we switched from is dying, destroy its struct
     thread.  This must happen late so that thread_exit() doesn't
     pull out the rug under itself.  (We don't free
     initial_thread because its memory was not obtained via
     palloc().) */
  if (prev != NULL && prev->status == THREAD_DYING && prev != initial_thread) 
    {
      ASSERT (prev != cur);
      palloc_free_page (prev);
    }
}

/* Schedules a new process.  At entry, interrupts must be off and
   the running process's state must have been changed from
   running to some other state.  This function finds another
   thread to run and switches to it.

   It's not safe to call printf() until thread_schedule_tail()
   has completed. */
static void
schedule (void) 
{
  struct thread *cur = running_thread ();
  struct thread *next = next_thread_to_run ();
  struct thread *prev = NULL;

  ASSERT (intr_get_level () == INTR_OFF);
  ASSERT (cur->status != THREAD_RUNNING);
  ASSERT (is_thread (next));

  if (cur != next)
    prev = switch_threads (cur, next);
  thread_schedule_tail (prev);
}

/* Returns a tid to use for a new thread. */
static tid_t
allocate_tid (void) 
{
  static tid_t next_tid = 1;
  tid_t tid;

  lock_acquire (&tid_lock);
  tid = next_tid++;
  lock_release (&tid_lock);

  return tid;
}

/* Offset of `stack' member within `struct thread'.
   Used by switch.S, which can't figure it out on its own. */
uint32_t thread_stack_ofs = offsetof (struct thread, stack);

/* Needed to be done for Task 1 */

/* 1. need to sort the ready list in terms of priority, in descending order.            
   2. when a thread is added to the ready list that has a higher priority than
      the current thread, the current thread should yield the processor to the 
      new thread.
   3. When threads are waiting for a lock, semaphore, etc. the highest priority
      waiting thread should be awakened first.
   4. threads can change priority at any time, but if it lowers itself, then it 
      yields the cpu. */

/* What is done:
   bullet point 1, 2 and 4. */

/* Task 1: Sorts the ready list in terms of priority in descending order, so 
   the thread with the highest priority is the head of the list. */

/* Return true if the priority of a is less than the priority of b. False
   otherwise */
bool
compare_priority_less (const struct list_elem *a, const struct list_elem *b, void *aux UNUSED)
{
  return list_entry(a, struct thread, elem)->priority 
         <= list_entry(b, struct thread, elem)->priority;
}

void
update_recent_cpu (struct thread *t)
{
  fixed_point old_cpu = { load_avg.value };
  fixed_point denominator = { load_avg.value };
  multiply_int (&old_cpu, 2);
  multiply_int (&denominator, 2);
  add_int (&denominator, 1);
  divide_fixed_point (&old_cpu, &denominator);
  multiply_fixed_point (&old_cpu, &(t->recent_cpu));
  add_int (&old_cpu, t->nice);
  t->recent_cpu.value = old_cpu.value;  
}

void
thread_update_priority_mlfqs (struct thread *t)
{
  fixed_point priority = { convert_to_fixed_point (PRI_MAX) };
  fixed_point recent_cpu = { t->recent_cpu.value };
  divide_int (&recent_cpu, 4);
  subtract_fixed_point (&priority, &recent_cpu);
  subtract_int (&priority, t->nice*2);
  int new_priority = convert_to_int (&priority);
  if (new_priority > PRI_MAX)
    new_priority = PRI_MAX;
  else if(new_priority < PRI_MIN)
    new_priority = PRI_MIN;
  t->priority = new_priority;
}

void
update_load_avg (void)
{
  fixed_point coefficient = { convert_to_fixed_point (59) };
  fixed_point ready_threads = { convert_to_fixed_point (1) };
  divide_int (&coefficient, 60);
  fixed_point old_load_avg = { load_avg.value };
  multiply_fixed_point (&old_load_avg, &coefficient);
  divide_int (&ready_threads, 60);
  multiply_int (&ready_threads, list_size (&ready_list));
  add_fixed_point (&old_load_avg, &ready_threads);
  load_avg.value = old_load_avg.value; 
}

