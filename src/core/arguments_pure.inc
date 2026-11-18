/* Microsoft CRT command-line grammar, independent of OS and allocation.
 * input=UTF-16LE, length=code units excluding NUL. buffer=NULL measures;
 * otherwise base=capacity. out=(argc without argv[0], record bytes).
 * Records are [uint64 UTF-8 length, UTF-8 bytes, zero padding to 8 bytes]. */
typedef struct { WideRequest *request;uint64_t position,count,begin; } ArgumentsWriter;
static unsigned arguments_unit(const WideRequest *r,uint64_t at)
{
    const unsigned char *p=(const unsigned char *)r->input;
    if(at>=(uint64_t)r->length)return 0;
    return (unsigned)p[at*2]|((unsigned)p[at*2+1]<<8);
}
static void arguments_byte(ArgumentsWriter *w,unsigned value)
{
    WideRequest *r=w->request;
    if(r->status)return;
    if(w->position>=INT64_MAX-8 || (r->buffer&&w->position>=(uint64_t)r->base)){r->status=70;return;}
    if(r->buffer)r->buffer[w->position]=(char)value;
    w->position++;
}
static void arguments_scalar(ArgumentsWriter *w,unsigned value)
{
    if(value<0x80)arguments_byte(w,value);
    else if(value<0x800){arguments_byte(w,0xc0|(value>>6));arguments_byte(w,0x80|(value&63));}
    else if(value<0x10000){arguments_byte(w,0xe0|(value>>12));arguments_byte(w,0x80|((value>>6)&63));arguments_byte(w,0x80|(value&63));}
    else{arguments_byte(w,0xf0|(value>>18));arguments_byte(w,0x80|((value>>12)&63));arguments_byte(w,0x80|((value>>6)&63));arguments_byte(w,0x80|(value&63));}
}
static void arguments_finish(ArgumentsWriter *w)
{
    uint64_t length=w->position-w->begin-8,i;
    if(w->request->buffer&&!w->request->status)
        for(i=0;i<8;i++)w->request->buffer[w->begin+i]=(char)(length>>(8*i));
    while(w->position%8&&!w->request->status)arguments_byte(w,0);
    w->count++;
}
static void arguments_execute(WideRequest *r)
{
    uint64_t at=0,slashes,j;unsigned c,low;int quoted=0,copy;
    ArgumentsWriter w={r,0,0,0};
    if(r->length<0||(uint64_t)r->length>INT64_MAX/2||r->base<0){r->status=70;return;}
    /* argv[0] has special CRT rules: quotes group spaces, backslashes are literal. */
    while((c=arguments_unit(r,at))!=0){
        if(c=='"')quoted=!quoted;
        else if(!quoted&&(c==' '||c=='\t'))break;
        at++;
    }
    for(;;){
        while((c=arguments_unit(r,at))==' '||c=='\t')at++;
        if(!c)break;
        w.begin=w.position;for(j=0;j<8;j++)arguments_byte(&w,0);quoted=0;
        for(;;){
            copy=1;slashes=0;
            while(arguments_unit(r,at)=='\\'){at++;slashes++;}
            c=arguments_unit(r,at);
            if(c=='"'){
                if(slashes%2==0){
                    if(quoted&&arguments_unit(r,at+1)=='"')at++;
                    else{copy=0;quoted=!quoted;}
                }
                slashes/=2;
            }
            for(j=0;j<slashes&&!r->status;j++)arguments_byte(&w,'\\');
            if(r->status)return;
            c=arguments_unit(r,at);
            if(!c||(!quoted&&(c==' '||c=='\t')))break;
            if(copy){
                if(c>=0xd800&&c<=0xdbff){
                    low=arguments_unit(r,at+1);
                    if(low<0xdc00||low>0xdfff){r->status=65;return;}
                    c=0x10000+((c-0xd800)<<10)+(low-0xdc00);at++;
                }else if(c>=0xdc00&&c<=0xdfff){r->status=65;return;}
                arguments_scalar(&w,c);
            }
            at++;
        }
        arguments_finish(&w);if(r->status)return;
    }
    r->out.lo=w.count;r->out.hi=w.position;
}
