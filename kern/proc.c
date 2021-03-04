#include "proc.h"
#include "spinlock.h"
#include "console.h"
#include "kalloc.h"
#include "trap.h"
#include "string.h"
#include "vm.h"
#include "mmu.h"
#include "fs.h"
#include "defs.h"

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
        p->priority = 0;
        p->cpus_allowed = ~0;

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
    p->tf->elr = 0;

    p->state = RUNNABLE;

    p->cwd = namei("/");
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

    int ran;

    for (;;) {
        /* Loop over process table looking for process to run. */
        /* TODO: Your code here. */

        ran = 0;
        acquire(&ptable.lock);
#ifdef PRINT_TRACE
        cprintf("scheduler: cpu%d acquired ptable lock\n", cpuid());
#endif
        for (int pri = 1; pri >= 0 && !ran; --pri) {
            for (p = ptable.proc; p < &ptable.proc[NPROC]; ++p) {
                if (p->state == RUNNABLE && p->priority == pri && ((p->cpus_allowed >> cpuid()) & 1)) {
#ifdef PRINT_TRACE
                    cprintf("scheduler: cpu%d running pid %d\n", cpuid(), p->pid);
#endif
                    c->proc = p;
                    uvm_switch(p);
                    p->state = RUNNING;
                    swtch(&c->scheduler, p->context);
                    c->proc = NULL;

                    ran = 1;
                    break;
                }
            }
        }

#ifdef PRINT_TRACE
        cprintf("scheduler: cpu%d released ptable lock\n", cpuid());
#endif
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
#ifdef PRINT_TRACE
    cprintf("forkret: cpu%d released ptable lock\n", cpuid());
#endif

    if (cpuid() == 1) {
        // Some initialization functions must be run in the context
        // of a regular process (e.g., they call sleep), and thus cannot
        // be run from main().
        // iinit(ROOTDEV);
        iinit(ROOTDEV);
        cprintf("initlog\n");
        initlog(ROOTDEV);
    }
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
#ifdef PRINT_TRACE
    cprintf("yield: cpu %d, pid %d acquired ptable lock\n", cpuid(), p->pid);
#endif
    p->state = RUNNABLE;
    sched();
    release(&ptable.lock);
#ifdef PRINT_TRACE
    cprintf("yield: cpu %d, pid %d released ptable lock\n", cpuid(), p->pid);
#endif
}

/*
 * Atomically release lock and sleep on chan.
 * Reacquires lock when awakened.
 */
void
sleep(void *chan, struct spinlock *lk)
{
    /* TODO: Your code here. */
    struct proc *p = thiscpu->proc;

    if (lk == NULL || !holding(lk)) {
        panic("sleep: no lock held\n");
    }
#ifdef PRINT_TRACE
    cprintf("sleep: cpu%d, pid %d\n", cpuid(), p->pid);
    cprintf("sleep: acquiring ptable lock and releasing sdlock...");
#endif
    if (lk != &ptable.lock) {
        acquire(&ptable.lock);
        release(lk);
    }
#ifdef PRINT_TRACE
    cprintf("Success!\n");
#endif
    p->chan = chan;
    p->state = SLEEPING;
    sched();
#ifdef PRINT_TRACE
    cprintf("sleep: cpu%d, pid %d returned from sleep\n", cpuid(), p->pid);
#endif
    p->chan = 0;
#ifdef PRINT_TRACE
    cprintf("sleep: releasing ptable lock and acquiring sdlock...");
#endif
    if (lk != &ptable.lock) {
        release(&ptable.lock);
        acquire(lk);
    }
#ifdef PRINT_TRACE
    cprintf("Success!\n");
#endif
}

/* Wake up all processes sleeping on chan. */
void
wakeup(void *chan)
{
    /* TODO: Your code here. */
    struct proc *p;
#ifdef PRINT_TRACE
    cprintf("wakeup: cpu%d, pid %d, chan %d\n", cpuid(), thiscpu->proc->pid, chan);
    cprintf("wakeup: acquiring ptable lock...");
#endif
    acquire(&ptable.lock);
#ifdef PRINT_TRACE
    cprintf("Success!\n");
#endif

    for(p = ptable.proc; p < &ptable.proc[NPROC]; ++p) {
        if (p->state == SLEEPING && p->chan == chan) {
#ifdef PRINT_TRACE
            cprintf("wakeup: pid %d runnable\n", p->pid);
#endif
            p->state = RUNNABLE;
        }
    }

    release(&ptable.lock);
#ifdef PRINT_TRACE
    cprintf("wakeup: released ptable lock\n");
#endif
}

/* 
 * Reduce priority of current process if possible
 * Typically called by timer()
 */
void
drop_priority()
{
    struct proc *p = thiscpu->proc;
    acquire(&ptable.lock);
    if (p->priority > 0) {
        p->priority--;
    }
    release(&ptable.lock);
}

/* 
 * Raise priority of current process if possible
 */
void
raise_priority()
{
    struct proc *p = thiscpu->proc;
    acquire(&ptable.lock);
    if (p->priority < 1) {
        p->priority++;
    }
    release(&ptable.lock);
}

/* 
 * Set cpus_allowed mask
 */
void
set_cpus_allowed(int mask)
{
    struct proc *p = thiscpu->proc;
    acquire(&ptable.lock);
    p->cpus_allowed = mask;
    release(&ptable.lock);
}

/*
 * Create a new process copying p as the parent.
 * Sets up stack to return as if from system call.
 * Caller must set state of returned proc to RUNNABLE.
 */
int
fork()
{
    /* TODO: Your code here. */
}

/*
 * Wait for a child process to exit and return its pid.
 * Return -1 if this process has no children.
 */
int
wait()
{
    /* TODO: Your code here. */
}

/*
 * Print a process listing to console.  For debugging.
 * Runs when user types ^P on console.
 * No lock to avoid wedging a stuck machine further.
 */
void
procdump()
{
    panic("unimplemented");
}

