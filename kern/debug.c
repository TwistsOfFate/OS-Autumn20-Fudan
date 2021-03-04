#include "fs.h"
#include "buf.h"
#include "string.h"
#include "defs.h"

void
printbufassb(struct buf *bp)
{
    struct superblock sb;
    memmove(&sb, bp->data, sizeof(sb));
    cprintf("%d %d %d %d %d %d\n",
        sb.size, sb.nblocks, sb.ninodes,
        sb.nlog, sb.logstart, sb.inodestart);
}

