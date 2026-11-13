/* Newlib's formatting/parsing routines may allocate through reentrant entry
 * points. Route those allocations into the same tracked native OS heap as
 * Quake. No separate sbrk arena, host syscalls, or firmware FILE objects exist.
 */
#include "firmware_s40.h"
#include <stdlib.h>
#include <errno.h>
void *_malloc_r(void *r,size_t n) {(void)r;return malloc(n);}
void *_calloc_r(void *r,size_t n,size_t s) {(void)r;return calloc(n,s);}
void *_realloc_r(void *r,void *p,size_t n) {(void)r;return realloc(p,n);}
void _free_r(void *r,void *p) {(void)r;free(p);}
void __malloc_lock(void *r) {(void)r;}
void __malloc_unlock(void *r) {(void)r;}
/* These are only link dependencies of newlib's shared formatting machinery.
 * Quake files use libc.c's private FILE API; accidental POSIX calls fail. */
int _write(int fd,const void *p,unsigned n) {
    if(fd==1||fd==2)return fwrite(p,1,n,fd==1?stdout:stderr);
    errno=EINVAL;return -1;
}
int _read(int fd,void *p,unsigned n) {(void)fd;(void)p;(void)n;return -1;}
int _close(int fd) {(void)fd;return -1;}
int _lseek(int fd,int off,int whence) {(void)fd;(void)off;(void)whence;return -1;}
int _fstat(int fd,void *st) {(void)fd;(void)st;return -1;}
int _isatty(int fd) {return fd==1||fd==2;}
int _getpid(void) {return 1;}
int _kill(int pid,int signal) {(void)pid;(void)signal;return -1;}
void _exit(int status) {s40_finish(status);}
/* This C-only image exits with setjmp/longjmp. Newlib archive members carry
 * EHABI tables, but no exception runtime is installed. Fail any unexpected
 * unwind request instead of pulling libgcc's dynamic C++ resolver into Quake. */
int __aeabi_unwind_cpp_pr0(int state,void *exception,void *context) {
    (void)state;(void)exception;(void)context;return 9; /* _URC_FAILURE */
}
int __aeabi_unwind_cpp_pr1(int state,void *exception,void *context) {
    return __aeabi_unwind_cpp_pr0(state,exception,context);
}
int __aeabi_unwind_cpp_pr2(int state,void *exception,void *context) {
    return __aeabi_unwind_cpp_pr0(state,exception,context);
}
