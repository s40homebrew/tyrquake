#ifndef S40_STDIO_H
#define S40_STDIO_H
#include <stddef.h>
#include <stdarg.h>
/* Keep our private FILE ABI separate from newlib's internal string formatter
 * streams. In particular, _svfprintf_r may pull newlib fclose/fflush into the
 * image; those must never receive a struct s40_file (or vice versa). */
#define stdout s40_stdout
#define stderr s40_stderr
#define stdin s40_stdin
#define fopen s40_fopen
#define fclose s40_fclose
#define fread s40_fread
#define fwrite s40_fwrite
#define fseek s40_fseek
#define ftell s40_ftell
#define fflush s40_fflush
#define feof s40_feof
#define ferror s40_ferror
#define fgetc s40_fgetc
#define fputc s40_fputc
#define fgets s40_fgets
#define fputs s40_fputs
#define putchar s40_putchar
#define puts s40_puts
#define printf s40_printf
#define fprintf s40_fprintf
#define vprintf s40_vprintf
#define vfprintf s40_vfprintf
#define remove s40_remove
#define rename s40_rename
typedef struct s40_file FILE;
extern FILE *stdout,*stderr,*stdin;
#define EOF (-1)
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2
FILE *fopen(const char *,const char *); int fclose(FILE *); size_t fread(void *,size_t,size_t,FILE *); size_t fwrite(const void *,size_t,size_t,FILE *);
int fseek(FILE *,long,int); long ftell(FILE *); int fflush(FILE *); int feof(FILE *); int ferror(FILE *);
int fgetc(FILE *); int fputc(int,FILE *); char *fgets(char *,int,FILE *); int fputs(const char *,FILE *);
int putchar(int); int puts(const char *); int printf(const char *,...); int fprintf(FILE *,const char *,...);
int vprintf(const char *,va_list); int vfprintf(FILE *,const char *,va_list);
int sprintf(char *,const char *,...); int snprintf(char *,size_t,const char *,...); int vsnprintf(char *,size_t,const char *,va_list);
int sscanf(const char *,const char *,...); int remove(const char *); int rename(const char *,const char *);
#define getc fgetc
#define putc fputc
#endif
