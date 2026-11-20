/* Path-based file APIs have positional arguments and own their handles. */
static int file_bulk_builtin(int b)
{return b>=BI_FILE_READ_TEXT&&b<=BI_FILE_WRITE_LINES;}
static void file_bulk_const_text(Node *arg,int path,int line)
{
    Value v;uint64_t off,width,count;int i;
    if(!is_const_expr(arg))return;
    v=eval_const(arg);
    if(grapheme_scan((const unsigned char *)v.s,(uint64_t)v.len,-1,&off,&width,&count))
        error_tok(arg->tok,QMSG_0825);
    for(i=0;i<v.len;i++){
        if(path&&!v.s[i])error_tok(arg->tok,QMSG_0826);
        if(line&&(v.s[i]=='\r'||v.s[i]=='\n'))error_tok(arg->tok,QMSG_0797);
    }
}
static Node *file_bulk_default(Node *node,Type ty,long long value,const char *text)
{
    Node *arg=calloc(1,sizeof(*arg));if(!arg)fatal(QMSG_0827);
    arg->tok=node->tok;arg->ty=ty;arg->kind=text?ND_STRLIT:ty==TY_BOOL?ND_BOOL:ND_NUM;
    arg->val=value;if(text){arg->str=(char *)text;arg->str_len=(int)strlen(text);}return arg;
}
static void check_file_bytes_async(Node *node)
{
    int read=node->val==BI_FILE_READ_BYTES_ASYNC,min=read?1:2;
    Node *arg,*data,*next;
    if(node->nargs<min||node->nargs>2)error_tok(node->tok,QMSG_0841,builtin_name((Builtin)node->val),min,2);
    for(arg=node->lhs;arg;arg=arg->next)if(arg->kind==ND_REFARG)error_tok(arg->tok,QMSG_0842);
    if(node->nargs==1){node->lhs->next=file_bulk_default(node,TY_I64,67108864,NULL);node->nargs=2;}
    arg=node->lhs;check_expr(arg);
    if(!type_is_text(arg->ty))error_tok(arg->tok,QMSG_0843,builtin_name((Builtin)node->val));
    file_bulk_const_text(arg,1,0);data=arg->next;next=data->next;
    if(read){
        check_expected(data,TY_I64);
        if(!type_is_integer(data->ty))error_tok(data->tok,QMSG_0829);
        data=coerce(data,TY_I64,data->tok,QMSG_0830);
        if(is_const_expr(data)&&(eval_const(data).i<0||eval_const(data).i>(INT64_MAX-32)/8))error_tok(data->tok,QMSG_0792);
    }else{
        check_expected(data,array_type(TY_U8));
        if(data->ty!=array_type(TY_U8))error_tok(data->tok,QMSG_0896,builtin_name((Builtin)node->val));
        if(data->kind!=ND_ARRAY_COPY)data=copy_array(data,NULL,0);
    }
    data->next=next;arg->next=data;node->ty=read?array_type(TY_U8):TY_I64;
}
static void check_file_bulk_builtin(Node *node)
{
    int b=(int)node->val;
    if(b>=BI_FILE_READ_BYTES_ASYNC&&b<=BI_FILE_APPEND_BYTES_ASYNC){check_file_bytes_async(node);return;}
    if(builtin_is_file_async(b)){static const int sync[]={BI_FILE_READ_TEXT,BI_FILE_WRITE_TEXT,BI_FILE_APPEND_TEXT,BI_FILE_READ_LINES,BI_FILE_WRITE_LINES};b=sync[b-BI_FILE_READ_TEXT_ASYNC];}
    int min=b==BI_FILE_READ_TEXT||b==BI_FILE_READ_LINES?1:2;
    int max=b==BI_FILE_READ_TEXT?2:b==BI_FILE_READ_LINES?3:b==BI_FILE_WRITE_LINES?4:2,i=0;
    Node *arg,**slot=&node->lhs;
    if(node->nargs<min||node->nargs>max)error_tok(node->tok,QMSG_0841,builtin_name((Builtin)node->val),min,max);
    for(arg=node->lhs;arg;arg=arg->next)if(arg->kind==ND_REFARG)error_tok(arg->tok,QMSG_0842);
    while(node->nargs<max){
        Node **tail=&node->lhs;while(*tail)tail=&(*tail)->next;
        *tail=b==BI_FILE_WRITE_LINES?file_bulk_default(node,node->nargs==2?TY_STR:TY_BOOL,1,node->nargs==2?"\n":NULL):
            file_bulk_default(node,TY_I64,node->nargs==1?67108864:1048576,NULL);
        node->nargs++;
    }
    for(;*slot;slot=&(*slot)->next,i++){
        arg=*slot;
        if(i==0||((b==BI_FILE_WRITE_TEXT||b==BI_FILE_APPEND_TEXT)&&i==1)||(b==BI_FILE_WRITE_LINES&&i==2)){
            check_expr(arg);if(!type_is_text(arg->ty))error_tok(arg->tok,QMSG_0843,builtin_name((Builtin)node->val));
            file_bulk_const_text(arg,i==0,0);
            if(b==BI_FILE_WRITE_LINES&&i==2&&is_const_expr(arg)){
                Value v=eval_const(arg);
                if(!((v.len==1&&(v.s[0]=='\n'||v.s[0]=='\r'))||(v.len==2&&v.s[0]=='\r'&&v.s[1]=='\n')))
                    error_tok(arg->tok,QMSG_0795);
            }
        }else if(b==BI_FILE_WRITE_LINES&&i==1){
            Node *part;check_expected(arg,array_type(TY_STR));
            if(arg->ty!=array_type(TY_STR))error_tok(arg->tok,QMSG_0844);
            if(is_const_expr(arg)){
                Value v=eval_const(arg);
                for(part=v.items;part;part=part->next)file_bulk_const_text(part,0,1);
            }
            if(arg->kind!=ND_ARRAY_COPY){arg=copy_array(arg,NULL,0);arg->next=(*slot)->next;*slot=arg;}
        }else if(b==BI_FILE_WRITE_LINES){
            check_expr(arg);if(arg->ty!=TY_BOOL)error_tok(arg->tok,QMSG_0828);
        }else{
            long long limit=i==1?INT64_MAX-32:(INT64_MAX-32)/8;
            check_expected(arg,TY_I64);if(!type_is_integer(arg->ty))error_tok(arg->tok,QMSG_0829);
            arg=coerce(arg,TY_I64,arg->tok,QMSG_0830);arg->next=(*slot)->next;*slot=arg;
            if(is_const_expr(arg)&&(eval_const(arg).i<0||eval_const(arg).i>limit))error_tok(arg->tok,QMSG_0792);
        }
    }
    node->ty=b==BI_FILE_READ_TEXT?TY_STR:b==BI_FILE_READ_LINES?array_type(TY_STR):TY_I64;
}
