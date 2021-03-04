#include <stdint.h>
#include "types.h"
#include "mmu.h"
#include "string.h"
#include "memlayout.h"
#include "console.h"

#include "kalloc.h"
#include "proc.h"
#include "defs.h"

extern uint64_t *kpgdir;

/* 
 * Given 'pgdir', a pointer to a page directory, pgdir_walk returns
 * a pointer to the page table entry (PTE) for virtual address 'va'.
 * This requires walking the four-level page table structure.
 *
 * The relevant page table page might not exist yet.
 * If this is true, and alloc == false, then pgdir_walk returns NULL.
 * Otherwise, pgdir_walk allocates a new page table page with kalloc.
 *   - If the allocation fails, pgdir_walk returns NULL.
 *   - Otherwise, the new page is cleared, and pgdir_walk returns
 *     a pointer into the new page table page.
 */

static uint64_t *
pgdir_walk(uint64_t *pgdir, const void *va, int64_t alloc)
{
    /* TODO: Your code here. */
    // if ((uint64_t)va >= ((uint64_t)1 << 47)) {
    //     /* Only lower virtual addresses allowed */
    //     panic("pgdir_walk");
    // }

    for (int level = 0; level < 3; ++level) {
        uint64_t *pte = &pgdir[PTX(level, va)];
        if (*pte & PTE_P) {
            pgdir = (uint64_t *)P2V(PTE_ADDR(*pte)); /* Does not ensure pgdir[63:48] = 0 */
        } else {
            if (!alloc || (pgdir = (uint64_t *)kalloc()) == 0)
                return NULL;
            memset(pgdir, 0, PGSIZE);
            *pte = V2P(PTE_ADDR(pgdir)) | PTE_P | PTE_TABLE;
        }
    }
    return &pgdir[PTX(3, va)];
}

/*
 * Create PTEs for virtual addresses starting at va that refer to
 * physical addresses starting at pa. va and size might **NOT**
 * be page-aligned.
 * Use permission bits perm|PTE_P|PTE_TABLE|(MT_NORMAL << 2)|PTE_AF|PTE_SH for the entries.
 *
 * Hint: call pgdir_walk to get the corresponding page table entry
 */

static int
map_region(uint64_t *pgdir, void *va, uint64_t size, uint64_t pa, int64_t perm)
{
    /* TODO: Your code here. */
    uint64_t a, last;
    uint64_t *pte;

    a = ROUNDDOWN((uint64_t)va, PGSIZE);
    last = ROUNDDOWN((uint64_t)va + size - 1, PGSIZE);

    for (;;) {
        if ((pte = pgdir_walk(pgdir, (void *)a, 1)) == NULL)
            return -1;
        if (*pte & PTE_P)
            panic("remap");
        *pte = PTE_ADDR(pa) | perm | PTE_P | PTE_TABLE | PTE_AF | PTE_NORMAL;
        if (a == last)
            break;
        a += PGSIZE;
        pa += PGSIZE;
    }

    return 0;
}

/* 
 * Free a page table.
 *
 * Hint: You need to free all existing PTEs for this pgdir.
 */

void
vm_free(uint64_t *pgdir, int level)
{
    /* TODO: Your code here. */
    for (int i = 0; i < (PGSIZE >> 3); ++i) {
        uint64_t pte = pgdir[i];
        if ((pte & PTE_P) && (pte & PTE_TABLE)) {
            /* This PTE points to a lower-level page table. */
            uint64_t child = (uint64_t)P2V(PTE_ADDR(pte));
            vm_free((uint64_t *)child, level + 1);
            pgdir[i] = 0;
        }
    }
    kfree((char *)pgdir);
}

/* Get a new page table */
uint64_t *
pgdir_init()
{
    /* TODO: Your code here. */
    uint64_t *pgt;

    if ((pgt = (uint64_t *)kalloc()) == 0)
        panic("pgdir_init: kalloc failed\n");
    
    memset(pgt, 0, PGSIZE);
    return pgt;
}

/* 
 * Load binary code into address 0 of pgdir.
 * sz must be less than a page.
 * The page table entry should be set with
 * additional PTE_USER|PTE_RW|PTE_PAGE permission
 */
void
uvm_init(uint64_t *pgdir, char *binary, int sz)
{
    /* TODO: Your code here. */
    char *mem;

    if (sz > PGSIZE)
        panic("uvm_init: page overflow!\n");
    
    mem = kalloc();
    memset(mem, 0, PGSIZE);
    map_region(pgdir, (void *)0, (uint64_t)PGSIZE, V2P(mem), PTE_USER|PTE_RW|PTE_PAGE);
    memmove(mem, (void *)binary, sz);
}

/*
 * switch to the process's own page table for execution of it
 */
void
uvm_switch(struct proc *p)
{
    /* TODO: Your code here. */
    if (p->pgdir == 0)
        panic("uvm_switch: no pgdir\n");

    lttbr0((uint64_t)V2P(p->pgdir));
}

/*
 * Allocate page tables and physical memory to grow process from oldsz to
 * newsz, which need not be page aligned. Returns new size or 0 on error.
 */
int
uvm_alloc(uint64_t *pgdir, uint64_t oldsz, uint64_t newsz)
{
    char *mem;
    uint64_t a;

    if (newsz >= UADDR_SZ) {
        return 0;
    }
    if (newsz < oldsz) {
        return oldsz;
    }
    a = ROUNDUP(oldsz, PGSIZE);
    for (; a < newsz; a += PGSIZE) {
        mem = kalloc();
        if (mem == 0) {
            cprintf("uvm_alloc: out of memory\n");
            uvm_dealloc(pgdir, newsz, oldsz);
            return 0;
        }
        memset(mem, 0, PGSIZE);
        if (map_region(pgdir, (char *)a, PGSIZE, V2P(mem), PTE_RW|PTE_USER) < 0) {
            cprintf("uvm_alloc: out of memory (2)\n");
            uvm_dealloc(pgdir, newsz, oldsz);
            kfree(mem);
            return 0;
        }
    }
    return newsz;
}

/* 
 * Deallocate user pages to bring the process size from oldsz to
 * newsz. oldsz and newsz need not be page-aligned, nor does newsz
 * need to be less than oldsx. oldsz can be larger than the actual
 * process size. Returns the new process size.
 */
int
uvm_dealloc(uint64_t *pgdir, uint64_t oldsz, uint64_t newsz)
{
    uint64_t *pte;
    uint64_t a, pa;

    if (newsz >= oldsz) {
        return oldsz;
    }

    a = ROUNDUP(newsz, PGSIZE);
    for (; a < oldsz; a += PGSIZE) {
        pte = pgdir_walk(pgdir, (char *)a, 0);
        if (!pte) {
            a = ROUNDUP(a, BKSIZE);
        } else if ((*pte & PTE_P) != 0) {
            pa = PTE_ADDR(*pte);
            if (pa == 0) {
                panic("uvm_dealloc\n");
            }
            char *v = P2V(pa);
            kfree(v);
            *pte = 0;
        }
    }
    return newsz;
}

/*
 * Load a program segment into pgdir. addr must be page-aligned
 * and the pages from addr to addr+sz must already be mapped.
 */
int
uvm_load(uint64_t *pgdir, char *addr, struct inode *ip, uint64_t offset, uint64_t sz) {
    uint64_t i, pa, n;
    uint64_t *pte;

    if (((uint64_t)addr - offset) % PGSIZE != 0) {
        panic("uvm_load: addr must be page aligned\n");
    }
    for (i = 0; i < sz; i += PGSIZE) {
        if ((pte = pgdir_walk(pgdir, addr + i, 0)) == 0) {
            panic("uvm_load: address should exist\n");
        }
        pa = PTE_ADDR(*pte);
        if (sz - i < PGSIZE) {
            n = sz - i;
        } else {
            n = PGSIZE;
        }
        if (readi(ip, P2V(pa), offset + i, n) != n) {
            return -1;
        }
    }
    return 0;
}

/*
 * Clear PTE_USER on a page. Used to create an inaccessible
 * page beneath the user stack.
 */
void
clearpteu(uint64_t *pgdir, char *uva)
{
    uint64_t *pte;

    pte = pgdir_walk(pgdir, uva, 0);
    if (pte == 0) {
        panic("clearpteu\n");
    }
    *pte &= ~PTE_USER;
}

/*
 * Copy len bytes from p to user address va in page table pgdir.
 * Most useful when pgdir is not the current page table.
 * uva2ka ensures this only works for PTE_USER pages.
 */
int
copyout(uint64_t *pgdir, uint64_t va, void *p, uint64_t len)
{
    char *buf, *pa0;
    uint64_t n, va0;

    buf = (char *)p;
    while (len > 0) {
        va0 = (uint64_t)ROUNDDOWN(va, PGSIZE);
        pa0 = uva2ka(pgdir, (char *)va0);
        if (pa0 == 0) {
            return -1;
        }
        n = PGSIZE - (va - va0);
        if (n > len) {
            n = len;
        }
        memmove(pa0 + (va - va0), buf, n);
        len -= n;
        buf += n;
        va = va0 + PGSIZE;
    }
    return 0;
}

/*
 * Map user virtual address to kernel address.
 */
char *
uva2ka(uint64_t *pgdir, char *uva)
{
    uint64_t *pte;

    pte = pgdir_walk(pgdir, uva, 0);
    if ((*pte & PTE_P) == 0) {
        return 0;
    }
    if ((*pte & PTE_USER) == 0) {
        return 0;
    }
    return (char *)P2V(PTE_ADDR(*pte));
}

/*
 * Test code by Master Han
 */

void
test_mem()
{
    cprintf("\ntest_mem() begin:\n");

    *((uint64_t *)P2V(4)) = 0xfd2020;
    char *pgdir = kalloc();
    memset(pgdir, 0, PGSIZE);
    map_region((uint64_t *)pgdir, (void *)0x1004, PGSIZE, 4, 0);
    asm volatile("msr ttbr0_el1, %[x]": : [x]"r"(V2P(pgdir)));

    cprintf("Memory content at 0x1004: 0x%x\n", *(uint64_t *)0x1004);
    cprintf("test_mem() end.\n\n");
}