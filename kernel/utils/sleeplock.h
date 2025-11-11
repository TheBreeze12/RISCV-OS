// Long-term locks for processes
#ifndef SLEEPLOCK_H
#define SLEEPLOCK_H

#include "../include/type.h"
#include "../include/def.h"
#include "spinlock.h"
struct sleeplock {
    uint locked;       // Is the lock held?
    struct spinlock lk; // spinlock protecting this sleep lock
    
    // For debugging:
    char *name;        // Name of lock.
    int pid;           // Process holding lock
  };
  
#endif
  