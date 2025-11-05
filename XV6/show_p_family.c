#include "types.h"
#include "user.h"

int
main(int argc, char *argv[])
{
  int pid1, pid2;
  int my_pid = getpid();   
  printf(1, "=== Process Family Test Program ===\n");
  printf(1, "Main Parent PID is: %d\n", my_pid);   
  pid1 = fork();   
  if(pid1 < 0){
    printf(2, "Test failed: fork 1 failed\n");
    exit();
  }  
  if(pid1 == 0) {
    sleep(5);
    
    printf(1, "\n--- Test run by: Child 1 (PID: %d) ---\n", getpid());
    show_process_family(getpid());
    exit();
  }  
  pid2 = fork();   
  if(pid2 < 0){
    printf(2, "Test failed: fork 2 failed\n");
    exit();
  }  
  if(pid2 == 0) {
    sleep(10);
    
    printf(1, "\n--- Test run by: Child 2 (PID: %d) ---\n", getpid());
    show_process_family(getpid());
    
    exit();
  }  
  sleep(15);
  
  printf(1, "\n--- Test run by: Main Parent (PID: %d) ---\n", my_pid);
  show_process_family(my_pid);   
  printf(1, "\n--- Test run by: Failure Case (PID: 99999) ---\n");
  int result = show_process_family(99999);
  
  if(result == -1){
    printf(1, "OK: Function correctly returned -1 for non-existent PID.\n");
  } else {
    printf(2, "FAILED: Function returned %d, but expected -1.\n", result);
  }  
  wait();
  wait();  
  printf(1, "\n=== Process Family Test Finished ===\n");
  exit();
}