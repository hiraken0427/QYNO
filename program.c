#include "qyc.h"

#include <stdlib.h>
#include <string.h>

typedef struct {size_t old_offset,offset,length;uint64_t hash;int alignment,used;} DataPiece;
/* Keep each logical constant intact. Live relocations select pieces before
 * final encoding; the second pass verifies the same data creation order. */
static DataPiece *pieces;
static int piece_count,piece_capacity,piece_cursor,data_mode;
static uint64_t data_hash(const void *bytes,size_t length)
{
    const unsigned char *p=bytes;uint64_t h=UINT64_C(1469598103934665603);size_t i;
    for(i=0;i<length;i++){h^=p[i];h*=UINT64_C(1099511628211);}return h;
}
void prog_plan_begin(void){data_mode=1;piece_count=piece_cursor=0;}
static size_t data_piece(size_t offset,const void *bytes,size_t length,int alignment)
{
    DataPiece *p;
    uint64_t hash=data_hash(bytes,length);
    if(data_mode==2){
        if(piece_cursor>=piece_count)fatal("内部エラー: データの計画が不足しています");
        p=&pieces[piece_cursor++];
        if(p->length!=length||p->hash!=hash||p->alignment!=alignment)fatal("内部エラー: データの生成順が一致しません");
        return p->used?p->offset:0;
    }
    if(data_mode==1){
        if(piece_count==piece_capacity){
            piece_capacity=piece_capacity?piece_capacity*2:256;
            pieces=realloc(pieces,(size_t)piece_capacity*sizeof(*pieces));
            if(!pieces)fatal("定数データの計画を確保できません");
        }
        p=&pieces[piece_count++];*p=(DataPiece){offset,0,length,hash,alignment,0};
    }
    return offset;
}
void prog_plan_finish(const Program *plan,Program *output)
{
    int i,lo,hi,mid;
    for(i=0;i<plan->reloc_count;i++){
        const Reloc *r=&plan->relocs[i];
        if(r->kind!=RELOC_DATA||!x64_provider_needed(r->provider))continue;
        lo=0;hi=piece_count;
        while(lo<hi){mid=lo+(hi-lo)/2;if(pieces[mid].old_offset<=r->target)lo=mid+1;else hi=mid;}
        if(!lo||(pieces[lo-1].length&&r->target-pieces[lo-1].old_offset>=pieces[lo-1].length))fatal("内部エラー: 定数データの参照が不正です");
        pieces[lo-1].used=1;
    }
    for(i=0;i<piece_count;i++)if(pieces[i].used){
        DataPiece *p=&pieces[i];
        while(output->data.len%(size_t)p->alignment)buf_add_u8(&output->data,0);
        p->offset=output->data.len;buf_add(&output->data,plan->data.data+p->old_offset,p->length);
    }
    piece_cursor=0;data_mode=2;
}
void prog_plan_end(void)
{
    if(data_mode==2&&piece_cursor!=piece_count)fatal("内部エラー: 定数データの生成数が一致しません");
    data_mode=0;piece_cursor=piece_count=0;
}

void prog_init(Program *prog)
{
    buf_init(&prog->text);
    buf_init(&prog->data);
    prog->relocs = NULL;
    prog->reloc_count = 0;
    prog->reloc_cap = 0;
    prog->entry = 0;
}

void prog_free(Program *prog)
{
    buf_free(&prog->text);
    buf_free(&prog->data);
    free(prog->relocs);
    prog->relocs = NULL;
    prog->reloc_count = 0;
    prog->reloc_cap = 0;
}

/* データを追加し、data 内の位置を返す */
size_t prog_add_data(Program *prog, const char *bytes, size_t n)
{
    size_t offset;
    if(data_mode==2)return data_piece(0,bytes,n,1);

    offset = prog->data.len;
    buf_add(&prog->data, (const unsigned char *)bytes, n);
    return data_piece(offset,bytes,n,1);
}

void prog_add_reloc(Program *prog, RelocKind kind, size_t pos, size_t target)
{
    Reloc *new_relocs;
    int new_cap;
    if(!x64_output_enabled())return;

    if (prog->reloc_count >= prog->reloc_cap) {
        new_cap = prog->reloc_cap == 0 ? 16 : prog->reloc_cap * 2;
        new_relocs = realloc(prog->relocs, sizeof(Reloc) * (size_t)new_cap);
        if (new_relocs == NULL) {
            fatal("メモリを確保できません");
        }
        prog->relocs = new_relocs;
        prog->reloc_cap = new_cap;
    }
    prog->relocs[prog->reloc_count].kind = kind;
    prog->relocs[prog->reloc_count].pos = pos;
    prog->relocs[prog->reloc_count].target = target;
    prog->relocs[prog->reloc_count].provider=x64_current_provider();
    prog->reloc_count++;
}

/* str の値（[長さ 8バイト][本体]）をデータに置き、その先頭の位置を返す */
size_t prog_add_str(Program *prog, const char *bytes, size_t n)
{
    size_t offset;
    int i;
    if(data_mode==2){
        size_t result;Buffer value;buf_init(&value);buf_add_u64(&value,(uint64_t)n);buf_add(&value,(const unsigned char *)bytes,n);
        result=data_piece(0,value.data,value.len,8);buf_free(&value);return result;
    }

    while (prog->data.len % 8 != 0) {
        buf_add_u8(&prog->data, 0);
    }
    offset = prog->data.len;
    for (i = 0; i < 8; i++) {
        buf_add_u8(&prog->data, (uint8_t)((uint64_t)n >> (8 * i)));
    }
    if (n > 0) {
        buf_add(&prog->data, (const unsigned char *)bytes, n);
    }
    return data_piece(offset,prog->data.data+offset,n+8,8);
}
