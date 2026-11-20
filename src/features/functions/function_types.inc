/* Intern structural signatures in the shared dynamic type table. */
int type_is_function(Type ty){return (int)ty>=TY_ARRAY && types[(int)ty-TY_ARRAY].signature!=NULL;}
Function *function_signature(Type ty){return types[(int)ty-TY_ARRAY].signature;}
const char *function_type_name(Type ty){return types[(int)ty-TY_ARRAY].name;}
static int function_dims_equal(ArrayDim *a,ArrayDim *b)
{
    for(;a||b;a=a?a->next:NULL,b=b?b->next:NULL){
        if(!a){if(b->checked?b->limit!=-1:b->bound!=NULL)return 0;continue;}
        if(!b){if(a->checked?a->limit!=-1:a->bound!=NULL)return 0;continue;}
        if(a->checked&&b->checked){if(a->limit!=b->limit)return 0;}
        else if(a->bound!=b->bound)return 0;
    }
    return 1;
}
Type function_type(Function *signature)
{
    DynamicType e={0};Var *p,*q,**tail;Function *f;int i;Buffer name;
    for(i=0;i<count;i++)if((f=types[i].signature)!=NULL&&f->ret==signature->ret&&f->param_count==signature->param_count&&function_dims_equal(f->return_dims,signature->return_dims)){
        for(p=f->params,q=signature->params;p&&q;p=p->next,q=q->next)
            if(p->ty!=q->ty||p->is_ref!=q->is_ref||p->is_readonly!=q->is_readonly||!function_dims_equal(p->dims,q->dims))break;
        if(!p&&!q)return (Type)(TY_ARRAY+i);
    }
    f=calloc(1,sizeof(*f));if(!f)fatal(QMSG_0589);
    f->ret=signature->ret;f->return_dims=signature->return_dims;f->param_count=signature->param_count;f->tok=signature->tok;tail=&f->params;
    for(p=signature->params;p;p=p->next){q=calloc(1,sizeof(*q));if(!q)fatal(QMSG_0589);*q=*p;q->next=NULL;*tail=q;tail=&q->next;}
    e.signature=f;buf_init(&name);
    buf_add(&name,(const unsigned char *)"func ",5);
    buf_add(&name,(const unsigned char *)type_name(array_base(f->ret)),strlen(type_name(array_base(f->ret))));
    buf_add(&name,(const unsigned char *)"(",1);
    for(p=f->params;p;p=p->next){
        if(p!=f->params)buf_add(&name,(const unsigned char *)", ",2);
        if(p->is_ref)buf_add(&name,(const unsigned char *)(p->is_readonly?"ref readonly ":"ref "),p->is_readonly?13:4);
        buf_add(&name,(const unsigned char *)type_name(p->ty),strlen(type_name(p->ty)));
        if(p->ty==TY_VOID&&f->param_count==1)buf_add(&name,(const unsigned char *)" value",6);
    }
    buf_add(&name,(const unsigned char *)")",1);
    for(i=0;i<array_rank(f->ret);i++)buf_add(&name,(const unsigned char *)"[]",2);
    buf_add_u8(&name,0);e.name=(char *)name.data;return add_type(e);
}
