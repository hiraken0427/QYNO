static int random_mentions(Node *n,Token *name)
{
    for(;n;n=n->next){
        if(n->kind==ND_VAR&&n->tok->len==name->len&&!memcmp(n->tok->loc,name->loc,(size_t)name->len))return 1;
        if(random_mentions(n->lhs,name)||random_mentions(n->rhs,name)||random_mentions(n->cond,name))return 1;
    }
    return 0;
}
static Node *random_default_pool(Token *tok)
{
    Node *n=calloc(1,sizeof(*n));int i;if(!n)fatal(QMSG_0492);
    n->kind=ND_STRLIT;n->tok=tok;n->ty=TY_STR;n->str=calloc(96,1);if(!n->str)fatal(QMSG_0492);
    for(i=0;i<95;i++)n->str[i]=(char)(32+i);
    n->str_len=95;return n;
}
static void check_random_builtin(Node *node)
{
    int b=(int)node->val,op=random_operation(b),count=node->nargs,i=0,constant=1;
    Type result=random_return_type(b);Node *a=node->lhs,*p;RandomRequest request={0};
    if(b==BI_RANDOM_INIT){
        Var param={0};
        if(!node->tok->source->module->builtin||strcmp(node->tok->source->module->name,"lib.random"))error_tok(node->tok,QMSG_0495);
        if(count!=2||!a||a->kind!=ND_REFARG)error_tok(node->tok,QMSG_0494);
        param.ty=standard_type(cur_ast,"lib.random","Random");check_ref_arg(a,&param,1);
        node->lhs=a->next;a->next=NULL;node->rhs=a;node->nargs=--count;a=node->lhs;
    }
    if(op==1||op==2){
        if(count==1&&a->kind==ND_CHAIN){
            Node *left=a->lhs,*right=left?left->next:NULL,*middle;
            int reverse;
            if(!right||right->next||left->rhs!=right->lhs||left->rhs->kind!=ND_VAR||(left->rhs->name&&strchr(left->rhs->name,'.')))error_tok(a->tok,QMSG_0499);
            middle=left->rhs;reverse=left->val==CMP_GT||left->val==CMP_GE;
            if((!reverse&&(left->val!=CMP_LT&&left->val!=CMP_LE))||
               (!reverse&&(right->val!=CMP_LT&&right->val!=CMP_LE))||
               (reverse&&(right->val!=CMP_GT&&right->val!=CMP_GE)))error_tok(a->tok,QMSG_0498);
            if(random_mentions(left->lhs,middle->tok)||random_mentions(right->rhs,middle->tok))error_tok(middle->tok,QMSG_0723);
            node->random_flags=1|(reverse?8:0)|
                ((reverse?right->val==CMP_GE:left->val==CMP_LE)?2:0)|
                ((reverse?left->val==CMP_GE:right->val==CMP_LE)?4:0);
            a=left->lhs;a->next=right->rhs;a->next->next=NULL;node->lhs=a;node->nargs=count=2;
        }else if(count||op==2)error_tok(node->tok,QMSG_0500);
    }else if((op==0&&count!=1)||(op==3&&count)||(op==4&&count>1)||(op==5&&(count<1||count>2)))
        error_tok(node->tok,QMSG_0488,builtin_name((Builtin)b));
    if(op==4&&!count){node->lhs=random_default_pool(node->tok);node->nargs=count=1;}
    if(op==5&&count==1){node->lhs->next=random_default_pool(node->tok);node->nargs=count=2;}
    {Node **link=&node->lhs;
     for(i=0;*link;i++){
        Node *n=*link,*next=n->next;Type target=TY_NONE;n->next=NULL;
        if(n->kind==ND_REFARG)error_tok(n->tok,QMSG_0493);
        if(op==2)check_expected(n,result);else if(op==0)check_expected(n,TY_U64);else check_expr(n);
        if(op==1){
            if(!type_is_integer(n->ty))error_tok(n->tok,QMSG_0663);
            if(integer_unsigned(n->ty)){node->random_flags|=i==0?64:128;target=TY_U128;}else target=TY_I128;
        }else if(op==2){n=coerce(n,result,n->tok,QMSG_0626);target=TY_QUAD;}
        else if(op==0|| (op==5&&i==0)){
            if(!type_is_integer(n->ty))error_tok(n->tok,QMSG_0489);
            target=op==0?TY_U64:TY_I64;
        }else if(!type_is_text(n->ty))error_tok(n->tok,QMSG_0491);
        if(target!=TY_NONE)n=coerce(n,target,n->tok,QMSG_0497);
        *link=n;n->next=next;link=&n->next;
     }}
    node->ty=result;request.op=op;request.ty=(op==1||op==2)?wide_tag(result):0;request.flags=node->random_flags;
    for(p=node->lhs;p;p=p->next)if(!is_const_expr(p))constant=0;
    if(!constant)return;
    a=node->lhs;
    if(op==0){request.first=value_bits(eval_const(a));}
    else if(op==1||op==2){
        if(a){request.first=value_bits(eval_const(a));request.second=value_bits(eval_const(a->next));}
        request.first_tag=op==2?W_QUAD:node->random_flags&64?128:-128;
        request.second_tag=op==2?W_QUAD:node->random_flags&128?128:-128;
    }else if(op==4||op==5){
        Value text=eval_const(op==4?a:a->next);request.text=text.s;request.length=text.len;
        if(op==5)request.count=eval_const(a).i;
    }
    {WideRequest wide={0};wide.op=W_RANDOM;wide.input=(const char *)&request;wide_execute(&wide);}
    if(request.status)error_tok(node->tok,QMSG_0502);
}
static int check_random_method(Node *node)
{
    Node *receiver=node->lhs,*self;int b;Var param={0};
    if(!type_is_model(receiver->ty)||receiver->ty!=standard_type(cur_ast,"lib.random","Random"))return 0;
    b=random_builtin_find(node->tok->loc,node->tok->len);if(b<0||b==BI_RANDOM_INIT)return 0;
    self=calloc(1,sizeof(*self));if(!self)fatal(QMSG_0505);
    self->kind=ND_REFARG;self->tok=node->tok;self->lhs=receiver;param.ty=receiver->ty;
    {Node *root=receiver;while(root->kind==ND_INDEX)root=root->lhs;
     if(root->kind!=ND_VAR){
        Node *temp=calloc(1,sizeof(*temp));if(!temp)fatal(QMSG_0504);
        temp->kind=ND_VAR;temp->var=hidden_local(receiver->ty,NULL,0,node->tok);temp->ty=receiver->ty;temp->tok=node->tok;
        self->rhs=receiver;self->lhs=temp;
     }}
    check_ref_arg(self,&param,1);
    if(readonly_place(receiver))error_tok(node->tok,QMSG_0123);
    node->kind=ND_BUILTIN;node->val=b;node->lhs=node->rhs;node->rhs=self;
    check_random_builtin(node);return 1;
}
