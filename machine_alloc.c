/* Shared scalar register allocator. Cached plans and fixed-layout streaming
 * use the same state transitions; no x64 bytes are produced here. */
#include "machine_ir.h"
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#ifndef QYNO_MACHINE_PLAN_BUDGET
#define QYNO_MACHINE_PLAN_BUDGET 262144
#endif

typedef struct {
    MachineRegion *r;
    int location[512],home[512],written[512],slots[512],gp[7],fp[6];
    int at,profile,fixed,peak;
    MachineSink sink;
    void *context;
} Allocator;

int machine_is_float(Type ty){return ty==TY_FLOAT||ty==TY_DOUBLE;}
int machine_is_barrier(const MachineInst *p)
{
    return p->op==MI_OPAQUE||(p->op==MI_STORE&&p->root->is_ref)||
        (p->op==MI_BINARY&&type_is_integer(p->ty)&&
         (p->source->kind==ND_DIV||p->source->kind==ND_MOD));
}
static int send(Allocator *s,int kind,int value,int reg,int slot,int a,int b,int d,int sa,int sb)
{
    MachineEvent e={kind,s->at,value,reg,slot,a,b,d,sa,sb};
    return s->sink(s->context,&e);
}
static int *owners(Allocator *s,int value)
{return machine_is_float(s->r->inst[value].ty)?s->fp:s->gp;}
static int expire(Allocator *s)
{
    int i,reg,slot;
    for(i=0;i<s->at;i++)if(s->r->inst[i].last<s->at&&(s->location[i]>=0||s->home[i]>=0)){
        reg=s->location[i];slot=s->home[i];
        if(!send(s,ME_EXPIRE,i,reg,slot,-1,-1,-1,-1,-1))return 0;
        if(reg>=0)owners(s,i)[reg]=-1;
        if(slot>=0)s->slots[slot]=-1;
        s->location[i]=s->home[i]=-1;s->written[i]=0;
    }
    return 1;
}
static int spill(Allocator *s,int value)
{
    int reg=s->location[value],slot=s->home[value],kind=ME_FORGET;
    if(reg<0)return 1;
    if(!s->written[value]){
        if(slot<0){
            if(s->fixed)slot=value;
            else for(slot=0;slot<512&&s->slots[slot]>=0;slot++);
            if(slot>=512)fatal("内部エラー: 数値退避領域が不足しています");
            s->home[value]=slot;s->slots[slot]=value;
            if(slot+1>s->peak)s->peak=slot+1;
        }
        s->written[value]=1;kind=ME_SPILL;
    }
    if(!send(s,kind,value,reg,slot,-1,-1,-1,-1,-1))return 0;
    owners(s,value)[reg]=-1;s->location[value]=-1;return 1;
}
static int get_register(Allocator *s,int fp,int avoid_a,int avoid_b)
{
    int *o=fp?s->fp:s->gp,count=fp?(s->profile?5:6):7,i,victim=-1,value;
    for(i=0;i<count;i++){
        value=o[i];
        if(value<0)return i;
        if(value!=avoid_a&&value!=avoid_b&&(victim<0||s->r->inst[value].last>s->r->inst[o[victim]].last))victim=i;
    }
    if(victim<0)fatal("内部エラー: 数値レジスタを割り当てられません");
    if(!spill(s,o[victim]))return -1;
    return victim;
}
static int operand(Allocator *s,int value,int other,int *reg)
{
    int n;
    if(value<0){*reg=-1;return 1;}
    if(s->location[value]>=0){*reg=s->location[value];return 1;}
    if(!s->written[value]||s->home[value]<0)fatal("内部エラー: 退避されていない数値です");
    n=get_register(s,machine_is_float(s->r->inst[value].ty),value,other);
    if(n<0||!send(s,ME_RELOAD,value,n,s->home[value],-1,-1,-1,-1,-1))return 0;
    owners(s,value)[n]=value;s->location[value]=n;*reg=n;return 1;
}
int machine_allocate(MachineRegion *r,int profile,int fixed,MachineSink sink,void *context,int *peak)
{
    Allocator state,*s=&state;
    int i,j,a,b,d,sa,sb;
    MachineInst *p;
    memset(s,0,sizeof(*s));s->r=r;s->profile=profile;s->fixed=fixed;s->sink=sink;s->context=context;
    for(i=0;i<512;i++)s->location[i]=s->home[i]=s->slots[i]=-1;
    for(i=0;i<7;i++)s->gp[i]=-1;
    for(i=0;i<6;i++)s->fp[i]=-1;
    for(i=0;i<r->count;i++){
        s->at=i;p=&r->inst[i];a=b=d=sa=sb=-1;
        if(!expire(s))return 0;
        if(machine_is_barrier(p)){
            for(j=0;j<i;j++)if(s->location[j]>=0&&!spill(s,j))return 0;
            if(p->a>=0)sa=s->home[p->a];
            if(p->b>=0)sb=s->home[p->b];
            if(p->op!=MI_STORE){d=get_register(s,machine_is_float(p->ty),-1,-1);if(d<0)return 0;}
        }else if(p->op!=MI_CONTEXT&&p->op!=MI_REF_CHECK){
            if(!operand(s,p->a,p->b,&a)||!operand(s,p->b,p->a,&b))return 0;
            if(p->op!=MI_STORE&&p->op!=MI_FIELD_STORE){
                d=get_register(s,machine_is_float(p->ty),p->a,p->b);if(d<0)return 0;
            }
        }
        if(!send(s,ME_STEP,i,-1,-1,a,b,d,sa,sb))return 0;
        if(d>=0){owners(s,i)[d]=i;s->location[i]=d;}
    }
    s->at=r->count;
    if(!expire(s))return 0;
    if(!r->statements){
        if(!operand(s,r->result,-1,&a)||!send(s,ME_RESULT,r->result,a,-1,-1,-1,-1,-1,-1))return 0;
    }
    *peak=s->peak;return 1;
}

typedef struct {MachinePlan *plan;size_t *budget;int capacity;} Recorder;
static int record(void *context,const MachineEvent *event)
{
    Recorder *r=context;MachinePlan *p=r->plan;
    if(!*r->budget)return 0;
    --*r->budget;
    if(p->count==r->capacity){
        int cap=r->capacity?r->capacity*2:64;
        MachineEvent *events=realloc(p->events,(size_t)cap*sizeof(*events));
        if(!events)fatal("数値退避計画を確保できません");
        p->events=events;r->capacity=cap;
    }
    p->events[p->count++]=*event;return 1;
}
static int register_ok(int profile,Type ty,int reg)
{return reg>=0&&reg<(machine_is_float(ty)?(profile?5:6):7);}
int machine_plan_valid(const MachineRegion *r,const MachinePlan *plan)
{
    int gp[7],fp[6],slots[512],last[512],i,j,next=0,result=0,peak=0;
    const MachineEvent *e;const MachineInst *p;int *o,value;
    if(!machine_region_valid(r)||!plan||!plan->events||plan->count<1||plan->profile<0||plan->profile>1||plan->spill_count<0||plan->spill_count>512)return 0;
    for(i=0;i<7;i++)gp[i]=-1;
    for(i=0;i<6;i++)fp[i]=-1;
    for(i=0;i<512;i++)slots[i]=last[i]=-1;
    for(i=0;i<r->count;i++){
        if(r->inst[i].a>=0)last[r->inst[i].a]=i;
        if(r->inst[i].b>=0)last[r->inst[i].b]=i;
    }
    if(!r->statements)last[r->result]=r->count;
    for(i=0;i<r->count;i++)if(last[i]!=r->inst[i].last)return 0;
    for(i=0;i<plan->count;i++){
        e=&plan->events[i];value=e->value;
        if(e->at!=next||value<0||value>=r->count)return 0;
        p=&r->inst[value];o=machine_is_float(p->ty)?fp:gp;
        if(e->kind==ME_EXPIRE){
            if(last[value]>=next||value>=next)return 0;
            if(e->reg>=0){if(!register_ok(plan->profile,p->ty,e->reg)||o[e->reg]!=value)return 0;o[e->reg]=-1;}
            if(e->slot>=0){if(e->slot>=plan->spill_count||slots[e->slot]!=value)return 0;slots[e->slot]=-1;}
        }else if(e->kind==ME_SPILL||e->kind==ME_FORGET||e->kind==ME_RELOAD){
            if(value>=next||last[value]<next||!register_ok(plan->profile,p->ty,e->reg)||e->slot<0||e->slot>=plan->spill_count)return 0;
            if(e->slot+1>peak)peak=e->slot+1;
            if(e->kind==ME_RELOAD){if(slots[e->slot]!=value||o[e->reg]>=0)return 0;o[e->reg]=value;}
            else{
                if(o[e->reg]!=value)return 0;
                if(e->kind==ME_SPILL){if(slots[e->slot]>=0&&slots[e->slot]!=value)return 0;slots[e->slot]=value;}
                else if(slots[e->slot]!=value)return 0;
                o[e->reg]=-1;
            }
        }else if(e->kind==ME_STEP){
            if(value!=next||next>=r->count)return 0;
            if(machine_is_barrier(p)){
                for(j=0;j<7;j++)if(gp[j]>=0)return 0;
                for(j=0;j<6;j++)if(fp[j]>=0)return 0;
                if(p->a>=0&&(e->sa<0||e->sa>=plan->spill_count||slots[e->sa]!=p->a))return 0;
                if(p->b>=0&&(e->sb<0||e->sb>=plan->spill_count||slots[e->sb]!=p->b))return 0;
            }else{
                int ids[2]={p->a,p->b},regs[2]={e->a,e->b};
                for(j=0;j<2;j++)if(ids[j]>=0){
                    Type ty=r->inst[ids[j]].ty;int *input=machine_is_float(ty)?fp:gp;
                    if(!register_ok(plan->profile,ty,regs[j])||input[regs[j]]!=ids[j])return 0;
                }
            }
            if(p->op==MI_CONTEXT||p->op==MI_REF_CHECK||p->op==MI_STORE||p->op==MI_FIELD_STORE){if(e->d!=-1)return 0;}
            else{if(!register_ok(plan->profile,p->ty,e->d)||o[e->d]>=0)return 0;o[e->d]=value;}
            next++;
        }else if(e->kind==ME_RESULT){
            if(r->statements||next!=r->count||value!=r->result||result++||!register_ok(plan->profile,p->ty,e->reg)||o[e->reg]!=value)return 0;
        }else return 0;
    }
    return next==r->count&&result==!r->statements&&peak==plan->spill_count;
}
void machine_plan_function(Function *fn)
{
    MachineRegion *r;MachinePlan *p;
    size_t budget=QYNO_MACHINE_PLAN_BUDGET;
    int profile,peak=0,old_slots=fn->opt_machine_slots;
    for(r=fn->machine_regions;r;r=r->next)for(profile=0;profile<2;profile++){
        Recorder recorder;
        p=calloc(1,sizeof(*p));if(!p)fatal("数値退避計画を確保できません");
        r->plans[profile]=p;p->profile=profile;
        recorder=(Recorder){p,&budget,0};
        if(!machine_allocate(r,profile,0,record,&recorder,&p->spill_count))goto fixed;
        if(!machine_plan_valid(r,p))fatal("内部エラー: 数値退避計画が不整合です");
        if(p->spill_count>peak)peak=p->spill_count;
    }
    fn->opt_machine_slots=peak;
    goto frame;
fixed:
    for(r=fn->machine_regions;r;r=r->next)for(profile=0;profile<2;profile++)if((p=r->plans[profile])!=NULL){free(p->events);free(p);r->plans[profile]=NULL;}
    fn->opt_machine_fixed=1;fn->opt_machine_slots=old_slots;
frame:
    if(fn->stack_size<0||fn->stack_size>INT_MAX-15||fn->opt_machine_slots<0||
       fn->opt_machine_slots>(INT_MAX-15-fn->stack_size)/8)fatal("関数のスタック領域が大きすぎます");
    fn->opt_machine_base=fn->stack_size;
    fn->stack_size=(fn->stack_size+fn->opt_machine_slots*8+15)/16*16;
}
