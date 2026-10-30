/* Directory mutation and enumeration use the same target path policy. */
static Node *directory_default(Node *node,long long value)
{
    Node *arg=calloc(1,sizeof(*arg));if(!arg)fatal("Cannot allocate a directory default");
    arg->kind=ND_NUM;arg->ty=TY_I64;arg->val=value;arg->tok=node->tok;return arg;
}
static void check_directory_management_builtin(Node *node)
{
    Node **slot=&node->lhs,*arg;int i=0,b=(int)node->val,max=b==BI_DIR_LIST?3:1;
    if(node->nargs<1||node->nargs>max)error_tok(node->tok,"io.dir.%s expects 1 to %d arguments",builtin_name((Builtin)b),max);
    while(node->nargs<max){
        Node **tail=&node->lhs;while(*tail)tail=&(*tail)->next;
        *tail=directory_default(node,node->nargs==1?1048576:67108864);node->nargs++;
    }
    for(;*slot;slot=&(*slot)->next,i++){
        arg=*slot;
        if(arg->kind==ND_REFARG)error_tok(arg->tok,"Directory APIs do not accept ref arguments");
        if(!i){
            check_expr(arg);if(!type_is_text(arg->ty))error_tok(arg->tok,"Directory paths require str or char");
            if(is_const_expr(arg)){
                Value v=eval_const(arg);WideRequest r={0};
                r.op=W_PATH;r.aux=9;r.from=checking_target==TARGET_WINDOWS;r.input=v.s;r.length=v.len;r.base=(long long)v.len+4;
                r.buffer=malloc((size_t)r.base);if(!r.buffer)fatal("Cannot validate a directory path");
                wide_execute(&r);
                if(r.status||!v.len)error_tok(arg->tok,"Invalid directory path: empty, NUL, or unsupported path format");
                if(b==BI_DIR_REMOVE){
                    r.aux=1;r.status=0;wide_execute(&r);
                    if(!r.out.lo||(r.out.lo==1&&r.buffer[0]=='.')||(r.out.lo==2&&r.buffer[0]=='.'&&r.buffer[1]=='.'))
                        error_tok(arg->tok,"Cannot remove a root, . or .. directory path");
                }
                free(r.buffer);
            }
        }else{
            long long cap=i==1?(INT64_MAX-32)/8:INT64_MAX-32;
            check_expected(arg,TY_I64);if(!type_is_integer(arg->ty))error_tok(arg->tok,"Directory limits must be nonnegative int_64 values");
            arg=coerce(arg,TY_I64,arg->tok,"directory limit");arg->next=(*slot)->next;*slot=arg;
            if(is_const_expr(arg)&&(eval_const(arg).i<0||eval_const(arg).i>cap))error_tok(arg->tok,"Directory limit is out of range");
        }
    }
    node->ty=b==BI_DIR_EXISTS?TY_BOOL:b==BI_DIR_LIST?array_type(TY_STR):TY_VOID;
}
/* Effectful paths are never evaluated against the compiler host's cwd. */
static void check_directory_builtin(Node *node)
{
    int expected=node->val==BI_DIR_CURRENT?0:1;
    if(node->val>=BI_DIR_EXISTS){check_directory_management_builtin(node);return;}
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
