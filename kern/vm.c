#include <stdint.h>
#include "types.h"
#include "mmu.h"
#include "string.h"
#include "memlayout.h"
#include "console.h"

#include "vm.h"
#include "kalloc.h"

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
 * Use permission bits perm|PTE_P|PTE_TABLE|PTE_AF|PTE_NORMAL for the entries.
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
        } else if (pte & PTE_P) {
            panic("vmfree: leaf");
        }
    }
    kfree((char *)pgdir);
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