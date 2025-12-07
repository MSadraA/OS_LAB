#include "types.h"
#include "user.h"
#include "fcntl.h"

const int PROCESS_COUNT = 4;

void cpu_bound_task(int id) {
    int start_time = uptime();
    volatile int i = 0; 
    while (uptime() < start_time + 50) {
        i++;
    }
}

int main(){
    printf(1 , "\n[TEST] Testing ticks...\n");
    for(int i = 0; i < PROCESS_COUNT; i++){
        int pid = fork();
        if(pid < 0){
            printf(1, "Fork failed!\n");
            exit();
        }
        if(pid == 0){
            cpu_bound_task(getpid());
            show_process_family(getpid());
            exit();
        }
    }

    for(int i = 0; i < PROCESS_COUNT; i++){
        wait();
    }
    printf(1, "[PASS] Ticks test completed.\n");
    exit();
}