#include "types.h"
#include "x86.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "spinlock.h"

int
sys_fork(void)
{
  return fork();
}

int
sys_exit(void)
{
  exit();
  return 0;  // not reached
}

int
sys_wait(void)
{
  return wait();
}

int
sys_kill(void)
{
  int pid;

  if(argint(0, &pid) < 0)
    return -1;
  return kill(pid);
}

int
sys_getpid(void)
{
  return myproc()->pid;
}

int
sys_sbrk(void)
{
  int addr;
  int n;

  if(argint(0, &n) < 0)
    return -1;
  addr = myproc()->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

int
sys_sleep(void)
{
  int n;
  uint ticks0;

  if(argint(0, &n) < 0)
    return -1;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(myproc()->killed){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

// return how many clock tick interrupts have occurred
// since start.
int
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

int
sys_simple_arithmetic_syscall(void)
{
  int a, b, result;
  struct proc *curproc = myproc();

  a = curproc->tf->ebx; // read 'a' from EBX in the current process kernel stack
  b = curproc->tf->ecx; // read 'b' from ECX in the current process kernel stack

  result = (a + b) * (a - b); // 

  cprintf("Calc: (%d+%d)*(%d-%d)=%d\n", a, b, a, b, result);

  return result;
}

int
sys_show_process_family(void){
  int pid;

  if(argint(0, &pid) < 0){
    cprintf("can't get pid\n");
    return -1;
  }
  return find_proc_family(pid);
}

// [NEW]
extern struct {
  struct spinlock lock;
  struct proc proc[NPROC];
} ptable;

int 
sys_set_priority_syscall(void) {
  int pid, priority;
  struct proc *p;

  // Read arguments from the syscall stack  
  if(argint(0, &pid) < 0 || argint(1, &priority) < 0) {
    cprintf("Invalid arguments for set_priority_syscall\n");
    return -1;
  }

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