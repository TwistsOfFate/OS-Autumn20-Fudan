#include <elf.h>

#include "trap.h"

#include "file.h"
#include "string.h"

#include "console.h"
#include "proc.h"
#include "memlayout.h"
#include "mmu.h"
#include "defs.h"

#define MAXARG 32   // max exec arguments

int
execve(const char *path, char *const argv[], char *const envp[])
{
    char *s, *last;
    uint64_t i, off, argc, sz, sp, ustack[3+MAXARG+1];
    Elf64_Ehdr elf;
    struct inode *ip;
    Elf64_Phdr ph;
    uint64_t *pgdir, *oldpgdir;

    begin_op();

    if ((ip = namei(path)) == 0) {
        end_op();
        cprintf("execve: fail\n");
        return -1;
    }
    ilock(ip);
    pgdir = 0;
    cprintf("execve: 1\n");
    /* Check ELF header */
    if (readi(ip, (char *)&elf, 0, sizeof(elf)) != sizeof(elf)) {
        goto bad;
    }
    if (strncmp(elf.e_ident, ELFMAG, SELFMAG)) {
        cprintf("execve: ELF magic does not match\n");
        goto bad;
    }
    if ((pgdir = pgdir_init()) == 0) {
        goto bad;
    }

    /* TODO: Load program into memory. */
    sz = 0;
    for (i = 0, off = elf.e_phoff; i < elf.e_phnum; ++i, off += sizeof(ph)) {
        if (readi(ip, (char *)&ph, off, sizeof(ph)) != sizeof(ph)) {
            goto bad;
        }
        if (ph.p_type != PT_LOAD) {
            continue;
        }
        if (ph.p_memsz < ph.p_filesz) {
            goto bad;
        }
        if (ph.p_vaddr + ph.p_memsz < ph.p_vaddr) {
            goto bad;
        }
        cprintf("execve: 11\n");
        if ((sz = uvm_alloc(pgdir, sz, ph.p_vaddr + ph.p_memsz)) == 0) {
            cprintf("execve: 111\n");
            goto bad;
        }
        // if (ph.p_vaddr % PGSIZE != 0) {
        //     cprintf("execve: 112\n");
        //     goto bad;
        // }
        cprintf("execve: 12\n");
        if (uvm_load(pgdir, (char *)ph.p_vaddr, ip, ph.p_offset, ph.p_filesz) < 0) {
            goto bad;
        }
        cprintf("execve: 13\n");
    }
    cprintf("execve: 2\n");
    iunlockput(ip);
    end_op();
    ip = 0;
    cprintf("execve: 3\n");

    /* TODO: Allocate user stack. */
    // Allocate two pages at the next page boundary.
    // Make the first inaccessible. Use the second as the user stack.
    sz = ROUNDUP(sz, PGSIZE);
    if ((sz = uvm_alloc(pgdir, sz, sz + 2 * PGSIZE)) == 0) {
        goto bad;
    }
    clearpteu(pgdir, (char *)(sz - 2 * PGSIZE));
    sp = sz;
    cprintf("execve: 4\n");

    /* TODO: Push argument strings. */
    // Push argument strings, prepare rest of stack in ustack.
    for (argc = 0; argv[argc]; ++argc) {
        if (argc >= MAXARG) {
            goto bad;
        }
        sp = (sp - (strlen(argv[argc]) + 1)) & -3;
        if (copyout(pgdir, sp, argv[argc], strlen(argv[argc]) + 1) < 0) {
            goto bad;
        }
        ustack[argc] = sp;
    }
    ustack[argc] = 0;
    cprintf("execve: 5\n");
    thisproc()->tf->r0 = argc;
    // auxv
    if ((argc & 1) == 0) {
        sp -= 8;
    } 
    uint64_t auxv[] = { 0, AT_PAGESZ, PGSIZE, AT_NULL };
    sp -= sizeof(auxv);
    if (copyout(pgdir, sp, auxv, sizeof(auxv)) < 0) {
        goto bad;
    } 
    cprintf("execve: 6\n");
    // skip envp
    sp -= 8;
    uint64_t temp = 0;
    if (copyout(pgdir, sp, &temp, 8) < 0) {
        goto bad;
    } 
    thisproc()->tf->r1 = sp = sp - (argc + 1) * 8;
    if (copyout(pgdir, sp, ustack, (argc + 1) * 8) < 0) {
        goto bad;
    }
    sp -= 8;
    if (copyout(pgdir, sp, &thisproc()->tf->r0, 8) < 0) {
        goto bad;
    }
    cprintf("execve: 7\n");

    oldpgdir = thisproc()->pgdir;
    thisproc()->pgdir = pgdir;
    thisproc()->sz = sz;
    thisproc()->tf->sp = sp;
    thisproc()->tf->elr = elf.e_entry;
    cprintf("execve: 8\n");
    uvm_switch(thisproc());
    cprintf("execve: 9\n");
    vm_free(oldpgdir, 1);
    cprintf("execve return\n");
    return thisproc()->tf->r0;

bad:
    cprintf("execve: bad\n");
    if (pgdir) {
        vm_free(pgdir, 1);
    }
    if (ip) {
        iunlockput(ip);
        end_op();
    }
    return -1;

    /*
     * The initial stack is like
     *
     *   +-------------+
     *   | auxv[o] = 0 | 
     *   +-------------+
     *   |    ....     |
     *   +-------------+
     *   |   auxv[0]   |
     *   +-------------+
     *   | envp[m] = 0 |
     *   +-------------+
     *   |    ....     |
     *   +-------------+
     *   |   envp[0]   |
     *   +-------------+
     *   | argv[n] = 0 |  n == argc
     *   +-------------+
     *   |    ....     |
     *   +-------------+
     *   |   argv[0]   |
     *   +-------------+
     *   |    argc     |
     *   +-------------+  <== sp
     *
     * where argv[i], envp[j] are 8-byte pointers and auxv[k] are
     * called auxiliary vectors, which are used to transfer certain
     * kernel level information to the user processes.
     *
     * ## Example 
     *
     * ```
     * sp -= 8; *(size_t *)sp = AT_NULL;
     * sp -= 8; *(size_t *)sp = PGSIZE;
     * sp -= 8; *(size_t *)sp = AT_PAGESZ;
     *
     * sp -= 8; *(size_t *)sp = 0;
     *
     * // envp here. Ignore it if your don't want to implement envp.
     *
     * sp -= 8; *(size_t *)sp = 0;
     *
     * // argv here.
     *
     * sp -= 8; *(size_t *)sp = argc;
     *
     * // Stack pointer must be aligned to 16B!
     *
     * thisproc()->tf->sp = sp;
     * ```
     *
     */

}

