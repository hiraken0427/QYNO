/* Cache token spans so nested signature lookahead stays linear. */
typedef struct FunctionTypeParse FunctionTypeParse;
struct FunctionTypeParse {FunctionTypeParse *next;Token *start,*end;Type type;};
static FunctionTypeParse *function_type_parses;
static Type parse_function_type(Token *at,Token **end)
{
    Token *saved=tok;Function f={0};Var **tail=&f.params;Type result;FunctionTypeParse *cached;
    for(cached=function_type_parses;cached;cached=cached->next)if(cached->start==at){if(end)*end=cached->end;return cached->type;}
    if(++parsing_signature>32)error_tok(at,QMSG_0905);
    tok=at->next;f.tok=at;f.ret=type_keyword(tok);
    if(f.ret==TY_NONE)error_tok(tok,QMSG_0897);
    skip_type();expect("(");
    if((equal(tok,"void")||equal(tok,"Void"))&&equal(tok->next,")"))tok=tok->next;
    while(!equal(tok,")")){
        Var *p=calloc(1,sizeof(*p));if(!p)fatal(QMSG_0437);p->tok=tok;
        p->is_ref=consume("ref");if(p->is_ref)p->is_readonly=consume("readonly");else consume("val");
        p->ty=type_keyword(tok);if(p->ty==TY_NONE)error_tok(tok,QMSG_0897);skip_type();
        if(tok->kind==TK_IDENT&&!is_keyword(tok)){p->name=tok->loc;p->name_len=tok->len;tok=tok->next;}
        p->dims=dimensions(&p->ty);*tail=p;tail=&p->next;f.param_count++;
        if(!consume(","))break;
    }
    expect(")");f.return_dims=dimensions(&f.ret);result=function_type(&f);
    cached=calloc(1,sizeof(*cached));if(!cached)fatal(QMSG_0437);cached->start=at;cached->end=tok;cached->type=result;cached->next=function_type_parses;function_type_parses=cached;
    if(end)*end=tok;
    tok=saved;parsing_signature--;return result;
}
