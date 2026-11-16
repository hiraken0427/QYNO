#ifndef _WIN32
#define _XOPEN_SOURCE 700
#endif
#include "qyc.h"
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#include <io.h>
#else
#include <unistd.h>
#endif

int diagnostic_japanese;
int diagnostic_plain;
int diagnostic_quiet;
static int color_mode; /* 0=auto, 1=always, 2=never */
static int phase;
static char *input_name;

static void append(Buffer *b,const char *fmt,...)
{
    va_list ap,copy;int n;char *s;
    va_start(ap,fmt);va_copy(copy,ap);n=vsnprintf(NULL,0,fmt,copy);va_end(copy);
    if(n<0){va_end(ap);return;}
    s=malloc((size_t)n+1);if(!s){va_end(ap);fputs(diagnostic_japanese?"qyc: メモリを確保できません\n":"qyc: out of memory\n",stderr);exit(1);}
    vsnprintf(s,(size_t)n+1,fmt,ap);va_end(ap);buf_add(b,(const unsigned char *)s,(size_t)n);free(s);
}
void diagnostic_phase(int value){phase=value;}
void diagnostic_color(int value){color_mode=value;}
#ifdef _WIN32
#ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
#define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
#endif
static HANDLE console_handles[2];
static DWORD console_modes[2];
static int console_saved[2],console_restore_registered;
static void restore_console_modes(void)
{
    int i;for(i=0;i<2;i++)if(console_saved[i])SetConsoleMode(console_handles[i],console_modes[i]);
}
static int enable_console_color(FILE *out)
{
    int index=out==stderr?1:0;HANDLE h=(HANDLE)_get_osfhandle(_fileno(out));DWORD mode;
    if(!GetConsoleMode(h,&mode))return 0;
    if(!console_saved[index]){
        console_handles[index]=h;console_modes[index]=mode;console_saved[index]=1;
        if(!console_restore_registered){atexit(restore_console_modes);console_restore_registered=1;}
    }
    return SetConsoleMode(h,mode|ENABLE_VIRTUAL_TERMINAL_PROCESSING)!=0;
}
#endif
static int colored(FILE *out)
{
    if(diagnostic_plain||color_mode==2)return 0;
    if(color_mode==1){
#ifdef _WIN32
        enable_console_color(out);
#endif
        return 1;
    }
    if(getenv("NO_COLOR"))return 0;
#ifdef _WIN32
    return enable_console_color(out);
#else
    return isatty(fileno(out))&&(!getenv("TERM")||strcmp(getenv("TERM"),"dumb"));
#endif
}
static char *absolute(const char *path)
{
    char *full,*cwd;size_t cap=256,n;
    if(!path)return NULL;
    if(path[0]=='<')return NULL;
#ifdef _WIN32
    full=_fullpath(NULL,path,0);if(full)return full;
#else
    full=realpath(path,NULL);if(full)return full;
    if(path[0]=='/')return NULL;
#endif
    for(;;){
        cwd=malloc(cap);if(!cwd)return NULL;
#ifdef _WIN32
        if(_getcwd(cwd,(int)cap))break;
#else
        if(getcwd(cwd,cap))break;
#endif
        free(cwd);if(errno!=ERANGE||cap>1024*1024)return NULL;cap*=2;
    }
    n=strlen(cwd)+strlen(path)+2;full=malloc(n);
    if(full)snprintf(full,n,"%s/%s",cwd,path);
    free(cwd);return full;
}
void diagnostic_input(const char *path)
{free(input_name);input_name=absolute(path);if(!input_name&&path){input_name=malloc(strlen(path)+1);if(input_name)strcpy(input_name,path);}}
const char *diagnostic_path(SourceFile *source)
{
    if(!source)return "<runtime>";
    if(diagnostic_plain)return source->filename;
    if(source->canonical_path)return source->canonical_path;
    if(!source->display_path)source->display_path=absolute(source->filename);
    return source->display_path?source->display_path:source->filename;
}
static void line_text(SourceFile *source,int number,const char **start,size_t *length)
{
    const char *p=source->input,*q;int line=1;
    while(*p&&line<number)if(*p++=='\n')line++;
    q=p;while(*q&&*q!='\n'&&*q!='\r')q++;
    *start=p;*length=(size_t)(q-p);
}
static int wide(uint32_t c)
{
    return c>=0x1100&&(c<=0x115f||c==0x2329||c==0x232a||
        (c>=0x2e80&&c<=0xa4cf&&c!=0x303f)||(c>=0xac00&&c<=0xd7a3)||
        (c>=0xf900&&c<=0xfaff)||(c>=0xfe10&&c<=0xfe19)||
        (c>=0xfe30&&c<=0xfe6f)||(c>=0xff01&&c<=0xff60)||
        (c>=0xffe0&&c<=0xffe6)||(c>=0x1f000&&c<=0x1faff)||c>=0x20000);
}
static int cluster_width(const unsigned char *p,size_t n)
{
    uint32_t c;size_t step;int width=0;
    while(n&&utf8_next(p,n,&c,&step)){
        if(c<32||c==127)return 4;
        if(wide(c))width=2;
        else if(!width&&c!=0x200d&&!(c>=0x300&&c<=0x36f)&&!(c>=0xfe00&&c<=0xfe0f))width=1;
        p+=step;n-=step;
    }
    return width;
}
/* Byte columns remain the compiler protocol; tabs/graphemes use terminal cells
 * only when drawing the source and underline. Invalid/control bytes are escaped. */
static size_t draw(Buffer *out,const char *s,size_t n,size_t first,size_t stop)
{
    GraphemeIterator it;uint64_t off,len;int white,r;size_t col=0,i;
    grapheme_begin(&it,(const unsigned char *)s,n);
    while((r=grapheme_next(&it,&off,&len,&white))>0){
        if(off>=stop)break;
        Buffer *visible=off>=first?out:NULL;
        if(len==1&&s[off]=='\t'){
            size_t count=4-col%4;for(i=0;i<count;i++)if(visible)append(visible," ");col+=count;
        }else if(len==1&&((unsigned char)s[off]<32||(unsigned char)s[off]==127)){
            if(visible)append(visible,"\\x%02X",(unsigned char)s[off]);
            col+=4;
        }else{
            if(visible)buf_add(visible,(const unsigned char *)s+off,(size_t)len);
            col+=(size_t)cluster_width((const unsigned char *)s+off,(size_t)len);
        }
    }
    if(r<0){
        /* The scanner stops on malformed UTF-8. Render the rest without ever
         * sending raw control/invalid bytes to the terminal. */
        i=(size_t)it.start;if(i>n)i=0;
        while(i<n&&i<stop){if(out&&i>=first)append(out,"\\x%02X",(unsigned char)s[i]);col+=4;i++;}
    }
    return col;
}
static void excerpt(Buffer *out,SourceFile *source,int number,int column,int span,int color)
{
    const char *line;size_t length,start,end,left,width,i,first,last,base;int digits;
    if(!source||number<1)return;
    line_text(source,number,&line,&length);
    start=column>0?(size_t)column-1:0;if(start>length)start=length;
    end=start+(span>0?(size_t)span:1);if(end>length)end=length;
    /* Bound excerpts in generated programs as well as terminal diagnostics.
     * Keep UTF-8 boundaries; tab stops retain their original line positions. */
    first=start>80?start-80:0;
    while(first&&((unsigned char)line[first]&0xc0)==0x80)first--;
    last=first+240;if(last>length)last=length;
    while(last<length&&((unsigned char)line[last]&0xc0)==0x80)last++;
    if(end>last)end=last;
    base=draw(NULL,line,length,0,first);
    left=draw(NULL,line,length,0,start)-base+(first?3:0);
    width=draw(NULL,line,length,0,end)-draw(NULL,line,length,0,start);if(!width)width=1;
    digits=snprintf(NULL,0,"%d",number);
    append(out,"  %s%d | %s",color?"\033[36m":"",number,color?"\033[0m":"");
    if(first)append(out,"...");
    draw(out,line,length,first,last);
    if(last<length)append(out,"...");
    append(out,"\n");
    append(out,"  %*s | ",digits,"");for(i=0;i<left;i++)append(out," ");
    if(color)append(out,"\033[1;31m");
    for(i=0;i<width;i++)append(out,"^");
    if(color)append(out,"\033[0m");
    append(out,"\n");
}
static const char *heading(int warning,const char *fmt)
{
    int p=phase;
    if(warning)return diagnostic_japanese?"警告":"Warning";
    /* Undefined names use the requested source/syntax-error heading. */
    if(strstr(fmt,"not defined")||strstr(fmt,"not declared")||strstr(fmt,"定義されていません")||strstr(fmt,"宣言されていません"))p=1;
    if(p==1)return diagnostic_japanese?"構文エラー":"Syntax error";
    if(p==2)return diagnostic_japanese?"型エラー":"Type error";
    return diagnostic_japanese?"コンパイルエラー":"Compile error";
}
static void report(SourceFile *source,int line,int col,int span,int warning,const char *fmt,va_list ap)
{
    Buffer b;va_list copy;int n;char *message;const char *level=warning?(diagnostic_japanese?"警告":"warning"):(diagnostic_japanese?"エラー":"error");
    va_copy(copy,ap);n=vsnprintf(NULL,0,fmt,copy);va_end(copy);if(n<0)n=0;
    message=malloc((size_t)n+1);if(!message){fputs(diagnostic_japanese?"qyc: メモリを確保できません\n":"qyc: out of memory\n",stderr);exit(1);}vsnprintf(message,(size_t)n+1,fmt,ap);
    buf_init(&b);
    if(diagnostic_plain){
        if(source){const char *s;size_t len,i;line_text(source,line,&s,&len);append(&b,"%s:%d:%d: %s: %s\n%.*s\n",source->filename,line,col,level,message,(int)len,s);for(i=0;i<(size_t)(col>0?col-1:0)&&i<len;i++)append(&b,s[i]=='\t'?"\t":" ");for(i=0;i<(size_t)(span>0?span:1)&&i<128;i++)append(&b,"^");append(&b,"\n");}
        else append(&b,"qyc: %s: %s\n",level,message);
    }else{
        int color=colored(stderr);const char *path=source?diagnostic_path(source):input_name;
        append(&b,"%s%s%s",color?(warning?"\033[1;33m":"\033[1;31m"):"",(source||warning?heading(warning,fmt):diagnostic_japanese?"コンパイルエラー":"Compile error"),color?"\033[0m":"");
        if(path)append(&b,": \"%s\"",path);
        append(&b,"\n");
        if(source)append(&b,diagnostic_japanese?"%d行目、%d列目: %s\n":"Line %d, column %d: %s\n",line,col,message);
        else append(&b,"%s\n",message);
        excerpt(&b,source,line,col,span,color);
    }
    fwrite(b.data,1,b.len,stderr);buf_free(&b);free(message);
}
void fatal(const char *fmt,...)
{va_list ap;va_start(ap,fmt);report(NULL,0,0,1,0,fmt,ap);va_end(ap);exit(1);}
void error_at(int line,int col,const char *fmt,...)
{va_list ap;va_start(ap,fmt);report(current_file,line,col,1,0,fmt,ap);va_end(ap);exit(1);}
void error_tok(Token *tok,const char *fmt,...)
{va_list ap;va_start(ap,fmt);report(tok->source,tok->line,tok->col,tok->len,0,fmt,ap);va_end(ap);exit(1);}
void warning_tok(Token *tok,const char *fmt,...)
{va_list ap;va_start(ap,fmt);report(tok->source,tok->line,tok->col,tok->len,1,fmt,ap);va_end(ap);}
void diagnostic_success(void)
{
    int color;if(diagnostic_quiet)return;color=colored(stdout);
    printf("%s%s%s\n",color?"\033[32m":"",diagnostic_japanese?"コンパイルが完了しました":"Compilation completed successfully",color?"\033[0m":"");
}
static void runtime_prefix(Buffer *out,Token *tok,const char *ns,const char *operation)
{
    const char *path=tok?diagnostic_path(tok->source):"<runtime>";
    int line=tok?tok->line:0,col=tok?tok->col:0;
    if(diagnostic_plain)append(out,diagnostic_japanese?"QYNO 実行時エラー: %s:%d:%d: ":"QYNO runtime error: %s:%d:%d: ",path,line,col);
    else{
        append(out,diagnostic_japanese?"実行時エラー: \"%s\"\n":"Runtime error: \"%s\"\n",path);
        if(tok)append(out,diagnostic_japanese?"%d行目、%d列目: ":"Line %d, column %d: ",line,col);
    }
    if(ns&&operation)append(out,"%s.%s: ",ns,operation);
}
void diagnostic_runtime(Buffer *out,Token *tok,const char *message)
{
    runtime_prefix(out,tok,NULL,NULL);append(out,"%s\n",message);
    if(!diagnostic_plain&&tok)excerpt(out,tok->source,tok->line,tok->col,tok->len,0);
}
/* Two consecutive length-prefixed strings: the public-compatible prefix and
 * an optional source excerpt, kept together by the backend's data selector. */
void diagnostic_context(Buffer *out,Token *tok,const char *ns,const char *operation)
{
    Buffer prefix,tail;buf_init(&prefix);buf_init(&tail);runtime_prefix(&prefix,tok,ns,operation);
    if(!diagnostic_plain&&tok)excerpt(&tail,tok->source,tok->line,tok->col,tok->len,0);
    buf_add_u64(out,(uint64_t)prefix.len);buf_add(out,prefix.data,prefix.len);
    buf_add_u64(out,(uint64_t)tail.len);buf_add(out,tail.data,tail.len);buf_free(&prefix);buf_free(&tail);
}
