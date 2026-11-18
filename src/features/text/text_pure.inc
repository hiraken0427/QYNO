#include "text_runtime.h"
#define TEXT_LIMIT ((uint64_t)INT64_MAX-64)
typedef struct { GraphemeIterator it;uint64_t position,index; } TextBoundary;
static int text_boundary(TextBoundary *b,uint64_t target)
{
    uint64_t off,n;int white;
    while(b->position<target){
        if(grapheme_next(&b->it,&off,&n,&white)<=0)return 0;
        b->position=off+n;b->index++;
    }
    return b->position==target;
}
static int text_append(TextRequest *t,const unsigned char *s,uint64_t n)
{
    uint64_t i;
    if(n>TEXT_LIMIT-t->used)return 0;
    if(t->phase==2){
        if(t->used+n>t->capacity)return 0;
        for(i=0;i<n;i++)t->buffer[t->used+i]=s[i];
    }
    t->used+=n;return 1;
}
static int text_source(TextRequest *t,uint64_t start,uint64_t length)
{
    return !length || text_append(t,t->text[0]+start,length);
}
static int text_part(TextRequest *t,uint64_t start,uint64_t length)
{
    if(t->count>TEXT_LIMIT/16-1)return 0;
    if(t->phase==2){
        uint64_t *out=(uint64_t *)t->buffer;
        if((t->count+1)*16>t->capacity)return 0;
        out[t->count*2]=start;out[t->count*2+1]=length;
    }
    t->count++;t->used=t->count*16;return 1;
}
static void text_execute(TextRequest *t)
{
    uint64_t i,j,n=t->length[0],m=t->length[1],last=0,off,width,begin=0,end=0;
    int white,status,seen=0,args=t->op==TEXT_TRIM?1:t->op==TEXT_REPLACE?3:2;
    GraphemeIterator it;TextBoundary first={0},second={0};
    t->status=0;t->used=0;t->count=0;t->result=t->op==TEXT_FIND?-1:0;
    if(t->op<0||t->op>TEXT_REPLACE||t->phase<0||t->phase>2){t->status=70;return;}
    if(!t->phase){
        for(i=0;i<(uint64_t)args;i++){
            if(t->length[i]>TEXT_LIMIT){t->status=70;return;}
            grapheme_begin(&it,t->text[i],t->length[i]);
            while((status=grapheme_next(&it,&off,&width,&white))>0){}
            if(status<0){t->status=65;return;}
        }
        if((t->op==TEXT_SPLIT||t->op==TEXT_REPLACE)&&!m){t->status=70;return;}
        if(t->op!=TEXT_TRIM&&m>TEXT_LIMIT/8){t->status=70;return;}
        t->scratch_size=t->op==TEXT_TRIM?0:m*8;return;
    }
    if(t->op==TEXT_TRIM){
        grapheme_begin(&it,t->text[0],n);
        while(grapheme_next(&it,&off,&width,&white)>0){
            if(!white){if(!seen)begin=off;end=off+width;seen=1;}
        }
        if(!text_source(t,begin,end-begin))t->status=70;
        return;
    }
    if(!m){t->result= t->op==TEXT_FIND?0:1;return;}
    if(!t->scratch||t->scratch_size<m*8){t->status=70;return;}
    /* KMP byte candidates; two monotonic iterators check original boundaries.
     * No source-sized table and no rescanning from the start per match. */
    t->scratch[0]=0;
    for(i=1,j=0;i<m;i++){
        while(j&&t->text[1][i]!=t->text[1][j])j=t->scratch[j-1];
        if(t->text[1][i]==t->text[1][j])j++;
        t->scratch[i]=j;
    }
    grapheme_begin(&first.it,t->text[0],n);grapheme_begin(&second.it,t->text[0],n);
    for(i=0,j=0;i<n;i++){
        uint64_t start;
        while(j&&t->text[0][i]!=t->text[1][j])j=t->scratch[j-1];
        if(t->text[0][i]==t->text[1][j])j++;
        if(j!=m)continue;
        start=i+1-m;j=t->scratch[j-1];
        if(!text_boundary(&first,start)||!text_boundary(&second,i+1))continue;
        if(t->op==TEXT_CONTAINS){t->result=1;return;}
        if(t->op==TEXT_STARTS){t->result=start==0;return;}
        if(t->op==TEXT_ENDS){if(i+1==n)t->result=1;continue;}
        if(t->op==TEXT_FIND){t->result=(int64_t)first.index;return;}
        if(t->op==TEXT_SPLIT){if(!text_part(t,last,start-last)){t->status=70;return;}}
        else if(!text_source(t,last,start-last)||!text_append(t,t->text[2],t->length[2])){t->status=70;return;}
        last=i+1;j=0;
    }
    if(t->op==TEXT_SPLIT){if(!text_part(t,last,n-last))t->status=70;}
    if(t->op==TEXT_REPLACE&&!text_source(t,last,n-last))t->status=70;
}
#undef TEXT_LIMIT
