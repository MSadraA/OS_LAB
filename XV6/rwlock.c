#include "types.h"
#include "defs.h"
#include "param.h"
#include "x86.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "spinlock.h"
#include "rwlock.h"

// Initialize the rwlock
void
rwlock_init(struct rwlock *rw, char *name)
{
  initlock(&rw->lk, "rw spinlock");
  rw->name = name;
  rw->read_count = 0;
  rw->write_locked = 0;
}

// Acquire lock for reading
void
rwlock_acquire_read(struct rwlock *rw)
{
  acquire(&rw->lk); // Lock internal state

  // While a writer holds the lock, readers must wait
  while(rw->write_locked) {
    sleep(rw, &rw->lk);
  }

  rw->read_count++; // Register as a reader
  release(&rw->lk); // Release internal state
}

// Release lock for reading
void
rwlock_release_read(struct rwlock *rw)
{
  acquire(&rw->lk);

  rw->read_count--;
  
  // If I am the last reader, wake up potential waiting writers
  if(rw->read_count == 0) {
    wakeup(rw);
  }

  release(&rw->lk);
}

// Acquire lock for writing
void
rwlock_acquire_write(struct rwlock *rw)
{
  acquire(&rw->lk);

  // Writers must wait if there are ANY readers OR another writer
  while(rw->read_count > 0 || rw->write_locked) {
    sleep(rw, &rw->lk);
  }

  rw->write_locked = 1; // Mark as locked by writer
  release(&rw->lk);
}

// Release lock for writing
void
rwlock_release_write(struct rwlock *rw)
{
  acquire(&rw->lk);

  rw->write_locked = 0;

  // Wake up everyone (readers and writers) to compete for the lock
  wakeup(rw);

  release(&rw->lk);
}