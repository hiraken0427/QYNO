static Value eval_time_builtin(Node *node)
{
    TimeRequest request={0};WideRequest call={0};Value v={0};Node *arg;int i=0;
    const TimeSpec *spec=time_spec((int)node->val-BI_TIME_NOW);
    request.op=(int)node->val-BI_TIME_NOW;v.ty=node->ty;
    if(!time_pure_builtin((int)node->val))const_error(node,"時計取得・待機・try_parseはconstに使えません");
    for(arg=node->lhs;arg;arg=arg->next,i++){
        Value a=eval_const(arg);
        if(spec->args[i]=='s'){
            if(request.op==T_FORMAT){request.pattern=a.s;request.pattern_length=a.len;}
            else {request.text=a.s;request.length=a.len;}
        }else if(spec->args[i]=='d'){
            Node *field;
            for(field=a.items;field;field=field->next){Value f=eval_const(field->lhs);int index=field->field->index;
                if(index<5){int64_t integer=f.i;memcpy((char *)&request.date+index*8,&integer,8);}
                else if(index==5)request.date.second=value_bits(f);else request.date.offset_minutes=f.i;}
        }else request.args[i]=value_bits(convert_value(node,a,TY_QUAD));
    }
    request.capacity=256+8*(request.length+request.pattern_length);
    if(request.capacity>INT_MAX)const_error(node,"timeの文字列が大きすぎます");
    request.buffer=calloc((size_t)request.capacity,1);if(!request.buffer)fatal("timeの文字列を確保できません");
    call.op=W_TIME;call.input=(const char *)&request;wide_execute(&call);
    if(request.status)const_error(node,"timeの引数・日時・形式・範囲が不正です");
    if(spec->result==TR_DATE){
        ModelField *f;Node **end=&v.items;v.len=7;
        for(f=type_model(v.ty)->fields;f;f=f->next){Node *item=calloc(1,sizeof(*item));Value component={0};int index=f->index;
            if(!item)fatal("DateTime定数を確保できません");
            component.ty=f->ty;
            if(index<5){int64_t integer;memcpy(&integer,(const char *)&request.date+index*8,8);component.i=integer;}else if(index==5)component.wide=request.date.second;else component.i=request.date.offset_minutes;
            item->kind=ND_FIELD;item->field=f;item->tok=node->tok;item->lhs=value_node(component,node->tok);*end=item;end=&item->next;
        }
    }else if(spec->result==TR_TEXT){v.s=request.buffer;v.len=(int)request.used;}
    else if(spec->result==TR_QUAD)v.wide=request.out;
    else v.i=integer_bits(request.out.lo);
    return v;
}
