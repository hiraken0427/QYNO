#include "grapheme_runtime.h"
#include "grapheme_data.h"
static int decode(const unsigned char *s,uint64_t len,uint32_t *cp,unsigned *width)
{
    uint32_t v,min;unsigned n,i;
    if(!len)return 0;
    v=s[0];
    if(v<128){n=1;min=0;}
    else if(v>=194&&v<=223){v&=31;n=2;min=128;}
    else if(v>=224&&v<=239){v&=15;n=3;min=2048;}
    else if(v>=240&&v<=244){v&=7;n=4;min=65536;}else return 0;
    if(len<n)return 0;
    for(i=1;i<n;i++){if(s[i]<128||s[i]>191)return 0;v=(v<<6)|(s[i]&63);}
    if(v<min||v>0x10ffff||(v>=0xd800&&v<=0xdfff))return 0;
    *cp=v;*width=n;return 1;
}
static unsigned property(uint32_t cp)
{
    unsigned lo=0,hi=sizeof(grapheme_ranges)/sizeof(*grapheme_ranges);
    while(lo<hi){unsigned mid=lo+(hi-lo)/2;if(cp<grapheme_ranges[mid].first)hi=mid;
        else if(cp>grapheme_ranges[mid].last)lo=mid+1;else return grapheme_ranges[mid].property;}
    return 0;
}
static int white_space(uint32_t cp)
{
    unsigned i;
    for(i=0;i<sizeof(white_space_ranges)/sizeof(*white_space_ranges);i++)
        if(cp>=white_space_ranges[i].first&&cp<=white_space_ranges[i].last)return 1;
    return 0;
}
void grapheme_begin(GraphemeIterator *it,const unsigned char *s,uint64_t length)
{
    GraphemeIterator zero={0};*it=zero;it->s=s;it->length=length;it->white=1;
}
int grapheme_next(GraphemeIterator *it,uint64_t *offset,uint64_t *width,int *white)
{
    while(it->pos<it->length){
        uint32_t cp;unsigned bytes,flags,p,incb;int boundary=1,emit;
        uint64_t pos=it->pos;
        if(!decode(it->s+pos,it->length-pos,&cp,&bytes))return -1;
        flags=property(cp);p=flags&15;incb=flags>>5;
        if(it->pos){
            if(it->prev==1&&p==2)boundary=0;                                      /* GB3 */
            else if(it->prev==1||it->prev==2||it->prev==3||p==1||p==2||p==3)boundary=1;   /* GB4/5 */
            else if(it->prev==9&&(p==9||p==10||p==12||p==13))boundary=0;         /* GB6 */
            else if((it->prev==12||it->prev==10)&&(p==10||p==11))boundary=0;         /* GB7 */
            else if((it->prev==13||it->prev==11)&&p==11)boundary=0;                  /* GB8 */
            else if(p==4||p==5||p==8||it->prev==7)boundary=0;                    /* GB9/9a/9b */
            else if(incb==1&&it->indic&&it->linker)boundary=0;                       /* GB9c */
            else if((flags&16)&&it->prev==5&&it->zwj_ep)boundary=0;                  /* GB11 */
            else if(it->prev==6&&p==6&&it->ri)boundary=0;                            /* GB12/13 */
        }
        emit=boundary&&it->started;
        if(boundary){
            *offset=it->start;*width=pos-it->start;*white=it->white;
            it->start=pos;it->started=1;it->white=1;
        }
        if(!white_space(cp))it->white=0;
        if(incb==1){it->indic=1;it->linker=0;}else if(incb==2){if(it->indic)it->linker=1;}
        else if(incb!=3){it->indic=0;it->linker=0;}
        it->zwj_ep=p==5&&it->ep;
        if(flags&16)it->ep=1;else if(p!=4)it->ep=0;
        it->ri=p==6?(it->prev==6?!it->ri:1):0;
        it->prev=p;it->pos+=bytes;
        if(emit)return 1;
    }
    if(it->started){*offset=it->start;*width=it->length-it->start;*white=it->white;it->started=0;return 1;}
    return 0;
}
static int scan(const unsigned char *s,uint64_t length,int64_t index,int mode,
                uint64_t *offset,uint64_t *width,uint64_t *count)
{
    GraphemeIterator it;uint64_t start,n,total=0;int white,status;
    *offset=UINT64_MAX;*width=0;*count=0;grapheme_begin(&it,s,length);
    while((status=grapheme_next(&it,&start,&n,&white))>0){
        if(mode==0?(uint64_t)index==total:mode<0?(uint64_t)index>start&&(uint64_t)index<=start+n:(uint64_t)index>=start&&(uint64_t)index<start+n){*offset=start;*width=n;}
        total++;
    }
    if(status<0)return 1;
    *count=total;return 0;
}
int grapheme_scan(const unsigned char *s,uint64_t length,int64_t index,uint64_t *offset,uint64_t *width,uint64_t *count)
{return scan(s,length,index,0,offset,width,count);}
uint64_t grapheme_cursor(const unsigned char *s,uint64_t length,uint64_t cursor,int direction)
{uint64_t off,width,count;if(scan(s,length,(int64_t)cursor,direction,&off,&width,&count)||off==UINT64_MAX)return cursor;return direction<0?off:off+width;}
