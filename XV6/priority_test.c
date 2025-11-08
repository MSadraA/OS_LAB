#include "types.h"
#include "user.h"
#include "fcntl.h"

#define PRIORITY_HIGH   0
#define PRIORITY_NORMAL 1
#define PRIORITY_LOW    2

#define HEAVY_TASK_ITER 999999999

void cpu_intensive_task(int pid, int priority_level) {
    volatile long i;
    for(i = 0; i < HEAVY_TASK_ITER; i++) {
    }
    printf(1, "Process %d (Priority: %d) finished its CPU task.\n", pid, priority_level);
}

void run_test_scenario(int priorities[], int num_tests, char* scenario_name) {
    int pids[3];
    int i;

    printf(1, "\n\n=========================================\n");
    printf(1, "SCENARIO: %s (Running %d processes)\n", scenario_name, num_tests);
    printf(1, "=========================================\n");

    for (i = 0; i < num_tests; i++) {
        pids[i] = fork();
        if (pids[i] == 0) {
            sleep(1); 
            cpu_intensive_task(getpid(), priorities[i]);
            exit();
        } else if (pids[i] < 0) {
            printf(2, "Fork failed for test %d.\n", i);
            return;
        }
    }

    printf(1, "Parent setting priorities...\n");
    for (i = 0; i < num_tests; i++) {
        if (set_priority_syscall(pids[i], priorities[i]) == 0) {
            printf(1, "Set PID %d to priority (%d).\n", pids[i], priorities[i]);
        } else {
            printf(2, "Parent FAILED to set priority for PID %d.\n", pids[i]);
        }
    }
    
    if (set_priority_syscall(99999, PRIORITY_HIGH) == -1) {
      printf(1, "TEST PASS: Invalid PID (99999) check returned -1.\n");
    }

    for (i = 0; i < num_tests; i++) {
        wait();
    }
    printf(1, "-----------------------------------------\n");
    printf(1, "SCENARIO %s finished.\n", scenario_name);
}


int
main(void)
{
    int p1_priorities[] = {PRIORITY_HIGH, PRIORITY_LOW};
    run_test_scenario(p1_priorities, 2, "Basic Preemption (High vs Low)");

    int p2_priorities[] = {PRIORITY_HIGH, PRIORITY_NORMAL, PRIORITY_LOW};
    run_test_scenario(p2_priorities, 3, "Full Spectrum (High vs Normal vs Low)");

    int p3_priorities[] = {PRIORITY_NORMAL, PRIORITY_NORMAL};
    run_test_scenario(p3_priorities, 2, "Equal Priority (Normal vs Normal - Expected: RR)");
    
    int p4_priorities[] = {PRIORITY_NORMAL, PRIORITY_NORMAL, PRIORITY_NORMAL};
    run_test_scenario(p4_priorities, 3, "Stress RR (Normal x 3)");


    printf(1, "\nAll priority tests completed. Please examine finish order.\n");
    exit();
}