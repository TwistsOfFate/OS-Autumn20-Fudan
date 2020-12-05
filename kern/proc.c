#include "proc.h"
#include "spinlock.h"
#include "console.h"
#include "kalloc.h"
#include "trap.h"
#include "string.h"
#include "vm.h"
#include "mmu.h"

struct {
    struct proc proc[NPROC];
    struct spinlock lock;
} ptable;

static struct proc *initproc;

int nextpid = 1;
void forkret();
extern void trapret();
void swtch(struct context **, struct context *);

/*
 * Initialize the spinlock for ptable to serialize the access to ptable
 */
void
proc_init()
{
    /* TODO: Your code here. */
    initlock(&ptable.lock, "ptable");
}

/*
 * Look through the process table for an UNUSED proc.
 * If found, change state to EMBRYO and initialize
 * state (allocate stack, clear trapframe, set context for switch...)
 * required to run in the kernel. Otherwise return 0.
 */
static struct proc *
proc_alloc()
{
    struct proc *p;
    /* TODO: Your code here. */
    char *sp;
    int found = 0;
    acquire(&ptable.lock);

    for (p = ptable.proc; p < &ptable.proc[NPROC]; ++p) {
        if (p->state == UNUSED) {
            found = 1;
            break;
        }
    }

    if (!found) {
        release(&ptable.lock);
        return 0;
    }
    else {
        /* Alloc kernel stack. */
        if ((p->kstack = kalloc()) == 0) {
            release(&ptable.lock);
            panic("proc_alloc: cannot alloc kstack.\n");
        }
        p->sz = PGSIZE;

        sp = p->kstack + KSTACKSIZE;
        
        /* Alloc trapframe. */
        sp -= sizeof(struct trapframe);
        p->tf = (struct trapframe *)sp;

        /* Fill trapret. */
        sp -= 8;
        *(uint64_t *)sp = (uint64_t)trapret;

        /* Fill sp. */
        sp -= 8;
        *(uint64_t *)sp = (uint64_t)p->kstack + KSTACKSIZE;

        /* Alloc context. */
        sp -= sizeof(struct context);
        p->context = (struct context *)sp;
        
        p->context->x30 = (uint64_t)forkret + 8;

        p->state = EMBRYO;
        p->pid = nextpid++;

        release(&ptable.lock);
    }

    return p;
}

/*
 * Set up first user process(Only used once).
 * Set trapframe for the new process to run
 * from the beginning of the user process determined 
 * by uvm_init
 */
void
user_init()
{
    struct proc *p;
    /* for why our symbols differ from xv6, please refer https://stackoverflow.com/questions/10486116/what-does-this-gcc-error-relocation-truncated-to-fit-mean */
    extern char _binary_obj_user_initcode_start[], _binary_obj_user_initcode_size[];
    
    /* TODO: Your code here. */
    p = proc_alloc();

    if ((p->pgdir = pgdir_init()) == 0)
        panic("user_init: kalloc failed\n");

    initproc = p;
    uvm_init(p->pgdir, (char *)_binary_obj_user_initcode_start, (int)_binary_obj_user_initcode_size);
    
    /* Fill trapframe */
    p->tf->spsr = 0;
    p->tf->sp = PGSIZE;
    // p->tf->r30 = 0;
    p->tf->elr = 0;

    p->state = RUNNABLE;
}

/*
 * Per-CPU process scheduler
 * Each CPU calls scheduler() after setting itself up.
 * Scheduler never returns.  It loops, doing:
 *  - choose a process to run
 *  - swtch to start running that process
 *  - eventually that process transfers control
 *        via swtch back to the scheduler.
 */
void
scheduler()
{
    struct proc *p;
    struct cpu *c = thiscpu;
    c->proc = NULL;
    
    for (;;) {
        /* Loop over process table looking for process to run. */
        /* TODO: Your code here. */

        acquire(&ptable.lock);

        for (p = ptable.proc; p < &ptable.proc[NPROC]; ++p) {
            if (p->state == RUNNABLE) {
                c->proc = p;
                uvm_switch(p);
                p->state = RUNNING;
                sti();
                swtch(&c->scheduler, p->context);
                c->proc = NULL;
            }
        }

        release(&ptable.lock);
    }
}

/*
 * Enter scheduler.  Must hold only ptable.lock
 */
void
sched()
{
    /* TODO: Your code here. */
    struct proc *p = thiscpu->proc;

    if (!holding(&ptable.lock))
        panic("sched: not holding ptable lock\n");

    if (p->state == RUNNING)
        panic("sched: process running\n");

    swtch(&p->context, thiscpu->scheduler);
}

/*
 * A fork child will first swtch here, and then "return" to user space.
 */
void
forkret()
{
    /* TODO: Your code here. */
    release(&ptable.lock);
}

/*
 * Exit the current process.  Does not return.
 * An exited process remains in the zombie state
 * until its parent calls wait() to find out it exited.
 */
void
exit()
{
    struct proc *p = thiscpu->proc;
    /* TODO: Your code here. */
    // if (p == initproc)
    //     panic("exit: init exiting\n");
    
    acquire(&ptable.lock);

    p->state = ZOMBIE;
    sched();

    panic("exit: zombie exit\n");
}

/*
 * Give up the CPU for one scheduling round.
 */
void
yield()
{
    struct proc *p = thiscpu->proc;
    acquire(&ptable.lock);
    p->state = RUNNABLE;
    sched();
    release(&ptable.lock);
}