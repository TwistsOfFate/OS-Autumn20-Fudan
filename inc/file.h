#ifndef INC_FILE_H
#define INC_FILE_H

#include <sys/stat.h>
#include "types.h"
#include "sleeplock.h"
#include "fs.h"

#define NFILE 100  // Open files per system

struct file {
    enum { FD_NONE, FD_PIPE, FD_INODE } type;
    int ref;
    char readable;
    char writable;
    struct pipe *pipe;
    struct inode *ip;
    size_t off;
};


/* In-memory copy of an inode. */
struct inode {
    uint32_t dev;             // Device number
    uint32_t inum;            // Inode number
    int ref;                  // Reference count
    struct sleeplock lock;    // Protects everything below here
    int valid;                // Inode has been read from disk?

    uint16_t type;            // Copy of disk inode
    uint16_t major;
    uint16_t minor;
    uint16_t nlink;
    uint32_t size;
    uint32_t addrs[NDIRECT+1];
};

/*
 * Table mapping major device number to
 * device functions
 */
struct devsw {
    ssize_t (*read)(struct inode *, char *, ssize_t);
    ssize_t (*write)(struct inode *, char *, ssize_t);
};

extern struct devsw devsw[];

#endif
