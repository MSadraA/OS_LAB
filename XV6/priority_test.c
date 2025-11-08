#include "types.h"
#include "user.h"

// تعریف سطوح اولویت در فضای کاربر
#define PRIORITY_HIGH   0
#define PRIORITY_NORMAL 1
#define PRIORITY_LOW    2

void cpu_intensive_task(int pid) {
    // یک حلقه تکرار بزرگ برای مشغول نگه داشتن CPU
    volatile long i;
    for(i = 0; i < 999999999; i++) {
        // انجام عملیات ساده برای جلوگیری از بهینه‌سازی کامپایلر
        if (i % 100000000 == 0) {
            // چاپ پیشرفت در حین اجرا (اختیاری)
        }
    }
    printf(1, "Process %d finished its CPU task.\n", pid);
}

int
main(void)
{
    int pid_high, pid_low;

    printf(1, "Priority Scheduling Test Started.\n");
    
    // ایجاد فرزند اول (با اولویت بالا)
    pid_high = fork();
    if (pid_high == 0) {
        // فرزند ۱: اجرای کار سنگین
        cpu_intensive_task(getpid());
        exit();
    } else if (pid_high < 0) {
        printf(2, "Fork failed for high priority child.\n");
        exit();
    }

    // ایجاد فرزند دوم (با اولویت پایین)
    pid_low = fork();
    if (pid_low == 0) {
        // فرزند ۲: اجرای کار سنگین
        cpu_intensive_task(getpid());
        exit();
    } else if (pid_low < 0) {
        printf(2, "Fork failed for low priority child.\n");
        // باید فرزند اول را هم Kill کند یا منتظر بماند
        kill(pid_high);
        wait();
        exit();
    }

    // تنظیم اولویت‌ها توسط والد
    // فرزند اول: اولویت بالا (0)
    if (set_priority_syscall(pid_high, PRIORITY_HIGH) == 0) {
        printf(1, "Set PID %d to HIGH priority (%d).\n", pid_high, PRIORITY_HIGH);
    } else {
        printf(2, "Failed to set high priority for PID %d.\n", pid_high);
    }

    // فرزند دوم: اولویت پایین (2)
    if (set_priority_syscall(pid_low, PRIORITY_LOW) == 0) {
        printf(1, "Set PID %d to LOW priority (%d).\n", pid_low, PRIORITY_LOW);
    } else {
        printf(2, "Failed to set low priority for PID %d.\n", pid_low);
    }

    // تست حالت PID نامعتبر
    if (set_priority_syscall(99999, PRIORITY_HIGH) == -1) {
        printf(1, "Test Passed: Invalid PID check returned -1.\n");
    }

    // والد منتظر اتمام فرزندان می‌ماند
    wait();
    wait();

    printf(1, "Priority Scheduling Test Finished. Check the order of 'finished' messages.\n");
    exit();
}