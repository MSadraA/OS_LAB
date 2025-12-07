#include "types.h"
#include "user.h"

void delay(int ticks_duration) {
    int start = uptime();
    while (uptime() - start < ticks_duration) {
        int x = 0;
        x = x + 1; 
    }
}


void test_1(int n) {
    printf(1, "\nTest 1: RR and Migration\n");
    start_measuring();
    printf(1, "making %d procs...\n", n);

    int pid;
    for (int i = 0; i < n; i++) {
        pid = fork();
        if (pid == 0) {
            // print_process_info();
            delay(500); 
            exit();
        }
    }

    printf(1, "\nStart info:\n");
    print_process_info();

    printf(1, "\nChecking RR:\n");
    for (int i = 0; i < 50; i++) {
        // delay(27);
        printf(1, "snap %d:\n", uptime());
        print_process_info();
    }

    for(int i = 0; i < n; i++) {
        wait();
    }

    stop_measuring();
    printf(1, "Test 1 done...\n");
    exit();
}

void test_2(int n) {
    printf(1, "\nTest 2: FCFS Finish\n");
    printf(1, "making %d procs...\n", n);
    start_measuring();
    int pid;

    for (int i = 0; i < n; i++) {
        pid = fork();
        if (pid == 0) {
            delay(500); 
            exit();
        }
    }

    printf(1, "\nChecking finish:\n");
    for (int i = 0; i < 5; i++) {
        delay(200); 
        print_process_info();
    }

    for (int i = 0; i < n; i++) {
        wait();
    }
    stop_measuring();
    printf(1, "Test 2 done.\n");
    exit();
}

void test_preemption_for_fcfs() { // change fork function (cpu size >= 4)
    int senior_count = 3;
    int junior_count = 3;
    int pid;

    printf(1, "\n=== FCFS Preemption Test ===\n");
    start_measuring();
    int start_time = uptime();

    for (int i = 0; i < senior_count; i++) {
        pid = fork();
        if (pid == 0) {
            sleep(50); 
            delay(500);
            exit();
        }
        printf(1, "[Info] Senior Created (PID: %d)\n", pid);
    }

    delay(2); 

    for (int i = 0; i < junior_count; i++) {
        pid = fork();
        if (pid == 0) {
            delay(500); 
            exit();
        }
        printf(1, "[Info] Junior Created (PID: %d)\n", pid);
    }

    delay(2);

    while(uptime() < start_time + 40) {
        delay(5); 
    }

    printf(1, "\n--- Checking for Preemption ---\n");

    for (int i = 0; i < 8; i++) {
        int now = uptime();
        printf(1, "\n>>> SNAPSHOT (Time: %d) <<<\n", now);
        print_process_info();
        
        delay(200); 
    }

    
    for (int i = 0; i < senior_count + junior_count; i++) {
        wait();
    }
    stop_measuring();
    printf(1, "\n=== Test FCFS Preemption Finished ===\n");
    exit();
}

int main(int argc, char *argv[]) {
    int n_procs = 20; 
    test_1(n_procs);
    // test_2(n_procs);
    // test_preemption_for_fcfs();
    return 0;
}

