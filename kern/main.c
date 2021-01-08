#include <stdint.h>

#include "arm.h"
#include "string.h"
#include "console.h"
#include "kalloc.h"
#include "trap.h"
#include "timer.h"
#include "spinlock.h"
#include "proc.h"
#include "sd.h"

struct cpu cpus[NCPU];

volatile static int started = 0;

void
main()
{
    /*
     * Before doing anything else, we need to ensure that all
     * static/global variables start out zero.
     */

    extern char edata[], end[], vectors[];

    /*
     * Determine which functions in main can only be
     * called once, and use lock to guarantee this.
     */
    /* TODO: Your code here. */
    
    if (cpuid() == 0) {
        /* TODO: Use `memset` to clear the BSS section of our program. */
        cprintf("main: [CPU%d] is init kernel\n", cpuid());
        memset(edata, 0, end - edata);    
        console_init();
        alloc_init();
        cprintf("main: allocator init success.\n");
        check_free_list();

        irq_init();
        proc_init();

        for (int i = 0; i < 4; ++i) {
            user_init();
        }
        
        sd_init();

        started = 1;
    }

    while (started == 0)
        ;
    
    lvbar(vectors);
    timer_init();

    cprintf("main: [CPU%d] Init success.\n", cpuid());

    scheduler();

}
