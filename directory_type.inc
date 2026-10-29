/* Effectful paths are never evaluated against the compiler host's cwd. */
static void check_directory_builtin(Node *node)
{
    int expected=node->val==BI_DIR_CURRENT?0:1;
    Node *arg=node->lhs;
    if(node->nargs!=expected)error_tok(node->tok,"%s.%s の引数は %d 個です",builtin_namespace((Builtin)node->val),builtin_name((Builtin)node->val),expected);
    if(arg){
        if(arg->kind==ND_REFARG)error_tok(arg->tok,"ディレクトリのパスに ref は付けません");
        check_expr(arg);
        if(!type_is_text(arg->ty))error_tok(arg->tok,"ディレクトリのパスは str・char にしてください");
        if(is_const_expr(arg)){
            Value v=eval_const(arg);WideRequest request={0};
            request.op=W_PATH;request.aux=9;request.from=checking_target==TARGET_WINDOWS;
            request.input=v.s;request.length=v.len;request.base=(long long)v.len+4;
            request.buffer=malloc((size_t)request.base);if(!request.buffer)fatal("パスの検査領域を確保できません");
            wide_execute(&request);free(request.buffer);
            if(request.status||(!v.len&&node->val==BI_DIR_CHANGE))error_tok(arg->tok,"ディレクトリのパスが不正です（空・NUL・パス形式を確認してください）");
        }
    }
    node->ty=node->val==BI_DIR_CHANGE?TY_VOID:TY_STR;
}
