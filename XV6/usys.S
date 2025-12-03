#include "syscall.h"
#include "traps.h"

#define SYSCALL(name) \
  .globl name; \
  name: \
    movl $SYS_ ## name, %eax; \
    int $T_SYSCALL; \
    ret

SYSCALL(fork)
SYSCALL(exit)
SYSCALL(wait)
SYSCALL(pipe)
SYSCALL(read)
SYSCALL(write)
SYSCALL(close)
SYSCALL(kill)
SYSCALL(exec)
SYSCALL(open)
SYSCALL(mknod)
SYSCALL(unlink)
SYSCALL(fstat)
SYSCALL(link)
SYSCALL(mkdir)
SYSCALL(chdir)
SYSCALL(dup)
SYSCALL(getpid)
SYSCALL(sbrk)
SYSCALL(sleep)
SYSCALL(uptime)
SYSCALL(make_duplicate)
SYSCALL(show_process_family)
SYSCALL(grep_syscall)
SYSCALL(set_priority_syscall)
SYSCALL(print_process_info)
SYSCALL(start_measuring)
SYSCALL(stop_measuring)

.globl simple_arithmetic_syscall
simple_arithmetic_syscall:
  movl 4(%esp), %ebx // a parameter to ebx register
  movl 8(%esp), %ecx // b parameter to ecx register

  movl $SYS_simple_arithmetic_syscall, %eax // syscall number to eax register
  int $T_SYSCALL       
  ret
