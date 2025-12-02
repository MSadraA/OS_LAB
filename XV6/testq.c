#include "types.h"
#include "user.h"
#include "fcntl.h"

#define STORM_COUNT 50    
#define LOAD_COUNT  12   
#define MIXED_COUNT 8    

void cpu_bound_task(int id) {
    int start_time = uptime();
    volatile int i = 0; 
    while (uptime() < start_time + 50) {
        i++;
    }
}

void io_bound_task(int id) {
    int i;
    volatile int j; 
    for (i = 0; i < 20; i++) {
        sleep(3);
        for(j = 0; j < 5000000; j++) { 
        }
    }
}
void test_fork_and_exit() {
    printf(1, "\n[TEST 1] Testing Fork and Exit storm (Heavy)...\n");
    int pid;
    for (int i = 0; i < STORM_COUNT; i++) {
        pid = fork();
        if (pid < 0) {
            printf(1, "Fork failed!\n");
            exit();
        }
        if (pid == 0) {
            exit();
        }
    }
    for (int i = 0; i < STORM_COUNT; i++) {
        wait();
    }
    printf(1, "[PASS] Fork/Exit storm completed.\n");
}

void test_cpu_load() {
    printf(1, "\n[TEST 2] Testing CPU Bound processes (Heavy Load)...\n");
    int pid;
    for (int i = 0; i < LOAD_COUNT; i++) {
        pid = fork();
        if (pid == 0) {
            cpu_bound_task(getpid());
            exit();
        }
    }
    
    sleep(10); 
    
    printf(1, "--- Snapshot during CPU load (Expect RUNNABLE processes) ---\n");
    print_process_info(); 
    
    for (int i = 0; i < LOAD_COUNT; i++) wait();
    printf(1, "[PASS] CPU load test completed.\n");
}

void test_io_load() {
    printf(1, "\n[TEST 3] Testing IO Bound processes (Heavy IO)...\n");
    int pid;
    for (int i = 0; i < LOAD_COUNT; i++) {
        pid = fork();
        if (pid == 0) {
            io_bound_task(getpid());
            exit();
        }
    }
    
    sleep(10); 
    printf(1, "--- Snapshot during IO load ---\n");
    print_process_info();

    for (int i = 0; i < LOAD_COUNT; i++) wait();
    printf(1, "[PASS] IO load test completed.\n");
}

void test_mixed_load() {
    printf(1, "\n[TEST 4] Testing Mixed Load (Heavy CPU + IO)...\n");
    
    for(int i=0; i<MIXED_COUNT; i++){
        if(fork() == 0){
            cpu_bound_task(getpid());
            exit();
        }
    }
    
    for(int i=0; i<MIXED_COUNT; i++){
        if(fork() == 0){
            io_bound_task(getpid());
            exit();
        }
    }

    sleep(10);
    printf(1, "--- Snapshot during Mixed load ---\n");
    print_process_info();

    for(int i=0; i < (MIXED_COUNT * 2); i++) wait();
    printf(1, "[PASS] Mixed load test completed.\n");
}

int main(int argc, char *argv[]) {
    printf(1, "Starting Scheduler Stress Tests (HEAVY MODE)...\n");

    test_fork_and_exit();
    test_cpu_load();
    test_io_load();
    test_mixed_load();

    printf(1, "\nALL TESTS PASSED SUCCESSFULLY!\n");
    exit();
}