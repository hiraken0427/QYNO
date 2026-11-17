static int time_pure_builtin(int b)
{return builtin_is_time(b)&&b>=BI_TIME_NANOSECONDS&&b<=BI_TIME_FORMAT_DURATION&&b!=BI_TIME_TRY_PARSE;}
static Value eval_time_builtin(Node *node);
static void check_time_builtin(Node *node)
{
    int op=(int)node->val-BI_TIME_NOW,i=0;const TimeSpec *spec=time_spec(op);Node **link=&node->lhs,*arg;
    if(op>=T_TICKS&&(!node->tok->source->module->builtin||strcmp(node->tok->source->module->name,"time")))error_tok(node->tok,QMSG_0338);
    if(node->nargs<spec->min_args||node->nargs>spec->max_args)error_tok(node->tok,QMSG_0345,builtin_name((Builtin)node->val),spec->min_args,spec->max_args);
    allow_void=0;
    for(;i<spec->max_args;i++){
        Node *next;
        if(!*link){arg=calloc(1,sizeof(*arg));if(!arg)fatal(QMSG_0343);arg->kind=ND_NUM;arg->ty=TY_I64;arg->tok=node->tok;arg->val=op==T_FORMAT_DURATION?3:0;*link=arg;node->nargs++;}
        arg=*link;next=arg->next;arg->next=NULL;
        if(spec->args[i]=='r'){
            Var target={0};target.ty=TY_QUAD;if(arg->kind!=ND_REFARG)error_tok(arg->tok,QMSG_0355);check_ref_arg(arg,&target,i+1);
        }else if(spec->args[i]=='d'){
            check_expected(arg,standard_type(cur_ast,"time","DateTime"));
            if(arg->ty!=standard_type(cur_ast,"time","DateTime"))error_tok(arg->tok,QMSG_0344);
        }else if(spec->args[i]=='s'){
            check_expr(arg);if(!type_is_text(arg->ty))error_tok(arg->tok,QMSG_0342);
        }else{
            check_expected(arg,spec->args[i]=='i'?TY_INT:TY_QUAD);
            if(!is_numeric(arg->ty)||(spec->args[i]=='i'&&!type_is_integer(arg->ty)))error_tok(arg->tok,QMSG_0340);
            arg=coerce(arg,spec->args[i]=='i'?TY_INT:TY_QUAD,arg->tok,QMSG_0339);*link=arg;
        }
        arg->next=next;link=&arg->next;
    }
    switch(spec->result){
    case TR_VOID:node->ty=TY_VOID;break;
    case TR_BOOL:node->ty=TY_BOOL;break;
    case TR_INT:node->ty=op==T_FREQUENCY?TY_I64:TY_INT;break;
    case TR_DATE:node->ty=standard_type(cur_ast,"time","DateTime");break;
    case TR_WEEKDAY:node->ty=standard_type(cur_ast,"time","Weekday");break;
    case TR_TEXT:node->ty=TY_STR;break;
    case TR_TICKS:node->ty=TY_I128;break;
    default:node->ty=TY_QUAD;break;
    }
    if(time_pure_builtin((int)node->val)&&is_const_expr(node))(void)eval_time_builtin(node);
    if((op==T_SLEEP||op==T_SLEEP_UNTIL||op==T_ELAPSED)&&is_const_expr(node->lhs)){
        Value v=eval_const(node->lhs);if(!wide_finite(v))error_tok(node->tok,QMSG_0341);
        if(op==T_SLEEP){TimeRequest request={0};WideRequest r={0};request.op=T_SLEEP;request.args[0]=value_bits(v);r.op=W_TIME;r.input=(const char *)&request;wide_execute(&r);if(r.status)error_tok(node->tok,QMSG_0333);}
    }
}
