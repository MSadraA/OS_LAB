#include "types.h"
#include "user.h"
int main(int argc, char *argv[])
{
    sleep(100);
    int pids[10];
    for (int i = 0; i < 10; i++)
    {
        pids[i] = fork();
        if (pids[i] < 0)
        {
            printf(1, "failed to create customer process\n");
            exit();
        }
        if (pids[i] == 0)
        {
            customer_arrive();
            exit();
        }
        sleep(5);
    }
    for (int i = 0; i < 10; i++)
    {
        wait();
    }
    exit();
}