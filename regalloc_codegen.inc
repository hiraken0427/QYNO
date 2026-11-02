/* Linear scan over scalar SSA regions. Every opaque edge spills its live
 * set before the legacy evaluator runs; that evaluator owns all GC roots. */
typedef struct {
    MachineRegion *region;
    int location[512],written[512];
    int gp[7],fp[6],at;
} MachineState;
static const Reg machine_gprs[7]={RAX,RCX,RDX,RSI,RDI,R8,R9};
static int machine_float(Type ty){return ty==TY_FLOAT||ty==TY_DOUBLE;}
static int machine_home(MachineState *s,int value)
{return -(s->region->owner->opt_machine_base+(value+1)*8);}
static void machine_spill(MachineState *s,int value)
{
    MachineInst *p=&s->region->inst[value];
    int reg=s->location[value];
    if(reg<0)return;
    if(!s->written[value]){
        if(machine_float(p->ty))x64_xmm_store(RBP,machine_home(s,value),reg,p->ty);
        else x64_store(RBP,machine_home(s,value),machine_gprs[reg]);
        s->written[value]=1;
    }
    (machine_float(p->ty)?s->fp:s->gp)[reg]=-1;s->location[value]=-1;
}
static void machine_flush(MachineState *s)
{
    int i;
    for(i=0;i<s->at;i++)if(s->location[i]>=0){
        if(s->region->inst[i].last>=s->at)machine_spill(s,i);
        else{(machine_float(s->region->inst[i].ty)?s->fp:s->gp)[s->location[i]]=-1;s->location[i]=-1;}
    }
}
static int machine_register(MachineState *s,int fp,int avoid_a,int avoid_b)
{
    int *owners=fp?s->fp:s->gp;
    int count=fp?(machine_carry?5:6):7,i,victim=-1;
    for(i=0;i<count;i++){
        int value=owners[i];
        if(value>=0&&value!=avoid_a&&value!=avoid_b&&s->region->inst[value].last<s->at){s->location[value]=-1;owners[i]=-1;}
        if(owners[i]<0)return i;
        if(value!=avoid_a&&value!=avoid_b&&(victim<0||s->region->inst[value].last>s->region->inst[owners[victim]].last))victim=i;
    }
    if(victim<0)fatal("内部エラー: 数値レジスタを割り当てられません");
    machine_spill(s,owners[victim]);return victim;
}
static int machine_operand(MachineState *s,int value,int other)
{
    int reg;
    MachineInst *p;
    if(value<0)return -1;
    if(s->location[value]>=0)return s->location[value];
    if(!s->written[value])fatal("内部エラー: 退避されていない値です");
    p=&s->region->inst[value];reg=machine_register(s,machine_float(p->ty),value,other);
    if(machine_float(p->ty))x64_xmm_load(reg,RBP,machine_home(s,value),p->ty);
    else x64_load(machine_gprs[reg],RBP,machine_home(s,value));
    (machine_float(p->ty)?s->fp:s->gp)[reg]=value;s->location[value]=reg;return reg;
}
static void machine_root(Var *v,Token *tok,Reg reg)
{
    if(v->is_ref){gen_ref_place(v,tok,reg);x64_load(reg,reg,0);}
    else x64_load(reg,RBP,-v->offset);
}
static void machine_load(Type ty,int reg,Reg base,int offset)
{
    if(machine_float(ty))x64_xmm_load(reg,base,offset,ty);
    else x64_load(machine_gprs[reg],base,offset);
}
static void machine_store(Type ty,int reg,Reg base,int offset)
{
    if(ty==TY_DOUBLE)x64_xmm_store(base,offset,reg,ty);
    else if(ty==TY_FLOAT){x64_movd_from_xmm(R10,reg);x64_store(base,offset,R10);}
    else x64_store(base,offset,machine_gprs[reg]);
}
static void machine_normalize(Type ty,Reg reg)
{
    if(type_is_integer(ty)&&integer_width(ty)<64)x64_extend(reg,integer_width(ty),integer_unsigned(ty));
}
static void machine_result_rax(Type ty,int reg)
{
    if(ty==TY_DOUBLE)x64_movq_from_xmm(RAX,reg);
    else if(ty==TY_FLOAT)x64_movd_from_xmm(RAX,reg);
    else if(machine_gprs[reg]!=RAX)x64_mov(RAX,machine_gprs[reg]);
}
static void machine_from_rax(Type ty,int reg)
{
    if(ty==TY_DOUBLE)x64_movq_to_xmm(reg,RAX);
    else if(ty==TY_FLOAT)x64_movd_to_xmm(reg,RAX);
    else if(machine_gprs[reg]!=RAX)x64_mov(machine_gprs[reg],RAX);
}
static int machine_integer_control(Node *n,int level)
{
    if(!n||level>32||!type_is_integer(n->ty)||type_is_wide(n->ty))return 0;
    switch(n->kind){
    case ND_NUM:return 1;
    case ND_VAR:return !n->var->is_const||machine_integer_control(n->var->const_val,level+1);
    case ND_LENGTH:return n->lhs->kind==ND_VAR&&type_is_array(n->lhs->ty)&&!n->lhs->var->is_const;
    case ND_CAST:return machine_integer_control(n->lhs,level+1);
    case ND_ADD:case ND_SUB:case ND_MUL:return machine_integer_control(n->lhs,level+1)&&machine_integer_control(n->rhs,level+1);
    default:return 0;
    }
}
static Var *machine_loop_carry(Node *loop)
{
    Node *body=loop->then;
    MachineRegion *r;
    Var *v;
    int i,stores=0;
    if(machine_disabled||!type_is_integer(loop->init->var->ty)||type_is_wide(loop->init->var->ty)||
        !body||body->next||body->kind!=ND_ASSIGN||!type_is_integer(loop->cond->lhs->cmp_ty)||
        !machine_integer_control(loop->cond->lhs->rhs,0)||!machine_integer_control(loop->for_amount,0))return NULL;
    v=body->var;r=body->opt_machine;
    if(!v||v->is_ref||v->opt_addressed||!machine_float(v->ty)||!r||r->owner!=emitting_function||!r->statements)return NULL;
    for(i=0;i<r->count;i++){
        MachineInst *p=&r->inst[i];
        if(p->op==MI_OPAQUE||p->op==MI_FIELD_STORE)return NULL;
        if(p->op==MI_STORE){if(p->root!=v||++stores>1)return NULL;}
    }
    return stores==1?v:NULL;
}
static int gen_machine(Node *node,int statements)
{
    MachineRegion *r=node->opt_machine;
    MachineState state,*s=&state;
    MachineInst *p;
    int i,a,b,d,reg,base_depth=depth;
    Reg dst,left,right;
    Cond cc;
    if(machine_disabled||!r||r->owner!=emitting_function||r->statements!=statements)return 0;
    if(!machine_region_valid(r))fatal("内部エラー: 不正な数値命令です");
    x64_comment("scalar region: %d operations",r->count);
    memset(s,0,sizeof(*s));s->region=r;
    for(i=0;i<512;i++)s->location[i]=-1;
    for(i=0;i<7;i++)s->gp[i]=-1;
    for(i=0;i<6;i++)s->fp[i]=-1;
    for(i=0;i<r->count;i++){
        s->at=i;p=&r->inst[i];
        if(p->op==MI_CONTEXT){collection_context(p->source->tok);continue;}
        if(p->op==MI_REF_CHECK){if(p->root->is_ref)gen_ref_place(p->root,p->source->lhs->tok,R10);continue;}
        if(p->op==MI_OPAQUE||(p->op==MI_STORE&&p->root->is_ref)||
            (p->op==MI_BINARY&&type_is_integer(p->ty)&&(p->source->kind==ND_DIV||p->source->kind==ND_MOD))){
            machine_flush(s);
            if(p->op==MI_OPAQUE){
                machine_disabled++;gen_expr(p->source);machine_disabled--;
            }else if(p->op==MI_STORE){
                x64_load(RAX,RBP,machine_home(s,p->a));gen_store_var(p->root,p->source->tok);continue;
            }else{
                x64_load(RAX,RBP,machine_home(s,p->a));x64_load(RDI,RBP,machine_home(s,p->b));
                gen_div_check(p->source);
                if(integer_unsigned(p->ty)){x64_xor(RDX,RDX);x64_div(RDI);}else{x64_cqo();x64_idiv(RDI);}
                if(p->source->kind==ND_MOD)x64_mov(RAX,RDX);
                gen_integer_normalize(p->ty);
            }
            d=machine_register(s,machine_float(p->ty),-1,-1);machine_from_rax(p->ty,d);
            (machine_float(p->ty)?s->fp:s->gp)[d]=i;s->location[i]=d;continue;
        }
        a=machine_operand(s,p->a,p->b);b=machine_operand(s,p->b,p->a);
        if(p->op==MI_STORE){
            if(p->root==machine_carry)x64_xmm_move(5,a);
            machine_store(r->inst[p->a].ty,a,RBP,-p->root->offset);continue;
        }
        if(p->op==MI_FIELD_STORE){
            /* Only a direct, unescaped value receiver and an effect-free
             * numeric RHS reach this instruction. Re-resolve after the RHS. */
            x64_load(R11,RBP,-p->root->offset);x64_load(R11,R11,24);
            machine_store(r->inst[p->a].ty,a,R11,(int)p->bits*8);continue;
        }
        d=machine_register(s,machine_float(p->ty),p->a,p->b);
        dst=machine_float(p->ty)?R10:machine_gprs[d];
        left=p->a>=0&&!machine_float(r->inst[p->a].ty)?machine_gprs[a]:R10;
        right=p->b>=0&&!machine_float(r->inst[p->b].ty)?machine_gprs[b]:R11;
        switch(p->op){
        case MI_CONSTANT:
            if(machine_float(p->ty)){x64_mov_imm(R10,integer_bits(p->bits));if(p->ty==TY_DOUBLE)x64_movq_to_xmm(d,R10);else x64_movd_to_xmm(d,R10);}
            else x64_mov_imm(dst,integer_bits(p->bits));
            break;
        case MI_VARIABLE:
            if(p->root==machine_carry)x64_xmm_move(d,5);
            else if(p->root->is_ref){gen_ref_place(p->root,p->source->tok,R10);machine_load(p->ty,d,R10,0);}
            else machine_load(p->ty,d,RBP,-p->root->offset);
            break;
        case MI_LENGTH:
            if(p->source->opt_cached&&p->source->opt_cached->owner==emitting_function)
                machine_load(p->ty,d,RBP,-p->source->opt_cached->slot);
            else{machine_root(p->root,p->source->lhs->tok,R10);machine_load(p->ty,d,R10,0);}
            break;
        case MI_FIELD:
            if(p->source->opt_cached&&p->source->opt_cached->owner==emitting_function){
                OptHoist *h=p->source->opt_cached;
                machine_load(p->ty,d,RBP,-h->slot);
            }else{machine_root(p->root,p->source->lhs->tok,R10);x64_load(R10,R10,24);machine_load(p->ty,d,R10,(int)p->bits*8);}
            break;
        case MI_ARRAY:
            machine_root(p->root,p->source->lhs->tok,R10);x64_load(R10,R10,24);
            x64_lea_index(R10,R10,left,3);machine_load(p->ty,d,R10,0);break;
        case MI_BINARY:
            if(machine_float(p->ty)){
                x64_xmm_move(d,a);
                if(p->ty==TY_DOUBLE)x64_sse_sd(sse_op(p->source->kind),d,b);else x64_sse_ss(sse_op(p->source->kind),d,b);
            }else{
                x64_mov(dst,left);
                switch(p->source->kind){
                case ND_ADD:x64_add(dst,right);break;case ND_SUB:x64_sub(dst,right);break;
                case ND_MUL:x64_imul(dst,right);break;case ND_BIT_AND:x64_and(dst,right);break;
                case ND_BIT_OR:x64_or(dst,right);break;default:x64_xor(dst,right);break;
                }
                machine_normalize(p->ty,dst);
            }
            break;
        case MI_CONVERT:
            if(p->ty==r->inst[p->a].ty){if(machine_float(p->ty))x64_xmm_move(d,a);else x64_mov(dst,left);}
            else if(p->ty==TY_DOUBLE&&r->inst[p->a].ty==TY_FLOAT)x64_cvtss2sd(d,a);
            else if(p->ty==TY_FLOAT&&r->inst[p->a].ty==TY_DOUBLE)x64_cvtsd2ss(d,a);
            else if(p->ty==TY_DOUBLE)x64_cvtsi2sd(d,left);
            else if(p->ty==TY_FLOAT)x64_cvtsi2ss(d,left);
            else x64_mov(dst,left);
            break;
        case MI_NEGATE:x64_mov(dst,left);x64_neg(dst);machine_normalize(p->ty,dst);break;
        case MI_NOT:x64_mov(dst,left);x64_xor_imm(dst,p->source->kind==ND_NOT?1:-1);machine_normalize(p->ty,dst);break;
        case MI_COMPARE:
            cc=int_cond((CmpKind)p->source->val);
            if(integer_unsigned(p->source->cmp_ty))cc=p->source->val==CMP_LT?CC_B:p->source->val==CMP_LE?CC_BE:p->source->val==CMP_GT?CC_A:p->source->val==CMP_GE?CC_AE:cc;
            x64_cmp(left,right);x64_setcc(cc,dst);x64_movzx8(dst,dst);break;
        default:fatal("内部エラー: 数値命令が未対応です");
        }
        (machine_float(p->ty)?s->fp:s->gp)[d]=i;s->location[i]=d;
    }
    if(!statements){s->at=r->count;reg=machine_operand(s,r->result,-1);machine_result_rax(r->inst[r->result].ty,reg);}
    if(depth!=base_depth)fatal("内部エラー: 数値命令のスタックが不整合です");
    x64_comment("end scalar region");
    return 1;
}
