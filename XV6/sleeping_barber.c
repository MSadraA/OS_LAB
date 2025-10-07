#include "types.h"
#include "user.h"
int main(int argc, char *argv[])
{
    if (argc != 1)
    {
        printf(1, "wrong command\n");
        exit();
    }
    int pid_barber = fork();
    if (pid_barber < 0)
    {
        printf(1, "failed to create barber\n");
        exit();
    }
    else if (pid_barber == 0)
    {
        printf(1, "barber with pid %d is ready to cut hair\n", getpid());
        sleep(50);
        int haircuts = 0;
        while (1)
        {
            barber_sleep();
            sleep(5);
            cut_hair();
            sleep(10);
        }
        exit();
    }
    int pid_customer = fork();
    if (pid_customer < 0)
    {
        printf(1, "failed to create customer\n");
        exit();
    }
    else if (pid_customer == 0)
    {
        char *customer_arg[] = {"customer", 0};
        exec("customer", customer_arg);
    }
    for (int i = 0; i < 2; i++)
        wait();
    exit();
}