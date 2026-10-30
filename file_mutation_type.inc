/* Path mutations reject ambiguous directory targets before any OS effects. */
static void check_file_mutation_builtin(Node *node)
{
    Node *arg;int count=node->val==BI_FILE_REMOVE?1:2;
    if(node->nargs!=count)error_tok(node->tok,"io.file.%s expects %d arguments",builtin_name((Builtin)node->val),count);
    for(arg=node->lhs;arg;arg=arg->next){
        if(arg->kind==ND_REFARG)error_tok(arg->tok,"File path operations do not accept ref arguments");
        check_expr(arg);if(!type_is_text(arg->ty))error_tok(arg->tok,"File paths require str or char");
        if(is_const_expr(arg)){
            Value v=eval_const(arg);WideRequest r={0};
            r.op=W_PATH;r.aux=9;r.from=checking_target==TARGET_WINDOWS;r.input=v.s;r.length=v.len;r.base=(long long)v.len+4;
            r.buffer=malloc((size_t)r.base);if(!r.buffer)fatal("Cannot validate a file path");
            wide_execute(&r);
            if(r.status||!v.len||v.s[v.len-1]=='/'||(r.from&&v.s[v.len-1]=='\\'))
                error_tok(arg->tok,"Invalid file path: empty, NUL, trailing separator, or unsupported format");
            r.aux=1;r.status=0;wide_execute(&r);
            if(!r.out.lo||(r.out.lo==1&&r.buffer[0]=='.')||(r.out.lo==2&&r.buffer[0]=='.'&&r.buffer[1]=='.'))
                error_tok(arg->tok,"A file path cannot name a root, . or ..");
            free(r.buffer);
        }
    }
    node->ty=TY_VOID;
}
