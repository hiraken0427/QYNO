/* Shared by compiler constant evaluation and the freestanding x64 runtime.
 * GCC's binary128 soft-float implements basic IEEE arithmetic. Decimal parsing
 * uses integer ratios and one ties-to-even rounding at the destination format.
 * No libc or libquadmath is needed by generated programs. */
#include "wide_runtime.h"
#include "grapheme_runtime.h"
__extension__ typedef unsigned __int128 U;
__extension__ typedef __int128 I;
__extension__ typedef __float128 Q;
typedef union { Q q; U u; } Quad;
typedef union { double d; uint64_t u; } Double;
typedef union { float f; uint32_t u; } Float;
#define BIG_WORDS 1100
typedef struct { uint32_t v[BIG_WORDS]; int n; } Big;
static U bits(WideBits v) { return ((U)v.hi << 64) | v.lo; }
static WideBits parts(U v) { WideBits b; b.lo=(uint64_t)v; b.hi=(uint64_t)(v>>64); return b; }
static int width(int t) { return t < 0 ? -t : t; }
static U mask(int t) { int n=width(t); return n==128 ? ~(U)0 : ((U)1<<n)-1; }
static U integer(WideBits b,int t) {
    U v=bits(b)&mask(t); int n=width(t);
    if(t<0 && n<128 && (v>> (n-1))) v|=~mask(t);
    return v;
}
static int negative(U v,int t) { return t<0 && (v>>(width(t)-1)&1); }
static int fits(U v,int from,int to) {
    int neg=negative(v,from), n=width(to);
    if(to>0) return !neg && (n==128 || v<((U)1<<n));
    if(neg) return n==128 || (I)v>=-(I)((U)1<<(n-1));
    return v<((U)1<<(n-1));
}
static void zero(Big *a) { a->n=0; }
static void trim(Big *a) { while(a->n && !a->v[a->n-1]) a->n--; }
static void copy(Big *a,const Big *b) { int i; a->n=b->n; for(i=0;i<b->n;i++) a->v[i]=b->v[i]; }
static void big_u(Big *a,U v) { a->n=0; while(v) { a->v[a->n++]=(uint32_t)v; v>>=32; } }
static int mul_small(Big *a,unsigned b) {
    uint64_t carry=0; int i;
    for(i=0;i<a->n;i++) { uint64_t n=(uint64_t)a->v[i]*b+carry; a->v[i]=(uint32_t)n; carry=n>>32; }
    if(carry) { if(a->n==BIG_WORDS) return 0; a->v[a->n++]=(uint32_t)carry; } return 1;
}
static int add_small(Big *a,unsigned b) {
    uint64_t carry=b; int i=0;
    while(carry) { if(i==a->n) { if(i==BIG_WORDS) return 0; a->v[a->n++]=0; }
        carry+=(uint64_t)a->v[i]; a->v[i++]=(uint32_t)carry; carry>>=32; } return 1;
}
static int cmp(const Big *a,const Big *b) {
    int i; if(a->n!=b->n) return a->n>b->n?1:-1;
    for(i=a->n-1;i>=0;i--) { if(a->v[i]!=b->v[i]) return a->v[i]>b->v[i]?1:-1; }
    return 0;
}
static void sub(Big *a,const Big *b) {
    uint64_t borrow=0; int i;
    for(i=0;i<a->n;i++) { uint64_t n=(uint64_t)(i<b->n?b->v[i]:0)+borrow;
        borrow=(uint64_t)a->v[i]<n; a->v[i]-=(uint32_t)n; } trim(a);
}
static int shift(Big *a,int n) {
    int words=n/32, rem=n%32, i, old=a->n; uint32_t carry=0;
    if(!old || !n) return 1;
    if(old+words+!!rem>BIG_WORDS) return 0;
    for(i=old-1;i>=0;i--) a->v[i+words]=a->v[i];
    for(i=0;i<words;i++) a->v[i]=0;
    a->n=old+words;
    if(rem) { for(i=words;i<a->n;i++) { uint64_t v=((uint64_t)a->v[i]<<rem)|carry; a->v[i]=(uint32_t)v; carry=(uint32_t)(v>>32); }
        if(carry) a->v[a->n++]=carry; } return 1;
}
static void shr(Big *a) { int i; uint32_t carry=0; for(i=a->n-1;i>=0;i--) { uint32_t v=a->v[i]; a->v[i]=(v>>1)|carry; carry=v<<31; } trim(a); }
static int bitlen(const Big *a) { int n=0; uint32_t v; if(!a->n) return 0; v=a->v[a->n-1]; while(v) { n++;v>>=1; } return (a->n-1)*32+n; }
static U quotient(Big *num,const Big *den) {
    Big d; U out=0; int n=bitlen(num)-bitlen(den), i;
    if(n<0) return 0;
    copy(&d,den); shift(&d,n);
    for(i=n;i>=0;i--) { if(cmp(num,&d)>=0) { sub(num,&d); if(i<128) out|=(U)1<<i; } shr(&d); } return out;
}
static int pow10(Big *a,int n) { while(n-->0) if(!mul_small(a,10)) return 0; return 1; }
static void format_info(int t,int *p,int *emin,int *emax,int *bias) {
    if(t==W_HALF) { *p=11;*emin=-14;*emax=15;*bias=15; }
    else if(t==W_FLOAT) { *p=24;*emin=-126;*emax=127;*bias=127; }
    else if(t==W_DOUBLE) { *p=53;*emin=-1022;*emax=1023;*bias=1023; }
    else { *p=113;*emin=-16382;*emax=16383;*bias=16383; }
}
static int signbit_pos(int t) { return t==W_HALF?15:t==W_FLOAT?31:t==W_DOUBLE?63:127; }
static Q real(WideBits b,int t) {
    Quad q; Double d; Float f;
    if(t<1000) { U v=integer(b,t); return t<0?(Q)(I)v:(Q)v; }
    if(t==W_QUAD) { q.u=bits(b); return q.q; }
    if(t==W_DOUBLE) { d.u=b.lo; return (Q)d.d; }
    if(t==W_FLOAT) { f.u=(uint32_t)b.lo; return (Q)f.f; }
    { unsigned h=(unsigned)b.lo, e=(h>>10)&31, m=h&1023, sign=(h&32768)<<16;
      if(!e) { if(!m) f.u=sign; else { int exp=-14; while(!(m&1024)) { m<<=1;exp--; } f.u=sign|((unsigned)(exp+127)<<23)|((m&1023)<<13); } }
      else f.u=sign|(e==31?255:e+112)<<23|(m<<13);
      return (Q)f.f; }
}
static U rounded_shift(U v,int n) {
    U q,rest,half;
    if(n<=0) return v<<-n;
    if(n>128) return 0;
    if(n==128) return v>((U)1<<127);
    q=v>>n; rest=v&(((U)1<<n)-1); half=(U)1<<(n-1);
    return q+(rest>half || (rest==half && (q&1)));
}
static WideBits real_bits(Q value,int t) {
    Quad q; Double d; Float f; U out;
    q.q=value;
    if(t==W_QUAD) return parts(q.u);
    if(t==W_DOUBLE) { d.d=(double)value; return parts(d.u); }
    if(t==W_FLOAT) { f.f=(float)value; return parts(f.u); }
    { U m=q.u&(((U)1<<112)-1); unsigned e=(unsigned)(q.u>>112)&32767, sign=(unsigned)(q.u>>127)<<15;
      int exponent=(int)e-16383;
      if(e==32767) out=sign|31744|(m?512:0);
      else if(!e && !m) out=sign;
      else { if(e) m|=(U)1<<112;
        if(exponent<-14) out=rounded_shift(m,112-10+(-14-exponent));
        else { out=rounded_shift(m,102); if(out==2048) { out>>=1; exponent++; }
          out=exponent>15?31744:((U)(exponent+15)<<10)|(out&1023); }
        out|=sign; }
      return parts(out); }
}
static int finite_q(Q v) { Quad q; q.q=v; return ((q.u>>112)&32767)!=32767; }
static Q power2(int e) { Quad q; q.u=(U)(e+16383)<<112; return q.q; }
static int digit(int c) { return c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?c-'a'+10:c>='A'&&c<='F'?c-'A'+10:-1; }
static int prefix(int c) { return c=='b'||c=='B'?2:c=='o'||c=='O'?8:c=='d'||c=='D'?10:c=='x'||c=='X'?16:0; }
static int whitespace(int c) { return c==' '||c=='\t'; }
static int read_integer(WideRequest *r) {
    int i=0,neg=0,base=r->base,d,first,detected; U value=0,limit=mask(r->ty);
    while(i<r->length && whitespace(r->input[i])) i++;
    if(i<r->length && (r->input[i]=='+'||r->input[i]=='-')) neg=r->input[i++]=='-';
    if(i+1<r->length && r->input[i]=='0' && (detected=prefix(r->input[i+1]))) {
        if(base && base!=detected) return 1;
        base=detected;i+=2;
    }
    if(!base) base=10;
    if(base!=2 && base!=8 && base!=10 && base!=16) return 1;
    if(neg && r->ty>0) return 2;
    if(base==10 && r->ty<0) limit=(limit>>1)+(unsigned)neg;
    first=i;
    for(;i<r->length;i++) {
        d=digit((unsigned char)r->input[i]); if(d<0 || d>=base) break;
        if(value>(limit-(unsigned)d)/(unsigned)base) return 2;
        value=value*(unsigned)base+(unsigned)d;
    }
    if(first==i) return 1;
    if(i<r->length && ((r->input[i]>='0'&&r->input[i]<='9')||(r->input[i]>='a'&&r->input[i]<='z')||(r->input[i]>='A'&&r->input[i]<='Z')||r->input[i]=='_'||r->input[i]=='.')) return 1;
    if(neg) value=0-value;
    r->out=parts(value&mask(r->ty)); r->out=parts(integer(r->out,r->ty));r->used=i;return 0;
}
static int read_real(WideRequest *r) {
    Big num,den,tmp; U value=0; int i=0,neg=0,nd=0,frac=0,point=0,exp=0,eneg=0,e,p,emin,emax,bias,k,c;
    zero(&num);big_u(&den,1);
    while(i<r->length && whitespace(r->input[i])) i++;
    if(i<r->length && (r->input[i]=='+'||r->input[i]=='-')) neg=r->input[i++]=='-';
    for(;i<r->length;i++) {
        c=(unsigned char)r->input[i];
        if(c=='.' && !point) { point=1;continue; }
        if(c<'0'||c>'9') break;
        if(++nd>4096 || !mul_small(&num,10) || !add_small(&num,(unsigned)(c-'0'))) return 2;
        if(point) frac++;
    }
    if(!nd) return 1;
    if(i<r->length && (r->input[i]=='e'||r->input[i]=='E')) {
        i++; if(i<r->length && (r->input[i]=='+'||r->input[i]=='-')) eneg=r->input[i++]=='-';
        k=i; while(i<r->length && r->input[i]>='0'&&r->input[i]<='9') { if(exp<100000) exp=exp*10+r->input[i]-'0';i++; }
        if(i==k) return 1;
    }
    if(i<r->length && ((r->input[i]>='a'&&r->input[i]<='z')||(r->input[i]>='A'&&r->input[i]<='Z')||r->input[i]=='_'||r->input[i]=='.')) return 1;
    r->used=i;format_info(r->ty,&p,&emin,&emax,&bias);
    if(!num.n) { r->out=parts((U)neg<<signbit_pos(r->ty));return 0; }
    exp=(eneg?-exp:exp)-frac;
    if(exp>10000 || exp<-10000) return 2;
    if(exp>=0) { if(!pow10(&num,exp)) return 2; } else if(!pow10(&den,-exp)) return 2;
    e=bitlen(&num)-bitlen(&den);
    if(e>=0) { copy(&tmp,&den);if(!shift(&tmp,e)) return 2;if(cmp(&num,&tmp)<0)e--; }
    else { copy(&tmp,&num);if(!shift(&tmp,-e))return 2;if(cmp(&tmp,&den)<0)e--; }
    if(e>emax) return 2;
    k=(p-1)-(e<emin?emin:e);
    if(k>=0) { if(!shift(&num,k))return 2; } else if(!shift(&den,-k))return 2;
    value=quotient(&num,&den);copy(&tmp,&num);shift(&tmp,1);c=cmp(&tmp,&den);
    if(c>0 || (c==0 && (value&1))) value++;
    if(!value) return 2;
    if(e<emin) e=emin;
    if(value==((U)1<<p)) { value>>=1;e++; }
    if(e>emax) return 2;
    if(value>=((U)1<<(p-1))) value=(value&(((U)1<<(p-1))-1))|((U)(e+bias)<<(p-1));
    r->out=parts(value|((U)neg<<signbit_pos(r->ty)));return 0;
}
static int integer_format(WideRequest *r) {
    U v=integer(r->a,r->ty);int n=0,pos=0,i,base=r->base?r->base:10,neg=base==10&&negative(v,r->ty);char tmp[128];
    if(base!=2&&base!=8&&base!=10&&base!=16)return 1;
    if(neg) v=0-v;else v&=mask(r->ty);
    do { tmp[n++]="0123456789abcdef"[(unsigned)(v%(unsigned)base)];v/=(unsigned)base; } while(v);
    if(neg)r->buffer[pos++]='-';
    if(r->aux) { r->buffer[pos++]='0';r->buffer[pos++]=base==2?'b':base==8?'o':base==10?'d':'x'; }
    for(i=n-1;i>=0;i--)r->buffer[pos++]=tmp[i];
    r->buffer[pos]=0;r->out=parts((unsigned)pos);return 0;
}
static int real_format(WideRequest *r) {
    Quad q; Big num,den,a,b,tmp; U m,n;int e,k,neg,pos=0,digits=r->ty==W_HALF?5:36,scale,c,i,len;char ds[40];
    q.q=real(r->a,r->ty);neg=(int)(q.u>>127);m=q.u&(((U)1<<112)-1);e=(int)((q.u>>112)&32767);
    if(neg && !(e==32767&&m))r->buffer[pos++]='-';
    if(e==32767) { const char *s=m?"nan":"inf";for(i=0;i<3;i++)r->buffer[pos++]=s[i]; }
    else if(!m&&!e) { r->buffer[pos++]='0';r->buffer[pos++]='.';r->buffer[pos++]='0'; }
    else {
        if(e)m|=(U)1<<112;else e=1;
        e=e-16383-112;big_u(&num,m);big_u(&den,1);
        if(e>=0)shift(&num,e);else shift(&den,-e);
        k=((bitlen(&num)-bitlen(&den))*30103)/100000;
        copy(&a,&num);copy(&b,&den);if(k>=0)pow10(&b,k);else pow10(&a,-k);
        while(cmp(&a,&b)<0) { k--;mul_small(&a,10); }
        copy(&tmp,&b);mul_small(&tmp,10);
        while(cmp(&a,&tmp)>=0) { k++;copy(&b,&tmp);mul_small(&tmp,10); }
        copy(&a,&num);copy(&b,&den);scale=digits-1-k;
        if(scale>=0)pow10(&a,scale);else pow10(&b,-scale);
        n=quotient(&a,&b);copy(&tmp,&a);shift(&tmp,1);c=cmp(&tmp,&b);
        if(c>0||(c==0&&(n&1)))n++;
        len=0;do { ds[len++]=(char)('0'+(unsigned)(n%10));n/=10; } while(n);
        if(len>digits) { for(i=1;i<len;i++)ds[i-1]=ds[i];len--;k++; }
        for(i=0;i<len/2;i++){char t=ds[i];ds[i]=ds[len-1-i];ds[len-1-i]=t;}
        while(len>1&&ds[len-1]=='0')len--;
        if(k>=-6&&k<digits) {
            if(k<0) { r->buffer[pos++]='0';r->buffer[pos++]='.';for(i=-1;i>k;i--)r->buffer[pos++]='0';for(i=0;i<len;i++)r->buffer[pos++]=ds[i]; }
            else { for(i=0;i<=k;i++)r->buffer[pos++]=i<len?ds[i]:'0';r->buffer[pos++]='.';
                if(len<=k+1)r->buffer[pos++]='0';else for(i=k+1;i<len;i++)r->buffer[pos++]=ds[i]; }
        } else {
            r->buffer[pos++]=ds[0];r->buffer[pos++]='.';if(len==1)r->buffer[pos++]='0';else for(i=1;i<len;i++)r->buffer[pos++]=ds[i];
            r->buffer[pos++]='e';if(k<0){r->buffer[pos++]='-';k=-k;}else r->buffer[pos++]='+';
            i=0;do{ds[i++]=(char)('0'+k%10);k/=10;}while(k);while(i)r->buffer[pos++]=ds[--i];
        }
    }
    r->buffer[pos]=0;r->out=parts((unsigned)pos);return 0;
}
static Q square_root(Q x) {
    Quad q; Big a,root,rem,test;U m,out=0;int e,i,n;
    q.q=x;if((q.u>>112&32767)==32767){if(x<0){q.u=((U)32767<<112)|((U)1<<111);return q.q;}return x+x;}
    if(x==0)return x;
    if(x<0){q.u=((U)32767<<112)|((U)1<<111);return q.q;}
    m=q.u&(((U)1<<112)-1);e=(int)(q.u>>112)&32767;
    if(e)m|=(U)1<<112;else e=1;
    e-=16383;while(!(m>>112)){m<<=1;e--;}
    big_u(&a,m);shift(&a,112+(e&1));n=(bitlen(&a)+1)/2;zero(&root);zero(&rem);
    for(i=n-1;i>=0;i--) {
        unsigned pair=(a.v[(i*2)/32]>>((i*2)%32))&3;
        shift(&rem,2);add_small(&rem,pair);shift(&root,1);copy(&test,&root);shift(&test,1);add_small(&test,1);
        if(cmp(&rem,&test)>=0){sub(&rem,&test);add_small(&root,1);}
    }
    for(i=root.n-1;i>=0;i--)out=(out<<32)|root.v[i];
    if(cmp(&rem,&root)>0)out++;
    e=(e-(e<0&&(e&1)))/2;
    if(out==((U)1<<113)){out>>=1;e++;}
    q.u=((U)(e+16383)<<112)|(out&(((U)1<<112)-1));return q.q;
}
/* Decimal rounding stays exact even when 10^digits cannot be represented as a
 * binary128 value. Only the final decimal multiple is rounded to the format. */
static WideBits decimal_round(WideRequest *r)
{
    Quad q;Big num,den,twice;U m,value;WideRequest parsed={0};
    int e,neg,pos=0,i,n=0;int64_t places=(int64_t)r->b.lo;uint64_t count;
    char text[80],ds[40];
    q.q=real(r->a,r->ty);neg=(int)(q.u>>127);m=q.u&(((U)1<<112)-1);e=(int)(q.u>>112)&32767;
    if(e==32767||(!e&&!m))return r->a;
    if(e)m|=(U)1<<112;else e=1;
    e-=16383+112;big_u(&num,m);big_u(&den,1);if(e>=0)shift(&num,e);else shift(&den,-e);
    count=places<0?0-(uint64_t)places:(uint64_t)places;
    if(places>=0){
        if(count>10000 || bitlen(&num)-bitlen(&den)+(int)(count*3321/1000)>130)return r->a;
        if(!pow10(&num,(int)count))return r->a;
    }else if(count>10000 || !pow10(&den,(int)count)){
        value=((r->aux==1&&!neg)||(r->aux==2&&neg))?1:0;goto encode;
    }
    if(bitlen(&num)-bitlen(&den)>127)return r->a;
    value=quotient(&num,&den);
    copy(&twice,&num);shift(&twice,1);
    if((r->aux==0&&cmp(&twice,&den)>=0)||(num.n&&((r->aux==1&&!neg)||(r->aux==2&&neg))))value++;
encode:
    if(!value)return parts((U)neg<<signbit_pos(r->ty));
    if(neg)text[pos++]='-';
    do{ds[n++]=(char)('0'+(unsigned)(value%10));value/=10;}while(value);
    for(i=n-1;i>=0;i--)text[pos++]=ds[i];
    text[pos++]='e';text[pos++]=places<0?'+':'-';
    if(count>10000)count=10001;
    n=0;do{ds[n++]=(char)('0'+count%10);count/=10;}while(count);
    while(n)text[pos++]=ds[--n];
    parsed.ty=r->ty;parsed.input=text;parsed.length=pos;
    if(read_real(&parsed)){
        int p,emin,emax,bias;format_info((int)r->ty,&p,&emin,&emax,&bias);
        return parts(((U)neg<<signbit_pos((int)r->ty))|((U)(bias*2+1)<<(p-1)));
    }
    return parsed.out;
}
#include "path_runtime.inc"
#include "time_pure.inc"
/* Compare exact binary values to exact integer powers of ten. */
static void decimal_digits(WideRequest *r)
{
    int n=1;
    if(r->from<1000){
        U a=integer(r->a,(int)r->from);
        if(negative(a,(int)r->from))a=0-a;
        while(a>=10){a/=10;n++;}
    }else{
        Quad q;Big a,p;U m;int e;
        q.q=real(r->a,(int)r->from);q.u&=~((U)1<<127);
        if(!finite_q(q.q)){r->status=70;return;}
        m=q.u&(((U)1<<112)-1);e=(int)(q.u>>112)&32767;
        if(e)m|=(U)1<<112;else e=1;
        e-=16383+112;
        big_u(&a,m);
        if(e>=0)shift(&a,e);else {
            int w=-e,i;
            if(w>=128)big_u(&a,0);else {for(i=0;i<w;i++)m>>=1;big_u(&a,m);}
        }
        big_u(&p,10);
        while(cmp(&a,&p)>=0){n++;mul_small(&p,10);}
    }
    r->out=parts((unsigned)n);
}
void wide_execute(WideRequest *r) {
    U a=0,b=0,v=0,n;Q x=0,y=0,z=0;int op=r->op,ty=r->ty,st=0,neg=0;int64_t e;
    r->status=0;r->out=parts(0);r->used=0;
    if(op==W_DIGITS){decimal_digits(r);return;}
    if(op==W_PATH){path_execute(r);return;}
    if(op==W_TIME){time_execute((TimeRequest *)r->input);r->status=((TimeRequest *)r->input)->status;return;}
    if(op==W_CURSOR){r->out.lo=grapheme_cursor((const unsigned char *)r->input,(uint64_t)r->length,r->b.lo,(int)r->aux);return;}
    if(op==W_TEXT){uint64_t off,width,count;r->status=grapheme_scan((const unsigned char *)r->input,(uint64_t)r->length,(int64_t)r->b.lo,&off,&width,&count);r->out.lo=off;r->out.hi=width;r->used=(int64_t)count;return;}
    if(op==W_READ&&ty==2000){int i=0;while(i<r->length&&whitespace(r->input[i]))i++;if(i+1>=r->length||r->input[i]!='('||r->input[i+1]!=')'){r->status=1;return;}r->used=i+2;return;}
    if(op==W_READ){r->status=ty<1000?read_integer(r):read_real(r);return;}
    if(op==W_FORMAT){r->status=ty<1000?integer_format(r):real_format(r);return;}
    if(op==W_ROUND&&ty>=1000){r->out=decimal_round(r);return;}
    if(op==W_CONVERT) {
        if(ty<1000&&r->from<1000) { a=integer(r->a,r->from);if(!fits(a,r->from,ty))st=2;v=a&mask(ty);r->out=parts(integer(parts(v),ty)); }
        else if(ty<1000) { x=real(r->a,r->from);y=power2(width(ty)-(ty<0));
            if(!finite_q(x)||x<(ty>0?0:-y)||x>=y)st=2;
            else { v=ty<0?(U)(I)x:(U)x;r->out=parts(integer(parts(v),ty)); } }
        else r->out=real_bits(real(r->a,r->from),ty);
        r->status=st;return;
    }
    if(ty<1000) {
        a=integer(r->a,ty);b=integer(r->b,ty);e=(int64_t)r->b.lo;
        switch(op) {
        case W_ADD:v=a+b;break;case W_SUB:v=a-b;break;case W_MUL:v=a*b;break;
        case W_DIV:case W_MOD:
            if(!b){st=3;break;}
            if(ty<0&&a==((U)1<<(width(ty)-1))&&b==~(U)0){if(op==W_DIV)st=2;break;}
            if(ty<0){int an=(I)a<0,bn=(I)b<0;U aa=an?0-a:a,bb=bn?0-b:b;v=op==W_DIV?aa/bb:aa%bb;if(op==W_DIV?an!=bn:an)v=0-v;}
            else v=op==W_DIV?a/b:a%b;
            break;
        case W_NEG:v=0-a;break;case W_ABS:v=negative(a,ty)?0-a:a;break;
        case W_AND:v=a&b;break;case W_OR:v=a|b;break;case W_XOR:v=a^b;break;case W_NOT:v=~a;break;
        case W_SHL:case W_SHR:
            if(e<0||e>=width(ty)){st=2;break;}
            v=op==W_SHL?a<<e:ty<0?(U)((I)a>>e):a>>e;break;
        case W_CMP: {
            int eq=a==b,lt=ty<0?(I)a<(I)b:a<b;
            v=r->aux==0?eq:r->aux==1?!eq:r->aux==2?lt:r->aux==3?lt||eq:r->aux==4?!lt&&!eq:!lt;break; }
        case W_POW:
            n=e<0?(U)(0-(uint64_t)e):(U)e;v=1;
            if(e<0&&a!=1&&(ty>0||a!=~(U)0)){st=a?4:3;break;}
            while(n){if(n&1)v*=a;n>>=1;if(n)a*=a;}break;
        case W_NEXT:
            v=r->aux>0?a+b:a-b;
            if(ty>0)st=r->aux>0?((v&mask(ty))<(a&mask(ty))?5:0):((a&mask(ty))<(b&mask(ty))?5:0);
            else { U sign=(U)1<<(width(ty)-1);st=((r->aux>0?!((a^b)&sign):!!((a^b)&sign))&&((a^v)&sign))?5:0; }break;
        case W_ROUND:
            v=a;if(e>=0||!a)break;
            neg=negative(a,ty);
            n=1;while(e++<0){if(n>mask(ty)/10){v=0;st=((r->aux==1&&!neg)||(r->aux==2&&neg))?2:6;break;}n*=10;}if(st){if(st==6)st=0;break;}
            neg=negative(a,ty);a=neg?0-a:a;b=a%n;v=a-b;
            if((r->aux==0&&b>=(n+1)/2)||(r->aux==1&&!neg&&b)||(r->aux==2&&neg&&b)) { if(v>mask(ty)-n){st=2;break;}v+=n; }
            if(neg)v=0-v;
            if(!fits(v,ty,ty))st=2;
            if(ty<0&&negative(v,ty)!=neg&&v)st=2;
            break;
        default:st=1;
        }
        r->out=parts(op==W_CMP?v:integer(parts(v),ty));
    } else {
        x=real(r->a,ty);y=real(r->b,ty);e=(int64_t)r->b.lo;
        switch(op) {
        case W_ADD:z=x+y;break;case W_SUB:z=x-y;break;case W_MUL:z=x*y;break;case W_DIV:z=x/y;break;
        case W_NEG:z=-x;break;case W_ABS:{Quad q;q.q=x;q.u&=~((U)1<<127);z=q.q;break;}
        case W_CMP:v=r->aux==0?x==y:r->aux==1?x!=y:r->aux==2?x<y:r->aux==3?x<=y:r->aux==4?x>y:x>=y;r->out=parts(v);return;
        case W_POW:
            n=e<0?(U)(0-(uint64_t)e):(U)e;z=1;
            if(e<0&&x==0){st=3;break;}
            if(e<0)x=real(real_bits(1/x,ty),ty);
            while(n){if(n&1)z=real(real_bits(z*x,ty),ty);n>>=1;if(n)x=real(real_bits(x*x,ty),ty);}break;
        case W_SQRT:z=square_root(x);break;
        default:st=1;
        }
        r->out=real_bits(z,ty);
    }
    r->status=st;
}
