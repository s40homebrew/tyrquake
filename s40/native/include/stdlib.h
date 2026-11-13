#ifndef S40_STDLIB_H
#define S40_STDLIB_H
#include <stddef.h>
void *malloc(size_t); void *calloc(size_t,size_t); void *realloc(void *,size_t); void free(void *);
int atoi(const char *); int abs(int); long strtol(const char *,char **,int); unsigned long strtoul(const char *,char **,int); double strtod(const char *,char **);
double atof(const char *);
int rand(void); void srand(unsigned); void qsort(void *,size_t,size_t,int (*)(const void *,const void *));
char *getenv(const char *); void exit(int) __attribute__((noreturn)); void abort(void) __attribute__((noreturn));
#define EXIT_SUCCESS 0
#define EXIT_FAILURE 1
#endif
