#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "x86.h"
#include "syscall.h"
#include "user_mgmt.h"

// User code makes a system call with INT T_SYSCALL.
// System call number in %eax.
// Arguments on the stack, from the user call to the C
// library system call function. The saved user %esp points
// to a saved program counter, and then the first argument.

// Fetch the int at addr from the current process.
int
fetchint(uint addr, int *ip)
{
  struct proc *curproc = myproc();

  if(addr >= curproc->sz || addr+4 > curproc->sz)
    return -1;
  *ip = *(int*)(addr);
  return 0;
}

// Fetch the nul-terminated string at addr from the current process.
// Doesn't actually copy the string - just sets *pp to point at it.
// Returns length of string, not including nul.
int
fetchstr(uint addr, char **pp)
{
  char *s, *ep;
  struct proc *curproc = myproc();

  if(addr >= curproc->sz)
    return -1;
  *pp = (char*)addr;
  ep = (char*)curproc->sz;
  for(s = *pp; s < ep; s++){
    if(*s == 0)
      return s - *pp;
  }
  return -1;
}

// Fetch the nth 32-bit system call argument.
int
argint(int n, int *ip)
{
  return fetchint((myproc()->tf->esp) + 4 + 4*n, ip);
}

// Fetch the nth word-sized system call argument as a pointer
// to a block of memory of size bytes.  Check that the pointer
// lies within the process address space.
int
argptr(int n, char **pp, int size)
{
  int i;
  struct proc *curproc = myproc();
 
  if(argint(n, &i) < 0)
    return -1;
  if(size < 0 || (uint)i >= curproc->sz || (uint)i+size > curproc->sz)
    return -1;
  *pp = (char*)i;
  return 0;
}

// Fetch the nth word-sized system call argument as a string pointer.
// Check that the pointer is valid and the string is nul-terminated.
// (There is no shared writable memory, so the string can't change
// between this check and being used by the kernel.)
int
argstr(int n, char **pp)
{
  int addr;
  if(argint(n, &addr) < 0)
    return -1;
  return fetchstr(addr, pp);
}

extern int sys_chdir(void);
extern int sys_close(void);
extern int sys_dup(void);
extern int sys_exec(void);
extern int sys_exit(void);
extern int sys_fork(void);
extern int sys_fstat(void);
extern int sys_getpid(void);
extern int sys_kill(void);
extern int sys_link(void);
extern int sys_mkdir(void);
extern int sys_mknod(void);
extern int sys_open(void);
extern int sys_pipe(void);
extern int sys_read(void);
extern int sys_sbrk(void);
extern int sys_sleep(void);
extern int sys_unlink(void);
extern int sys_wait(void);
extern int sys_write(void);
extern int sys_uptime(void);
extern int sys_next_palindrome(void);
extern int sys_set_sleep_syscall(void);
extern int sys_get_system_time(void);
// log sys calls
extern int sys_make_user(void);
extern int sys_login(void);
extern int  sys_logout(void);
extern int  sys_logs(void);
// diff
extern int sys_diff(void);

extern int sys_list_programs(void);

extern int sys_create_realtime_process(void); //additional
extern int sys_change_process_queue(void); 
extern int sys_print_process_info(void);
extern int sys_barber_sleep(void);
extern int sys_customer_arrive(void);
extern int sys_cut_hair(void);
extern int sys_init_rw_lock(void);
extern int sys_get_rw_pattern(void);
extern int sys_critical_section(void);

static int (*syscalls[])(void) = {
[SYS_fork]    sys_fork,
[SYS_exit]    sys_exit,
[SYS_wait]    sys_wait,
[SYS_pipe]    sys_pipe,
[SYS_read]    sys_read,
[SYS_kill]    sys_kill,
[SYS_exec]    sys_exec,
[SYS_fstat]   sys_fstat,
[SYS_chdir]   sys_chdir,
[SYS_dup]     sys_dup,
[SYS_getpid]  sys_getpid, 
[SYS_sbrk]    sys_sbrk,
[SYS_sleep]   sys_sleep,
[SYS_uptime]  sys_uptime,
[SYS_open]    sys_open,
[SYS_write]   sys_write,
[SYS_mknod]   sys_mknod,
[SYS_unlink]  sys_unlink,
[SYS_link]    sys_link,
[SYS_mkdir]   sys_mkdir,
[SYS_close]   sys_close,
[SYS_next_palindrome] sys_next_palindrome,
[SYS_set_sleep_syscall] sys_set_sleep_syscall,
[SYS_get_system_time] sys_get_system_time,

[SYS_make_user] sys_make_user,
[SYS_login] sys_login,
[SYS_logout] sys_logout,
[SYS_logs] sys_logs,

// diff
[SYS_diff] sys_diff,


[SYS_create_realtime_process] sys_create_realtime_process, //additional
[SYS_change_process_queue] sys_change_process_queue, 
[SYS_print_process_info] sys_print_process_info,
[SYS_barber_sleep] sys_barber_sleep,
[SYS_customer_arrive] sys_customer_arrive,
[SYS_cut_hair] sys_cut_hair,
[SYS_init_rw_lock] sys_init_rw_lock,
[SYS_get_rw_pattern] sys_get_rw_pattern,
[SYS_critical_section] sys_critical_section,

[SYS_list_programs] sys_list_programs,
};

int 
should_log_syscall(int num) {
  int allowed[] = {
    SYS_make_user,
    SYS_login,
    SYS_logout,
    SYS_next_palindrome,
    SYS_mkdir,
    SYS_open,
    SYS_unlink,
    SYS_exec,
    SYS_fork,
    SYS_kill,
    SYS_wait,
    SYS_chdir,
    SYS_getpid,
    SYS_sbrk,
    SYS_sleep,
    SYS_uptime,
    SYS_logs,
  };
  int len = sizeof(allowed) / sizeof(int);
  for(int i = 0; i < len; i++)
    if (allowed[i] == num)
      return 1;
  return 0;
}


void
syscall(void)
{
  int num;
  struct proc *curproc = myproc();

  num = curproc->tf->eax;
  if(num > 0 && num < NELEM(syscalls) && syscalls[num]) {
    curproc->tf->eax = syscalls[num]();
    
    //Add authorized system call
    if (curproc->name[0] != '\0' && curproc->pid > 2 && should_log_syscall(num)) {
      log_syscall(num);
    }

  } else {
    cprintf("%d %s: unknown sys call %d\n",
            curproc->pid, curproc->name, num);
    curproc->tf->eax = -1;
  }
}
