#ifndef SPINLOCK_H  
#define SPINLOCK_H  

typedef unsigned long long uint64;

// Mutual exclusion lock.
struct spinlock {
  uint locked;       // Is the lock held?

  // For debugging:
  char *name;        // Name of lock.
  struct cpu *cpu;   // The cpu holding the lock.
  uint pcs[10];      // The call stack (an array of program counters)
                     // that locked the lock.

  // LAB4
  uint64 acq_count[NCPU];
  uint64 total_spins[NCPU];

};

#endif

