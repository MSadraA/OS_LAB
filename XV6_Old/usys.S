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
SYSCALL(next_palindrome)
SYSCALL(set_sleep_syscall)
SYSCALL(get_system_time)

SYSCALL(make_user)
SYSCALL(login)
SYSCALL(logout)
SYSCALL(logs)
SYSCALL(diff)

SYSCALL(create_realtime_process)
SYSCALL(change_process_queue)
SYSCALL(print_process_info)
SYSCALL(barber_sleep)
SYSCALL(customer_arrive)
SYSCALL(cut_hair)
SYSCALL(init_rw_lock)
SYSCALL(get_rw_pattern)
SYSCALL(critical_section)

SYSCALL(list_programs)

