/* vectors.S send all traps here. */
.global alltraps
alltraps:
    /*
     * Build your trap frame.
     * Hint:
     * 1. Use stp/ldp in favor of str/ldr to maintain stack alignment
          For example, `stp x1, x2, [sp, #-16]!` is equivalent to 
          first `push x2` and then `push x1`. Be careful about the order.
     * 2. Use mrs/msr to move from/to system registers such as
     *    ELR_EL1, SPSR_EL1, and SP_EL0.
     */

    /* TODO: Your code here. */
    /* A total of 34 registers saved: x0~x30, SPSR_EL1, ELR_EL1, SP_EL0 */
    sub     sp, sp, #16
    str     x30, [sp]
    mov     x30, sp
    str     x30, [sp, #8]

    stp     x28, x29, [sp, #-16]!
    stp     x26, x27, [sp, #-16]!
    stp     x24, x25, [sp, #-16]!
    stp     x22, x23, [sp, #-16]!
    stp     x20, x21, [sp, #-16]!
    stp     x18, x19, [sp, #-16]!
    stp     x16, x17, [sp, #-16]!
    stp     x14, x15, [sp, #-16]!
    stp     x12, x13, [sp, #-16]!
    stp     x10, x11, [sp, #-16]!
    stp     x8, x9, [sp, #-16]!
    stp     x6, x7, [sp, #-16]!
    stp     x4, x5, [sp, #-16]!
    stp     x2, x3, [sp, #-16]!
    stp     x0, x1, [sp, #-16]!

    mrs     x10, elr_el1
    mrs     x9, spsr_el1
    stp     x9, x10, [sp, #-16]!

    /*
     * Save Q0 and TPIDR_EL0 to placate musl such as
     * - `str q0, [sp, #-16]!`
     * - `mrs x12, tpidr_el0` and then push to stack
     */

    /* TODO: Your code here. */

    /*
     * Call trap(struct *trapframe).
     * Hint: The first argument is a stack pointer.
     */

    /* TODO: Your code here. */
    mov     x0, sp
    bl      trap

/* Return falls through to trapret. */
.global trapret
trapret:
    /*
     * Restore registers.
     * Hint: `ldp x1, x2, [sp], #16` is equivalent to first `pop x1`
     * and then `pop x2`.
     */

    /* TODO: Your code here. */
    ldp     x9, x10, [sp], #16
    msr     spsr_el1, x9
    msr     elr_el1, x10

    ldp	    x0, x1, [sp], #16
	ldp	    x2, x3, [sp], #16
	ldp	    x4, x5, [sp], #16
	ldp	    x6, x7, [sp], #16
	ldp	    x8, x9, [sp], #16
	ldp	    x10, x11, [sp], #16
	ldp    	x12, x13, [sp], #16
	ldp    	x14, x15, [sp], #16
	ldp    	x16, x17, [sp], #16
	ldp	    x18, x19, [sp], #16
	ldp	    x20, x21, [sp], #16
	ldp	    x22, x23, [sp], #16
	ldp	    x24, x25, [sp], #16
	ldp	    x26, x27, [sp], #16
    ldr	    x28, [sp], #8

    mov     x29, sp
    ldr     x30, [x29, #16]
    mov     sp, x30
    ldr     x30, [x29, #8]
    ldr     x29, [x29]

    /* Restore Q0 and TPIDR_EL0. */

    /* TODO: Your code here. */

    eret
