#include "types.h"
#include "user.h"

#define UNABLE_TO_CREATE_PROCESS "unable to create a process\n"

enum class_and_level {
    WITHOUT_PRIORITY,
    EARLIEST_DEADLINE_FIRST,
    MULTILEVEL_FEEDBACK_QUEUE_FIRST_LEVEL,
    MULTILEVEL_FEEDBACK_QUEUE_SECOND_LEVEL
};

int REAL_TIME_PROCESS_DEADLINE = 500; 

void delay() {
    volatile int i, j;
    int slices = 10000;
    for(i = 0; i < slices; i++) {
        for(j = 0; j < 5000; j++) {
            continue;
        }
    }
}

int main(void)
{
    int pid1 = fork();
    if (pid1 < 0) {
        printf(1, UNABLE_TO_CREATE_PROCESS);
        exit();
    } else if (pid1 == 0) {
        change_process_queue(getpid(), MULTILEVEL_FEEDBACK_QUEUE_FIRST_LEVEL);
        delay();
        print_process_info();
        delay();
        exit();
    }

    int pid2 = fork();
    if (pid2 < 0) {
        printf(1, UNABLE_TO_CREATE_PROCESS);
        exit();
    } else if (pid2 == 0) {
        change_process_queue(getpid(), MULTILEVEL_FEEDBACK_QUEUE_FIRST_LEVEL);
        delay();
        print_process_info();
        delay();
        exit();
    }

    int pid3 = fork();
    if (pid3 < 0) {
        exit();
    } else if (pid3 == 0) {
        create_realtime_process(1000);
        delay();
        print_process_info();
        delay();
        exit();
    }

    int pid4 = fork();
    if (pid4 < 0) {
        printf(1, UNABLE_TO_CREATE_PROCESS);
        exit();
    } else if (pid4 == 0) {
        create_realtime_process(1000);
        delay();
        print_process_info();
        delay();
        exit();
    }

    int pid5 = fork();
    if (pid5 < 0) {
        printf(1, UNABLE_TO_CREATE_PROCESS);
        exit();
    } else if (pid5 == 0) {
        change_process_queue(getpid(), MULTILEVEL_FEEDBACK_QUEUE_SECOND_LEVEL);
        delay();
        delay();
        exit();
    }

    for (int i = 0; i < 5; i++) {
        wait();
    }
    exit();
}
