#include "types.h"
#include "user.h"

int main(int argc, char *argv[])
{
  printf(1, "Test: Parent acquiring test lock...\n");
  
  // 1. Parent acquires the lock
  acquire_test_lock();
  printf(1, "Test: Parent acquired lock. Forking...\n");

  int pid = fork();

  if(pid < 0){
    printf(1, "Test: Fork failed\n");
    exit();
  }

  if(pid == 0){
    // Child Process
    printf(1, "Test: Child process running (pid: %d)...\n", getpid());
    
    // Slight delay to ensure parent definitely holds the lock (optional but good practice)
    sleep(10); 

    printf(1, "Test: Child attempting to release Parent's lock (Expect PANIC)...\n");
    
    // 2. Child tries to release the lock owned by Parent
    // This should trigger the panic in releasesleep
    release_test_lock();

    // If we reach here, the protection FAILED
    printf(1, "Test: FAILURE! Child released the lock without panic.\n");
    exit();
  } else {
    // Parent Process
    wait(); // Wait for child
    printf(1, "Test: Parent finished. (If you see this, panic did not happen or child exited normally)\n");
    
    // Clean up
    release_test_lock();
    exit();
  }
}