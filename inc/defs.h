#ifndef INC_DEFS_H
#define INC_DEFS_H

// #define PRINT_TRACE

struct buf;
struct spinlock;
struct trapframe;

// bio.c
void            binit();
void            bwrite(struct buf *b);
void            brelse(struct buf *b);
struct buf *    bread(uint32_t dev, uint32_t blockno);

// fs.c
void            readsb(int, struct superblock *);
int             dirlink(struct inode *, char *, uint32_t);
struct inode *  dirlookup(struct inode *, char *, size_t *);
struct inode *  ialloc(uint32_t, short);
struct inode *  idup(struct inode *);
void            iinit(int dev);
void            ilock(struct inode *);
void            iput(struct inode *);
void            iunlock(struct inode *);
void            iunlockput(struct inode *);
void            iupdate(struct inode *);
int             namecmp(const char *, const char *);
struct inode *  namei(char *);
struct inode *  nameiparent(char *, char *);
void            stati(struct inode *, struct stat *);
ssize_t         readi(struct inode *, char *, size_t, size_t);
ssize_t         writei(struct inode *, char *, size_t, size_t);

// proc.c
void            proc_init();
void            user_init();
void            scheduler();
void            yield();
void            exit();
int             fork();
int             wait();
void            sleep(void *, struct spinlock *);
void            wakeup(void *);
void            drop_priority();
void            raise_priority();
void            set_cpus_allowed(int);

// sd.c
void            sd_init();
void            sd_intr();
void            sdrw(struct buf *);
void            sd_test();

// spinlock.c
int             holding(struct spinlock *);
void            acquire(struct spinlock *);
void            release(struct spinlock *);
void            initlock(struct spinlock *, char *);

// syscall.c
int             fetchstr(uint64_t, char **);
int             argint(int, uint64_t *);
int             argptr(int, char **, int);
int             argstr(int, char **);
int             syscall();

// trap.c
void            trap(struct trapframe *);
void            irq_init();
void            irq_error();

#endif