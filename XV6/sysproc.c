#include "types.h"
#include "x86.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"

#include "spinlock.h"
#include "sleeplock.h"

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

int 
sys_set_priority_syscall(void) {
  int pid, priority;

  // Read arguments from the syscall stack  
  if(argint(0, &pid) < 0 || argint(1, &priority) < 0) {
    cprintf("Invalid arguments for set_priority_syscall\n");
    return -1;
  }

  return set_priority_syscall_Helper(pid, priority);
}

int
sys_print_process_info(void) {
  print_process_info();
  return 0;
}

int
sys_start_measuring(void)
{
  return cpu_start_measuring();
}

int
sys_stop_measuring(void)
{
  return cpu_stop_measuring();
}

// Define a global test lock within the kernel
// used for LAB4
// NOTE: we could have declared testlock and test_rwlock
// in the test.c files too but! 
struct sleeplock testlock;
void
testlockinit(void)
{
  initsleeplock(&testlock, "test_lock");
}

int 
sys_acquire_test_lock(void)
{
  acquiresleep(&testlock);
  return 0;
}

int
sys_release_test_lock(void)
{
  releasesleep(&testlock);
  return 0;
}

// LAB4
#include "rwlock.h"
struct rwlock test_rwlock;
void
testrwlockinit(void)
{
  rwlock_init(&test_rwlock, "test_rwlock");
}

int
sys_rwlock_acquire_read(void)
{
  rwlockAcquireRead(&test_rwlock);
  return 0;
}

int
sys_rwlock_release_read(void)
{
  rwlockReleaseRead(&test_rwlock);
  return 0;
}

int
sys_rwlock_acquire_write(void)
{
  rwlockAcquireWrite(&test_rwlock);
  return 0;
}

int
sys_rwlock_release_write(void)
{
  rwlockReleaseWrite(&test_rwlock);
  return 0;
}

int
sys_getlockstat(void)
{
  uint *scores;

  if(argptr(0, (void*)&scores, sizeof(uint) * NCPU) < 0)
    return -1;

  return getlockstat(scores);
}

// LAB4: plock
#include "plock.h"
extern struct plock global_plock;

int
sys_plock_acquire(void)
{
  int priority;
  
  if(argint(0, &priority) < 0)
    return -1;
    
  plock_acquire(&global_plock, priority);
  return 0;
}

int
sys_plock_release(void)
{
  plock_release(&global_plock);
  return 0;
}