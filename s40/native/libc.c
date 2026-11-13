/* Freestanding C/POSIX subset. No Java calls; filesys_* uses the firmware VFS directly; our FILE layout stays private.
 * All allocations and open files are reclaimed when the native session ends.
 */
#include "firmware_s40.h"
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
int errno;
int isupper(int c) {return c>='A'&&c<='Z';}
int islower(int c) {return c>='a'&&c<='z';}
int isalpha(int c) {return isupper(c)||islower(c);}
int isdigit(int c) {return c>='0'&&c<='9';}
int isprint(int c) {return c>=32&&c<127;}
int isspace(int c) {return c==' '||(c>='\t'&&c<='\r');}
int tolower(int c) {return isupper(c)?c+32:c;}
int toupper(int c) {return islower(c)?c-32:c;}
typedef struct block { struct block *prev,*next; size_t size; unsigned pad; } block;
static block *blocks;
void *malloc(size_t n) {
    if (n > UINT_MAX-sizeof(block)) return NULL;
    block *b=fw_os_block_alloc_no_wait(sizeof(block)+(n?n:1));
    if (!b) { errno=ENOMEM; return NULL; }
    b->size=n; b->prev=NULL; b->next=blocks;
    if (blocks) blocks->prev=b;
    blocks=b; return b+1;
}
void free(void *p) {
    if (!p) return;
    block *b=(block *)p-1;
    if (b->prev) b->prev->next=b->next; else blocks=b->next;
    if (b->next) b->next->prev=b->prev;
    fw_os_block_dealloc(b);
}
void *calloc(size_t n,size_t s) {
    if (s && n>UINT_MAX/s) return NULL;
    void *p=malloc(n*s); if(p) memset(p,0,n*s); return p;
}
void *realloc(void *p,size_t n) {
    if (!p) return malloc(n);
    if (!n) { free(p); return NULL; }
    block *b=(block *)p-1;
    if (n<=b->size) return p;
    void *q=malloc(n); if(q) {memcpy(q,p,b->size);free(p);} return q;
}
char *strdup(const char *s) { size_t n=strlen(s)+1; char *p=malloc(n); if(p)memcpy(p,s,n);return p; }
char *strrchr(const char *s,int c) { const char *p=NULL;do {if(*s==(char)c)p=s;}while(*s++);return (char *)p; }
int strncasecmp(const char *a,const char *b,size_t n) { while(n--) {int x=tolower((unsigned char)*a++),y=tolower((unsigned char)*b++);if(x!=y||!x)return x-y;}return 0; }
int strcasecmp(const char *a,const char *b) {return strncasecmp(a,b,UINT_MAX);}
char *getenv(const char *s) {(void)s;return NULL;}
double atof(const char *s) {return strtod(s,NULL);}
char *strerror(int e) {(void)e;return "native file error";}
void exit(int n) {s40_finish(n);} void abort(void) {s40_finish(-99);}

struct s40_file { int fd,eof,error; struct s40_file *next; };
static FILE *files,*logfile;
static FILE console_out={-1,0,0,NULL},console_err={-2,0,0,NULL},console_in={-3,0,0,NULL};
FILE *stdout=&console_out,*stderr=&console_err,*stdin=&console_in;
static char root[96]="D:\\quake\\";
void s40_set_root(const char *s) {snprintf(root,sizeof(root),"%s",s);}
const char *s40_get_root(void) {return root;}
static int path16(const char *s,uint16_t *out) {
    char full[256]; size_t n;
    if (!s || !*s) {errno=EINVAL;return 0;}
    if (s[1]==':') n=snprintf(full,sizeof(full),"%s",s);
    else {while(s[0]=='.' && (s[1]=='/'||s[1]=='\\'))s+=2;n=snprintf(full,sizeof(full),"%s%s",root,s);}
    if(n>=sizeof(full)) {errno=EINVAL;return 0;}
    for(size_t i=0;i<=n;i++)out[i]=(unsigned char)(full[i]=='/'?'\\':full[i]);
    return 1;
}
FILE *fopen(const char *path,const char *mode) {
    uint16_t p[256]; if(!path16(path,p)) return NULL;
    unsigned flags;
    if (*mode=='r') flags=0; else if(*mode=='w')flags=769;else if(*mode=='a')flags=265;else return NULL;
    if (strchr(mode,'+')) flags=(flags|2)&~1u;
    int fd=fw_filesys_open(p,flags,384);
    if(fd<=0) {errno=ENOENT;return NULL;}
    FILE *f=calloc(1,sizeof(*f));
    if(!f){fw_filesys_close(fd);return NULL;}
    f->fd=fd;f->next=files;files=f;return f;
}
int fclose(FILE *f) {
    if(!f||f->fd<0)return EOF;
    FILE **p=&files;while(*p&&*p!=f)p=&(*p)->next;
    if(!*p)return EOF;
    *p=f->next;int r=fw_filesys_close(f->fd);if(logfile==f)logfile=NULL;free(f);return r<0?EOF:0;
}
size_t fread(void *p,size_t s,size_t n,FILE *f) {
    if(!s||!n||!f||f->fd<0||n>INT_MAX/s)return 0;
    unsigned bytes=s*n,done=0;
    while(done<bytes) {int r=fw_filesys_read(f->fd,(char *)p+done,bytes-done);if(r<=0){if(r<0)f->error=1;else f->eof=1;break;}done+=r;}
    return done/s;
}
size_t fwrite(const void *p,size_t s,size_t n,FILE *f) {
    if(!s||!n||!f||n>INT_MAX/s)return 0;
    if(f==stdout||f==stderr) {if(logfile) return fwrite(p,s,n,logfile);return n;}
    if(f->fd<0)return 0;
    unsigned bytes=s*n,done=0;
    while(done<bytes) {int r=fw_filesys_write(f->fd,(const char *)p+done,bytes-done);if(r<=0){f->error=1;break;}done+=r;}
    return done/s;
}
int fseek(FILE *f,long off,int whence) {if(!f||f->fd<0)return -1;int r=fw_filesys_lseek(f->fd,off,whence);if(r<0){f->error=1;return -1;}f->eof=0;return 0;}
long ftell(FILE *f) {return !f||f->fd<0?-1:fw_filesys_lseek(f->fd,0,SEEK_CUR);}
int fflush(FILE *f) {(void)f;return 0;} /* writes are unbuffered */
int feof(FILE *f) {return f->eof;} int ferror(FILE *f) {return f->error;}
int fgetc(FILE *f) {unsigned char c;return fread(&c,1,1,f)==1?c:EOF;}
int fputc(int c,FILE *f) {unsigned char b=c;return fwrite(&b,1,1,f)==1?b:EOF;}
char *fgets(char *p,int n,FILE *f) {int i=0,c;while(i<n-1&&(c=fgetc(f))!=EOF){p[i++]=c;if(c=='\n')break;}p[i]=0;return i?p:NULL;}
int fputs(const char *s,FILE *f) {size_t n=strlen(s);return fwrite(s,1,n,f)==n?0:EOF;}
int vfprintf(FILE *f,const char *fmt,va_list ap) {char buf[1024];int n=vsnprintf(buf,sizeof(buf),fmt,ap);if(n<0)return n;fwrite(buf,1,(unsigned)n<sizeof(buf)?(unsigned)n:sizeof(buf)-1,f);return n;}
int fprintf(FILE *f,const char *fmt,...) {va_list ap;va_start(ap,fmt);int n=vfprintf(f,fmt,ap);va_end(ap);return n;}
int vprintf(const char *fmt,va_list ap) {return vfprintf(stdout,fmt,ap);}
int printf(const char *fmt,...) {va_list ap;va_start(ap,fmt);int n=vprintf(fmt,ap);va_end(ap);return n;}
int putchar(int c) {return fputc(c,stdout);} int puts(const char *s) {fputs(s,stdout);return putchar('\n');}
int remove(const char *s) {uint16_t p[256];return path16(s,p)?fw_filesys_unlink(p):-1;}
int mkdir(const char *s,unsigned mode) {uint16_t p[256];return path16(s,p)?fw_filesys_mkdir(p,mode):-1;}
/* Firmware rename is not exported. Copy+close+unlink stays native but is
 * not atomic on power loss. */
int rename(const char *a,const char *b) {
    FILE *in=fopen(a,"rb");if(!in)return -1;
    FILE *out=fopen(b,"wb");if(!out){fclose(in);return -1;}
    char buf[4096];size_t n;int ok=1;
    while((n=fread(buf,1,sizeof(buf),in)))if(fwrite(buf,1,n,out)!=n){ok=0;break;}
    if(ferror(in))ok=0;
    fclose(in);if(fclose(out))ok=0;
    return ok?remove(a):-1;
}
void s40_set_log(FILE *f) {logfile=f;}
void s40_log_close(void) {if(logfile)fclose(logfile);}
void s40_cleanup(void) {while(files)fclose(files);while(blocks)free(blocks+1);}
