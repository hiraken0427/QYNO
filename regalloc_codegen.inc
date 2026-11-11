/* Emit a verified allocation plan. Opaque evaluation retains all of its
 * ordinary GC roots, and cannot reenter this function's scalar regions. */
static const Reg machine_gprs[7]={RAX,RCX,RDX,RSI,RDI,R8,R9};
static int machine_float(Type ty){return machine_is_float(ty);}
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

typedef struct { MachineRegion *region; } MachineEmitter;
static int machine_slot(MachineRegion *r,int slot)
{
    if(slot<0||slot>=r->owner->opt_machine_slots)fatal("内部エラー: 数値退避先がフレーム外です");
    return -(r->owner->opt_machine_base+(slot+1)*8);
}
static int machine_emit(void *context,const MachineEvent *event)
{
    MachineRegion *r=((MachineEmitter *)context)->region;
    const MachineEvent *e=event;
    MachineInst *p=&r->inst[e->value];
    int a=e->a,b=e->b,d=e->d;
    Reg dst,left,right;
    Cond cc;
    if(e->kind==ME_SPILL){machine_store(p->ty,e->reg,RBP,machine_slot(r,e->slot));return 1;}
    if(e->kind==ME_RELOAD){machine_load(p->ty,e->reg,RBP,machine_slot(r,e->slot));return 1;}
    if(e->kind==ME_FORGET||e->kind==ME_EXPIRE)return 1;
    if(e->kind==ME_RESULT){machine_result_rax(p->ty,e->reg);return 1;}
    if(e->kind!=ME_STEP)fatal("内部エラー: 数値退避イベントが不正です");
    if(p->op==MI_CONTEXT){collection_context(p->source->tok);return 1;}
    if(p->op==MI_REF_CHECK){if(p->root->is_ref)gen_ref_place(p->root,p->source->lhs->tok,R10);return 1;}
    if(machine_is_barrier(p)){
        if(p->op==MI_OPAQUE){machine_disabled++;gen_expr(p->source);machine_disabled--;}
        else if(p->op==MI_STORE){
            x64_load(RAX,RBP,machine_slot(r,e->sa));gen_store_var(p->root,p->source->tok);return 1;
        }else{
            x64_load(RAX,RBP,machine_slot(r,e->sa));x64_load(RDI,RBP,machine_slot(r,e->sb));
            gen_div_check(p->source);
            if(integer_unsigned(p->ty)){x64_xor(RDX,RDX);x64_div(RDI);}else{x64_cqo();x64_idiv(RDI);}
            if(p->source->kind==ND_MOD)x64_mov(RAX,RDX);
            gen_integer_normalize(p->ty);
        }
        machine_from_rax(p->ty,d);return 1;
    }
    if(p->op==MI_STORE){
        if(p->root==machine_carry)x64_xmm_move(5,a);
        machine_store(r->inst[p->a].ty,a,RBP,-p->root->offset);return 1;
    }
    if(p->op==MI_FIELD_STORE){
        x64_load(R11,RBP,-p->root->offset);x64_load(R11,R11,24);
        machine_store(r->inst[p->a].ty,a,R11,(int)p->bits*8);return 1;
    }
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

    return 1;
}
static int gen_machine(Node *node,int statements)
{
    MachineRegion *r=node->opt_machine;
    MachinePlan *plan;
    MachineEmitter emitter;
    int i,peak,base_depth=depth,profile=machine_carry!=NULL;
    if(machine_disabled||!r||r->owner!=emitting_function||r->statements!=statements)return 0;
    if(!machine_region_valid(r))fatal("内部エラー: 不正な数値命令です");
    emitter.region=r;
    x64_comment("scalar region: %d operations",r->count);
    if(r->owner->opt_machine_fixed){
        x64_comment("scalar spill slots: %d (fixed)",r->owner->opt_machine_slots);
        if(!machine_allocate(r,profile,1,machine_emit,&emitter,&peak)||peak>r->owner->opt_machine_slots)
            fatal("内部エラー: 固定数値退避領域が不整合です");
    }else{
        plan=r->plans[profile];
        if(!plan||plan->profile!=profile||!machine_plan_valid(r,plan)||plan->spill_count>r->owner->opt_machine_slots)
            fatal("内部エラー: 数値退避計画が不整合です");
        x64_comment("scalar spill slots: %d",plan->spill_count);
        for(i=0;i<plan->count;i++)machine_emit(&emitter,&plan->events[i]);
    }
    if(depth!=base_depth)fatal("内部エラー: 数値命令のスタックが不整合です");
    x64_comment("end scalar region");return 1;
}
