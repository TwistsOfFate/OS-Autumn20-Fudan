#include <syscall.h>
#include "string.h"
#include "proc.h"
#include "console.h"
#include "sd.h"
#include "defs.h"

/* 
 * User code makes a system call with SVC, system call number in r0.
 * Arguments on the stack, from the user call to the C
 * library system call function.
 */

/* Fetch the int at addr from the current process. */
int
fetchint(uint64_t addr, int64_t *ip)
{
    struct proc *proc = thiscpu->proc;

    if (addr >= proc->sz || addr + 8 > proc->sz) {
        return -1;
    }
    *ip = *(int64_t*)(addr);
    return 0;
}

/* 
 * Fetch the nul-terminated string at addr from the current process.
 * Doesn't actually copy the string - just sets *pp to point at it.
 * Returns length of string, not including nul.
 */
int
fetchstr(uint64_t addr, char **pp)
{
    char *s, *ep;
    struct proc *proc = thiscpu->proc;

    if (addr >= proc->sz) {
        return -1;
    }

    *pp = (char*)addr;
    ep = (char*)proc->sz;

    for (s = *pp; s < ep; s++) {
        if (*s == 0) {
            return s - *pp;
        }
    }

    return -1;
}

/* 
 * Fetch the nth (starting from 0) 32-bit system call argument.
 * In our ABI, r0 contains system call index, r1-r4 contain parameters.
 * now we support system calls with at most 4 parameters.
 */
int
argint(int n, uint64_t *ip)
{
    if (n > 3) {
        panic("argint: too many system call parameters\n");
    }

    struct proc *proc = thiscpu->proc;

    *ip = *(&proc->tf->r0 + n);

    return 0;
}

/* 
 * Fetch the nth word-sized system call argument as a pointer
 * to a block of memory of size n bytes.  Check that the pointer
 * lies within the process address space.
 */
int
argptr(int n, char **pp, int size)
{
    uint64_t i;

    if (argint(n, &i) < 0) {
        return -1;
    }

    struct proc *proc = thiscpu->proc;

    if ((uint64_t)i >= proc->sz || (uint64_t)i + size > proc->sz) {
        return -1;
    }

    *pp = (char*)i;
    return 0;
}

/* 
 * Fetch the nth word-sized system call argument as a string pointer.
 * Check that the pointer is valid and the string is nul-terminated.
 * (There is no shared writable memory, so the string can't change
 * between this check and being used by the kernel.)
 */
int
argstr(int n, char **pp)
{
    uint64_t addr;

    if (argint(n, &addr) < 0) {
        return -1;
    }

    return fetchstr(addr, pp);
}

extern int sys_exec();
extern int sys_exit();

int
syscall1(struct trapframe *tf)
{
    thisproc()->tf = tf;
    int sysno = tf->r8;
    int tret;
    // cprintf("syscall1: sysno=%d, r0=%s\n", sysno, tf->r0);
    switch (sysno) {
        // FIXME: Use pid instead of tid since we don't have threads :)
        case SYS_set_tid_address:
        case SYS_gettid:            // 96
            cprintf("%d=%d\n", sysno, thisproc()->pid);
            return thisproc()->pid;

        // FIXME: Hack TIOCGWINSZ(get window size)
        case SYS_ioctl:
            if (tf->r1 == 0x5413) return 0;
            else panic("ioctl unimplemented. ");
        
        // FIXME: Always return 0 since we don't have signals  :)
        case SYS_rt_sigprocmask:
            return 0;
        case SYS_brk:
            tret = sys_brk();
            // cprintf("%d=%d\n", sysno, tret);
            return tret;
        case SYS_execve:            // 221
            tret = sys_exec();
            cprintf("%d=%d\n", sysno, tret);
            return tret;
        case SYS_sched_yield:
            tret = sys_yield();
            // cprintf("%d=%d\n", sysno, tret);
            return tret;
        case SYS_clone:             // 220
            tret = sys_clone();
            // cprintf("%d=%d\n", sysno, tret);
            return tret;
        case SYS_wait4:
            tret = sys_wait4();
            // cprintf("%d=%d\n", sysno, tret);
            return tret;
            
        // FIXME: exit_group should kill every thread in the current thread group.
        case SYS_exit_group:
        case SYS_exit:
            tret = sys_exit();
            // cprintf("%d=%d\n", sysno, tret);
            return tret;
        case SYS_dup:               // 23
            tret = sys_dup();
            // cprintf("%d=%d\n", sysno, tret);
            return tret;
        case SYS_chdir:
            tret = sys_chdir();
            // cprintf("%d=%d\n", sysno, tret);
            return tret;
        case SYS_fstat:
            tret = sys_fstat();
            // cprintf("%d=%d\n", sysno, tret);
            return tret;
        case SYS_newfstatat:
            tret = sys_fstatat();
            // cprintf("%d=%d\n", sysno, tret);
            return tret;
        case SYS_mkdirat:
            tret = sys_mkdirat();
            // cprintf("%d=%d\n", sysno, tret);
            return tret;
        case SYS_mknodat:           // 33
            tret = sys_mknodat();
            // cprintf("%d=%d\n", sysno, tret);
            return tret;
        case SYS_openat:            // 56
            tret = sys_openat();
            cprintf("%d=%d\n", sysno, tret);
            return tret;
        case SYS_writev:            // 66
            tret = sys_writev();
            cprintf("%d=%d\n", sysno, tret);
            return tret;
        case SYS_read:
            tret = sys_read();
            cprintf("%d=%d\n", sysno, tret);
            return tret;
        case SYS_close:
            tret = sys_close();
            cprintf("%d=%d\n", sysno, tret);
            return tret;
        
        default:
            // FIXME: don't panic.
            // debug_reg();
            panic("Unexpected syscall #%d\n", sysno);
            return 0;
    }
}
