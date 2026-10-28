/* Enum declarations are nominal scalar types. Resolve their constant recipes
 * lazily so ordinary const dependencies retain cycle diagnostics. */
static EnumDef *enum_initial_owner;
static int enum_initial_limit;
static void ensure_enum_member(EnumMember *m)
{
    EnumDef *saved=enum_initial_owner;int limit=enum_initial_limit;EnumMember *p=NULL;
    if(enum_initial_owner==m->owner&&m->index>=enum_initial_limit)
        error_tok(m->tok,"enum の後続メンバーへの前方参照・循環参照はできません");
    if(m->state==2)return;
    if(m->state==1)error_tok(m->tok,"enum メンバー %s の定義が循環しています",m->name);
    m->state=1;
    enum_initial_owner=m->owner;enum_initial_limit=m->index;
    if(m->initial){
        Value v;
        check_expr(m->initial);
        if(!type_is_integer(m->initial->ty)||!is_const_expr(m->initial))error_tok(m->tok,"enum の値は int_32 の範囲内の整数定数式にしてください");
        v=eval_const(m->initial);
        if(type_is_wide(v.ty))v=wide_calculate(m->initial,W_CONVERT,v,(Value){0},TY_I32,0);
        if(!integer_fits(v.i,v.ty,TY_I32))error_tok(m->tok,"enum の値が int_32 の範囲外です");
        m->value=v.i;
    }else{
        EnumMember *it;for(it=m->owner->members;it&&it!=m;it=it->next)p=it;
        if(p){ensure_enum_member(p);m->value=p->value+1;}else m->value=0;
        if(m->value>INT32_MAX)error_tok(m->tok,"enum の連番が int_32 の範囲外です");
    }
    m->symbol->const_val=calloc(1,sizeof(Node));if(!m->symbol->const_val)fatal("enum の値を確保できません");
    m->symbol->const_val->kind=ND_NUM;m->symbol->const_val->ty=m->owner->ty;
    m->symbol->const_val->val=m->value;m->symbol->const_val->tok=m->tok;
    m->symbol->const_state=2;m->state=2;enum_initial_owner=saved;enum_initial_limit=limit;
}
static void ensure_enum(EnumDef *e)
{
    EnumMember *m,*p;int zero=0;
    if(e->state==2)return;
    if(e->state==1)error_tok(e->tok,"enum %s の定義が循環しています",e->name);
    e->state=1;
    for(m=e->members;m;m=m->next){
        ensure_enum_member(m);if(m->value==0)zero=1;
        for(p=e->members;p!=m;p=p->next)if(p->value==m->value)error_tok(m->tok,"enum の値 %lld が重複しています",m->value);
    }
    if(!zero)error_tok(e->tok,"enum には既定値となる値0のメンバーが必要です");
    e->state=2;
}
static EnumMember *enum_value_member(Type ty,long long value)
{
    EnumMember *m;ensure_enum(type_enum(ty));
    for(m=type_enum(ty)->members;m;m=m->next)if(m->value==value)return m;
    return NULL;
}
static int enum_context_name(Node *node,Type expected)
{
    EnumMember *m;char name[256];const char *text;
    if(!type_is_enum(expected)||node->kind!=ND_VAR||node->var)return 0;
    if(node->name)text=node->name;
    else{if(node->tok->len>=256)return 0;memcpy(name,node->tok->loc,(size_t)node->tok->len);name[node->tok->len]=0;text=name;}
    if(strchr(text,'.')||module_name_exists(node))return 0;
    m=enum_find(type_enum(expected),text);if(!m)return 0;
    node->var=m->symbol;node->var->is_used=1;return 1;
}
static int enum_unbound_name(Node *node)
{return node->kind==ND_VAR&&!node->var&&!module_name_exists(node);}
static void check_enum_conversion(Node *node)
{
    Type to=node->cmp_ty;
    if(node->nargs!=1)error_tok(node->tok,"enum の変換には引数を1個指定してください");
    if(node->lhs->kind==ND_REFARG)error_tok(node->tok,"enum の変換に ref は付けません");
    check_expected(node->lhs,to);
    if(node->lhs->ty!=to&&!type_is_integer(node->lhs->ty)&&!type_is_text(node->lhs->ty))
        error_tok(node->tok,"enum の変換元は同じenum・整数・文字列にしてください");
    node->ty=to;
    if(is_const_expr(node))(void)eval_const(node);
}
static void check_enum_names(Ast *ast)
{
    EnumDef *e,*p;Model *m;Interface *in;Function *fn;Var *v;
    for(e=ast->enums;e;e=e->next){
        for(p=ast->enums;p!=e;p=p->next)if(p->tok->source->module==e->tok->source->module&&!strcmp(p->name,e->name))error_tok(e->tok,"enum 型名 %s が重複しています",e->name);
        for(m=ast->models;m;m=m->next)if(m->tok->source->module==e->tok->source->module&&!strcmp(m->name,e->name))error_tok(e->tok,"enum 型名 %s がmodelと衝突しています",e->name);
        for(in=ast->interfaces;in;in=in->next)if(in->tok->source->module==e->tok->source->module&&!strcmp(in->name,e->name))error_tok(e->tok,"enum 型名 %s がinterfaceと衝突しています",e->name);
        for(fn=ast->funcs;fn;fn=fn->next)if(!fn->model&&fn->tok->source->module==e->tok->source->module&&same_name(fn->name,fn->name_len,e->name,(int)strlen(e->name)))error_tok(e->tok,"enum 型名 %s が関数と衝突しています",e->name);
        for(v=ast->global_vars;v;v=v->next)if(v->tok->source->module==e->tok->source->module&&same_name(v->name,v->name_len,e->name,(int)strlen(e->name)))error_tok(e->tok,"enum 型名 %s がconstと衝突しています",e->name);
    }
}
