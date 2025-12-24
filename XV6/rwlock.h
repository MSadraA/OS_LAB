#ifndef _RWLOCK_H_
#define _RWLOCK_H_

#include "spinlock.h"

struct rwlock {
  struct spinlock lk;      // Spinlock to protect the state of this rwlock
  char *name;              // Name of the lock (for debugging)
  
  int read_count;          // Number of concurrent readers
  int write_locked;        // Is a writer holding the lock? (1 = yes, 0 = no)
};

void rwlock_init(struct rwlock *rw, char *name);
void rwlockAcquireRead(struct rwlock *rw);
void rwlockReleaseRead(struct rwlock *rw);
void rwlockAcquireWrite(struct rwlock *rw);
void rwlockReleaseWrite(struct rwlock *rw);

#endif