/* The source implementation stays generic; predicates receive an expected
 * signature so an untyped named function can be specialized at the call. */
static void check_collection_query(Node *node,Function *fn)
{
    Var parameter={0};Function predicate={0};Node *values,*test;Type expected;
    if(!fn->tok->source->module->builtin||strcmp(fn->tok->source->module->name,"lib"))return;
    if(!same_name(fn->name,fn->name_len,"filter",6)&&!same_name(fn->name,fn->name_len,"count",5)&&
       !same_name(fn->name,fn->name_len,"any",3)&&!same_name(fn->name,fn->name_len,"all",3))return;
    values=node->lhs;test=values->next;
    if(values->kind==ND_REFARG)error_tok(values->tok,QMSG_0014,1);
    if(test->kind==ND_REFARG)error_tok(test->tok,QMSG_0014,2);
    check_expr(values);
    if(!type_is_array(values->ty))error_tok(values->tok,QMSG_0907,fn->name_len,fn->name);
    parameter.ty=array_element(values->ty);parameter.tok=test->tok;
    predicate.ret=TY_BOOL;predicate.params=&parameter;predicate.param_count=1;predicate.tok=test->tok;
    check_function_shape(&predicate);expected=function_type(&predicate);
    check_expected(test,expected);
    if(test->ty!=expected)error_tok(test->tok,QMSG_0902,type_name(expected));
}
