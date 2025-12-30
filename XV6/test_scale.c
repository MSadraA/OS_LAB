#include "types.h"
#include "user.h"
#include "fcntl.h"

// Define NCPU if not available in user space headers, usually 8 is safe max for xv6
#define NCPU 8

int main(int argc, char *argv[])
{
  uint scores_before[NCPU];
  uint scores_after[NCPU];
  int pid;
  int i, j;
  int n_children = 4; // Number of competing processes

  printf(1, "--- Starting Scalability Test (Contention on tickslock) ---\n");

  // 1. Get initial stats
  if(getlockstat(scores_before) < 0){
    printf(1, "Error: getlockstat failed\n");
    exit();
  }

  printf(1, "Initial Scores (Acq/Spin): ");
  for(i = 0; i < NCPU; i++){
    // Only print if there is some data (optional)
    printf(1, "[CPU%d: %d] ", i, (int)scores_before[i]); 
  }
  printf(1, "\n");

  // 2. Create load
  printf(1, "Creating %d children to spam uptime()...\n", n_children);
  
  for(i = 0; i < n_children; i++) {
    pid = fork();
    if(pid < 0){
      printf(1, "Fork failed\n");
      exit();
    }

    if(pid == 0){
      // Child process: Generate contention
      // Calling uptime() acquires tickslock inside the kernel
      for(j = 0; j < 10000; j++){
        uptime(); 
      }
      exit();
    }
  }

  // 3. Wait for all children
  for(i = 0; i < n_children; i++){
    wait();
  }

  // 4. Get final stats
  if(getlockstat(scores_after) < 0){
    printf(1, "Error: getlockstat failed\n");
    exit();
  }

  printf(1, "Final Scores (Acq/Spin) after load:\n");
  for(i = 0; i < NCPU; i++){
    // We expect lower scores if contention was high (Acquire / High_Spin)
    printf(1, "CPU%d: Before=%d -> After=%d\n", 
           i, (int)scores_before[i], (int)scores_after[i]);
  }

  printf(1, "--- Test Finished ---\n");
  exit();
}