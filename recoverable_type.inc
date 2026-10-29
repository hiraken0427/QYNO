/* Checked try syntax; control-flow and resource effects are analyzed after typing. */
static Type io_standard_type(const char *name)
{
    return standard_type(cur_ast,"io",name);
}
static void check_try(Node *n)
{
    Node *arm,*p; unsigned seen=0;int fallback=0;
    n->var->ty=io_standard_type("Error");
    if(n->lhs)check_stmt(n->lhs);
    check_stmts(n->then);
    for(arm=n->els;arm;arm=arm->next) {
        if(fallback)error_tok(arm->tok,"case() は最後の case にしてください");
        if(!arm->lhs)fallback=1;
        for(p=arm->lhs;p;p=p->next){
            Value v;Node *next=p->next,*c;p->next=NULL;
            check_expected(p,io_standard_type("ErrorCode"));
            c=coerce(p,io_standard_type("ErrorCode"),p->tok,"case");
            if(!is_const_expr(c))error_tok(p->tok,"case は io.ErrorCode の定数にしてください");
            v=eval_const(c);
            if(v.i==0)error_tok(p->tok,"no_error は捕捉できません");
            if(seen&(1u<<v.i))error_tok(p->tok,"case のエラーコードが重複しています");
            seen|=1u<<v.i;p->kind=ND_NUM;p->ty=io_standard_type("ErrorCode");p->val=v.i;p->lhs=p->rhs=NULL;p->next=next;
        }
        check_stmts(arm->then);
    }
    check_stmts(n->rhs);
}
static void check_try_open(Node *n)
{
    Node *p=n->lhs,*dest=p?p->next:NULL,*error=dest?dest->next:NULL,*mode=error?error->next:NULL;
    Var target={0};
    if(n->nargs!=3&&n->nargs!=4)error_tok(n->tok,"try_open の引数は path, ref destination, ref error, mode省略可です");
    check_expr(p);if(!type_is_text(p->ty))error_tok(p->tok,"ファイルのパスは str・char にしてください");
    target.ty=TY_FILE;if(dest->kind!=ND_REFARG)error_tok(dest->tok,"try_open の出力先は ref file にしてください");check_ref_arg(dest,&target,2);
    target.ty=io_standard_type("Error");if(error->kind!=ND_REFARG)error_tok(error->tok,"try_open の error は ref io.Error にしてください");check_ref_arg(error,&target,3);
    if(!mode){mode=calloc(1,sizeof(*mode));if(!mode)fatal("モードを確保できません");mode->kind=ND_STRLIT;mode->tok=n->tok;mode->str="r";mode->str_len=1;error->next=mode;n->nargs=4;}
    check_expr(mode);if(!type_is_text(mode->ty))error_tok(mode->tok,"ファイルのモードは str・char にしてください");
    if(is_const_expr(mode)){Value v=eval_const(mode);if((v.len!=1&&(v.len!=2||v.s[1]!='b'))||(v.s[0]!='r'&&v.s[0]!='w'&&v.s[0]!='a'))error_tok(mode->tok,"ファイルのモードは r・w・a・rb・wb・ab のいずれかです");}
    n->ty=TY_BOOL;
}
