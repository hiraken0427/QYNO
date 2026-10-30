/*
 * Windows x64 用の実行ファイル（PE32+ 形式）を書き出す
 *
 * ファイルの構成
 *   0x000  DOS ヘッダ（"MZ" と PE ヘッダの位置だけを持つ）
 *   0x040  PE シグネチャ、COFF ヘッダ、オプションヘッダ、セクションヘッダ
 *   0x200  .text   機械語（エントリ処理・実行時ルーチン・main）
 *   ....   .rdata  文字列などの読み取り専用データ
 *   ....   .idata  kernel32.dll / bcrypt.dll / ntdll.dll のインポート情報
 */

#include "qyc.h"

#include <stdlib.h>
#include <string.h>

#define IMAGE_BASE        0x140000000ULL
#define SECTION_ALIGNMENT 0x1000
#define FILE_ALIGNMENT    0x200
#define HEADERS_SIZE      0x200
#define SECTION_COUNT     3

#define PE_OFFSET         0x40
#define COFF_OFFSET       (PE_OFFSET + 4)
#define OPT_OFFSET        (COFF_OFFSET + 20)
#define OPT_SIZE          240
#define SECTION_OFFSET    (OPT_OFFSET + OPT_SIZE)

#define TEXT_RVA          0x1000

/* Import の順番と合わせる */
static const char *import_names[IMP_COUNT] = {
    "ExitProcess",
    "GetStdHandle",
    "WriteFile",
    "GetConsoleOutputCP",
    "SetConsoleOutputCP",
    "GetProcessHeap",
    "HeapAlloc",
    "ReadFile",
    "GetConsoleMode",
    "ReadConsoleW",
    "ReadConsoleInputW",
    "SetConsoleMode",
    "GetConsoleScreenBufferInfo",
    "SetConsoleCursorPosition",
    "HeapFree",
    "CreateFileW",
    "CloseHandle",
    "GetLastError",
    "GetFileType",
    "GetFileInformationByHandle",
    "MultiByteToWideChar",
    "SetFilePointerEx", "GetSystemTimePreciseAsFileTime",
    "QueryPerformanceCounter", "QueryPerformanceFrequency",
    "GetCurrentProcess", "GetProcessTimes", "Sleep",
    "GetCurrentDirectoryW", "SetCurrentDirectoryW", "WideCharToMultiByte",
    "GetCommandLineW", "CreateDirectoryW", "GetFileInformationByHandleEx", "SetFileInformationByHandle", "GetFileAttributesW", "BCryptGenRandom", "RtlNtStatusToDosError"
};

const char *import_name(Import imp)
{
    if ((int)imp < 0 || imp >= IMP_COUNT) {
        return "?";
    }
    return import_names[imp];
}

/* .idata 内の配置 */
typedef struct {
    uint32_t ilt;           /* インポートルックアップテーブル */
    uint32_t iat;           /* インポートアドレステーブル（ローダが関数アドレスを書き込む） */
    uint32_t hint_name[IMP_COUNT];
    uint32_t dll_name[3];
    uint32_t size;
} IdataLayout;

static uint32_t align_up(uint32_t v, uint32_t align)
{
    return (v + align - 1) / align * align;
}

static void put_u16(unsigned char *p, uint16_t v)
{
    p[0] = (unsigned char)v;
    p[1] = (unsigned char)(v >> 8);
}

static void put_u32(unsigned char *p, uint32_t v)
{
    int i;

    for (i = 0; i < 4; i++) {
        p[i] = (unsigned char)(v >> (8 * i));
    }
}

static void put_u64(unsigned char *p, uint64_t v)
{
    int i;

    for (i = 0; i < 8; i++) {
        p[i] = (unsigned char)(v >> (8 * i));
    }
}

static void put_section(unsigned char *p, const char *name, uint32_t virtual_size, uint32_t rva,
                        uint32_t raw_size, uint32_t raw_offset, uint32_t characteristics)
{
    memcpy(p, name, strlen(name));
    put_u32(p + 8, virtual_size);
    put_u32(p + 12, rva);
    put_u32(p + 16, raw_size);
    put_u32(p + 20, raw_offset);
    put_u32(p + 36, characteristics);
}

/*
 * .idata の配置を決める
 *   0                    3 DLLの記述子＋終端（20バイト×4）
 *   ilt                  (IMP_COUNT + 3) × 8 バイト（各DLLの終端を含む）
 *   iat                  (IMP_COUNT + 3) × 8 バイト
 *   hint_name[i]         ヒント（2バイト）＋関数名＋ '\0'（偶数バイトにそろえる）
 *   dll_name             KERNEL32.dll / bcrypt.dll / ntdll.dll
 */
static int import_slot(int index)
{ return index+(index>=IMP_BCRYPT_GEN_RANDOM)+(index>=IMP_RTL_NTSTATUS_TO_DOS_ERROR); }
static const char *import_libraries[]={"KERNEL32.dll","bcrypt.dll","ntdll.dll"};
static IdataLayout layout_idata(void)
{
    IdataLayout l;
    uint32_t pos;
    int i;

    pos = 80;
    l.ilt = pos;
    pos += (IMP_COUNT + 3) * 8;
    l.iat = pos;
    pos += (IMP_COUNT + 3) * 8;
    for (i = 0; i < IMP_COUNT; i++) {
        l.hint_name[i] = pos;
        pos += align_up(2 + (uint32_t)strlen(import_names[i]) + 1, 2);
    }
    for(i=0;i<3;i++){l.dll_name[i]=pos;pos+=(uint32_t)strlen(import_libraries[i])+1;}
    l.size = pos;
    return l;
}

static void write_idata(unsigned char *idata, uint32_t idata_rva, const IdataLayout *l)
{
    int i;

    int starts[]={0,IMP_BCRYPT_GEN_RANDOM,IMP_RTL_NTSTATUS_TO_DOS_ERROR};
    for(i=0;i<3;i++){
        uint32_t slot=(uint32_t)import_slot(starts[i])*8;
        put_u32(idata+i*20,idata_rva+l->ilt+slot);
        put_u32(idata+i*20+12,idata_rva+l->dll_name[i]);
        put_u32(idata+i*20+16,idata_rva+l->iat+slot);
        memcpy(idata+l->dll_name[i],import_libraries[i],strlen(import_libraries[i])+1);
    }
    for(i=0;i<IMP_COUNT;i++){
        put_u64(idata+l->ilt+8*(uint32_t)import_slot(i),idata_rva+l->hint_name[i]);
        put_u64(idata+l->iat+8*(uint32_t)import_slot(i),idata_rva+l->hint_name[i]);
        memcpy(idata+l->hint_name[i]+2,import_names[i],strlen(import_names[i])+1);
    }
}

/* rip 相対の参照を埋める（disp32 = 参照先 - 次の命令の位置） */
static void apply_relocs(unsigned char *text, const Program *prog,
                         uint32_t rdata_rva, uint32_t idata_rva, const IdataLayout *l)
{
    int i;
    const Reloc *r;
    uint32_t target;
    uint32_t next;

    for (i = 0; i < prog->reloc_count; i++) {
        r = &prog->relocs[i];
        if (r->kind == RELOC_DATA) {
            target = rdata_rva + (uint32_t)r->target;
        } else {
            target = idata_rva + l->iat + 8 * (uint32_t)import_slot((int)r->target);
        }
        next = TEXT_RVA + (uint32_t)r->pos + 4;
        put_u32(text + r->pos, target - next);
    }
}

void write_pe(const char *path, const Program *prog)
{
    IdataLayout idl;
    uint32_t text_size;
    uint32_t text_raw_size;
    uint32_t rdata_size;
    uint32_t rdata_rva;
    uint32_t rdata_raw_size;
    uint32_t rdata_offset;
    uint32_t idata_rva;
    uint32_t idata_raw_size;
    uint32_t idata_offset;
    uint32_t image_size;
    uint32_t file_size;
    unsigned char *img;
    unsigned char *opt;
    FILE *fp;

    idl = layout_idata();

    text_size = (uint32_t)prog->text.len;
    text_raw_size = align_up(text_size, FILE_ALIGNMENT);
    rdata_size = prog->data.len > 0 ? (uint32_t)prog->data.len : 1;
    rdata_rva = TEXT_RVA + align_up(text_size, SECTION_ALIGNMENT);
    rdata_raw_size = align_up(rdata_size, FILE_ALIGNMENT);
    rdata_offset = HEADERS_SIZE + text_raw_size;
    idata_rva = rdata_rva + align_up(rdata_size, SECTION_ALIGNMENT);
    idata_raw_size = align_up(idl.size, FILE_ALIGNMENT);
    idata_offset = rdata_offset + rdata_raw_size;
    image_size = idata_rva + align_up(idl.size, SECTION_ALIGNMENT);
    file_size = idata_offset + idata_raw_size;

    img = calloc(1, file_size);
    if (img == NULL) {
        fatal("メモリを確保できません");
    }

    /* DOS ヘッダ */
    img[0] = 'M';
    img[1] = 'Z';
    put_u32(img + 0x3C, PE_OFFSET);

    /* PE シグネチャ */
    memcpy(img + PE_OFFSET, "PE\0\0", 4);

    /* COFF ヘッダ */
    put_u16(img + COFF_OFFSET, 0x8664);             /* Machine: x64 */
    put_u16(img + COFF_OFFSET + 2, SECTION_COUNT);  /* NumberOfSections */
    put_u16(img + COFF_OFFSET + 16, OPT_SIZE);      /* SizeOfOptionalHeader */
    put_u16(img + COFF_OFFSET + 18, 0x0022);        /* EXECUTABLE_IMAGE | LARGE_ADDRESS_AWARE */

    /* オプションヘッダ（PE32+） */
    opt = img + OPT_OFFSET;
    put_u16(opt + 0, 0x020B);                       /* Magic: PE32+ */
    put_u32(opt + 4, text_raw_size);                /* SizeOfCode */
    put_u32(opt + 8, rdata_raw_size + idata_raw_size);  /* SizeOfInitializedData */
    put_u32(opt + 16, TEXT_RVA + (uint32_t)prog->entry);    /* AddressOfEntryPoint */
    put_u32(opt + 20, TEXT_RVA);                    /* BaseOfCode */
    put_u64(opt + 24, IMAGE_BASE);                  /* ImageBase */
    put_u32(opt + 32, SECTION_ALIGNMENT);
    put_u32(opt + 36, FILE_ALIGNMENT);
    put_u16(opt + 40, 6);                           /* MajorOperatingSystemVersion */
    put_u16(opt + 48, 6);                           /* MajorSubsystemVersion */
    put_u32(opt + 56, image_size);                  /* SizeOfImage */
    put_u32(opt + 60, HEADERS_SIZE);                /* SizeOfHeaders */
    put_u16(opt + 68, 3);                           /* Subsystem: コンソール */
    put_u16(opt + 70, 0x8160);                      /* HIGH_ENTROPY_VA | DYNAMIC_BASE | NX_COMPAT | TERMINAL_SERVER_AWARE */
    put_u64(opt + 72, 0x100000);                    /* SizeOfStackReserve */
    put_u64(opt + 80, 0x1000);                      /* SizeOfStackCommit */
    put_u64(opt + 88, 0x100000);                    /* SizeOfHeapReserve */
    put_u64(opt + 96, 0x1000);                      /* SizeOfHeapCommit */
    put_u32(opt + 108, 16);                         /* NumberOfRvaAndSizes */
    put_u32(opt + 112 + 8 * 1, idata_rva);          /* インポートディレクトリ */
    put_u32(opt + 112 + 8 * 1 + 4, 80);
    put_u32(opt + 112 + 8 * 12, idata_rva + idl.iat);   /* IAT */
    put_u32(opt + 112 + 8 * 12 + 4, (IMP_COUNT + 3) * 8);

    /* セクションヘッダ */
    put_section(img + SECTION_OFFSET, ".text", text_size, TEXT_RVA,
                text_raw_size, HEADERS_SIZE, 0x60000020);       /* CODE | EXECUTE | READ */
    put_section(img + SECTION_OFFSET + 40, ".rdata", rdata_size, rdata_rva,
                rdata_raw_size, rdata_offset, 0x40000040);      /* INITIALIZED_DATA | READ */
    put_section(img + SECTION_OFFSET + 80, ".idata", idl.size, idata_rva,
                idata_raw_size, idata_offset, 0xC0000040);      /* INITIALIZED_DATA | READ | WRITE */

    /* 各セクションの中身 */
    memcpy(img + HEADERS_SIZE, prog->text.data, prog->text.len);
    if (prog->data.len > 0) {
        memcpy(img + rdata_offset, prog->data.data, prog->data.len);
    }
    write_idata(img + idata_offset, idata_rva, &idl);
    apply_relocs(img + HEADERS_SIZE, prog, rdata_rva, idata_rva, &idl);

    fp = fopen(path, "wb");
    if (fp == NULL) {
        fatal("%s に書き込めません（実行中の場合は終了してください）", path);
    }
    if (fwrite(img, 1, file_size, fp) != file_size) {
        fclose(fp);
        fatal("%s の書き込みに失敗しました", path);
    }
    if (fclose(fp) != 0) {
        fatal("%s の書き込みに失敗しました", path);
    }
    free(img);
}
