static int async_origin(Node *n)
{
    Node *a;int origin=0,value;
    if(!n)return 0;
    if(n->kind==ND_AWAIT)return 0;
    if(n->kind==ND_VAR&&n->var)return n->var->task_origin;
    if((n->kind==ND_CALL&&n->func->is_async)||(n->kind==ND_BUILTIN&&type_is_task(n->ty)))origin=n->async_scope;
    for(a=n->lhs;a;a=a->next){value=async_origin(a);if(value>origin)origin=value;}
    for(a=n->rhs;a;a=a->next){value=async_origin(a);if(value>origin)origin=value;}
    return origin;
}
static void async_region_store(Node *place,Node *value,Var *variable,Token *tok)
{
    int origin=async_origin(value);
    if(!variable){while(place&&(place->kind==ND_INDEX||place->kind==ND_FIELD))place=place->lhs;if(place&&place->kind==ND_VAR)variable=place->var;}
    if(!variable)return;
    if(origin>variable->task_scope)error_tok(tok,QMSG_0889);
    if(origin>variable->task_origin)variable->task_origin=origin;
}
static void async_regions(Node *n)
{
    for(;n;n=n->next){
        if((n->kind==ND_DECL||n->kind==ND_ASSIGN)&&type_is_task(array_base(n->var->ty)))async_region_store(NULL,n->lhs,n->var,n->tok);
        if(n->kind==ND_STORE && type_is_task(array_base(n->lhs->ty)))async_region_store(n->lhs,n->rhs,NULL,n->tok);
        if(n->kind==ND_METHOD&&n->val==AM_APPEND&&type_is_task(array_base(n->lhs->ty)))async_region_store(n->lhs,n->rhs,NULL,n->tok);
        async_regions(n->then);async_regions(n->els);async_regions(n->rhs);
        async_regions(n->lhs);async_regions(n->init);async_regions(n->step);
    }
}

/* Initial task handles stay within async ownership scopes. References and file
 * handles may be used locally, but never captured by another task. */
static int async_unsafe(Type ty, Type *path, int count, int tasks)
{
    int i; ModelField *field;
    if (ty==TY_ADDR || ty==TY_FILE || type_is_interface(ty)) return 1;
    if (type_is_task(ty)) return tasks || async_unsafe(task_result(ty),path,count,1);
    if (type_is_array(ty)) return async_unsafe(array_element(ty),path,count,tasks);
    for(i=0;i<count;i++)if(path[i]==ty)return 0;
    if(count==MAX_PLACE_DEPTH)return 1;
    path[count++]=ty;
    if(type_is_model(ty))for(field=type_model(ty)->fields;field;field=field->next)
        if(async_unsafe(field->ty,path,count,tasks))return 1;
    return 0;
}
static int async_suspends(Node *n)
{
    InputField *f;
    for(;n;n=n->next){
        if(n->kind==ND_GROUP || n->kind==ND_AWAIT || (n->kind==ND_CALL && n->func->is_async) ||
           (n->kind==ND_BUILTIN && (n->val==BI_TASK_YIELD || n->val==BI_TIME_SLEEP_ASYNC || n->val==BI_TASK_ALL || n->val==BI_TASK_TIMEOUT || builtin_is_file_async((int)n->val))))return 1;
        if(async_suspends(n->lhs)||async_suspends(n->rhs)||async_suspends(n->cond)||
           async_suspends(n->then)||async_suspends(n->els)||async_suspends(n->init)||
           async_suspends(n->step)||async_suspends(n->end))return 1;
        for(f=n->fields;f;f=f->next)if(async_suspends(f->var))return 1;
    }
    return 0;
}
static void async_finalizers(Node *n)
{
    for(;n;n=n->next){
        if(n->kind==ND_TRY && async_suspends(n->rhs))error_tok(n->tok,QMSG_0876);
        async_finalizers(n->then);async_finalizers(n->els);
    }
}
static void check_async_program(Ast *ast)
{
    Function *fn;Var *v;Type path[MAX_PLACE_DEPTH];
    for(fn=ast->funcs;fn;fn=fn->next){
        if(fn->is_template)continue;
        async_regions(fn->body);
        if(async_unsafe(fn->ret,path,0,1) && type_is_task(array_base(fn->ret)))
            error_tok(fn->tok,QMSG_0871);
        if(fn->is_async){
            if(async_unsafe(fn->ret,path,0,1))error_tok(fn->tok,QMSG_0872);
            for(v=fn->params;v;v=v->next)
                if(v->is_ref || async_unsafe(v->ty,path,0,0))error_tok(v->tok,QMSG_0873);
            /* Conservatively forbid persistent raw places in a resumable body.
             * Temporary ref arguments to synchronous calls remain supported. */
            for(v=fn->locals;v;v=v->next)
                if(v->name_len && (v->is_ref || v->ty==TY_ADDR || type_is_interface(v->ty)))
                    error_tok(v->tok,QMSG_0874);
            async_finalizers(fn->body);
        }else{
            for(v=fn->params;v;v=v->next)if(type_is_task(array_base(v->ty)))error_tok(v->tok,QMSG_0875);
        }
    }
}
