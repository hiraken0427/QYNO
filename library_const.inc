static long long library_const_size(Node *where,Value v)
{
    int width=value_bit_width(v.ty);long long sum=0,n;Node *item;
    if(width>=0)return width;
    if(type_is_text(v.ty))return (long long)v.len*8;
    for(item=v.items;item;item=item->next){
        n=library_const_size(where,eval_const(type_is_model(v.ty)?item->lhs:item));
        if(n>LLONG_MAX-sum)const_error(where,QMSG_0287);
        sum+=n;
    }
    return sum;
}
static Value eval_library_builtin(Node *node)
{
    Value v={0},a=eval_const(node->lhs);WideRequest r={0};
    v.ty=node->ty;
    if(node->val==BI_LIB_SIZE){v.i=library_const_size(node,a);return v;}
    if(node->val==BI_LIB_LENGTH){
        int offset,length;
        if(type_is_array(a.ty)){v.i=a.len;return v;}
        if(type_is_text(a.ty)){v.i=string_info(node,a,-1,&offset,&length);return v;}
        r.op=W_DIGITS;r.from=wide_tag(a.ty);r.a=value_bits(a);wide_execute(&r);
        if(r.status)const_error(node,QMSG_0284);
        v.i=(long long)r.out.lo;return v;
    }
    r.op=W_PATH;r.from=checking_target==TARGET_WINDOWS;r.aux=node->val-BI_PATH_JOIN;
    r.input=a.s;r.length=a.len;r.base=(long long)a.len+4;
    if(node->lhs->next){Value b=eval_const(node->lhs->next);r.b.lo=(uint64_t)(uintptr_t)b.s;r.b.hi=(uint64_t)b.len;r.base+=b.len;}
    if(r.base>INT_MAX)const_error(node,QMSG_0278);
    r.buffer=calloc((size_t)r.base,1);if(!r.buffer)fatal(QMSG_0279);wide_execute(&r);
    if(r.status)const_error(node,r.status==65?QMSG_0274:QMSG_0276);
    if(v.ty==TY_BOOL)v.i=(long long)r.out.lo;
    else{v.s=r.buffer;v.len=(int)r.out.lo;}
    return v;
}
