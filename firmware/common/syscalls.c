/* Minimal newlib system calls. _write is in uart3.c.
 * There is no file system, so everything except the heap is a stub. */

#include <errno.h>
#include <stdint.h>
#include <sys/stat.h>

extern char end;            /* start of the heap, from linker.ld */
extern char _heap_limit;    /* top of the heap, leaves room for the stack */

void *_sbrk(int incr)
{
    static char *heap_end = &end;
    if (heap_end + incr > &_heap_limit) {
        errno = ENOMEM;
        return (void *)-1;
    }
    char *prev = heap_end;
    heap_end += incr;
    return prev;
}

int _close(int fd)                       { (void)fd; return -1; }
int _read(int fd, char *p, int len)      { (void)fd; (void)p; (void)len; return 0; }
int _lseek(int fd, int off, int whence)  { (void)fd; (void)off; (void)whence; return 0; }
int _isatty(int fd)                      { (void)fd; return 1; }
int _fstat(int fd, struct stat *st)      { (void)fd; st->st_mode = S_IFCHR; return 0; }
int _getpid(void)                        { return 1; }
int _kill(int pid, int sig)              { (void)pid; (void)sig; errno = EINVAL; return -1; }
void _exit(int status)                   { (void)status; for (;;) {} }
