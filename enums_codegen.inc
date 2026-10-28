static void gen_enum_cast(Node *node)
{
    Type from=node->lhs->ty,to=node->ty;EnumDef *e;
    int bad=new_label("enum.cast.invalid"),done=new_label("enum.cast.done");
    gen_expr(node->lhs);
    if(type_is_enum(from)){
        if(to==TY_STR)x64_call(type_enum(from)->name_label);
        else if(from!=to)gen_integer_checked(TY_I32,to,node->tok);
        return;
    }
    e=type_enum(to);
    if(type_is_text(from)){
        x64_load(RDX,RAX,0);x64_lea(RSI,RAX,8);x64_add(RDX,RSI);
        x64_call(skip_ws_label);x64_call(e->parse_label);x64_test(RCX,RCX);x64_jcc(CC_NZ,bad);
        push();x64_call(skip_ws_label);x64_cmp(RSI,RDX);x64_jcc(CC_NZ,bad);pop(RAX);
    }else{
        gen_integer_checked(from,TY_I32,node->tok);x64_call(e->check_label);x64_test(RCX,RCX);x64_jcc(CC_NZ,bad);
    }
    x64_jmp(done);x64_bind(bad);gen_fail(node->tok,"enum の名前または数値が未定義です",EX_SOFTWARE);x64_bind(done);
}
static void gen_enum_runtime(void)
{
    EnumDef *e;EnumMember *m;
    for(e=emitting_enums;e;e=e->next){
        int valid=new_label("enum.valid"),parsed=new_label("enum.parsed");
        x64_bind(e->check_label);
        for(m=e->members;m;m=m->next){rt_cmp(RAX,m->value);x64_jcc(CC_Z,valid);}
        x64_mov_imm(RCX,2);x64_ret();x64_bind(valid);x64_xor(RCX,RCX);x64_ret();
        x64_bind(e->name_label);
        for(m=e->members;m;m=m->next){
            int next=new_label("enum.name.next");rt_cmp(RAX,m->value);x64_jcc(CC_NZ,next);
            x64_lea_data(RAX,prog_add_str(prog,m->name,strlen(m->name)));x64_ret();x64_bind(next);
        }
        gen_fail(e->tok,"enum の値が未定義です",EX_SOFTWARE);
        rt_enter(e->parse_label,2);rt_save(1,RSI);
        for(m=e->members;m;m=m->next){
            int next=new_label("enum.parse.next");rt_load(RSI,1);match_word(m->name,next);
            x64_add_imm(RSI,(int)strlen(m->name));x64_call(word_follows_label);x64_test(RAX,RAX);x64_jcc(CC_NZ,next);
            x64_mov_imm(RAX,m->value);x64_xor(RCX,RCX);rt_leave();x64_bind(next);
        }
        rt_load(RSI,1);x64_xor(RDI,RDI);gen_parse_integer(TY_I32);
        x64_test(RCX,RCX);x64_jcc(CC_NZ,parsed);x64_call(e->check_label);
        x64_bind(parsed);rt_leave();
    }
}
