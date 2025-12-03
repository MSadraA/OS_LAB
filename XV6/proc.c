#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"

struct {
  struct spinlock lock;
  struct proc proc[NPROC];
} ptable;

static struct proc *initproc;

struct spinlock print_lock;

int nextpid = 1;
extern void forkret(void);
extern void trapret(void);

static void wakeup1(void *chan);

const int THRESHOLD_LOAD_BALANCE = 3; // for load balancing

void
pinit(void)
{
  initlock(&ptable.lock, "ptable");
  initlock(&print_lock, "print_lock"); // for debug systemcalls

  // ====== CHANGE cpu queue ======
  for(int i = 0; i < NCPU; i++){
    initlock(&cpus[i].queuelock, "cpu_runq");
    cpus[i].runq_head = 0;
    cpus[i].runq_tail = 0;
    cpus[i].proc_count = 0;
    cpus[i].rr_ticks = 0; // CHANGE RR algorithm
    cpus[i].monitor_ticks = 0; // CHANGE load balancing
  }
  // ==============================
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
  p->priority = PRIORITY_NORMAL;

  // ====== CHANGE cpu queue ======
  p->next = 0;
  p->cpu_id = 0; // Not yet assigned to any CPU
  // ==============================
  p->arrival_time_to_system = ticks; // CHANGE FCFS algorithm

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

// ====== CHANGE helper function to work with cpu run queue =====
void
push_cpu_sorted(struct cpu *c, struct proc *p) // FCFS: insert process based on arrival time
{
  struct proc *curr;
  struct proc *prev = 0;

  // if it's the first process in the queue
  if(c->runq_head == 0 || p->arrival_time_to_system < c->runq_head->arrival_time_to_system){
    p->next = c->runq_head;
    c->runq_head = p;
    if(c->runq_tail == 0) 
       c->runq_tail = p;
  } 
  else {
    curr = c->runq_head;
    while(curr && curr->arrival_time_to_system <= p->arrival_time_to_system){
      prev = curr;
      curr = curr->next;
    }
    
    p->next = curr;
    prev->next = p;
    
    // if it's the last process in the queue
    if(curr == 0)
      c->runq_tail = p;
  }
  c->proc_count++;
}

void
push_cpu(struct cpu *c, struct proc *p)
{
  acquire(&c->queuelock);

  p->cpu_id = c - cpus; 

  if(c->type == CPU_P_CORE){
    push_cpu_sorted(c, p);
  }
  else{ // FIFO policy for rr algorithm
    p->next = 0;
    if(c->runq_head == 0){
      c->runq_head = p;
      c->runq_tail = p;
    } else {
      c->runq_tail->next = p;
      c->runq_tail = p;
    }
    c->proc_count++;
  }
  
  release(&c->queuelock);
}

struct proc*
pop_cpu(struct cpu *c) // pop from head works for both FCFS and RR
{
  struct proc *p = 0;
  
  acquire(&c->queuelock);
  
  if(c->runq_head != 0){
    p = c->runq_head;
    c->runq_head = p->next;
    
    if(c->runq_head == 0)
      c->runq_tail = 0;
      
    c->proc_count--;
  }
  
  release(&c->queuelock);
  return p;
}

struct cpu*
find_best_ecore(void) // for load balancing among E-cores
{
  int min_procs = NPROC;
  struct cpu *best_cpu = 0;

  for(int i = 0; i < ncpu; i++) {
    if (cpus[i].type == CPU_E_CORE) {
      
      acquire(&cpus[i].queuelock);
      int count = cpus[i].proc_count;
      release(&cpus[i].queuelock);

      if (count < min_procs) {
        min_procs = count;
        best_cpu = &cpus[i];
      }
    }
  }
  
  if (best_cpu == 0) return &cpus[0];
  return best_cpu;
}

struct cpu*
find_best_pcore(void) // for load balancing among P-cores
{
  int min_procs = NPROC;
  struct cpu *best_cpu = 0;

  for(int i = 0; i < ncpu; i++) {
    if (cpus[i].type == CPU_P_CORE) {
      
      acquire(&cpus[i].queuelock);
      int count = cpus[i].proc_count;
      release(&cpus[i].queuelock);

      if (count < min_procs) {
        min_procs = count;
        best_cpu = &cpus[i];
      }
    }
  }
  
  // if (best_cpu == 0) return &cpus[0];
  return best_cpu;
}
// =============================================================


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

  // [NEW] Set initial process priority
  p->priority = PRIORITY_NORMAL;

  
  // this assignment to p->state lets other cores
  // run this process. the acquire forces the above
  // writes to be visible, and the lock is also needed
  // because the assignment might not be atomic.
  acquire(&ptable.lock);
  
  p->state = RUNNABLE;
  struct cpu *best_cpu = find_best_ecore(); // CHANGE cpu queue
  push_cpu(best_cpu, p); // CHANGE cpu queue

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

  // [NEW] Parent Priority to Child
  np->priority = curproc->priority;

  pid = np->pid;

  acquire(&ptable.lock);

  np->state = RUNNABLE;
  // struct cpu *best_cpu = find_best_pcore(); // test preemption for FCFS
  struct cpu *best_cpu = find_best_ecore(); // CHANGE cpu queue
  push_cpu(best_cpu, np); // CHANGE cpu queue

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

  acquire(&ptable.lock);

  // Parent might be sleeping in wait().
  wakeup1(curproc->parent);

  // Pass abandoned children to init.
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->parent == curproc){
      p->parent = initproc;
      if(p->state == ZOMBIE)
        wakeup1(initproc);
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

//PAGEBREAK: 42
// Per-CPU process scheduler.
// Each CPU calls scheduler() after setting itself up.
// Scheduler never returns.  It loops, doing:
//  - choose a process to run
//  - swtch to start running that process
//  - eventually that process transfers control
//      via swtch back to the scheduler.
// void
// scheduler(void)
// {
//   struct proc *p;
//   struct cpu *c = mycpu();
//   c->proc = 0;
  
//   for(;;){
//     // Enable interrupts on this processor.
//     sti();

//     acquire(&ptable.lock);

//     struct proc *chosen_proc = 0;
//     int lowest_priority_value = PRIORITY_LOW + 1; 

//     for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
//       if (p->state != RUNNABLE) 
//         continue;
        
//       // Check if this process has a higher priority (lower numerical value)
//       if (p->priority < lowest_priority_value) {
//         lowest_priority_value = p->priority;
//         chosen_proc = p; 
//       }
//     }
    
//     if (chosen_proc) {
//       p = chosen_proc;
      
//       // Switch to chosen process.
//       c->proc = p;
//       switchuvm(p);
//       p->state = RUNNING;

//       swtch(&(c->scheduler), p->context);
//       switchkvm();

//       c->proc = 0;
//     } 
    
//     release(&ptable.lock);
//   }
// }


// === CHANGE for FCSFS algorithm ===
int
check_fcfs_preemption(void)
{
  struct cpu *c = mycpu();
  struct proc *running_p = myproc();
  int should_yield = 0;

  acquire(&c->queuelock);
  
  if(c->runq_head != 0 && c->runq_head->arrival_time_to_system < running_p->arrival_time_to_system){
    should_yield = 1;
  }
  
  release(&c->queuelock);
  return should_yield;
}
// =================================

// === CHANGE load balancing ===
void
monitor_load_balancing(void)
{
  struct cpu *c = mycpu();
  struct cpu *target = find_best_pcore();
  struct proc *p;

  acquire(&c->queuelock);

  if (target && c->proc_count >= target->proc_count + THRESHOLD_LOAD_BALANCE) { // we don't take target's lock because of potential deadlock
      
      p = c->runq_head;

      if (p && p->pid > 2) { // do not migrate init and sh processes
          // pop from c queue
          c->runq_head = p->next;
          if(c->runq_head == 0)
            c->runq_tail = 0;
          c->proc_count--;

          p->next = 0;
          release(&c->queuelock);
          push_cpu(target, p);
          return;
      }
  }
  
  release(&c->queuelock);
}// =============================

// === CHANGE general scheduler for both algorithms ===
void
scheduler(void)
{
  struct cpu *c = mycpu();
  c->proc = 0;
  
  for(;;){
    sti();

    // Check CPU type and call appropriate scheduler
    struct proc *p;

    // Try to get a process from this CPU's run queue
    p = pop_cpu(c);

      if(p){
        c->proc = p;

        // Switch to user virtual memory
        switchuvm(p);

        acquire(&ptable.lock);
        p->state = RUNNING;

        // Context switch to the process
        swtch(&(c->scheduler), p->context);

        // Process is done running for now (yielded or exited)
        switchkvm();
        c->proc = 0;
        release(&ptable.lock);
      }
  }
}
// =======================================================

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
  push_cpu(mycpu(), myproc()); // CHANGE cpu queue
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

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->state == SLEEPING && p->chan == chan){
      p->state = RUNNABLE;

      // ===== CHANGE cpu queue (Hybrid Wakeup) ======

      if(p->cpu_id >= 0 && p->cpu_id < ncpu) {
        struct cpu *prev_cpu = &cpus[p->cpu_id];
        
        // If it was on P-CORE -> Stay on P-Core
        if(prev_cpu->type == CPU_P_CORE){
           push_cpu(prev_cpu, p);
        }
        // If it was on E-CORE -> Find best E-Core for load balancing
        else {
           struct cpu *best_cpu = find_best_ecore();
           push_cpu(best_cpu, p);
        }
      }
      else {
        struct cpu *best_cpu = find_best_ecore();
        push_cpu(best_cpu, p);
      }
      // ==============================================
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
      if(p->state == SLEEPING){
        p->state = RUNNABLE;

        // ===== CHANGE cpu queue ======
        if(p->cpu_id >= 0 && p->cpu_id < ncpu)
          push_cpu(&cpus[p->cpu_id], p);
        else{
          struct cpu *best_cpu = find_best_ecore();
          push_cpu(best_cpu, p);
          // push_cpu(mycpu(), p);
        }
        // ==============================
      }
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

  cprintf("\nPID\tState\tName\tCPU\tPriority\n");

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->state == UNUSED)
      continue;
    if(p->state >= 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "???";
    
    cprintf("%d\t%s\t%s\t%d\t%d", p->pid, state, p->name, p->cpu_id, p->priority);

    if(p->state == SLEEPING){
      getcallerpcs((uint*)p->context->ebp+2, pc);
      for(i=0; i<10 && pc[i] != 0; i++)
        cprintf(" %p", pc[i]);
    }
    cprintf("\n");
  }
}

int
find_proc_family(int pid){
  struct proc *p = 0;
  struct proc *target = 0;

  acquire(&ptable.lock);
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->pid == pid)
      target = p;
  }

  if(target == 0){
    cprintf("No process with pid %d\n", pid);
    release(&ptable.lock);
    return -1;
  }

  cprintf("My id: %d, My parent id: %d\n", pid, target->parent->pid);

  // find children
  int has_child = 0;
  cprintf("Children of process %d:\n", pid);
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->parent == target){
      cprintf("Child pid: %d\n", p->pid);
      has_child = 1;
    }
  }
  if(!has_child)
    cprintf("there is no children for process %d\n", pid);

  // find siblings
  int has_sibling = 0;
  cprintf("Siblings of process %d:\n", pid);
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->parent == target->parent && p->pid != pid){
      has_sibling = 1;
      cprintf("Sibling pid: %d\n", p->pid);
    }
  }
  if(!has_sibling)
    cprintf("there is no siblings for process %d\n", pid);

  release(&ptable.lock);
  return 0;
}


int
set_priority_syscall_Helper(int pid, int priority) {
  struct proc *p;

  // Validate priority value
  if(priority < PRIORITY_HIGH || priority > PRIORITY_LOW) {
    cprintf("Invalid priority value. Use 0 (High), 1 (Normal), or 2 (Low).\n");
    return -1;
  }

  acquire(&ptable.lock);
  
  // Search for the process with the given PID
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
    if(p->pid == pid) {
      p->priority = priority;
      release(&ptable.lock);
      return 0;
    }
  }

  release(&ptable.lock);
  cprintf("Process with PID %d not found.\n", pid);
  return -1;
}



// ===== Debug systemcalls ======
// Helper function to calculate number of digits for formatting
// Helper to get number of digits
int get_len(int val) {
  if (val == 0) return 1;
  int cnt = 0;
  if (val < 0) { val = -val; cnt++; }
  for (; val > 0; val /= 10) cnt++;
  return cnt;
}

// Process states
static char *p_states[] = {
  [UNUSED]    "UNUSED",
  [EMBRYO]    "EMBRYO",
  [SLEEPING]  "SLEEP ",
  [RUNNABLE]  "RUNBLE",
  [RUNNING]   "RUN   ",
  [ZOMBIE]    "ZOMBIE"
};

void
print_process_info(void)
{
  struct proc *p;
  int j, c_id;
  int now = ticks;

  acquire(&print_lock);

  // Lock ptable and all queues to freeze state
  acquire(&ptable.lock);
  for(j = 0; j < ncpu; j++){
    acquire(&cpus[j].queuelock);
  }

  // --- Part 1: CPU Queues ---
  cprintf("\n--- CPU QUEUES (Tick: %d) ---\n", now);
  
  for(j = 0; j < ncpu; j++){
    struct cpu *c = &cpus[j];
    char *mode = (c->type == CPU_E_CORE) ? "RR" : "FCFS";
    
    cprintf("CPU %d [%s] cnt=%d: ", j, mode, c->proc_count);
    
    if(c->runq_head == 0){
      cprintf("(empty)");
    } else {
      // Print queue linked list
      struct proc *tmp = c->runq_head;
      cprintf("Head: ");
      while(tmp){
        cprintf("%d", tmp->pid);
        if(tmp->next) cprintf("->");
        tmp = tmp->next;
      }
      cprintf(" :Tail");
    }
    cprintf("\n");
  }

  // --- Part 2: Process Details ---
  cprintf("\n--- Process Details ---\n");
  cprintf("PID    State     Name         CPU        Algo    ArrTime    Age\n");
  cprintf("---------------------------------------------------------------\n");

  // Iterate per CPU
  for (c_id = 0; c_id < ncpu; c_id++) {
      
      for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
        if(p->state == UNUSED) continue;
        if(p->cpu_id != c_id) continue; 

        // PID
        cprintf("%d", p->pid);
        for(int k=0; k < 7 - get_len(p->pid); k++) cprintf(" ");

        // State
        char *st = (p->state >= 0 && p->state < 6) ? p_states[p->state] : "???";
        cprintf("%s    ", st);

        // Name
        cprintf("%s", p->name);
        int len = strlen(p->name);
        for(int k=0; k < 13 - len; k++) cprintf(" ");

        // CPU ID
        cprintf("%d          ", p->cpu_id);

        // Algo Name
        char *alg = (cpus[p->cpu_id].type == CPU_E_CORE) ? "RR" : "FCFS";
        cprintf("%s", alg);
        for(int k=0; k < 8 - strlen(alg); k++) cprintf(" ");

        // Arrival
        cprintf("%d", p->arrival_time_to_system);
        for(int k=0; k < 11 - get_len(p->arrival_time_to_system); k++) cprintf(" ");

        // Age
        cprintf("%d\n", now - p->arrival_time_to_system);
      }
  }

  // Print processes without CPU (just in case)
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->state == UNUSED) continue;
      if(p->cpu_id >= 0 && p->cpu_id < ncpu) continue;

      cprintf("%d", p->pid);
      for(int k=0; k < 7 - get_len(p->pid); k++) cprintf(" ");
      
      char *st = (p->state >= 0 && p->state < 6) ? p_states[p->state] : "???";
      cprintf("%s    ", st);
      
      cprintf("%s", p->name);
      for(int k=0; k < 13 - strlen(p->name); k++) cprintf(" ");
      
      cprintf("Global     -       ");
      
      cprintf("%d", p->arrival_time_to_system);
      for(int k=0; k < 11 - get_len(p->arrival_time_to_system); k++) cprintf(" ");
      cprintf("%d\n", now - p->arrival_time_to_system);
  }
  
  cprintf("---------------------------------------------------------------\n");

  // Release locks
  for(j = 0; j < ncpu; j++){
    release(&cpus[j].queuelock);
  }
  release(&ptable.lock);
  release(&print_lock);
}