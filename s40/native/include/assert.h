void abort(void) __attribute__((noreturn));
#define assert(x) ((x)?(void)0:abort())
