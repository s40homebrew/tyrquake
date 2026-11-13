/* SPDX-License-Identifier: GPL-2.0-or-later
 * Small native implementation of the file calls used by TyrQuake. No host
 * POSIX or libretro VFS runtime is linked. Buffer small reads for save parsing.
 */
#include "firmware_s40.h"
#include <streams/file_stream.h>
#include <retro_dirent.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>
#include "sys.h"

struct s40_rfile { FILE *f; unsigned at,end; int pushed; unsigned char buf[4096]; };
RFILE *rfopen(const char *path,const char *mode) {
    FILE *f=fopen(path,mode); if(!f) return NULL;
    RFILE *r=calloc(1,sizeof(*r));
    if(!r) { fclose(f); return NULL; }
    r->f=f; r->pushed=-1; return r;
}
int rfclose(RFILE *r) { int e=fclose(r->f); free(r); return e; }
int64_t rftell(RFILE *r) { return ftell(r->f)-(r->end-r->at)-(r->pushed>=0); }
int64_t rfseek(RFILE *r,int64_t off,int origin) {
    if(origin==SEEK_CUR) off-=r->end-r->at+(r->pushed>=0);
    if(off<INT_MIN || off>INT_MAX) return -1;
    int ret=fseek(r->f,(long)off,origin);
    if(!ret) { r->at=r->end=0; r->pushed=-1; }
    return ret;
}
int rfgetc(RFILE *r) {
    if(r->pushed>=0) { int c=r->pushed; r->pushed=-1; return c; }
    if(r->at==r->end) { r->end=fread(r->buf,1,sizeof(r->buf),r->f); r->at=0; }
    return r->at<r->end ? r->buf[r->at++] : EOF;
}
int64_t rfread(void *out,size_t size,size_t count,RFILE *r) {
    if(!size || count>INT_MAX/size) return 0;
    size_t bytes=size*count, done=0; unsigned char *p=out;
    if(r->pushed>=0 && bytes) { p[done++]=r->pushed; r->pushed=-1; }
    size_t n=r->end-r->at; if(n>bytes-done) n=bytes-done;
    memcpy(p+done,r->buf+r->at,n); r->at+=n; done+=n;
    if(done<bytes) done+=fread(p+done,1,bytes-done,r->f);
    return done/size;
}
int64_t rfwrite(const void *p,size_t size,size_t count,RFILE *r) {
    if((r->end!=r->at || r->pushed>=0) && rfseek(r,0,SEEK_CUR)) return 0;
    return fwrite(p,size,count,r->f);
}
int rferror(RFILE *r) { return ferror(r->f); }
int rfeof(RFILE *r) { return r->pushed<0 && r->at==r->end && feof(r->f); }
int64_t rfflush(RFILE *r) { return fflush(r->f); }
char *rfgets(char *p,int n,RFILE *r) {
    int i=0,c;
    while(i<n-1 && (c=rfgetc(r))!=EOF) { p[i++]=c; if(c=='\n') break; }
    p[i]=0; return i?p:NULL;
}
int rfprintf(RFILE *r,const char *fmt,...) {
    va_list ap; va_start(ap,fmt); int n=vfprintf(r->f,fmt,ap); va_end(ap); return n;
}
int rfscanf(RFILE *r,const char *fmt,...) {
    /* The engine uses exactly one %i, %f or bounded/unbounded %s per call,
     * followed by whitespace. Keep parsing isolated from newlib's FILE ABI. */
    const char *f=fmt;
    if(*f++!='%') Sys_Error("Unsupported save format");
    unsigned width=0;
    while(isdigit(*f)) width=width*10+(*f++-'0');
    if((*f!='i' && *f!='f' && *f!='s') || (width && width>255))
        Sys_Error("Unsupported save conversion");
    if(!width) width=255;
    char token[256]; unsigned n=0; int c;
    do { c=rfgetc(r); } while(c!=EOF && isspace(c));
    if(c==EOF) return EOF;
    while(c!=EOF && !isspace(c) && n<width) { token[n++]=c; c=rfgetc(r); }
    token[n]=0;
    if(c!=EOF && !isspace(c)) { r->pushed=c; }
    else {
        while(c!=EOF && isspace(c)) c=rfgetc(r);
        r->pushed=c;
    }
    va_list ap; va_start(ap,fmt); int result;
    if(*f=='s') { char *out=va_arg(ap,char *); memcpy(out,token,n+1); result=1; }
    else if(*f=='i') result=sscanf(token,"%i",va_arg(ap,int *));
    else result=sscanf(token,"%f",va_arg(ap,float *));
    va_end(ap); return result;
}
bool path_is_valid(const char *p) { FILE *f=fopen(p,"rb"); if(!f)return false; fclose(f); return true; }
bool path_mkdir(const char *p) { extern int mkdir(const char *,unsigned); return mkdir(p,0777)==0; }
/* Directory completion is optional. Pak enumeration and numbered save slots
 * use known filenames, so no firmware directory-enumeration ABI is needed. */
RDIR *retro_opendir(const char *p) { (void)p; return NULL; }
int retro_readdir(RDIR *d) { (void)d; return 0; }
void retro_closedir(RDIR *d) { (void)d; }
const char *retro_dirent_get_name(RDIR *d) { (void)d; return ""; }
