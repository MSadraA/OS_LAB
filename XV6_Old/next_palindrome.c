#include "types.h"
#include "user.h"
#define WRONGINPUTTRYAGAIN "invalid command!try again"
int main(int argc,char *argv[])
{
    if(argc!=2)
    {
        printf(1,"%s\n",WRONGINPUTTRYAGAIN);
        exit();
    }
    int num=atoi(argv[1]),prev_val_ebx;
    asm volatile(
        "movl %%ebx, %0\n\t"
        "movl %1, %%ebx"
        :"=r" (prev_val_ebx)
        :"r" (num)
        :"ebx" 
    );
    next_palindrome(num);
    asm volatile(
        "movl %0, %%ebx"
        :
        :"r" (prev_val_ebx)
        :"ebx" 
    );
    exit();
}