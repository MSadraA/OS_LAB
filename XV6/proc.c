#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"

#include "user_mgmt.h" 

#define MAX_PROC_INFO 64

struct spinlock print_lock;

struct proc_snapshot {
  char name[16];
  int pid;
  int state;
  int cal;
  int waiting_time;
  int deadline;
  int continous_time_to_run;
  int entering_time_to_the_fcfs_queue;
  int arrival_time_to_system;
};



typedef struct barber_duty
{
  struct spinlock barber_is_working;
  struct proc *barber_process;
  int is_barber_sleeping;
} barber_duty;
barber_duty barber;
typedef struct customer_duty
{
  struct spinlock modify_customer_queue;
  int number_of_customer;
  int waiting_customer[5];
  int waiting_customer_head;
  int waiting_customer_tail;
} customer_duty;
customer_duty customer;
int number_of_all_customer = 0;




typedef struct reader_writer
{
  struct spinlock critical_section;
  struct spinlock is_writer_waiting;
  struct spinlock waiting_reader;
  int counter;
  int active_reader_count;
  int active_writer_count;
  int number_of_waiting_writer;
  int number_of_waiting_reader;
  int waiting_writer_head;
  int waiting_writer_tail;
  int waiting_writer_pid[64];
  int waiting_reader_head;
  int waiting_reader_tail;
  int waiting_reader_pid[64];
  int number_of_woke_up_reader;
} reader_writer;
reader_writer rw;
int sequence[100];
int number_of_processes_in_sequence = 0;
struct spinlock sequence_process_lock;


int number_of_runnable_processes_in_edf_queue=0; //additional
int number_of_runnable_multilevel_feedback_queue[2]={0,0}; //additional

const char *states[] = {
  "UNUSED", "EMBRYO", "SLEEPING", "RUNNABLE", "RUNNING", "ZOMBIE"
};
const char *scheduling_classes[] = {
  [EARLIEST_DEADLINE_FIRST] = "real-time",
  [MULTILEVEL_FEEDBACK_QUEUE_FIRST_LEVEL] = "normal",
  [MULTILEVEL_FEEDBACK_QUEUE_SECOND_LEVEL] = "normal"
};
const char *scheduling_algorithms[] = {
  [EARLIEST_DEADLINE_FIRST] = "EDF",
  [MULTILEVEL_FEEDBACK_QUEUE_FIRST_LEVEL] = "mlfq(RR)",
  [MULTILEVEL_FEEDBACK_QUEUE_SECOND_LEVEL] = "mlfq(FCFS)"
};

extern struct user *curr_user;
extern struct spinlock login_lock;

struct {
  struct spinlock lock;
  struct proc proc[NPROC];
} ptable;

static struct proc *initproc;

int nextpid = 1;
extern void forkret(void);
extern void trapret(void);

static void wakeup1(void *chan);

void
pinit(void)
{
  initlock(&print_lock , "print");
  initlock(&ptable.lock, "ptable");
  initlock(&barber.barber_is_working, "barber_is_working");
  initlock(&customer.modify_customer_queue, "modify_customer_queue");
  for (int i = 0; i < 5; i++)
  {
    customer.waiting_customer[i] = 0;
  }
  customer.number_of_customer = 0;
  barber.is_barber_sleeping = 0;
  barber.barber_process = 0;
  customer.waiting_customer_head = 0;
  customer.waiting_customer_tail = 0;
  number_of_all_customer=0;
}

// Must be called with interrupts disabled
int
cpuid() {
  return mycpu()-cpus;
}

// Must be called with interrupts disabled to avoid the caller being
// rescheduled between reading lapicid and running through the loop.
struct cpu*
mycpu(void)
{
  int apicid, i;
  
  if(readeflags()&FL_IF)
    panic("mycpu called with interrupts enabled\n");
  
  apicid = lapicid();
  // APIC IDs are not guaranteed to be contiguous. Maybe we should have
  // a reverse map, or reserve a register to store &cpus[i].
  for (i = 0; i < ncpu; ++i) {
    if (cpus[i].apicid == apicid)
      return &cpus[i];
  }
  panic("unknown apicid\n");
}

// Disable interrupts so that we are not rescheduled
// while reading proc from the cpu structure
struct proc*
myproc(void) {
  struct cpu *c;
  struct proc *p;
  pushcli();
  c = mycpu();
  p = c->proc;
  popcli();
  return p;
}

//PAGEBREAK: 32
// Look in the process table for an UNUSED proc.
// If found, change state to EMBRYO and initialize
// state required to run in the kernel.
// Otherwise return 0.
static struct proc*
allocproc(void)
{
  struct proc *p;
  char *sp;

  acquire(&ptable.lock);

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == UNUSED)
      goto found;

  release(&ptable.lock);
  return 0;

found:
  p->state = EMBRYO;
  p->pid = nextpid++;
  p->waiting_time=0; //additional
  p->arrival_time_to_system=ticks; //additional
  p->continous_time_to_run=0; //additional

  release(&ptable.lock);

  // Allocate kernel stack.
  if((p->kstack = kalloc()) == 0){
    p->state = UNUSED;
    return 0;
  }
  sp = p->kstack + KSTACKSIZE;

  // Leave room for trap frame.
  sp -= sizeof *p->tf;
  p->tf = (struct trapframe*)sp;

  // Set up new context to start executing at forkret,
  // which returns to trapret.
  sp -= 4;
  *(uint*)sp = (uint)trapret;

  sp -= sizeof *p->context;
  p->context = (struct context*)sp;
  memset(p->context, 0, sizeof *p->context);
  p->context->eip = (uint)forkret;

  return p;
}

//PAGEBREAK: 32
// Set up first user process.
void
userinit(void)
{
  struct proc *p;
  extern char _binary_initcode_start[], _binary_initcode_size[];

  p = allocproc();
  
  initproc = p;
  if((p->pgdir = setupkvm()) == 0)
    panic("userinit: out of memory?");
  inituvm(p->pgdir, _binary_initcode_start, (int)_binary_initcode_size);
  p->sz = PGSIZE;
  memset(p->tf, 0, sizeof(*p->tf));
  p->tf->cs = (SEG_UCODE << 3) | DPL_USER;
  p->tf->ds = (SEG_UDATA << 3) | DPL_USER;
  p->tf->es = p->tf->ds;
  p->tf->ss = p->tf->ds;
  p->tf->eflags = FL_IF;
  p->tf->esp = PGSIZE;
  p->tf->eip = 0;  // beginning of initcode.S

  safestrcpy(p->name, "initcode", sizeof(p->name));
  p->cwd = namei("/");

  // this assignment to p->state lets other cores
  // run this process. the acquire forces the above
  // writes to be visible, and the lock is also needed
  // because the assignment might not be atomic.
  acquire(&ptable.lock);

  p->state = RUNNABLE;
  p->cal=MULTILEVEL_FEEDBACK_QUEUE_FIRST_LEVEL; //additional
  p->arrival_time_to_system=ticks;
  number_of_runnable_multilevel_feedback_queue[0]++; //additional

  release(&ptable.lock);
}

// Grow current process's memory by n bytes.
// Return 0 on success, -1 on failure.
int
growproc(int n)
{
  uint sz;
  struct proc *curproc = myproc();

  sz = curproc->sz;
  if(n > 0){
    if((sz = allocuvm(curproc->pgdir, sz, sz + n)) == 0)
      return -1;
  } else if(n < 0){
    if((sz = deallocuvm(curproc->pgdir, sz, sz + n)) == 0)
      return -1;
  }
  curproc->sz = sz;
  switchuvm(curproc);
  return 0;
}

// Create a new process copying p as the parent.
// Sets up stack to return as if from system call.
// Caller must set state of returned proc to RUNNABLE.
int
fork(void)
{
  int i, pid;
  struct proc *np;
  struct proc *curproc = myproc();

  // Allocate process.
  if((np = allocproc()) == 0){
    return -1;
  }

  // Copy process state from proc.
  if((np->pgdir = copyuvm(curproc->pgdir, curproc->sz)) == 0){
    kfree(np->kstack);
    np->kstack = 0;
    np->state = UNUSED;
    return -1;
  }
  np->sz = curproc->sz;
  np->parent = curproc;
  *np->tf = *curproc->tf;

  // Clear %eax so that fork returns 0 in the child.
  np->tf->eax = 0;

  for(i = 0; i < NOFILE; i++)
    if(curproc->ofile[i])
      np->ofile[i] = filedup(curproc->ofile[i]);
  np->cwd = idup(curproc->cwd);

  safestrcpy(np->name, curproc->name, sizeof(curproc->name));

  pid = np->pid;

  acquire(&ptable.lock);

  np->state = RUNNABLE;
  if(curproc==initproc || (strlen(np->name)==2 && strncmp(np->name, "sh", 2) == 0)) //additional
  {
    np->cal=MULTILEVEL_FEEDBACK_QUEUE_FIRST_LEVEL; //additional
    number_of_runnable_multilevel_feedback_queue[0]++; //additional
    np->arrival_time_to_system=ticks;
  }
  else 
  {
    np->cal=MULTILEVEL_FEEDBACK_QUEUE_SECOND_LEVEL; //additional
    number_of_runnable_multilevel_feedback_queue[1]++; //additional
    np->entering_time_to_the_fcfs_queue=ticks;
    np->arrival_time_to_system=ticks;
  }

  release(&ptable.lock);

  return pid;
}

// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait() to find out it exited.
void
exit(void)
{
  struct proc *curproc = myproc();
  struct proc *p;
  int fd;

  if(curproc == initproc)
    panic("init exiting");

  // Close all open files.
  for(fd = 0; fd < NOFILE; fd++){
    if(curproc->ofile[fd]){
      fileclose(curproc->ofile[fd]);
      curproc->ofile[fd] = 0;
    }
  }

  begin_op();
  iput(curproc->cwd);
  end_op();
  curproc->cwd = 0;

  // // Logout current user if this process was logged in
  // acquire(&login_lock);
  // if ((curr_user->is_logged_in == 1) && (curproc->user_id == curr_user->user_id)) {
  //   curr_user->is_logged_in = 0;
  // }
  // release(&login_lock);
  // ///////////////////////////////////////////////////////

  acquire(&ptable.lock);

  if (curproc->state == RUNNABLE) //additional
  {
    if (curproc->cal == EARLIEST_DEADLINE_FIRST)                     // additional
      number_of_runnable_processes_in_edf_queue--;                   // additional
    else if (curproc->cal == MULTILEVEL_FEEDBACK_QUEUE_FIRST_LEVEL)  // additiona;
      number_of_runnable_multilevel_feedback_queue[0]--;             // additional
    else if (curproc->cal == MULTILEVEL_FEEDBACK_QUEUE_SECOND_LEVEL) // additional
    {
      number_of_runnable_multilevel_feedback_queue[1]--; // additional
      curproc->entering_time_to_the_fcfs_queue = -1;
    }
  }
  // if(curproc->state!=RUNNING)
  //   curproc->continous_time_to_run=0; //additional



  int previous_witing_time=curproc->waiting_time; //additional

  // Parent might be sleeping in wait().
  wakeup1(curproc->parent);
  curproc->waiting_time=previous_witing_time; //additional

  // Pass abandoned children to init.
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->parent == curproc)
    {
      p->parent = initproc;
      if(p->state == ZOMBIE)
      {
        previous_witing_time=p->waiting_time; //additional
        wakeup1(initproc);
        p->waiting_time=previous_witing_time; //additional
      }
    }
  }

  // Jump into the scheduler, never to return.
  curproc->state = ZOMBIE;
  sched();
  panic("zombie exit");
}

// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
int
wait(void)
{
  struct proc *p;
  int havekids, pid;
  struct proc *curproc = myproc();
  
  acquire(&ptable.lock);
  for(;;){
    // Scan through table looking for exited children.
    havekids = 0;
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->parent != curproc)
        continue;
      havekids = 1;
      if(p->state == ZOMBIE){
        // Found one.
        pid = p->pid;
        kfree(p->kstack);
        p->kstack = 0;
        freevm(p->pgdir);
        p->pid = 0;
        p->parent = 0;
        p->name[0] = 0;
        p->killed = 0;
        p->state = UNUSED;
        release(&ptable.lock);
        return pid;
      }
    }

    // No point waiting if we don't have any children.
    if(!havekids || curproc->killed){
      release(&ptable.lock);
      return -1;
    }

    // Wait for children to exit.  (See wakeup1 call in proc_exit.)
    sleep(curproc, &ptable.lock);  //DOC: wait-sleep
  }
}












struct proc*
earliest_deadline_first_scheduler() //additional
{
  struct proc* edf_process_to_schedule=0;
  int lowest_remainder_time_to_deadline=0x7fffffff;
  for(struct proc* p=&ptable.proc[0];p<&ptable.proc[NPROC];p++)
  {
    if(p->cal==EARLIEST_DEADLINE_FIRST && p->state==RUNNABLE)
    {
      int remainder_time_to_deadline=p->deadline-ticks;
      if(remainder_time_to_deadline<=lowest_remainder_time_to_deadline)
      {
        lowest_remainder_time_to_deadline=remainder_time_to_deadline;
        edf_process_to_schedule=p;
        // cprintf("pid edf is:\t%d\t",edf_process_to_schedule->pid);
        // cprintf("name of edf program:\t%s\n",edf_process_to_schedule->name);
      }
    }
  }
  
  return edf_process_to_schedule;
}







struct proc*
multilevel_feedback_queue_scheduler(struct proc* last_scheduled_process_in_multilevel_feedback_queue_first_level) // additional
{
  if(number_of_runnable_multilevel_feedback_queue[0]!=0)
  {
    struct proc* first_level_process=last_scheduled_process_in_multilevel_feedback_queue_first_level;
    for(;;)
    {
      first_level_process++;
      if(first_level_process>=&ptable.proc[NPROC])
        first_level_process=&ptable.proc[0];
      if(first_level_process->state==RUNNABLE && first_level_process->cal==MULTILEVEL_FEEDBACK_QUEUE_FIRST_LEVEL)
      {
        //cprintf("%s\n",first_level_process->name);
        // cprintf("rr proc name is:\t%s\t",first_level_process->name);
        // cprintf("%d\n",first_level_process->pid);
        return first_level_process;
      }
    }
  }
  else if(number_of_runnable_multilevel_feedback_queue[1]!=0)
  {
    int min_arrival_time=ticks;
    struct proc* chosen_fcfs=0;
    for(struct proc* second_level_process=&ptable.proc[0];second_level_process<&ptable.proc[NPROC];second_level_process++)
    {
      if(second_level_process->state==RUNNABLE && second_level_process->cal==MULTILEVEL_FEEDBACK_QUEUE_SECOND_LEVEL)
      {
        if(second_level_process->entering_time_to_the_fcfs_queue<=min_arrival_time && second_level_process->entering_time_to_the_fcfs_queue!=-1)
        {
          min_arrival_time=second_level_process->entering_time_to_the_fcfs_queue;
          chosen_fcfs=second_level_process;
        }
        // if(strncmp(second_level_process->name,"init",4)==0)
        // printf("%d\n",second_level_process->cal==WITHOUT_PRIORITY);
        // cprintf("%s\n",second_level_process->name);
        // cprintf("%d\n",second_level_process->cal==MULTILEVEL_FEEDBACK_QUEUE_FIRST_LEVEL);
      }
      
      //cprintf("%s\n",second_level_process->name);
    }
    // cprintf("%s\n",chosen_fcfs->pid);
    // cprintf("fcfs proc name is:\t%s\t",chosen_fcfs->name);
    // cprintf("%d\n",chosen_fcfs->pid);
    return chosen_fcfs;
  }
  return 0;
}


















//PAGEBREAK: 42
// Per-CPU process scheduler.
// Each CPU calls scheduler() after setting itself up.
// Scheduler never returns.  It loops, doing:
//  - choose a process to run
//  - swtch to start running that process
//  - eventually that process transfers control
//      via swtch back to the scheduler.
void scheduler(void)
{
  struct proc *p;
  struct cpu *c = mycpu();
  c->proc = 0;
  struct proc *last_scheduled_process_in_multilevel_feedback_queue_first_level = &ptable.proc[0];  // additional
  struct proc* last_scheduled_process=0;

  for (;;)
  {
    // Enable interrupts on this processor.
    sti();
    p=0; //additional

    // Loop over process table looking for process to run.
    acquire(&ptable.lock);
    if(number_of_runnable_processes_in_edf_queue!=0) // additional
    {
      p=earliest_deadline_first_scheduler(); // additional
    }
    else if (number_of_runnable_processes_in_edf_queue == 0 && (number_of_runnable_multilevel_feedback_queue[0] != 0 || number_of_runnable_multilevel_feedback_queue[1] != 0)) // additional
    {
      p=multilevel_feedback_queue_scheduler(last_scheduled_process_in_multilevel_feedback_queue_first_level);
    }
    if(p==0 || p->state!=RUNNABLE) // aditional
    {
      c->proc=0;
      release(&ptable.lock); //additional
      continue; //additional
    } // additional

    // Switch to chosen process.  It is the process's job
    // to release ptable.lock and then reacquire it
    // before jumping back to us.
    c->proc = p;
    switchuvm(p);
    p->state = RUNNING;
    if(p->cal==EARLIEST_DEADLINE_FIRST) //additional
      number_of_runnable_processes_in_edf_queue--; //additional
    else if(p->cal==MULTILEVEL_FEEDBACK_QUEUE_FIRST_LEVEL) //additional
    {
      last_scheduled_process_in_multilevel_feedback_queue_first_level = p; //additional
      number_of_runnable_multilevel_feedback_queue[0]--; //additional
    }
    else if(p->cal==MULTILEVEL_FEEDBACK_QUEUE_SECOND_LEVEL) //additional
    {
      p->waiting_time=0;
      number_of_runnable_multilevel_feedback_queue[1]--; //additional
    }
    if(last_scheduled_process==0)
    {
      c->time_for_roundrobin=0;
      last_scheduled_process=p;
      p->continous_time_to_run=0;
    }
    else if(p->pid!=last_scheduled_process->pid)
    {
      c->time_for_roundrobin=0;
      p->continous_time_to_run=0;
    }
      

    swtch(&(c->scheduler), p->context);
    switchkvm();

    // Process is done running for now.
    // It should have changed its p->state before coming back.
    last_scheduled_process=p;
    c->proc = 0;
    if(p->cal==MULTILEVEL_FEEDBACK_QUEUE_FIRST_LEVEL)
      c->time_for_roundrobin=0;
    release(&ptable.lock);
  }
}

// Enter scheduler.  Must hold only ptable.lock
// and have changed proc->state. Saves and restores
// intena because intena is a property of this
// kernel thread, not this CPU. It should
// be proc->intena and proc->ncli, but that would
// break in the few places where a lock is held but
// there's no process.
void
sched(void)
{
  int intena;
  struct proc *p = myproc();

  if(!holding(&ptable.lock))
    panic("sched ptable.lock");
  if(mycpu()->ncli != 1)
    panic("sched locks");
  if(p->state == RUNNING)
    panic("sched running");
  if(readeflags()&FL_IF)
    panic("sched interruptible");
  intena = mycpu()->intena;
  swtch(&p->context, mycpu()->scheduler);
  mycpu()->intena = intena;
}

// Give up the CPU for one scheduling round.
void
yield(void)
{
  acquire(&ptable.lock);  //DOC: yieldlock
  myproc()->state = RUNNABLE;
  if(myproc()->cal==EARLIEST_DEADLINE_FIRST) //additional
    number_of_runnable_processes_in_edf_queue++; //additional
  else if(myproc()->cal==MULTILEVEL_FEEDBACK_QUEUE_FIRST_LEVEL) //additional
    number_of_runnable_multilevel_feedback_queue[0]++; //additional
  else if(myproc()->cal==MULTILEVEL_FEEDBACK_QUEUE_SECOND_LEVEL) //additional
  {
    number_of_runnable_multilevel_feedback_queue[1]++; //additional
    myproc()->waiting_time=0;
  }
  sched();
  release(&ptable.lock);
}

// A fork child's very first scheduling by scheduler()
// will swtch here.  "Return" to user space.
void
forkret(void)
{
  static int first = 1;
  // Still holding ptable.lock from scheduler.
  release(&ptable.lock);

  if (first) {
    // Some initialization functions must be run in the context
    // of a regular process (e.g., they call sleep), and thus cannot
    // be run from main().
    first = 0;
    iinit(ROOTDEV);
    initlog(ROOTDEV);
  }

  // Return to "caller", actually trapret (see allocproc).
}

// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
void
sleep(void *chan, struct spinlock *lk)
{
  struct proc *p = myproc();
  
  if(p == 0)
    panic("sleep");

  if(lk == 0)
    panic("sleep without lk");

  // Must acquire ptable.lock in order to
  // change p->state and then call sched.
  // Once we hold ptable.lock, we can be
  // guaranteed that we won't miss any wakeup
  // (wakeup runs with ptable.lock locked),
  // so it's okay to release lk.
  if(lk != &ptable.lock){  //DOC: sleeplock0
    acquire(&ptable.lock);  //DOC: sleeplock1
    release(lk);
  }
  // Go to sleep.
  p->chan = chan;
  if (p->state == RUNNABLE) //additional
  {
    if (p->cal == EARLIEST_DEADLINE_FIRST)                     // additional
      number_of_runnable_processes_in_edf_queue--;             // additional
    else if (p->cal == MULTILEVEL_FEEDBACK_QUEUE_FIRST_LEVEL)  // additional
      number_of_runnable_multilevel_feedback_queue[0]--;       // additional
    else if (p->cal == MULTILEVEL_FEEDBACK_QUEUE_SECOND_LEVEL) // additional
    {
      number_of_runnable_multilevel_feedback_queue[1]--; // additiona;
      //p->entering_time_to_the_fcfs_queue = 0;           // additional
    }
  }
  p->state = SLEEPING;

  sched();

  // Tidy up.
  p->chan = 0;

  // Reacquire original lock.
  if(lk != &ptable.lock){  //DOC: sleeplock2
    release(&ptable.lock);
    acquire(lk);
  }
}

//PAGEBREAK!
// Wake up all processes sleeping on chan.
// The ptable lock must be held.
static void
wakeup1(void *chan)
{
  struct proc *p;

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == SLEEPING && p->chan == chan)
    {
      p->state = RUNNABLE;
      if(p->cal==EARLIEST_DEADLINE_FIRST) //additional
        number_of_runnable_processes_in_edf_queue++; //additional
      else if(p->cal==MULTILEVEL_FEEDBACK_QUEUE_FIRST_LEVEL) //additional
        number_of_runnable_multilevel_feedback_queue[0]++; //additional
      else if(p->cal==MULTILEVEL_FEEDBACK_QUEUE_SECOND_LEVEL) //additional
      {
        number_of_runnable_multilevel_feedback_queue[1]++; //additional
        p->entering_time_to_the_fcfs_queue=ticks; //additional
        p->waiting_time=0; //additional
      }
    }
}

// Wake up all processes sleeping on chan.
void
wakeup(void *chan)
{
  acquire(&ptable.lock);
  wakeup1(chan);
  release(&ptable.lock);
}

// Kill the process with the given pid.
// Process won't exit until it returns
// to user space (see trap in trap.c).
int
kill(int pid)
{
  struct proc *p;

  acquire(&ptable.lock);
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->pid == pid){
      p->killed = 1;
      // Wake process from sleep if necessary.
      if(p->state == SLEEPING)
        p->state = RUNNABLE;
      release(&ptable.lock);
      return 0;
    }
  }
  release(&ptable.lock);
  return -1;
}

//PAGEBREAK: 36
// Print a process listing to console.  For debugging.
// Runs when user types ^P on console.
// No lock to avoid wedging a stuck machine further.
void
procdump(void)
{
  static char *states[] = {
  [UNUSED]    "unused",
  [EMBRYO]    "embryo",
  [SLEEPING]  "sleep ",
  [RUNNABLE]  "runble",
  [RUNNING]   "run   ",
  [ZOMBIE]    "zombie"
  };
  int i;
  struct proc *p;
  char *state;
  uint pc[10];

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->state == UNUSED)
      continue;
    if(p->state >= 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "???";
    cprintf("%d %s %s", p->pid, state, p->name);
    if(p->state == SLEEPING){
      getcallerpcs((uint*)p->context->ebp+2, pc);
      for(i=0; i<10 && pc[i] != 0; i++)
        cprintf(" %p", pc[i]);
    }
    cprintf("\n");
  }
}


void
next_palindrome(int num)
{
  int i=num;
  while(1)
  {
    int number=i,reverse=0;
    while(number>0)
    {
      int digit=number%10;
      reverse=reverse*10+digit;
      number/=10;
    }
    if(reverse==i)
      break;
    i++;
  }
  cprintf("result: %d\n",i);
}

int
make_user(int user_id, const char* password) {
    return add_user(user_id, password);
}

int
login(int user_id, const char* password) {
  return login_user(user_id , password);
}

int
logout(){
  return logout_user();
}

int logs(){
  get_user_logs();
  return 0;
}

int
diff(const char *file1, const char *file2)
{
  struct inode *ip1, *ip2;
  char line1[128], line2[128];
  uint offset1 = 0, offset2 = 0;
  int line_num = 1;
  int equal = 1;

  ip1 = namei(file1);
  ip2 = namei(file2);

  if (ip1 == 0 || ip2 == 0)
  {
    cprintf("Error: one of the files does not exist\n");
    return -1;
  }

  ilock(ip1);
  ilock(ip2);

  // test
  // char buf[512];
  // int bytes_read;
  // uint offset = 0;
  // cprintf("File 1: %s\n", file1);
  // while ((bytes_read = readi(ip1, buf, offset, sizeof(buf))) > 0) {
  //   offset += bytes_read;
  //   for (int i = 0; i < bytes_read; i++) {
  //     cprintf("%c", buf[i]);
  //   }
  // }
  // cprintf("\nFile 2: %s\n", file1);
  // while ((bytes_read = readi(ip2, buf, offset, sizeof(buf))) > 0) {
  //   offset += bytes_read;
  //   for (int i = 0; i < bytes_read; i++) {
  //     cprintf("%c", buf[i]);
  //   }
  // }

  while (1) {
    int i = 0;
    char ch;

    memset(line1, 0, sizeof(line1));
    while (i < sizeof(line1) - 1 && readi(ip1, &ch, offset1, 1) == 1) {
      offset1++;
      if (ch == '\n' || ch == '\r')
        break;
      line1[i++] = ch;
    }
    line1[i] = 0;
    int len1 = i;

    i = 0;
    memset(line2, 0, sizeof(line2));
    while (i < sizeof(line2) - 1 && readi(ip2, &ch, offset2, 1) == 1) {
      offset2++;
      if (ch == '\n' || ch == '\r')
        break;
      line2[i++] = ch;
    }
    line2[i] = 0;
    int len2 = i;

    if (len1 == 0 && len2 == 0)
      break;

    if (strncmp(line1, line2, sizeof(line1)) != 0) {
      cprintf("Line %d:\n  %s: %s\n  %s: %s\n", line_num, file1, line1, file2, line2);
      equal = 0;
    }

    line_num++;
  }

  iunlockput(ip1);
  iunlockput(ip2);

  return equal ? 0 : -1;
}


int
set_sleep_syscall(int input_tick)
{
  int current_time;
  acquire(&tickslock);
  current_time=ticks;
  release(&tickslock);
  while(1)
  {
    acquire(&tickslock);
    if(ticks-current_time==input_tick)
    {
      release(&tickslock);
      break;
    }
    release(&tickslock);
  }
  return 0;
} 





void
aging_mechanism() //additional
{
  acquire(&ptable.lock);
  for(struct proc* p=&ptable.proc[0];p<&ptable.proc[NPROC];p++)
  {
    if(p->state==RUNNABLE && p->cal==MULTILEVEL_FEEDBACK_QUEUE_SECOND_LEVEL)
    {
      p->waiting_time++;
      if(p->waiting_time==800)
      {
        //cprintf("wait time is:\t%d\n",p->waiting_time);
        p->cal=MULTILEVEL_FEEDBACK_QUEUE_FIRST_LEVEL;
        number_of_runnable_multilevel_feedback_queue[1]--;
        number_of_runnable_multilevel_feedback_queue[0]++;
        p->waiting_time=0;
        p->arrival_time_to_system=ticks;
      }
    }
  }
  // for (struct cpu *c = cpus; c < &cpus[NCPU]; c++)
  // {
  //   struct proc *p = c->proc;
  //   if (p && p->state == RUNNING)
  //     p->continous_time_to_run++;
  // }
  release(&ptable.lock);
}



int
create_realtime_process(int decided_deadline)
{
  struct proc* p = myproc();

  acquire(&ptable.lock);

  if (p->state == RUNNABLE) {
    if (p->cal == MULTILEVEL_FEEDBACK_QUEUE_FIRST_LEVEL)
      number_of_runnable_multilevel_feedback_queue[0]--;
    else if (p->cal == MULTILEVEL_FEEDBACK_QUEUE_SECOND_LEVEL)
      number_of_runnable_multilevel_feedback_queue[1]--;
  }
  int saved_tick=0;

  acquire(&tickslock);
  saved_tick=ticks;
  p->deadline = saved_tick + decided_deadline;
  release(&tickslock);

  
  p->arrival_time_to_system=saved_tick;



  p->cal = EARLIEST_DEADLINE_FIRST;

  if (p->state == RUNNABLE)
    number_of_runnable_processes_in_edf_queue++;

  release(&ptable.lock);
  return 0;
}



struct proc*
get_proc_by_pid(int pid)
{
  struct proc *p;
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
    if(p->pid == pid && p->state != UNUSED)
      return p;
  }
  return 0;
}


int
change_process_queue(int pid, int new_queue_type)
{
  struct proc *p;
  int result = 0;
  int saved_ticks = 0;

  acquire(&ptable.lock);

  if (new_queue_type != MULTILEVEL_FEEDBACK_QUEUE_FIRST_LEVEL &&
  new_queue_type != MULTILEVEL_FEEDBACK_QUEUE_SECOND_LEVEL) {
    release(&ptable.lock);
    cprintf("Error: invalid queue type\n");
    return -1;
  }

  p = get_proc_by_pid(pid);
  cprintf("pid\t%d\t%d\t%d\n",p->pid,p->continous_time_to_run,ticks);
  if (p == 0) {
    release(&ptable.lock);
    cprintf("Error: no process found with pid %d\n", pid);
    return -1;
  }

  if (p->cal == new_queue_type) {
    release(&ptable.lock);
    cprintf("Error: process %d is already in the specified queue\n", pid);
    return -1;
  }

  if (p->state == RUNNING && p != myproc()) {
    release(&ptable.lock);
    cprintf("Error: Cannot move RUNNING process from another CPU\n");
    return -1;
  }

  if (p->state == RUNNABLE) {
  if (p->cal == MULTILEVEL_FEEDBACK_QUEUE_FIRST_LEVEL)
  number_of_runnable_multilevel_feedback_queue[0]--;
  else if (p->cal == MULTILEVEL_FEEDBACK_QUEUE_SECOND_LEVEL)
  number_of_runnable_multilevel_feedback_queue[1]--;
  }

  p->cal = new_queue_type;
  if (p->state == RUNNABLE) {
  if (new_queue_type == MULTILEVEL_FEEDBACK_QUEUE_FIRST_LEVEL)
  number_of_runnable_multilevel_feedback_queue[0]++;
  else
  number_of_runnable_multilevel_feedback_queue[1]++;
  }



  acquire(&tickslock);
  saved_ticks = ticks;
  release(&tickslock);

  
  if (new_queue_type == MULTILEVEL_FEEDBACK_QUEUE_SECOND_LEVEL) {
  p->entering_time_to_the_fcfs_queue = saved_ticks;
  }
  p->arrival_time_to_system=saved_ticks;

  p->waiting_time = 0;
  release(&ptable.lock);
  
  return 0;
}



int num_digits(int n) {
  if (n == 0) return 1;
  int count = 0;
  if (n < 0) {
    count++;
    n = -n;
  }
  while (n > 0) {
    count++;
    n /= 10;
  }
  return count;
}

int
collect_process_snapshots(struct proc_snapshot *list, int max_count)
{
  int count = 0;

  acquire(&ptable.lock);
  for (struct proc *p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
    if (p->state != UNUSED && count < max_count) {
      safestrcpy(list[count].name, p->name, sizeof(p->name));
      list[count].pid = p->pid;
      list[count].state = p->state;
      list[count].cal = p->cal;
      list[count].waiting_time = p->waiting_time;
      list[count].deadline = p->deadline;
      list[count].continous_time_to_run = p->continous_time_to_run;
      list[count].entering_time_to_the_fcfs_queue = p->entering_time_to_the_fcfs_queue;
      list[count].arrival_time_to_system = p->arrival_time_to_system;
      count++;
    }
  }
  release(&ptable.lock);

  return count;
}

void
print_process_info(void)
{
  cli();
  acquire(&print_lock);
  struct proc_snapshot list[MAX_PROC_INFO];
  int count = collect_process_snapshots(list, MAX_PROC_INFO);
  cprintf("ticks:\t%d\n",ticks);
  
  cprintf("name           pid     state     class     algorithm    wait time   deadline     run        arrival\n");
  cprintf("------------------------------------------------------------------------------------------------------\n");

  for (int i = 0; i < count; i++) {
    struct proc_snapshot *p = &list[i];

    cprintf("%s", p->name);
    int name_len = strlen(p->name);
    for (int i = name_len; i < 16; i++) cprintf(" ");
  
    cprintf("%d", p->pid);
    if (p->pid < 10) cprintf("      ");
    else if (p->pid < 100) cprintf("     ");
    else if (p->pid < 1000) cprintf("    ");
    else cprintf("   ");
  
    cprintf("%s", states[p->state]);
    int state_len = strlen(states[p->state]);
    for (int i = state_len; i < 10; i++) cprintf(" ");
  
    cprintf("%s", scheduling_classes[p->cal]);
    int class_len = strlen(scheduling_classes[p->cal]);
    for (int i = class_len; i < 10; i++) cprintf(" ");
  
    cprintf("%s", scheduling_algorithms[p->cal]);
    int algo_len = strlen(scheduling_algorithms[p->cal]);
    for (int i = algo_len; i < 15; i++) cprintf(" ");

    int wait = p->waiting_time;
    int dl = (p->cal == EARLIEST_DEADLINE_FIRST ? p->deadline : 0);
    int run = p->continous_time_to_run;
    int arrival = (p->cal == MULTILEVEL_FEEDBACK_QUEUE_SECOND_LEVEL ? p->entering_time_to_the_fcfs_queue : p->arrival_time_to_system);
    
    // wait time
    cprintf("%d", wait);
    for (int i = 0; i < 12 - num_digits(wait); i++) cprintf(" ");
    
    // deadline
    cprintf("%d", dl);
    for (int i = 0; i < 12 - num_digits(dl); i++) cprintf(" ");
    
    // run
    cprintf("%d", run);
    for (int i = 0; i < 12 - num_digits(run); i++) cprintf(" ");
    
    // arrival
    cprintf("%d\n", arrival);
  }
  release(&print_lock);
  sti();
}



void barber_sleep()
{
  if (number_of_all_customer >= 10 && customer.number_of_customer==0)
  {
    cprintf("barber with pid %d is exiting", myproc()->pid);
    exit();
  }
  acquire(&barber.barber_is_working);
  if (customer.number_of_customer == 0)
  {
    cprintf("barber with pid %d is going to sleep\n", myproc()->pid);
    barber.barber_process = myproc();
    barber.is_barber_sleeping = 1;
    sleep(barber.barber_process, &barber.barber_is_working);
    barber.is_barber_sleeping = 0;
    if (number_of_all_customer >= 10 && customer.number_of_customer==0)
    {
      cprintf("barber with pid %d is exiting", myproc()->pid);
      release(&barber.barber_is_working);
      exit();
    }
    else
      cprintf("barber with pid %d woke up\n", myproc()->pid);
  }
  release(&barber.barber_is_working);
}


int customer_arrive()
{
  cprintf("customer with pid %d is entering the shop\n", myproc()->pid);
  acquire(&customer.modify_customer_queue);
  if (number_of_all_customer >= 10)
  {
    if (barber.is_barber_sleeping)
      wakeup(barber.barber_process);
    release(&customer.modify_customer_queue);
    return 1;
  }
  number_of_all_customer++;
  if (customer.number_of_customer >= 5)
  {
    cprintf("customer with pid %d can't enter for getting service because number of waitind customer is 5\n", myproc()->pid);
    release(&customer.modify_customer_queue);
    return 1;
  }
  customer.waiting_customer[customer.waiting_customer_tail] = myproc()->pid;
  customer.waiting_customer_tail = (customer.waiting_customer_tail + 1) % 5;
  customer.number_of_customer++;
  if (barber.is_barber_sleeping)
  {
    cprintf("customer with pid %d is waking up the barber\n", myproc()->pid);
    wakeup(barber.barber_process);
  }
  sleep(myproc(), &customer.modify_customer_queue);
  cprintf("customer with pid %d got haircut and is exiting\n", myproc()->pid);
  release(&customer.modify_customer_queue);
  return 0;
}


void cut_hair()
{
  int current_customer_pid = 0;
  if (number_of_all_customer >= 10 && customer.number_of_customer == 0)
  {
    cprintf("barber with pid %d is exiting\n", myproc()->pid);
    exit();
  }
  acquire(&barber.barber_is_working);
  if (customer.number_of_customer > 0)
  {
    acquire(&customer.modify_customer_queue);
    current_customer_pid = customer.waiting_customer[customer.waiting_customer_head];
    customer.waiting_customer_head = (customer.waiting_customer_head + 1) % 5;
    customer.number_of_customer--;
    cprintf("barber with pid %d is cutting hair of customer with pid %d\n", myproc()->pid, current_customer_pid, number_of_all_customer);
    release(&customer.modify_customer_queue);
    int k = 0;
    for (int i = 0; i < 1000; i++)
      for (int j = 0; j < 1000; j++)
        k++;
    cprintf("barber with pid %d finished cutting hair of customer with pid %d\n", myproc()->pid, current_customer_pid);
    release(&barber.barber_is_working);
    wakeup(get_proc_by_pid(current_customer_pid));
    if (number_of_all_customer >= 10 && customer.number_of_customer == 0)
    {
      cprintf("barber with pid %d is exiting\n", myproc()->pid);
      exit();
    }
  }
  else
    release(&barber.barber_is_working);
}


void init_rw_lock()
{
  initlock(&rw.critical_section, "critical_section");
  initlock(&rw.is_writer_waiting, "is_writer_waiting");
  initlock(&rw.waiting_reader, "waiting_reader");
  initlock(&sequence_process_lock, "sequence_process_lock");
  rw.counter = 0;
  rw.active_reader_count = 0;
  rw.active_writer_count = 0;
  rw.number_of_waiting_writer = 0;
  rw.waiting_writer_head = 0;
  rw.waiting_writer_tail = 0;
  rw.waiting_reader_head = 0;
  rw.waiting_reader_tail = 0;
  rw.number_of_waiting_reader = 0;
  rw.number_of_woke_up_reader=0;
  for (int i = 0; i < 64; i++) {
    rw.waiting_writer_pid[i] = 0;
    rw.waiting_reader_pid[i] = 0;
  }
}

void writer_write_lock()
{
  acquire(&rw.is_writer_waiting);
  if (rw.active_reader_count > 0 || rw.active_writer_count > 0 || rw.number_of_waiting_writer > 0 || rw.number_of_woke_up_reader>0)
  {
    rw.waiting_writer_pid[rw.waiting_writer_tail] = myproc()->pid;
    rw.waiting_writer_tail = (rw.waiting_writer_tail + 1) % 64;
    rw.number_of_waiting_writer++;
    cprintf("writer with pid %d is going to sleep\n", myproc()->pid);
    sleep(myproc(), &rw.is_writer_waiting);
    rw.number_of_waiting_writer--;
    cprintf("writer with pid %d is waking up\n", myproc()->pid);
    release(&rw.is_writer_waiting);
  }
  else
    release(&rw.is_writer_waiting);
  acquire(&rw.critical_section);
  rw.active_writer_count = 1;
}

void reader_read_lock()
{
  acquire(&rw.waiting_reader);
  if (rw.number_of_waiting_writer > 0 || rw.active_writer_count > 0)
  {
    cprintf("reader with pid %d is going to sleep\n", myproc()->pid);
    rw.waiting_reader_pid[rw.waiting_reader_tail] = myproc()->pid;
    rw.waiting_reader_tail = (rw.waiting_reader_tail + 1) % 64;
    rw.number_of_waiting_reader++;
    sleep(myproc(), &rw.waiting_reader);
    rw.number_of_waiting_reader--;
    cprintf("reader with pid %d woke up\n", myproc()->pid);
    release(&rw.waiting_reader);
  }
  else
    release(&rw.waiting_reader);
  acquire(&rw.critical_section);
  rw.active_reader_count++;
  release(&rw.critical_section);
}

void get_rw_pattern(int pattern) // reader_writer
{
  acquire(&sequence_process_lock);
  number_of_processes_in_sequence = 0;
  int temp_pattern = pattern;
  while (temp_pattern > 0)
  {
    sequence[number_of_processes_in_sequence] = temp_pattern % 2;
    temp_pattern /= 2;
    number_of_processes_in_sequence++;
  }
  number_of_processes_in_sequence--;
  release(&sequence_process_lock);
}

void reader_critical_section()
{
  cprintf("reader with pid %d is in critical section\n", myproc()->pid);
  cprintf("reader with pid %d is reading counter value that is %d\n", myproc()->pid, rw.counter);
  cprintf("reader with pid %d is exiting critical section\n", myproc()->pid);
}

void reader_release_lock()
{
  rw.active_reader_count--;
  rw.number_of_woke_up_reader=0;
  if (rw.active_reader_count == 0 && rw.number_of_waiting_writer > 0)
  {
    acquire(&rw.is_writer_waiting);
    if (rw.number_of_waiting_writer > 0)
    {
      cprintf("reader with pid %d is waking up writer with pid %d\n", myproc()->pid, rw.waiting_writer_pid[rw.waiting_writer_head]);
      wakeup(get_proc_by_pid(rw.waiting_writer_pid[rw.waiting_writer_head]));
      rw.waiting_writer_head = (rw.waiting_writer_head + 1) % 64;
    }
    release(&rw.is_writer_waiting);
  }
}

void writer_release_lock()
{
  
  
  if (rw.number_of_waiting_writer > 0)
  {
    rw.active_writer_count--;
    release(&rw.critical_section);
    acquire(&rw.is_writer_waiting);
    if (rw.number_of_waiting_writer > 0)
    {
      cprintf("writer with pid %d is waking up writer with pid %d\n", myproc()->pid, rw.waiting_writer_pid[rw.waiting_writer_head]);
      wakeup(get_proc_by_pid(rw.waiting_writer_pid[rw.waiting_writer_head]));
      rw.waiting_writer_head = (rw.waiting_writer_head + 1) % 64;
    }
    release(&rw.is_writer_waiting);
  }
  else if (rw.number_of_waiting_reader > 0)
  {
    rw.active_writer_count--;
    release(&rw.critical_section);
    acquire(&rw.waiting_reader);
    int number_of_reader_to_wake = rw.number_of_waiting_reader;
    for (int i = 0; i < number_of_reader_to_wake; i++)
    {
      if(rw.number_of_waiting_writer>0)
        break;
      int new_reader_pid = rw.waiting_reader_pid[rw.waiting_reader_head];
      if (new_reader_pid != 0)
      {
        cprintf("writer with pid %d is waking up reader with pid %d\n", myproc()->pid, new_reader_pid);
        rw.number_of_woke_up_reader++;
        wakeup(get_proc_by_pid(rw.waiting_reader_pid[rw.waiting_reader_head]));
      }
      rw.waiting_reader_head = (rw.waiting_reader_head + 1) % 64;
    }
    rw.number_of_waiting_reader = 0;
    release(&rw.waiting_reader);
  }
  else
  {
    rw.active_writer_count--;
    release(&rw.critical_section);
  }
  
  
}

void writer_critical_section()
{
  cprintf("writer with pid %d is in critical section\n", myproc()->pid);
  rw.counter++;
  cprintf("writer with pid %d incremented the count and new count value is %d\n", myproc()->pid, rw.counter);
  cprintf("writer with pid %d is exiting critical section\n", myproc()->pid);
}

void critical_section()
{
  int duty=0;
  acquire(&sequence_process_lock);
  duty = sequence[number_of_processes_in_sequence - 1];
  number_of_processes_in_sequence--;
  cprintf("Process %d got duty %d\n", myproc()->pid, duty, number_of_processes_in_sequence);
  release(&sequence_process_lock);
  
  if (duty == 0)
  {
    reader_read_lock();
    reader_critical_section();
    reader_release_lock();
  }
  else if (duty == 1)
  {
    writer_write_lock();
    writer_critical_section();
    writer_release_lock();
  }
}

