/* Function values share the ordinary argument and reference checker. */
static int function_unsafe(Type ty,Type *path,int count)
{
    int i;ModelField *f;Var *p;Function *sig;
    if(ty==TY_FILE||ty==TY_ADDR||ty==TY_GROUP||type_is_interface(ty)||type_is_task(ty))return 1;
    for(i=0;i<count;i++)if(path[i]==ty)return 0;
    if(count>=MAX_PLACE_DEPTH)return 1;
    path[count++]=ty;
    if(type_is_array(ty))return function_unsafe(array_element(ty),path,count);
    if(type_is_model(ty))for(f=type_model(ty)->fields;f;f=f->next)if(function_unsafe(f->ty,path,count))return 1;
    if(type_is_function(ty)){sig=function_signature(ty);if(function_unsafe(sig->ret,path,count))return 1;for(p=sig->params;p;p=p->next)if(function_unsafe(p->ty,path,count))return 1;}
    return 0;
}
void check_function_shape(Function *sig)
{
    Var *p;Type path[MAX_PLACE_DEPTH];
    check_dims(sig->return_dims);
    if(function_unsafe(sig->ret,path,0))error_tok(sig->tok,QMSG_0903);
    for(p=sig->params;p;p=p->next){check_dims(p->dims);if(function_unsafe(p->ty,path,0))error_tok(p->tok,QMSG_0903);}
}
static void check_function_value(Node *node,Type expected)
{
    Function *fn=node->func;
    if(fn->is_async||fn->model||fn->contract||same_name(fn->name,fn->name_len,"main",4))error_tok(node->tok,QMSG_0898);
    if(fn->is_generic){
        Function *sig;Var *p;Node call={0},**tail=&call.lhs;
        if(!type_is_function(expected))error_tok(node->tok,QMSG_0899);
        sig=function_signature(expected);
        if(fn->param_count!=sig->param_count)error_tok(node->tok,QMSG_0902,type_name(expected));
        for(p=sig->params;p;p=p->next){Node *a=calloc(1,sizeof(*a));if(!a)fatal(QMSG_0437);a->kind=ND_NUM;a->tok=node->tok;a->ty=p->ty;
            if(p->is_ref){Node *ref=calloc(1,sizeof(*ref));if(!ref)fatal(QMSG_0437);ref->kind=ND_REFARG;ref->lhs=a;ref->tok=node->tok;a=ref;}
            *tail=a;tail=&a->next;}
        call.func=fn;call.tok=node->tok;call.nargs=fn->param_count;fn=node->func=specialize_generic_call(&call);
    }
    if(fn->infer_return||(fn->generic_origin&&fn->check_state!=1))ensure_function(fn,node->tok);
    check_function_shape(fn);node->ty=function_type(fn);
    if(type_is_function(expected)&&node->ty!=expected)error_tok(node->tok,QMSG_0902,type_name(expected));
    fn->value_taken=1;
}
static void check_call(Node *node);
static void check_invoke(Node *node)
{
    Function *sig;
    check_expr(node->rhs);
    if(!type_is_function(node->rhs->ty))error_tok(node->tok,QMSG_0901);
    sig=function_signature(node->rhs->ty);
    if(is_const_expr(node->rhs)&&!eval_const(node->rhs).function)error_tok(node->tok,QMSG_0900);
    if(node->nargs!=sig->param_count)error_tok(node->tok,QMSG_0904,sig->param_count,node->nargs);
    node->func=sig;check_call(node);
}

