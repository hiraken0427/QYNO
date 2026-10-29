static Value eval_text_method(Node *node);
static int check_text_method(Node *node)
{
    static const char *names[]={"contains","starts_with","ends_with","find","trim","split","replace"};
    Node *arg;int op,count=0,expected;
    if(!type_is_text(node->lhs->ty))return 0;
    for(op=0;op<7;op++)if(!strcmp(node->name,names[op]))break;
    if(op==7)error_tok(node->tok,"%s にはメソッド %s はありません",type_name(node->lhs->ty),node->name);
    expected=op==TEXT_TRIM?0:op==TEXT_REPLACE?2:1;
    for(arg=node->rhs;arg;arg=arg->next)count++;
    if(count!=expected)error_tok(node->tok,"%s の引数は %d 個です",names[op],expected);
    for(arg=node->rhs;arg;arg=arg->next){
        if(arg->kind==ND_REFARG)error_tok(arg->tok,"文字列メソッドに ref は指定できません");
        check_expr(arg);
        if(!type_is_text(arg->ty))error_tok(arg->tok,"%s の引数は str / char にしてください",names[op]);
    }
    node->kind=ND_TEXT_METHOD;node->val=op;
    node->ty=op<=TEXT_ENDS?TY_BOOL:op==TEXT_FIND?TY_I64:op==TEXT_SPLIT?array_type(TY_STR):TY_STR;
    if(is_const_expr(node))(void)eval_text_method(node);
    else if((op==TEXT_SPLIT||op==TEXT_REPLACE)&&is_const_expr(node->rhs)&&!eval_const(node->rhs).len)
        error_tok(node->rhs->tok,"%s の検索文字列は空にできません",names[op]);
    return 1;
}
