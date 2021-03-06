#include "memlayout.h"

#define ventry .align 7; b alltraps

#define verror(type) .align 7; mov x0, #(type); b irq_error

.globl vectors

.align 11
vectors:
el1_sp0:
    verror(0)
    verror(1)
    verror(2)
    verror(3)

el1_spx:
    /* Current EL with SPx */
    verror(4)
    verror(5)
    verror(6)
    verror(7)

el0_aarch64:
    /* Lower EL using AArch64 */
    ventry
    ventry
    verror(10)
    verror(11)

el0_aarch32:
    /* Lower EL using AArch32 */
    verror(12)
    verror(13)
    verror(14)
    verror(15)
