/* OS-independent calendar, units and formatting shared by const evaluation and
 * the freestanding numeric image. No allocation or host locale. */
#include "time_runtime.h"
static Q tq(WideBits b){Quad v;v.u=bits(b);return v.q;}
static WideBits tb(Q q){Quad v;v.q=q;return parts(v.u);}
static int64_t tfloor(Q q){int64_t n=(int64_t)q;return q<(Q)n?n-1:n;}
static int64_t tdiv(int64_t a,int64_t b){int64_t n=a/b;return a%b<0?n-1:n;}
static int tleap(int64_t y){return y%4==0&&(y%100!=0||y%400==0);}
static int tmonth(int64_t y,int64_t m)
{static const int ds[]={31,28,31,30,31,30,31,31,30,31,30,31};return m>=1&&m<=12?ds[m-1]+(m==2&&tleap(y)):0;}
static int64_t tdays(int64_t y,int64_t m,int64_t d)
{
    int64_t era,yo,day;y-=m<=2;era=tdiv(y,400);yo=y-era*400;
    day=(153*(m+(m>2?-3:9))+2)/5+d-1;
    return era*146097+yo*365+yo/4-yo/100+day-719468;
}
static void tcivil(int64_t z,TimeDate *d)
{
    int64_t era,day,yo,yd,mp;z+=719468;era=tdiv(z,146097);day=z-era*146097;
    yo=(day-day/1460+day/36524-day/146096)/365;d->year=yo+era*400;
    yd=day-(365*yo+yo/4-yo/100);mp=(5*yd+2)/153;
    d->day=yd-(153*mp+2)/5+1;d->month=mp+(mp<10?3:-9);d->year+=d->month<=2;
}
static int tstamp_range(Q t){return finite_q(t)&&t>=(Q)-62135596800LL&&t<(Q)253402300800LL;}
static int tint(Q q,int64_t *out)
{
    if(!finite_q(q)||q<(Q)INT64_MIN||q>=(Q)9223372036854775808ULL)return 0;
    *out=(int64_t)q;return q==(Q)*out;
}
static int tdate_valid(const TimeDate *d)
{
    Q s=tq(d->second);
    return d->year>=1&&d->year<=9999&&d->month>=1&&d->month<=12&&d->day>=1&&d->day<=tmonth(d->year,d->month)&&
        d->hour>=0&&d->hour<24&&d->minute>=0&&d->minute<60&&finite_q(s)&&s>=0&&s<60&&d->offset_minutes>=-840&&d->offset_minutes<=840;
}
static int tfrom_stamp(Q value,int64_t offset,TimeDate *d)
{
    Q local=value+(Q)(offset*60);int64_t whole,day,within;
    if(offset < -840||offset>840||!tstamp_range(value)||!tstamp_range(local))return 0;
    whole=tfloor(local);day=tdiv(whole,86400);within=whole-day*86400;
    tcivil(day,d);d->hour=within/3600;d->minute=within%3600/60;
    d->second=tb((Q)(within%60)+(local-(Q)whole));d->offset_minutes=offset;return 1;
}
static int tto_stamp(const TimeDate *d,Q *value)
{
    if(!tdate_valid(d))return 0;
    *value=(Q)(tdays(d->year,d->month,d->day)*86400+d->hour*3600+d->minute*60-d->offset_minutes*60)+tq(d->second);
    return tstamp_range(*value);
}
static void tput(TimeRequest *r,char c)
{if(r->used>=r->capacity){r->status=70;return;}r->buffer[r->used++]=c;}
static void tnumber(TimeRequest *r,uint64_t value,int width)
{
    char buf[24];int i=0;do{buf[i++]=(char)('0'+value%10);value/=10;}while(value);
    while(i<width)buf[i++]='0';
    while(i)tput(r,buf[--i]);
}
static I tround(Q value)
{I n=(I)value;Q fraction=value-(Q)n;if(fraction>(Q)0.5||(fraction==(Q)0.5&&(n&1)))n++;if(fraction<(Q)-0.5||(fraction==(Q)-0.5&&(n&1)))n--;return n;}
static int tformatted_date(TimeRequest *r,Q stamp,int64_t offset,int iso)
{
    I total,secs;int64_t ns;TimeDate d;const char *p;int64_t length,i;
    static const char iso_pattern[]="%Y-%m-%dT%H:%M:%S.%N";
    if(!tstamp_range(stamp)||offset < -840||offset>840)return 0;
    total=tround(stamp*(Q)1000000000);secs=total/1000000000;ns=(int64_t)(total%1000000000);
    if(ns<0){ns+=1000000000;secs--;}
    if(!tfrom_stamp((Q)secs,offset,&d))return 0;
    p=iso?iso_pattern:r->pattern;length=iso?(int64_t)(sizeof(iso_pattern)-1):r->pattern_length;
    for(i=0;i<length;i++){
        char c=p[i];int colon=0;
        if(c!='%'){tput(r,c);continue;}
        if(++i>=length)return 0;
        c=p[i];
        if(c==':'){colon=1;if(++i>=length)return 0;
        c=p[i];if(c!='z')return 0;}
        switch(c){
        case 'Y':tnumber(r,(uint64_t)d.year,4);break;
        case 'm':tnumber(r,(uint64_t)d.month,2);break;
        case 'd':tnumber(r,(uint64_t)d.day,2);break;
        case 'H':tnumber(r,(uint64_t)d.hour,2);break;
        case 'M':tnumber(r,(uint64_t)d.minute,2);break;
        case 'S':tnumber(r,(uint64_t)tq(d.second),2);break;
        case 'f':tnumber(r,(uint64_t)ns/1000,6);break;
        case 'N':tnumber(r,(uint64_t)ns,9);break;
        case 'j':tnumber(r,(uint64_t)(tdays(d.year,d.month,d.day)-tdays(d.year,1,1)+1),3);break;
        case 'u':{int64_t w=(tdays(d.year,d.month,d.day)+3)%7;if(w<0)w+=7;tnumber(r,(uint64_t)w+1,1);break;}
        case 'z':{int64_t o=offset;if(o<0){tput(r,'-');o=-o;}else tput(r,'+');tnumber(r,(uint64_t)o/60,2);if(colon)tput(r,':');tnumber(r,(uint64_t)o%60,2);break;}
        case '%':tput(r,'%');break;
        default:return 0;
        }
    }
    if(iso){if(offset==0)tput(r,'Z');else{int64_t o=offset;if(o<0){tput(r,'-');o=-o;}else tput(r,'+');tnumber(r,(uint64_t)o/60,2);tput(r,':');tnumber(r,(uint64_t)o%60,2);}}
    return r->status==0;
}
static int tdigits(const char *s,int64_t length,int64_t *pos,int count,int64_t *out)
{int i;*out=0;for(i=0;i<count;i++){if(*pos>=length||s[*pos]<'0'||s[*pos]>'9')return 0;*out=*out*10+s[(*pos)++]-'0';}return 1;}
static int tsep(const char *s,int64_t length,int64_t *pos,char c)
{if(*pos>=length||s[*pos]!=c)return 0;(*pos)++;return 1;}
static int tparse(TimeRequest *r,Q *out)
{
    TimeDate d={0};int64_t p=0,s=0,n=0,scale=1,hh,mm;const char *text=r->text;
    if(!tdigits(text,r->length,&p,4,&d.year)||!tsep(text,r->length,&p,'-')||!tdigits(text,r->length,&p,2,&d.month)||
       !tsep(text,r->length,&p,'-')||!tdigits(text,r->length,&p,2,&d.day)||!tsep(text,r->length,&p,'T')||
       !tdigits(text,r->length,&p,2,&d.hour)||!tsep(text,r->length,&p,':')||!tdigits(text,r->length,&p,2,&d.minute)||
       !tsep(text,r->length,&p,':')||!tdigits(text,r->length,&p,2,&s))return 0;
    if(p<r->length&&text[p]=='.'){
        int count=0;p++;while(p<r->length&&text[p]>='0'&&text[p]<='9'){if(++count>9)return 0;n=n*10+text[p++]-'0';scale*=10;}if(!count)return 0;
    }
    d.second=tb((Q)s+(Q)n/(Q)scale);
    if(p<r->length&&text[p]=='Z')p++;
    else if(p<r->length&&(text[p]=='+'||text[p]=='-')){
        int sign=text[p++]=='-'?-1:1;
        if(!tdigits(text,r->length,&p,2,&hh)||!tsep(text,r->length,&p,':')||!tdigits(text,r->length,&p,2,&mm)||hh>14||mm>59||(hh==14&&mm))return 0;
        d.offset_minutes=sign*(hh*60+mm);
    }else return 0;
    return p==r->length&&tto_stamp(&d,out);
}
static void time_execute(TimeRequest *r)
{
    Q a=tq(r->args[0]),b=tq(r->args[1]),value=0;int64_t integer=0,other=0;int op=(int)r->op;
    static const int64_t factors[]={1000000000,1000000,1000,1,60,3600,86400,604800};
    r->status=0;r->used=0;r->out=parts(0);
    if(!finite_q(a)||!finite_q(b)||!finite_q(tq(r->args[2]))){r->status=70;return;}
    if(op>=T_NANOSECONDS&&op<=T_IN_WEEKS){
        int i=(op-T_NANOSECONDS)%8,inverse=op>=T_IN_NANOSECONDS;
        value=(i<3)^inverse?a/(Q)factors[i]:a*(Q)factors[i];
        if(!finite_q(value)){r->status=70;return;}r->out=tb(value);return;
    }
    switch(op){
    case T_ELAPSED:case T_SLEEP_UNTIL:r->out=tb(a);return;
    case T_WAIT_PARTS:{
        int64_t sec,ns,ms;Q fraction;
        if(a<0||a>(Q)INT64_MAX){r->status=70;return;}
        if(a>86400)a=86400;
        sec=(int64_t)a;fraction=(a-(Q)sec)*(Q)1000000000;ns=(int64_t)fraction;if((Q)ns<fraction)ns++;
        if(ns==1000000000){sec++;ns=0;}
        fraction=a*(Q)1000;ms=(int64_t)fraction;if((Q)ms<fraction)ms++;
        r->out.lo=(uint64_t)sec;r->out.hi=(uint64_t)ns;r->used=ms;return;}

    case T_SLEEP:if(a<0||a>(Q)INT64_MAX)r->status=70;else r->out=tb(a);return;
    case T_TO_DATETIME:if(!tint(b,&integer)||!tfrom_stamp(a,integer,&r->date))r->status=70;return;
    case T_TO_TIMESTAMP:if(!tto_stamp(&r->date,&value))r->status=70;else r->out=tb(value);return;
    case T_WEEKDAY:case T_DAY_OF_YEAR:
        if(!tto_stamp(&r->date,&value)){r->status=70;return;}
        integer=tdays(r->date.year,r->date.month,r->date.day);
        if(op==T_WEEKDAY){integer=(integer+3)%7;if(integer<0)integer+=7;}
        else integer-=tdays(r->date.year,1,1)-1;
        r->out=parts((U)integer);return;
    case T_IS_LEAP_YEAR:case T_DAYS_IN_MONTH:
        if(!tint(a,&integer)||integer<1||integer>9999){r->status=70;return;}
        if(op==T_IS_LEAP_YEAR)r->out=parts((U)tleap(integer));
        else if(!tint(b,&other)||other<1||other>12)r->status=70;else r->out=parts((U)tmonth(integer,other));
        return;
    case T_PARSE:case T_TRY_PARSE:if(!tparse(r,&value))r->status=65;else r->out=tb(value);return;
    case T_ISOFORMAT:case T_FORMAT:
        if(!tint(op==T_FORMAT?tq(r->args[2]):b,&integer)||!tformatted_date(r,a,integer,op==T_ISOFORMAT))r->status=70;
        return;
    case T_FORMAT_DURATION:{
        I total;U ticks;uint64_t seconds,scale=1;int i,neg=a<0;
        if(!tint(b,&integer)||integer<0||integer>9||a<-(Q)9223372036854775808ULL||a>=(Q)9223372036854775808ULL){r->status=70;return;}
        for(i=0;i<integer;i++)scale*=10;
        total=tround(a*(Q)scale);ticks=(U)(total<0?-total:total);
        if(ticks/scale>(U)INT64_MAX){r->status=70;return;}seconds=(uint64_t)(ticks/scale);
        if(neg&&ticks)tput(r,'-');
        if(seconds>=86400){tnumber(r,seconds/86400,1);tput(r,'d');tput(r,' ');}
        tnumber(r,seconds%86400/3600,2);tput(r,':');tnumber(r,seconds%3600/60,2);tput(r,':');tnumber(r,seconds%60,2);
        if(integer){tput(r,'.');tnumber(r,(uint64_t)(ticks%scale),(int)integer);}return;}
    default:r->status=70;return;
    }
}
