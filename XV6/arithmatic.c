#include "types.h"
#include "stat.h"
#include "user.h"

int
main(int argc, char *argv[])
{
    int a = 5;
    int b = 3;
    int result = simple_arithmetic_syscall(a, b);
    printf(1 , "Return value: %d\n", result);
    exit();
}