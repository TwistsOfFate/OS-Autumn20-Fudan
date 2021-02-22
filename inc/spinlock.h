#ifndef INC_SPINLOCK_H
#define INC_SPINLOCK_H

struct spinlock {
    volatile int locked;
    
    /* For debugging: */
    char        *name;      /* Name of lock. */
    struct cpu  *cpu;       /* The cpu holding the lock. */
};

#endif
