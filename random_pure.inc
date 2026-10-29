#include "random_runtime.h"
/* All arithmetic below is unsigned modular arithmetic, independent of QYNO's
 * checked user arithmetic. Requests validate before touching generator state. */
static uint64_t random_rotl(uint64_t a,int n){return (a<<n)|(a>>(64-n));}
static uint64_t random_word(uint64_t s[4])
{
    uint64_t result=random_rotl(s[1]*5,7)*9,t=s[1]<<17;
    s[2]^=s[0];s[3]^=s[1];s[1]^=s[2];s[0]^=s[3];s[2]^=t;s[3]=random_rotl(s[3],45);return result;
}
static void random_seed(uint64_t s[4],uint64_t seed)
{
    int i;for(i=0;i<4;i++){
        uint64_t z;seed+=UINT64_C(0x9e3779b97f4a7c15);z=seed;
        z=(z^(z>>30))*UINT64_C(0xbf58476d1ce4e5b9);z=(z^(z>>27))*UINT64_C(0x94d049bb133111eb);s[i]=z^(z>>31);
    }
}
static U random_draw(uint64_t s[4],int n)
{
    U a=random_word(s);
    if(n<=64)return n==64?a:a>>(64-n);
    return (a<<(n-64))|((U)random_word(s)>>(128-n));
}
static U random_uniform(uint64_t s[4],U count)
{
    int n=0;U r,value=count-1;
    if(count==1)return 0;
    while(value){n++;value>>=1;}
    do{r=random_draw(s,n);}while(r>=count);return r;
}
typedef struct {U magnitude;int negative;} RandomInteger;
static RandomInteger random_integer_value(WideBits b,int tag)
{
    RandomInteger v;U x=integer(b,tag);v.negative=tag<0&&(I)x<0;v.magnitude=v.negative?0-x:x;return v;
}
static int random_integer_compare(RandomInteger a,RandomInteger b)
{
    if(!a.magnitude)a.negative=0;
    if(!b.magnitude)b.negative=0;
    if(a.negative!=b.negative)return a.negative?-1:1;
    if(a.magnitude==b.magnitude)return 0;
    return (a.magnitude<b.magnitude?-1:1)*(a.negative?-1:1);
}
static int random_integer_range(RandomRequest *r,U *lo,U *hi,int *full)
{
    int n=width((int)r->ty),flags=(int)r->flags,tag1=(int)r->first_tag,tag2=(int)r->second_tag;
    RandomInteger low,high,min,max;WideBits a=r->first,b=r->second;
    min.negative=r->ty<0;min.magnitude=r->ty<0?(U)1<<(n-1):0;
    max.negative=0;max.magnitude=r->ty<0?((U)1<<(n-1))-1:mask((int)r->ty);
    *lo=min.negative?0-min.magnitude:0;*hi=max.magnitude;
    if(flags&1){
        if(flags&8){WideBits t=a;int tag=tag1;a=b;b=t;tag1=tag2;tag2=tag;}
        low=random_integer_value(a,tag1);high=random_integer_value(b,tag2);
        if(random_integer_compare(low,max)>0||random_integer_compare(high,min)<0)return 0;
        if(random_integer_compare(low,min)>=0){
            *lo=low.negative?0-low.magnitude:low.magnitude;
            if(!(flags&2)){if(random_integer_compare(low,max)==0)return 0;(*lo)++;}
        }
        if(random_integer_compare(high,max)<=0){
            *hi=high.negative?0-high.magnitude:high.magnitude;
            if(!(flags&4)){if(random_integer_compare(high,min)==0)return 0;(*hi)--;}
        }
    }
    if(r->ty<0?(I)*lo>(I)*hi:*lo>*hi)return 0;
    *full=*lo==(min.negative?0-min.magnitude:0)&&*hi==max.magnitude;return 1;
}
static WideBits random_next_real(WideBits a,int ty,int up)
{
    U x=bits(a),sign=(U)1<<signbit_pos(ty);
    if(!(x&~sign))x=up?1:sign|1;
    else if((x&sign)?up:!up)x--;else x++;
    return parts(x);
}
static int random_real_range(RandomRequest *r,WideBits *lower,WideBits *upper,WideBits *a,WideBits *b)
{
    int ty=(int)r->ty;Q x,y;
    if(!(r->flags&1))return 0;
    *a=real_bits(real(r->first,(int)r->first_tag),ty);*b=real_bits(real(r->second,(int)r->second_tag),ty);
    if(r->flags&8){WideBits t=*a;*a=*b;*b=t;}
    x=real(*a,ty);y=real(*b,ty);if(!finite_q(x)||!finite_q(y)||x>y)return 0;
    *lower=(r->flags&2)?*a:random_next_real(*a,ty,1);
    *upper=(r->flags&4)?*b:random_next_real(*b,ty,0);
    return real(*lower,ty)<=real(*upper,ty);
}
static void random_big_product(Big *out,U a,U b)
{
    int i,j;for(i=0;i<8;i++)out->v[i]=0;out->n=8;
    for(i=0;i<4;i++){
        uint64_t carry=0;for(j=0;j<4;j++){
            uint64_t value=(uint64_t)(uint32_t)(a>>(32*i))*(uint32_t)(b>>(32*j))+out->v[i+j]+carry;
            out->v[i+j]=(uint32_t)value;carry=value>>32;
        }out->v[i+4]=(uint32_t)carry;
    }trim(out);
}
static void random_big_add(Big *a,const Big *b)
{
    uint64_t carry=0;int i,n=a->n>b->n?a->n:b->n;
    for(i=0;i<n;i++){uint64_t value=(uint64_t)(i<a->n?a->v[i]:0)+(i<b->n?b->v[i]:0)+carry;a->v[i]=(uint32_t)value;carry=value>>32;}
    a->n=n;if(carry)a->v[a->n++]=(uint32_t)carry;
}
static int random_big_bit(const Big *a,int bit)
{return bit>=0&&bit/32<a->n?(int)((a->v[bit/32]>>(bit%32))&1):0;}
static U random_big_round(const Big *a,int right)
{
    U value=0;int i,sticky=0;
    for(i=0;i<128;i++)if(random_big_bit(a,right+i))value|=(U)1<<i;
    if(right<=0)return value;
    for(i=0;i<right-1&&i/32<a->n;i++)if(random_big_bit(a,i)){sticky=1;break;}
    if(random_big_bit(a,right-1)&&(sticky||(value&1)))value++;
    return value;
}
static void random_dyadic(WideBits a,int ty,U *mantissa,int *exponent,int *negative_value)
{
    Quad q;unsigned e;q.q=real(a,ty);*negative_value=(int)(q.u>>127);
    *mantissa=q.u&(((U)1<<112)-1);e=(unsigned)(q.u>>112)&32767;
    if(e)*mantissa|=(U)1<<112;else e=1;
    *exponent=(int)e-16383-112;
}
static WideBits random_interpolate(WideBits a,WideBits b,int ty,U k,int precision)
{
    Big first,second;U m1,m2,value;int e1,e2,negative1,negative2,e,common,p,emin,emax,bias;
    random_dyadic(a,ty,&m1,&e1,&negative1);random_dyadic(b,ty,&m2,&e2,&negative2);
    common=e1<e2?e1:e2;
    random_big_product(&first,m1,((U)1<<precision)-k);random_big_product(&second,m2,k);
    shift(&first,e1-common);shift(&second,e2-common);
    if(negative1==negative2)random_big_add(&first,&second);
    else if(cmp(&first,&second)>=0)sub(&first,&second);
    else{sub(&second,&first);copy(&first,&second);negative1=negative2;}
    if(!first.n)return parts(0);
    format_info(ty,&p,&emin,&emax,&bias);e=bitlen(&first)-1+common-precision;
    value=random_big_round(&first,(e<emin?emin:e)-(p-1)-common+precision);
    if(e<emin)e=emin;
    if(value==((U)1<<p)){value>>=1;e++;}
    if(value>=((U)1<<(p-1)))value=(value&(((U)1<<(p-1))-1))|((U)(e+bias)<<(p-1));
    if(value&&negative1)value|=(U)1<<signbit_pos(ty);
    return parts(value);
}
static void random_execute(RandomRequest *r)
{
    uint64_t state[4],offset=0,length=0,count=0;U low=0,high=0,span=0,threshold,result=0;
    WideBits a={0,0},b={0,0},lower={0,0},upper={0,0};int full=0,i,ty=(int)r->ty;
    r->status=0;r->used=0;r->capacity=0;r->need_random=0;r->out=parts(0);
    if(r->op==1){
        if(!random_integer_range(r,&low,&high,&full)){r->status=70;return;}
        r->need_random=low!=high;r->out=parts(integer(parts(low),ty));
    }else if(r->op==2){
        if(!random_real_range(r,&lower,&upper,&a,&b)){r->status=70;return;}
        r->need_random=real(lower,ty)!=real(upper,ty);r->out=real(lower,ty)==0?parts(0):lower;
    }else if(r->op==3)r->need_random=1;
    else if(r->op==4||r->op==5){
        if(r->length<0||grapheme_scan((const unsigned char *)r->text,(uint64_t)r->length,-1,&offset,&length,&count)){r->status=65;return;}
        if(!count|| (r->op==5&&(r->count<0||r->count>INT64_MAX-32))){r->status=70;return;}
        if(r->op==5){int64_t at;for(at=0;at<r->length;at++)if((unsigned char)r->text[at]<32||(unsigned char)r->text[at]>126){r->status=70;return;}}
        r->capacity=r->op==4?r->length:r->count;
        r->need_random=count>1&&(r->op==4||r->count>0);
    }else if(r->op!=0){r->status=70;return;}
    if(!r->phase)return;
    for(i=0;i<4;i++)state[i]=r->state[i];
    if(r->seed_auto)random_seed(state,r->seed_value);
    if(r->op==0)random_seed(state,r->first.lo);
    else if(r->op==1&&r->need_random){
        int n=width(ty);
        if(full)result=random_draw(state,n);
        else{span=high-low+1;threshold=n==128?(0-span)%span:((U)1<<n)%span;do{result=random_draw(state,n);}while(result<threshold);result=low+result%span;}
        r->out=parts(integer(parts(result),ty));
    }else if(r->op==2&&r->need_random){
        int p,emin,emax,bias;U first,last,k;Q value;
        format_info(ty,&p,&emin,&emax,&bias);first=r->flags&2?0:1;last=((U)1<<p)-(r->flags&4?0:1);
        do{k=first+random_uniform(state,last-first+1);r->out=random_interpolate(a,b,ty,k,p);value=real(r->out,ty);}
        while(value<real(lower,ty)||value>real(upper,ty));
    }else if(r->op==3)r->out=parts(random_word(state)>>63);
    else if(r->op==4){
        U selected=random_uniform(state,count);
        grapheme_scan((const unsigned char *)r->text,(uint64_t)r->length,(int64_t)selected,&offset,&length,&count);
        if(!r->buffer){r->status=70;return;}for(uint64_t at=0;at<length;at++)r->buffer[at]=r->text[offset+at];r->used=(int64_t)length;
    }else if(r->op==5){
        if(r->count&&!r->buffer){r->status=70;return;}
        for(int64_t at=0;at<r->count;at++)r->buffer[at]=r->text[(uint64_t)random_uniform(state,count)];
        r->used=r->count;
    }
    for(i=0;i<4;i++)r->state[i]=state[i];
}
