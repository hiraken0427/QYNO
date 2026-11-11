/* One shared body per concrete array type and operation. Bounds remain runtime
 * shape data. Unsupported or excessive specialization uses the generic ABI. */
typedef struct ArrayCode ArrayCode;
#ifndef QYNO_ARRAY_CODE_LIMIT
#define QYNO_ARRAY_CODE_LIMIT 1024
#endif
#ifndef QYNO_ARRAY_CODE_BUDGET
#define QYNO_ARRAY_CODE_BUDGET (2u * 1024u * 1024u)
#endif
struct ArrayCode { ArrayCode *next;Type ty;int labels[4],emitted[4]; };
static ArrayCode *array_codes,*array_codes_tail;
static int array_code_count;
static size_t array_code_bytes;
static void typed_arrays_reset(void)
{
    ArrayCode *p=array_codes;
    while(p){ArrayCode *next=p->next;free(p);p=next;}
    array_codes=array_codes_tail=NULL;array_code_count=0;array_code_bytes=0;
}
static int array_runtime_label(Type ty,int operation)
{
    static const char *names[]={"copy","append","equal","print"};
    int generic[]={array_copy_label,array_append_label,array_equal_label,array_print_label};
    ArrayCode *p;
    char name[96];
    int i;
    if(operation<0||operation>3||!type_is_array(ty))fatal("内部エラー: 配列操作の型が不正です");
    if(type_is_interface(array_base(ty)))return generic[operation];
    for(p=array_codes;p;p=p->next)if(p->ty==ty)break;
    if(!p){
        if(array_code_count>=QYNO_ARRAY_CODE_LIMIT)return generic[operation];
        p=calloc(1,sizeof(*p));if(!p)fatal("配列操作の計画を確保できません");
        p->ty=ty;for(i=0;i<4;i++)p->labels[i]=-1;
        if(array_codes_tail)array_codes_tail->next=p;else array_codes=p;
        array_codes_tail=p;array_code_count++;
    }
    if(p->labels[operation]<0){
        /* Reserve a conservative 4 KiB per body before emitting it. Child
         * operations have their own entries and consume the same budget. */
        if(array_code_bytes>QYNO_ARRAY_CODE_BUDGET||QYNO_ARRAY_CODE_BUDGET-array_code_bytes<4096u)return generic[operation];
        array_code_bytes+=4096u;
        snprintf(name,sizeof(name),".L.qy_array_%s.t%d",names[operation],ty);
        p->labels[operation]=x64_new_label(name);
    }
    return p->labels[operation];
}
static void gen_typed_arrays(void)
{
    ArrayCode *p;
    int op,changed;
    emitting_function=NULL;
    /* New child types append to this queue. The same type has lower-rank
     * children, and model recursion is handled by separately cached bodies. */
    do{
    changed=0;
    for(p=array_codes;p;p=p->next)for(op=0;op<4;op++)if(p->labels[op]>=0&&!p->emitted[op]){
        p->emitted[op]=1;changed=1;
        if(op==AR_COPY)gen_array_copy_runtime(p->ty,p->labels[op]);
        else if(op==AR_APPEND)gen_array_append_runtime(p->ty,p->labels[op]);
        else if(op==AR_EQUAL)gen_array_equal_runtime(p->ty,p->labels[op]);
        else gen_array_print_runtime(p->ty,p->labels[op]);
    }
    }while(changed);
}
