#include "types.h"
#include "user.h"
#include "fcntl.h"

void worker(int prio, int sleep_ticks) {
  int pid = getpid();
  printf(1, "[PID %d] Requesting lock with Priority %d...\n", pid, prio);
  
  // Try to acquire the lock with specific priority
  plock_acquire(prio);
  
  // Critical Section
  printf(1, "!!! [PID %d] ACQUIRED lock with Priority %d !!!\n", pid, prio);
  
  if(sleep_ticks > 0){
    printf(1, "[PID %d] Holding lock for %d ticks...\n", pid, sleep_ticks);
    sleep(sleep_ticks);
  }
  
  printf(1, "[PID %d] Releasing lock (Priority %d).\n", pid, prio);
  plock_release();
  
  exit();
}

int main(int argc, char *argv[])
{
  printf(1, "\n--- Starting Priority Lock Test ---\n");

  // Step 1: Create a low priority process that holds the lock initially
  // This forces everyone else to wait in the queue.
  int pid = fork();
  if(pid == 0){
    // Priority 5 (Very Low), Hold for 200 ticks
    worker(5, 200); 
  }

  // Allow the first worker to grab the lock
  sleep(10);

  printf(1, "--- Spawning waiting processes ---\n");

  // Step 2: Spawn workers with different priorities in MIXED order
  // We want to prove that wakeup is NOT based on arrival time (FIFO).
  
  // Worker A: Priority 20 (Medium) - Arrives 1st
  pid = fork();
  if(pid == 0){ worker(20, 0); }
  sleep(10); // Ensure correct arrival order

  // Worker B: Priority 10 (Low) - Arrives 2nd
  pid = fork();
  if(pid == 0){ worker(10, 0); }
  sleep(10);

  // Worker C: Priority 50 (High) - Arrives 3rd (Last)
  // Even though it arrives last, it MUST wake up first!
  pid = fork();
  if(pid == 0){ worker(50, 0); }
  
  // Wait for all 4 children to finish
  for(int i = 0; i < 4; i++){
    wait();
  }

  printf(1, "\n--- Priority Lock Test Finished ---\n");
  printf(1, "Expected Order of Acquisition after release:\n");
  printf(1, "1. Priority 50 (Highest)\n");
  printf(1, "2. Priority 20 (Medium)\n");
  printf(1, "3. Priority 10 (Lowest)\n");
  exit();
}