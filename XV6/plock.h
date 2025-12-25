#ifndef PLOCK_H
#define PLOCK_H

#include "spinlock.h"

// Forward declaration to avoid recursive include issues
struct proc;

// A node in the priority waiting queue
struct node {
  struct proc *p;        // The waiting process
  int priority;          // Cached priority of the process
  struct node *next;     // Pointer to the next node in the queue
};

// The priority lock structure
struct plock {
  struct spinlock lk;    // Spinlock to protect the queue and state
  char *name;            // Name of lock
  int locked;            // Is the lock held? (1 = yes, 0 = no)
  struct node *head;     // Head of the priority queue (Linked List)
};

#endif