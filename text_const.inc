static Value eval_text_method(Node *node)
{
    TextRequest t={0};WideRequest r={0};Value v={0},inputs[3]={{0}};Node *arg;int i=0;
    inputs[i++]=eval_const(node->lhs);
    for(arg=node->rhs;arg;arg=arg->next)inputs[i++]=eval_const(arg);
    for(i=0;i<3;i++){t.text[i]=(const unsigned char *)(inputs[i].s?inputs[i].s:"");t.length[i]=(uint64_t)inputs[i].len;}
    t.op=node->val;r.op=W_TEXT_METHOD;r.input=(const char *)&t;wide_execute(&r);
    if(t.status)const_error(node,t.status==65?"文字列メソッドに不正な UTF-8 です":"文字列メソッドの検索文字列は空にできません、またはサイズが不正です");
    if(t.scratch_size>INT_MAX)const_error(node,"文字列メソッドの作業領域が大きすぎます");
    t.scratch=calloc((size_t)t.scratch_size+1,1);if(!t.scratch)fatal("文字列の作業領域を確保できません");
    t.phase=1;wide_execute(&r);
    if(t.status||t.used>INT_MAX)const_error(node,"文字列メソッドの結果が大きすぎます");
    v.ty=node->ty;
    if(node->val<=TEXT_FIND){v.i=t.result;free(t.scratch);return v;}
    t.capacity=t.used;t.buffer=calloc((size_t)t.capacity+1,1);if(!t.buffer)fatal("文字列の結果を確保できません");
    t.phase=2;wide_execute(&r);
    if(t.status)const_error(node,"文字列メソッドの結果を生成できません");
    if(node->val==TEXT_SPLIT){
        uint64_t *parts=(uint64_t *)t.buffer,k;Node **tail=&v.items;
        v.len=(int)t.count;
        for(k=0;k<t.count;k++){
            Value part={0};part.ty=TY_STR;part.len=(int)parts[k*2+1];part.s=calloc((size_t)part.len+1,1);
            if(!part.s)fatal("分割文字列を確保できません");
            if(part.len)memcpy(part.s,inputs[0].s+parts[k*2],(size_t)part.len);
            *tail=value_node(part,node->tok);tail=&(*tail)->next;
        }
        free(t.buffer);
    }else{v.s=(char *)t.buffer;v.len=(int)t.used;}
    free(t.scratch);return v;
}
