/* Checked try syntax; control-flow and resource effects are analyzed after typing. */
static Type io_standard_type(const char *name)
{
    return standard_type(cur_ast,"io",name);
}
static void check_try(Node *n)
{
    Node *arm,*p,*earlier,*previous;int fallback=0,common=0;
    Type io_code=io_standard_type("ErrorCode");
    /* Resolve patterns first, so every binding has its final nominal type
     * before its handler is checked. The body cannot see these bindings. */
    for(arm=n->els;arm;arm=arm->next){
        int domain=-1;
        if(fallback)error_tok(arm->tok,QMSG_0111);
        if(!arm->lhs)fallback=1;
        for(p=arm->lhs;p;p=p->next){
            Value v;Node *next=p->next;int d;long long packed;
            p->next=NULL;
            if(enum_unbound_name(p))check_expected(p,io_code);else check_expr(p);
            d=error_code_domain(p->ty);
            if(type_is_integer(p->ty)&&is_const_expr(p)){
                /* Preserve the existing numeric io.ErrorCode conversion.
                 * Numeric spellings never select a different domain. */
                Node *converted=coerce(p,io_code,p->tok,"case");
                v=eval_const(converted);d=0;
            }else{
                if(d<0&&!error_common_code(p->ty))error_tok(p->tok,QMSG_0855);
                if(!is_const_expr(p))error_tok(p->tok,QMSG_0855);
                v=eval_const(p);
            }
            packed=error_common_code(p->ty)?v.i:((long long)d<<ERROR_CODE_SHIFT)|v.i;
            d=(int)(packed>>ERROR_CODE_SHIFT);
            if(!(packed&ERROR_CODE_MASK))error_tok(p->tok,QMSG_0304);
            for(earlier=n->els;earlier;earlier=earlier->next){
                for(previous=earlier->lhs;previous&&previous!=p;previous=previous->next)
                    if(previous->kind==ND_NUM&&previous->val==packed)error_tok(p->tok,QMSG_0107);
                if(earlier==arm)break;
            }
            if(domain==-1)domain=d;else if(domain!=d)domain=2;
            if(d)common=1;
            p->cmp_ty=d?standard_type(cur_ast,"task","ErrorCode"):io_code;
            p->kind=ND_NUM;p->ty=standard_type(cur_ast,"lib","ErrorCode");
            p->val=packed;p->lhs=p->rhs=NULL;p->next=next;
        }
        arm->val=domain;
        if(arm->var)arm->var->ty=domain==0?io_standard_type("Error"):
            domain==1?standard_type(cur_ast,"task","Error"):standard_type(cur_ast,"lib","Error");
    }
    n->var->ty=common?standard_type(cur_ast,"lib","Error"):io_standard_type("Error");
    if(n->lhs)check_stmt(n->lhs);
    check_stmts(n->then);
    for(arm=n->els;arm;arm=arm->next)check_stmts(arm->then);
    check_stmts(n->rhs);
}
static void check_try_open(Node *n)
{
    Node *p=n->lhs,*dest=p?p->next:NULL,*error=dest?dest->next:NULL,*mode=error?error->next:NULL;
    Var target={0};
    if(n->nargs!=3&&n->nargs!=4)error_tok(n->tok,QMSG_0354);
    check_expr(p);if(!type_is_text(p->ty))error_tok(p->tok,QMSG_0410);
    target.ty=TY_FILE;if(dest->kind!=ND_REFARG)error_tok(dest->tok,QMSG_0352);check_ref_arg(dest,&target,2);
    target.ty=io_standard_type("Error");if(error->kind!=ND_REFARG)error_tok(error->tok,QMSG_0351);check_ref_arg(error,&target,3);
    if(!mode){mode=calloc(1,sizeof(*mode));if(!mode)fatal(QMSG_0469);mode->kind=ND_STRLIT;mode->tok=n->tok;mode->str="r";mode->str_len=1;error->next=mode;n->nargs=4;}
    check_expr(mode);if(!type_is_text(mode->ty))error_tok(mode->tok,QMSG_0412);
    if(is_const_expr(mode)){Value v=eval_const(mode);if((v.len!=1&&(v.len!=2||v.s[1]!='b'))||(v.s[0]!='r'&&v.s[0]!='w'&&v.s[0]!='a'))error_tok(mode->tok,QMSG_0411);}
    n->ty=TY_BOOL;
}
