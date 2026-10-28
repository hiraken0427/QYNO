static Value enum_convert_value(Node *site,Value v,Type to)
{
    if(type_is_enum(v.ty)&&to==TY_STR){
        EnumMember *m=enum_value_member(v.ty,v.i);
        if(!m)const_error(site,"enum の値が未定義です");
        v.ty=TY_STR;v.s=m->name;v.len=(int)strlen(m->name);return v;
    }
    if(type_is_enum(v.ty)){v.ty=TY_I32;return convert_value(site,v,to);}
    if(type_is_text(v.ty)){
        int a=0,b=v.len;EnumMember *m;WideRequest r={0};
        while(a<b&&(v.s[a]==' '||v.s[a]=='\t'||v.s[a]=='\r'||v.s[a]=='\n'))a++;
        while(b>a&&(v.s[b-1]==' '||v.s[b-1]=='\t'||v.s[b-1]=='\r'||v.s[b-1]=='\n'))b--;
        ensure_enum(type_enum(to));
        for(m=type_enum(to)->members;m;m=m->next)if((int)strlen(m->name)==b-a&&!memcmp(m->name,v.s+a,(size_t)(b-a))){v.ty=to;v.i=m->value;return v;}
        r.op=W_READ;r.ty=-32;r.input=v.s+a;r.length=b-a;wide_execute(&r);
        if(r.status||r.used!=b-a)const_error(site,"enum の名前または数値を読み取れません");
        v.ty=TY_I32;v.i=integer_bits(r.out.lo);
    }
    v=convert_value(site,v,TY_I32);
    if(!enum_value_member(to,v.i))const_error(site,"enum に宣言されていない数値です");
    v.ty=to;return v;
}
