/* Pure, target-specific lexical paths, shared by const evaluation and image.
 * Request: input/length=left UTF-8; b=right pointer/length; from=Windows;
 * aux=operation 0..8; buffer/base=output/capacity. status is 65 or 70.
 */
typedef struct { uint64_t end; int absolute, drive_relative, root_relative; } PathRoot;
static int path_sep(unsigned char c,int win) { return c=='/'||(win&&c=='\\'); }
static int path_alpha(unsigned char c) { return (c>='a'&&c<='z')||(c>='A'&&c<='Z'); }
static PathRoot path_root(const char *s,uint64_t n,int win)
{
    PathRoot r={0,0,0,0};uint64_t i,j;
    if(win&&n>=2&&path_alpha((unsigned char)s[0])&&s[1]==':'){
        r.end=2;r.drive_relative=1;
        if(n>2&&path_sep((unsigned char)s[2],win)){r.end=3;r.absolute=1;r.drive_relative=0;}
        return r;
    }
    if(!n||!path_sep((unsigned char)s[0],win))return r;
    r.end=1;
    if(!win){r.absolute=1;if(n>=2&&s[1]=='/'&&(n==2||s[2]!='/'))r.end=2;return r;}
    r.root_relative=1;
    if(n>=2&&path_sep((unsigned char)s[1],win)){
        i=2;while(i<n&&!path_sep((unsigned char)s[i],win))i++;
        if(i==2||i==n)return r;
        j=++i;while(i<n&&!path_sep((unsigned char)s[i],win))i++;
        if(i==j)return r;
        r.end=i<n?i+1:i;r.absolute=1;r.root_relative=0;
    }
    return r;
}
static uint64_t path_trim(const char *s,uint64_t n,int win,PathRoot root)
{while(n>root.end&&path_sep((unsigned char)s[n-1],win))n--;return n;}
static int path_put(WideRequest *r,char c,uint64_t *pos)
{
    if(*pos>=(uint64_t)r->base){r->status=70;return 0;}
    r->buffer[(*pos)++]=c;return 1;
}
static void path_copy(WideRequest *r,const char *s,uint64_t begin,uint64_t end,uint64_t *pos,int win)
{uint64_t i;for(i=begin;i<end&&!r->status;i++)path_put(r,win&&s[i]=='\\'?'/':s[i],pos);}
static int path_valid(WideRequest *r,const char *s,uint64_t n,int win)
{
    uint64_t off,width,count,i;
    if(grapheme_scan((const unsigned char *)s,n,-1,&off,&width,&count)){r->status=65;return 0;}
    for(i=0;i<n;i++)if(!s[i]){r->status=70;return 0;}
    /* Device namespaces have different rules; do not misparse them as UNC. */
    if(win&&n>=4&&path_sep((unsigned char)s[0],win)&&path_sep((unsigned char)s[1],win)&&
       (s[2]=='?'||s[2]=='.')&&path_sep((unsigned char)s[3],win)){r->status=70;return 0;}
    return 1;
}
static void path_execute(WideRequest *r)
{
    const char *s=r->input,*t=(const char *)(uintptr_t)r->b.lo;
    uint64_t n=(uint64_t)r->length,m=r->b.hi,end,begin,dot,pos=0,i,start;
    int win=r->from!=0,op=(int)r->aux;PathRoot root,other;
    if(!path_valid(r,s,n,win))return;
    if((op==0||op==8)&&!path_valid(r,t,m,win))return;
    root=path_root(s,n,win);end=path_trim(s,n,win,root);
    begin=end;while(begin>root.end&&!path_sep((unsigned char)s[begin-1],win))begin--;
    dot=end;
    for(i=begin+1;i<end;i++)if(s[i]=='.')dot=i;
    if((end-begin==1&&s[begin]=='.')||(end-begin==2&&s[begin]=='.'&&s[begin+1]=='.'))dot=end;
    if(op==5||op==6){r->out.lo=(uint64_t)(op==5?root.absolute:!root.absolute);return;}
    if(op==0){
        other=path_root(t,m,win);
        if(!n){path_copy(r,t,0,m,&pos,win);}
        else if(!m){path_copy(r,s,0,n,&pos,win);}
        else if(other.absolute){path_copy(r,t,0,m,&pos,win);}
        else if(win&&(root.drive_relative||other.drive_relative||other.root_relative)){r->status=70;return;}
        else{
            path_copy(r,s,0,end,&pos,win);
            if(pos&&!path_sep((unsigned char)r->buffer[pos-1],win))path_put(r,'/',&pos);
            i=0;while(i<m&&path_sep((unsigned char)t[i],win))i++;
            path_copy(r,t,i,m,&pos,win);
        }
    }else if(op==1){path_copy(r,s,begin,end,&pos,win);}
    else if(op==2){
        i=begin;while(i>root.end&&path_sep((unsigned char)s[i-1],win))i--;
        if(i)path_copy(r,s,0,i,&pos,win);else path_put(r,'.',&pos);
    }else if(op==3){path_copy(r,s,dot,end,&pos,win);}
    else if(op==4){path_copy(r,s,begin,dot,&pos,win);}
    else if(op==7){
        path_copy(r,s,0,root.end,&pos,win);i=root.end;
        while(i<n){
            while(i<n&&path_sep((unsigned char)s[i],win))i++;
            start=i;while(i<n&&!path_sep((unsigned char)s[i],win))i++;
            if(i==start||(i-start==1&&s[start]=='.'))continue;
            if(pos&&!path_sep((unsigned char)r->buffer[pos-1],win)&&!(root.drive_relative&&pos==2))path_put(r,'/',&pos);
            path_copy(r,s,start,i,&pos,win);
        }
        if(!pos)path_put(r,'.',&pos);
    }else if(op==8){
        if(begin==end||(end-begin==1&&s[begin]=='.')||(end-begin==2&&s[begin]=='.'&&s[begin+1]=='.')||(m&&t[0]!='.')){r->status=70;return;}
        for(i=0;i<m;i++)if(path_sep((unsigned char)t[i],win)){r->status=70;return;}
        path_copy(r,s,0,dot,&pos,win);path_copy(r,t,0,m,&pos,win);
    }else {r->status=70;return;}
    r->out.lo=pos;
}
