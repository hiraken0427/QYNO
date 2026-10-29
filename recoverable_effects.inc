/* Error effects and lexical control checks after interface/generic specialization. */
static void io_controls(Node *list,int caught,int final_loop,int loops)
{
    Node *n,*arm;
    for(n=list;n;n=n->next){
        if(n->kind==ND_TRY){
            io_controls(n->lhs,caught,final_loop,loops);io_controls(n->then,caught,final_loop,loops);
            for(arm=n->els;arm;arm=arm->next)io_controls(arm->then,1,final_loop,loops);
            if(n->rhs)io_controls(n->rhs,0,loops+1,loops);
        }else{
            if(n->kind==ND_THROW&&!caught)error_tok(n->tok,"throw; は case の本体でだけ使えます");
            if(final_loop&&(n->kind==ND_RETURN||((n->kind==ND_BREAK||n->kind==ND_CONTINUE)&&loops<final_loop)))
                error_tok(n->tok,"finally の外へ return・break・continue できません");
            if(n->kind==ND_IF){io_controls(n->then,caught,final_loop,loops);io_controls(n->els,caught,final_loop,loops);}
            if(n->kind==ND_FOR||n->kind==ND_WHILE)io_controls(n->then,caught,final_loop,loops+1);
        }
    }
}
static int io_effect_nodes(Node *n,int caught,int mark)
{
    int effect=0;InputField *f;
    for(;n;n=n->next){
        if(n->kind==ND_TRY){
            io_has_recovery=1;
            effect|=io_effect_nodes(n->lhs,caught||n->els!=NULL,mark);
            effect|=io_effect_nodes(n->then,caught||n->els!=NULL,mark);
            effect|=io_effect_nodes(n->els,caught,mark);effect|=io_effect_nodes(n->rhs,caught,mark);continue;
        }
        if(n->kind==ND_BUILTIN&&builtin_may_raise((int)n->val))effect=1;
        if(n->kind==ND_THROW)effect=1;
        if(type_is_array(n->ty)&&type_is_model(array_base(n->ty))){Function *fn=type_model(array_base(n->ty))->default_builder;if(fn){effect|=fn->io_effect;if(mark&&caught)fn->io_caught=1;}}
        if(n->kind==ND_CALL||n->kind==ND_CONSTRUCT||n->kind==ND_MODEL_NEW){
            Function *fn=n->kind==ND_MODEL_NEW?type_model(n->ty)->default_builder:n->func;
            if(fn){effect|=fn->io_effect;if(mark&&caught)fn->io_caught=1;}
        }
        effect|=io_effect_nodes(n->lhs,caught,mark);effect|=io_effect_nodes(n->rhs,caught,mark);
        effect|=io_effect_nodes(n->cond,caught,mark);effect|=io_effect_nodes(n->then,caught,mark);effect|=io_effect_nodes(n->els,caught,mark);
        effect|=io_effect_nodes(n->init,caught,mark);effect|=io_effect_nodes(n->step,caught,mark);effect|=io_effect_nodes(n->end,caught,mark);
        for(f=n->fields;f;f=f->next)effect|=io_effect_nodes(f->var,caught,mark);
    }
    return effect;
}
static void check_io_effects(Ast *ast)
{
    Function *fn;int changed;
    io_has_recovery=0;
    for(fn=ast->funcs;fn;fn=fn->next)if(!fn->is_template)io_controls(fn->body,0,0,0);
    do{changed=0;for(fn=ast->funcs;fn;fn=fn->next)if(!fn->is_template){int e=io_effect_nodes(fn->body,0,0);if(e&&!fn->io_effect){fn->io_effect=1;changed=1;}}}while(changed);
    do{int before=0,after=0;for(fn=ast->funcs;fn;fn=fn->next)before+=fn->io_caught;
        for(fn=ast->funcs;fn;fn=fn->next)if(!fn->is_template)io_effect_nodes(fn->body,fn->io_caught,1);
        for(fn=ast->funcs;fn;fn=fn->next)after+=fn->io_caught;
        changed=before!=after;
    }while(changed);
}
