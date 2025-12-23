#include "types.h"
#include "user.h"
#include "fcntl.h"

void reader(int id) {
  int pid = getpid();
  printf(1, "[Reader %d | PID %d] Attempting to acquire Read Lock...\n", id, pid);
  
  rwlock_acquire_read();
  
  printf(1, "[Reader %d | PID %d] ACQUIRED Read Lock. Reading...\n", id, pid);
  
  // Simulate reading work
  sleep(100); 
  
  printf(1, "[Reader %d | PID %d] Releasing Read Lock.\n", id, pid);
  
  rwlock_release_read();
  
  exit();
}

void writer(int id) {
  int pid = getpid();
  printf(1, "[Writer %d | PID %d] Attempting to acquire Write Lock...\n", id, pid);
  
  rwlock_acquire_write();
  
  printf(1, "[Writer %d | PID %d] ACQUIRED Write Lock. Writing (Exclusive)...\n", id, pid);
  
  // Simulate writing work (Long enough to block others)
  sleep(300); 
  
  printf(1, "[Writer %d | PID %d] Releasing Write Lock.\n", id, pid);
  
  rwlock_release_write();
  
  exit();
}

int main() {
  printf(1, "\n--- Starting Comprehensive RWLock Test ---\n");

  int pid;

  // 1. Create two Readers (Simulate concurrent reading)
  // They should enter together immediately.
  for (int i = 1; i <= 2; i++) {
    pid = fork();
    if (pid == 0) {
      reader(i);
    }
  }
  
  // Wait a bit to let readers acquire the lock
  sleep(20);

  // 2. Create Writer 1
  // Should wait for Readers 1 & 2 to finish, then acquire.
  pid = fork();
  if (pid == 0) {
    writer(1);
  }

  // Wait until Readers 1 & 2 likely finish and Writer 1 acquires the lock
  sleep(150);

  // At this point, Writer 1 should be holding the lock exclusively.

  // 3. Create Writer 2
  // Should verify that a NEW WRITER is blocked by Writer 1
  pid = fork();
  if (pid == 0) {
    writer(2);
  }

  // Small delay
  sleep(20);

  // 4. Create Reader 3
  // Should verify that a NEW READER is also blocked by Writer 1
  pid = fork();
  if (pid == 0) {
    reader(3);
  }

  // Wait for all 5 children to exit
  for (int i = 0; i < 5; i++) {
    wait();
  }

  printf(1, "--- RWLock Test Finished ---\n");
  exit();
}