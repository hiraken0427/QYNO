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
static int scan(const unsigned char *s,uint64_t length,int64_t index,int mode,
                  uint64_t *offset,uint64_t *width,uint64_t *count)
{
    uint64_t pos=0,start=0,total=0;unsigned prev=0,ri=0,ep=0,zwj_ep=0,indic=0,linker=0;
    *offset=UINT64_MAX;*width=0;*count=0;
    while(pos<length){
        uint32_t cp;unsigned bytes,flags,p,incb;int boundary=1;
        if(!decode(s+pos,length-pos,&cp,&bytes))return 1;
        flags=property(cp);p=flags&15;incb=flags>>5;
        if(pos){
            if(prev==1&&p==2)boundary=0;                                      /* GB3 */
            else if(prev==1||prev==2||prev==3||p==1||p==2||p==3)boundary=1;   /* GB4/5 */
            else if(prev==9&&(p==9||p==10||p==12||p==13))boundary=0;         /* GB6 */
            else if((prev==12||prev==10)&&(p==10||p==11))boundary=0;         /* GB7 */
            else if((prev==13||prev==11)&&p==11)boundary=0;                  /* GB8 */
            else if(p==4||p==5||p==8||prev==7)boundary=0;                    /* GB9/9a/9b */
            else if(incb==1&&indic&&linker)boundary=0;                       /* GB9c */
            else if((flags&16)&&prev==5&&zwj_ep)boundary=0;                  /* GB11 */
            else if(prev==6&&p==6&&ri)boundary=0;                            /* GB12/13 */
        }
        if(boundary){
            if(total&&(mode==0?(uint64_t)index==total-1:mode<0?(uint64_t)index>start&&(uint64_t)index<=pos:(uint64_t)index>=start&&(uint64_t)index<pos)){*offset=start;*width=pos-start;}
            start=pos;total++;
        }
        if(incb==1){indic=1;linker=0;}else if(incb==2){if(indic)linker=1;}
        else if(incb!=3){indic=0;linker=0;}
        zwj_ep=p==5&&ep;
        if(flags&16)ep=1;else if(p!=4)ep=0;
        ri=p==6?(prev==6?!ri:1):0;
        prev=p;pos+=bytes;
    }
    if(total&&(mode==0?(uint64_t)index==total-1:mode<0?(uint64_t)index>start&&(uint64_t)index<=pos:(uint64_t)index>=start&&(uint64_t)index<pos)){*offset=start;*width=pos-start;}
    *count=total;return 0;
}

int grapheme_scan(const unsigned char *s,uint64_t length,int64_t index,uint64_t *offset,uint64_t *width,uint64_t *count)
{return scan(s,length,index,0,offset,width,count);}
uint64_t grapheme_cursor(const unsigned char *s,uint64_t length,uint64_t cursor,int direction)
{uint64_t off,width,count;if(scan(s,length,(int64_t)cursor,direction,&off,&width,&count)||off==UINT64_MAX)return cursor;return direction<0?off:off+width;}
