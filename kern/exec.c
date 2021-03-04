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
        cprintf("exec: fail\n");
        return -1;
    }
    ilock(ip);
    pgdir = 0;

    /* Check ELF header */
    if (readi(ip, (char *)&elf, 0, sizeof(elf)) != sizeof(elf)) {
        goto bad;
    }
    if (strncmp(elf.e_ident, ELFMAG, SELFMAG)) {
        cprintf("exec: ELF magic does not match\n");
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
        if ((sz = uvm_alloc(pgdir, sz, ph.p_vaddr + ph.p_memsz)) == 0) {
            goto bad;
        }
        if (ph.p_vaddr % PGSIZE != 0) {
            goto bad;
        }
        if (uvm_load(pgdir, (char *)ph.p_vaddr, ip, ph.p_offset, ph.p_filesz) < 0) {
            goto bad;
        }
    }
    iunlockput(ip);
    end_op();
    ip = 0;

    /* TODO: Allocate user stack. */

    /* TODO: Push argument strings. */

    bad:

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

