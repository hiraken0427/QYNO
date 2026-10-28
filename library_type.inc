static int library_size_type(Type ty,Type *seen,int depth)
{
    ModelField *f;int i;
    if(value_bit_width(ty)>=0||type_is_text(ty))return 1;
    for(i=0;i<depth;i++)if(seen[i]==ty)return 1;
    if(depth==MAX_PLACE_DEPTH)return 0;
    seen[depth++]=ty;
    if(type_is_array(ty))return library_size_type(array_element(ty),seen,depth);
    if(type_is_model(ty)){
        for(f=type_model(ty)->fields;f;f=f->next)if(!library_size_type(f->ty,seen,depth))return 0;
        return 1;
    }
    return 0;
}
static void check_library_builtin(Node *node)
{
    Builtin b=(Builtin)node->val;Node *arg;Type seen[MAX_PLACE_DEPTH];
    int expected=b==BI_PATH_JOIN||b==BI_PATH_WITH_EXTENSION?2:1;
    if(node->nargs!=expected)error_tok(node->tok,"%s.%s の引数は %d 個です",builtin_namespace(b),builtin_name(b),expected);
    allow_void=0;
    for(arg=node->lhs;arg;arg=arg->next){
        if(arg->kind==ND_REFARG)error_tok(arg->tok,"lib / io.path の呼び出しに ref は付けません");
        check_expr(arg);
    }
    if(b==BI_LIB_SIZE){
        if(!library_size_type(node->lhs->ty,seen,0))error_tok(node->tok,"lib.size は %s の値のサイズに対応していません",type_name(node->lhs->ty));
        node->ty=TY_I64;
    }else if(b==BI_LIB_LENGTH){
        Type t=node->lhs->ty;
        if(!type_is_text(t)&&!type_is_array(t)&&!is_numeric(t))error_tok(node->tok,"lib.length は %s の長さに対応していません",type_name(t));
        node->ty=TY_I64;
    }else{
        for(arg=node->lhs;arg;arg=arg->next)if(!type_is_text(arg->ty))error_tok(arg->tok,"io.path.%s の引数は str・char にしてください",builtin_name(b));
        node->ty=b==BI_PATH_IS_ABSOLUTE||b==BI_PATH_IS_RELATIVE?TY_BOOL:TY_STR;
    }
    if(is_const_expr(node))(void)eval_const(node);
}
