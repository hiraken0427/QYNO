/* All wide constants use the same bit-oriented engine as generated programs. */
static WideBits value_bits(Value v)
{
    WideBits b = {0,0};
    if (type_is_wide(v.ty)) return v.wide;
    if (type_is_integer(v.ty)) { b.lo=(uint64_t)v.i; return b; }
    if (v.ty==TY_FLOAT) { float f=(float)v.d; uint32_t n; memcpy(&n,&f,4); b.lo=n; }
    else memcpy(&b.lo,&v.d,8);
    return b;
}
static Value bits_value(WideBits b, Type ty)
{
    Value v={0};v.ty=ty;
    if(type_is_wide(ty))v.wide=b;
    else if(type_is_integer(ty)||ty==TY_BOOL)v.i=integer_bits(b.lo);
    else if(ty==TY_FLOAT){uint32_t n=(uint32_t)b.lo;float f;memcpy(&f,&n,4);v.d=f;}
    else memcpy(&v.d,&b.lo,8);
    return v;
}
static Value wide_calculate(Node *node,int op,Value a,Value b,Type ty,int aux)
{
    WideRequest r={0};r.op=op;r.ty=wide_tag(ty);r.from=wide_tag(a.ty);r.aux=aux;r.a=value_bits(a);r.b=value_bits(b);
    wide_execute(&r);
    if(r.status)const_error(node,r.status==3?"0 で割ることや0の負の累乗はできません":r.status==4?"負の累乗の結果を整数で表せません":"数値の変換・演算結果が対象型の範囲外です");
    return bits_value(r.out,op==W_CMP?TY_BOOL:ty);
}
static int wide_fits(Value v,Type ty)
{
    WideRequest r={0};r.op=W_CONVERT;r.ty=wide_tag(ty);r.from=wide_tag(v.ty);r.a=value_bits(v);wide_execute(&r);return !r.status;
}
static int wide_operation(NodeKind k)
{
    return k==ND_ADD?W_ADD:k==ND_SUB?W_SUB:k==ND_MUL?W_MUL:k==ND_DIV?W_DIV:k==ND_MOD?W_MOD:
           k==ND_BIT_AND?W_AND:k==ND_BIT_OR?W_OR:k==ND_BIT_XOR?W_XOR:k==ND_BIT_NOT?W_NOT:k==ND_SHL?W_SHL:W_SHR;
}
static void wide_literal_context(Node *n,Type ty)
{
    if(ty!=TY_HALF&&ty!=TY_QUAD)return;
    if((n->kind==ND_FNUM||n->kind==ND_WIDE)&&n->raw_numeric&&n->tok->kind==TK_FNUM){
        WideRequest r={0};r.op=W_READ;r.ty=wide_tag(ty);r.input=n->tok->loc;r.length=n->tok->len;wide_execute(&r);
        if(r.status)error_tok(n->tok,"小数リテラルが %s の範囲外です",type_name(ty));
        n->kind=ND_WIDE;n->ty=ty;n->wide=r.out;return;
    }
    if(n->kind==ND_NEG||n->kind==ND_ADD||n->kind==ND_SUB||n->kind==ND_MUL||n->kind==ND_DIV||n->kind==ND_POW){
        /* Explicit conversions and already checked nonliteral expressions retain their type. */
        if(n->ty!=TY_NONE)return;
        wide_literal_context(n->lhs,ty);if(n->rhs&&n->kind!=ND_POW)wide_literal_context(n->rhs,ty);
    }
}
static Value wide_read_value(Node *node,Value text,Type ty,int base)
{
    WideRequest r={0};int end=text.len;
    while(end&& (text.s[end-1]==' '||text.s[end-1]=='\t'))end--;
    r.op=W_READ;r.ty=wide_tag(ty);r.input=text.s;r.length=end;r.base=base;wide_execute(&r);
    if(r.status||r.used!=end)const_error(node,"文字列を指定した数値型として読み取れません（範囲・基数・文字列を確認してください）");
    return bits_value(r.out,ty);
}

static int wide_finite(Value v)
{
    return v.ty==TY_HALF ? (v.wide.lo&31744)!=31744 : v.ty==TY_QUAD ? (v.wide.hi&UINT64_C(0x7fff000000000000))!=UINT64_C(0x7fff000000000000) : 1;
}
static int wide_sign(Value v)
{
    WideRequest r={0};r.op=W_CMP;r.ty=wide_tag(v.ty);r.a=v.wide;r.aux=CMP_EQ;wide_execute(&r);if(r.out.lo)return 0;
    return integer_unsigned(v.ty)?1 : (v.ty==TY_HALF ? v.wide.lo>>15 : v.wide.hi>>63) ? -1 : 1;
}
