#include "types.h"
#include "user.h"
int main(int argc, char *argv[])
{
  if (argc != 2)
  {
    printf(1, "wrong command\n");
    exit();
  }
  int pattern = atoi(argv[1]);
  int number_of_processes = 0;
  int calculate_number_of_processes = pattern;
  printf(1, "target number is:%s\n", argv[1]);
  while (1)
  {
    calculate_number_of_processes /= 2;
    if (calculate_number_of_processes == 0)
      break;
    number_of_processes++;
  }
  init_rw_lock();
  get_rw_pattern(pattern);
  for (int i = 0; i < number_of_processes; i++)
  {
    int pid = fork();
    if (pid < 0)
    {
      printf(1, "Fork didn't work\n");
      exit();
    }
    else if (pid == 0)
    {
      critical_section();
      exit();
    }
  }
  for (int i = 0; i < number_of_processes; i++)
    wait();
  exit();
}