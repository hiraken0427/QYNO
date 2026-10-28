/* Public API types must be nameable by importing modules. Private state inside
 * a public model remains permitted, as do private implementation contracts. */
static void check_export_type(Type ty,Token *site)
{
    int hidden=0;Type base=array_base(ty);
    if(type_is_model(base))hidden=type_model(base)->is_private;
    else if(type_is_interface(base))hidden=type_interface(base)->is_private;
    if(hidden)error_tok(site,"公開する定義に private の型 %s を使用できません。定義を private にするか公開型へ変換してください",type_name(base));
}
static void check_export_function(Function *fn)
{
    Var *p;
    if(fn->is_internal||fn->is_private||(fn->model&&fn->model->is_private)||(fn->contract&&fn->contract->is_private))return;
    if(fn->ret!=TY_NONE)check_export_type(fn->ret,fn->tok);
    for(p=fn->params;p;p=p->next)if(p->ty!=TY_NONE)check_export_type(p->ty,p->tok);
}
static void check_module_exports(Ast *ast)
{
    Var *v;Model *m;Interface *in;Function *fn;
    for(v=ast->global_vars;v;v=v->next)if(!v->is_private)check_export_type(v->ty,v->tok);
    for(m=ast->models;m;m=m->next)if(!m->is_private){ModelField *f;for(f=m->fields;f;f=f->next)if(!f->is_private)check_export_type(f->ty,f->tok);}
    for(in=ast->interfaces;in;in=in->next)if(!in->is_private)for(fn=in->methods;fn;fn=fn->next)check_export_function(fn);
    for(fn=ast->funcs;fn;fn=fn->next)check_export_function(fn);
}
