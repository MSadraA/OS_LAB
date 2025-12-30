#include "types.h"
#include "defs.h"
#include "param.h"
#include "x86.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "plock.h"

// Define the global priority lock instance
struct plock global_plock;

// Initialize the priority lock
void
plock_init(struct plock *pl, char *name)
{
  initlock(&pl->lk, "plock_spin");
  pl->name = name;
  pl->locked = 0;
  pl->head = 0;
}

// Acquire the lock with priority
void
plock_acquire(struct plock *pl, int priority)
{
  acquire(&pl->lk);

  // If the lock is free, take it immediately
  if (pl->locked == 0) {
    pl->locked = 1;
    release(&pl->lk);
    return;
  }

  // --- The Stack Node Trick ---
  // Since we don't have malloc in kernel for small structs, 
  // we allocate the node on the kernel stack of this process.
  // This is safe because the stack persists while sleeping.
  struct node my_node;
  my_node.p = myproc();
  my_node.priority = priority;
  
  // Insert at the head of the waiting list (O(1))
  my_node.next = pl->head;
  pl->head = &my_node;

  // Sleep until woken up. 
  // IMPORTANT: We sleep on the unique address of our own node (&my_node).
  // This allows specific wakeup (only THIS process will be woken).
  sleep(&my_node, &pl->lk);
  
  // When we wake up here, it means plock_release() has chosen us,
  // removed us from the list, and handed off the lock to us.
  // So we already hold the lock (logically).
  
  release(&pl->lk);
}

// Release the lock and wake up the highest priority waiter
void
plock_release(struct plock *pl)
{
  acquire(&pl->lk);

  if (pl->head == 0) {
    // No one is waiting, just release the lock
    pl->locked = 0;
  } else {
    // Find the node with the HIGHEST priority
    struct node *max_node = pl->head;
    struct node *max_prev = 0;
    
    struct node *curr = pl->head;
    struct node *prev = 0;

    // Iterate through the list to find the winner
    while (curr != 0) {
      if (curr->priority > max_node->priority) {
        max_node = curr;
        max_prev = prev;
      }
      prev = curr;
      curr = curr->next;
    }

    // Remove the winner node from the list
    if (max_prev == 0) {
      // The winner is the head
      pl->head = max_node->next;
    } else {
      // The winner is in the middle or end
      max_prev->next = max_node->next;
    }

    wakeup(max_node); 
  }

  release(&pl->lk);
}