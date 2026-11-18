/* Lower a checked match to a hidden scalar declaration and ordinary if/else.
 * All later return, initialization, readonly and close analyses see the same
 * control flow. The exhaustive final case becomes the unconditional else. */
static Node *match_node(NodeKind kind,Token *tok)
{Node *n=calloc(1,sizeof(*n));if(!n)fatal(QMSG_0303);n->kind=kind;n->tok=tok;return n;}
static void check_match(Node *node)
{
    Node *arm,*pattern,*tail,*original_next=node->next;EnumMember *m;EnumDef *e;
    Node **arms;int count=0,i,j,*seen;
    check_expr(node->cond);
    if(!type_is_enum(node->cond->ty))error_tok(node->tok,QMSG_0299);
    e=type_enum(node->cond->ty);ensure_enum(e);
    for(arm=node->then;arm;arm=arm->next)count++;
    arms=calloc((size_t)count+1,sizeof(*arms));seen=calloc((size_t)e->count,sizeof(*seen));
    if(!arms||!seen)fatal(QMSG_0300);
    for(arm=node->then,i=0;arm;arm=arm->next,i++){
        arms[i]=arm;
        for(pattern=arm->lhs;pattern;pattern=pattern->next){
            Node *next=pattern->next;Value v;Node *converted;
            pattern->next=NULL;check_expected(pattern,e->ty);converted=coerce(pattern,e->ty,pattern->tok,"case");
            if(!is_const_expr(converted))error_tok(pattern->tok,QMSG_0110);
            v=eval_const(converted);m=enum_value_member(e->ty,v.i);
            if(!m)error_tok(pattern->tok,QMSG_0106);
            if(seen[m->index])error_tok(pattern->tok,QMSG_0105,m->name);
            seen[m->index]=1;
            /* Canonicalize without disturbing the pattern list. */
            pattern->kind=ND_NUM;pattern->ty=e->ty;pattern->val=v.i;pattern->lhs=pattern->rhs=NULL;pattern->next=next;
        }
    }
    if(!node->els)for(j=0;j<e->count;j++)if(!seen[j]){
        for(m=e->members;m&&m->index!=j;m=m->next){}
        error_tok(node->tok,QMSG_0294,m->name);
    }
    tail=node->els;
    if(!tail&&count){tail=arms[--count]->then;}
    for(i=count-1;i>=0;i--){
        Node *condition=NULL,*branch=match_node(ND_IF,arms[i]->tok);
        for(pattern=arms[i]->lhs;pattern;pattern=pattern->next){
            Node *cmp=match_node(ND_CMP,pattern->tok),*chain=match_node(ND_CHAIN,pattern->tok);
            Node *value=match_node(ND_VAR,node->tok),*literal=match_node(ND_NUM,pattern->tok);
            value->var=node->var;literal->ty=e->ty;literal->val=pattern->val;
            cmp->lhs=value;cmp->rhs=literal;cmp->val=CMP_EQ;chain->lhs=cmp;
            if(!condition)condition=chain;else{Node *either=match_node(ND_OR,pattern->tok);either->lhs=condition;either->rhs=chain;condition=either;}
        }
        branch->cond=condition;branch->then=arms[i]->then;branch->els=tail;tail=branch;
    }
    node->var->ty=e->ty;node->kind=ND_DECL;node->lhs=node->cond;node->cond=node->then=node->els=NULL;
    node->next=tail;
    if(tail){Node *last=tail;while(last->next)last=last->next;last->next=original_next;}else node->next=original_next;
    free(arms);free(seen);check_stmt(node);
}
