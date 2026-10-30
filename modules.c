/* ファイルごとの解析、import グラフ、字句スコープ付きの名前解決。 */
#ifndef _WIN32
#define _XOPEN_SOURCE 700
#endif
#include "qyc.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "standard_time_source.h"

typedef enum { SYM_NONE, SYM_ENUM, SYM_MODEL, SYM_INTERFACE, SYM_MODULE, SYM_FUNCTION, SYM_CONSTANT, SYM_BUILTIN } SymbolKind;
typedef struct {
    SymbolKind kind;
    EnumDef *enumeration;
    Model *model;
    Interface *contract;
    Module *module;
    Function *function;
    Var *constant;
    int builtin;
} Symbol;

static Module *modules;
static const char **search_roots;
static int search_root_count;
static int module_count;
static Module *load_stack[256];
static Token *load_sites[256];
static int load_depth;

static void *allocate(size_t size)
{
    void *p = calloc(1, size);
    if (p == NULL) fatal("モジュールの領域を確保できません");
    return p;
}

static char *copy_name(const char *s, size_t length)
{
    char *copy = allocate(length + 1);
    memcpy(copy, s, length);
    return copy;
}

static int token_matches(Token *t, const char *name)
{
    return (size_t)t->len == strlen(name) && memcmp(t->loc, name, (size_t)t->len) == 0;
}

static int name_matches(const char *a, int length, const char *b)
{
    return (size_t)length == strlen(b) && memcmp(a, b, (size_t)length) == 0;
}

static char *canonical_path(const char *path)
{
#ifdef _WIN32
    struct stat st;
    if (stat(path, &st) != 0) return NULL;
    return _fullpath(NULL, path, 0);
#else
    return realpath(path, NULL);
#endif
}

static char *join_path(const char *root, const char *name)
{
    size_t a = strlen(root);
    size_t b = strlen(name);
    char *path = allocate(a + b + 5);
    size_t i;
    memcpy(path, root, a);
    path[a++] = '/';
    for (i = 0; i < b; i++) path[a + i] = name[i] == '.' ? '/' : name[i];
    memcpy(path + a + b, ".qy", 4);
    return path;
}

static SourceFile *read_source(const char *filename, char *canonical, Module *module)
{
    SourceFile *source = allocate(sizeof(*source));
    FILE *file = fopen(filename, "rb");
    long size;
    if (file == NULL) fatal("%s を開けません: %s", filename, strerror(errno));
    if (fseek(file, 0, SEEK_END) != 0 || (size = ftell(file)) < 0 || fseek(file, 0, SEEK_SET) != 0) {
        fatal("%s のサイズを取得できません", filename);
    }
    source->text = allocate((size_t)size + 1);
    if (fread(source->text, 1, (size_t)size, file) != (size_t)size) fatal("%s を読み込めません", filename);
    fclose(file);
    source->filename = copy_name(filename, strlen(filename));
    source->canonical_path = canonical;
    source->module = module;
    source->input = source->text;
    if (size >= 3 && memcmp(source->text, "\xef\xbb\xbf", 3) == 0) source->input += 3;
    return source;
}

static void import_trace(void)
{
    int i;
    fprintf(stderr, "import 経路: ");
    for (i = 0; i < load_depth; i++) {
        if (i != 0) fprintf(stderr, " -> ");
        fprintf(stderr, "%s", load_stack[i]->name[0] ? load_stack[i]->name : "<entry>");
    }
    fprintf(stderr, "\n");
    for (i = 1; i < load_depth; i++) {
        Token *site = load_sites[i];
        fprintf(stderr, "  %s:%d:%d: import %s\n", site->source->filename, site->line, site->col, load_stack[i]->name);
    }
}

static Module *standard_module(const char *name)
{
    Module *module;
    for (module = modules; module != NULL; module = module->next) {
        if (module->builtin && strcmp(module->name, name) == 0) return module;
    }
    return NULL;
}

static void standard_source(const char *name,const char *declarations)
{
    Module *mod=standard_module(name);
    mod->source=allocate(sizeof(*mod->source));mod->source->module=mod;
    mod->source->canonical_path=copy_name(name,strlen(name));
    mod->source->filename=mod->source->canonical_path;
    mod->source->text=copy_name(declarations,strlen(declarations));
    mod->source->input=mod->source->text;
    select_source(mod->source);mod->ast=parse(tokenize(mod->source->input));
}

static Module *load_module(const char *name, const char *filename, char *canonical, Token *site)
{
    Module *module;
    ModuleImport *item;
    int i;
    module = standard_module(name);
    if (module != NULL) return module;
    for (module = modules; module != NULL; module = module->next) {
        size_t length = strlen(module->name);
        if (module->builtin && strncmp(name, module->name, length) == 0 && name[length] == '.') {
            error_tok(site, "標準モジュール %s に %s はありません", module->name, name + length + 1);
        }
    }
    for (module = modules; module != NULL; module = module->next) {
        if (strcmp(module->name, name) == 0) {
            if (module->state == 1) {
                import_trace();
                error_tok(site, "循環 import: %s を読み込み中です", name);
            }
            return module;
        }
    }
    if (filename == NULL) {
        for (i = 0; i < search_root_count; i++) {
            char *candidate = join_path(search_roots[i], name);
            canonical = canonical_path(candidate);
            if (canonical != NULL) {
                filename = candidate;
                break;
            }
            if (errno != ENOENT && errno != ENOTDIR) {
                error_tok(site, "モジュール %s を探索できません: %s (%s)", name, candidate, strerror(errno));
            }
            free(candidate);
        }
        if (filename == NULL) {
            import_trace();
            for (i = 0; i < search_root_count; i++) fprintf(stderr, "探索ルート: %s\n", search_roots[i]);
            error_tok(site, "モジュール %s が見つかりません（探索ルートを -I で追加できます）", name);
        }
    }
    for (module = modules; module != NULL; module = module->next) {
        if (module->source != NULL && strcmp(module->source->canonical_path, canonical) == 0) {
            if (module->state == 1) {
                import_trace();
                error_tok(site, "循環 import: %s は読み込み中の %s と同じファイルです", name, module->source->filename);
            }
            error_tok(site, "モジュール %s と %s は同じファイルを別の名前で参照しています", name, module->name);
        }
    }
    if (load_depth == (int)(sizeof(load_stack) / sizeof(load_stack[0]))) error_tok(site, "import の依存が深すぎます");
    module = allocate(sizeof(*module));
    module->name = copy_name(name, strlen(name));
    module->id = module_count++;
    module->state = 1;
    module->next = modules;
    modules = module;
    module->source = read_source(filename, canonical, module);
    select_source(module->source);
    module->ast = parse(tokenize(module->source->input));
    load_stack[load_depth] = module;
    load_sites[load_depth++] = site;
    for (item = module->ast->imports; item != NULL; item = item->next) {
        item->module = load_module(item->name, NULL, NULL, item->tok);
    }
    load_depth--;
    module->state = 2;
    return module;
}

Ast *load_program(const char *path, const char **roots, int root_count)
{
    Ast *program = allocate(sizeof(*program));
    char *canonical = canonical_path(path);
    char *directory;
    char *last;
    Module *module;
    Function **function_tail = &program->funcs;
    Node **global_tail = &program->globals;
    Var **variable_tail = &program->global_vars;
    Model **model_tail = &program->models;
    EnumDef **enum_tail = &program->enums;
    Interface **interface_tail = &program->interfaces;
    int i;
    if (canonical == NULL) fatal("%s を開けません: %s", path, strerror(errno));
    directory = copy_name(canonical, strlen(canonical));
#ifdef _WIN32
    for (last = directory; *last; last++) if (*last == '\\') *last = '/';
#endif
    last = strrchr(directory, '/');
    if (last == directory) last[1] = '\0';
    else if (last != NULL) *last = '\0';
    search_roots = allocate(((size_t)root_count + 1) * sizeof(*search_roots));
    search_roots[0] = directory;
    search_root_count = root_count + 1;
    for (i = 0; i < root_count; i++) {
        struct stat st;
        char *root = canonical_path(roots[i]);
        if (root == NULL || stat(root, &st) != 0 || !S_ISDIR(st.st_mode)) fatal("探索ルートがディレクトリではありません: %s", roots[i]);
        search_roots[i + 1] = root;
    }
    modules = NULL;
    module_count = 0;
    load_depth = 0;
    for (i = 0; i < BI_COUNT; i++) {
        const char *name = builtin_namespace((Builtin)i);
        if (name != NULL && standard_module(name) == NULL) {
            Module *standard = allocate(sizeof(*standard));
            standard->name = copy_name(name, strlen(name));
            standard->builtin = 1;
            standard->state = 2;
            standard->id = module_count++;
            standard->next = modules;
            modules = standard;
        }
    }
    {
        Module *io = standard_module("io");
        static const char declarations[] =
            "enum ErrorCode;\n\tno_error\n\tnot_found\n\tpermission_denied\n\talready_exists\n"
            "\tinvalid_path\n\tnot_regular_file\n\ttoo_many_open_files\n\tno_space\n"
            "\tread_only\n\tbroken_pipe\n\tresource_busy\n\tio_error\n\tunsupported_operation\n\tout_of_range\n\tsystem_error\n\tnot_directory\n\tdirectory_not_empty\n"
            "model Error;\n\tErrorCode code\n\tstr message\n\tstr operation\n\tstr path\n"
            "\tint_64 os_code\n\tstr source\n\tint line\n\tint column\n";
        io->source = allocate(sizeof(*io->source));
        io->source->module = io; io->source->filename = "<standard:io>";
        io->source->canonical_path = "<standard:io>";
        io->source->input = io->source->text = copy_name(declarations, strlen(declarations));
        select_source(io->source);
        io->ast = parse(tokenize(io->source->input));
    }
    standard_source("io.file","enum SeekOrigin;\n\tbegin\n\tcurrent\n\tend\n");
    standard_source("time",standard_time_source);
    standard_source("lib.random","model Random;\n\tprivate unsigned int_64 a\n\tprivate unsigned int_64 b\n\tprivate unsigned int_64 c\n\tprivate unsigned int_64 d\n\tvoid init(unsigned int_64 seed=0);\n\t\tlib.random._seed(ref this,seed)\n");
    program->entry = load_module("", path, canonical, NULL);
    program->modules = modules;
    /* 所有モジュールは Token.source に残す。リストだけを出力単位へ連結する。 */
    for (module = modules; module != NULL; module = module->next) {
        if (!module->ast) continue;
        *enum_tail=module->ast->enums;while(*enum_tail)enum_tail=&(*enum_tail)->next;
        *interface_tail = module->ast->interfaces;
        while (*interface_tail) interface_tail = &(*interface_tail)->next;
        *model_tail = module->ast->models;
        while (*model_tail) model_tail = &(*model_tail)->next;
        *function_tail = module->ast->funcs;
        while (*function_tail != NULL) function_tail = &(*function_tail)->next;
        *global_tail = module->ast->globals;
        while (*global_tail != NULL) global_tail = &(*global_tail)->next;
        *variable_tail = module->ast->global_vars;
        while (*variable_tail != NULL) variable_tail = &(*variable_tail)->next;
    }
    return program;
}

static Symbol namespace_symbol(Module *module)
{
    Symbol symbol = {0};
    symbol.kind = SYM_MODULE;
    symbol.module = module;
    return symbol;
}

static Symbol exported_symbol(Module *module, const char *name)
{
    Symbol symbol = {0};
    Function *function;
    Var *constant;
    if (module->builtin) {
        char *qualified = malloc(strlen(module->name) + strlen(name) + 2);
        const char *dot = strchr(name, '.');
        Module *child;
        sprintf(qualified, "%s.%s", module->name, name);
        child = standard_module(qualified);
        free(qualified);
        if (child != NULL) return namespace_symbol(child);
        if (dot != NULL) {
            char *prefix = copy_name(name, (size_t)(dot - name));
            Symbol parent = exported_symbol(module, prefix);
            free(prefix);
            if (parent.kind == SYM_MODULE) return exported_symbol(parent.module, dot + 1);
            if (parent.kind == SYM_ENUM) {
                EnumMember *m = enum_find(parent.enumeration, dot + 1);
                if (m) { symbol.kind = SYM_CONSTANT; symbol.constant = m->symbol; }
            }
            return symbol;
        }
        Token ns = {0};
        Token member = {0};
        int builtin;
        ns.loc = module->name;
        ns.len = (int)strlen(module->name);
        member.loc = name;
        member.len = (int)strlen(name);
        builtin = builtin_find(&ns, &member);
        if (builtin >= 0) {
            symbol.kind = SYM_BUILTIN;
            symbol.builtin = builtin;
        }
        if (symbol.kind != SYM_NONE || !module->ast) return symbol;
    }
    { EnumDef *e;
      for(e=module->ast->enums;e;e=e->next)if(e->tok->source->module==module){
          size_t n=strlen(e->name);
          if(!strcmp(e->name,name)){symbol.kind=SYM_ENUM;symbol.enumeration=e;return symbol;}
          if(!strncmp(e->name,name,n)&&name[n]=='.'){
              EnumMember *m=enum_find(e,name+n+1);
              if(m){symbol.kind=SYM_CONSTANT;symbol.constant=m->symbol;return symbol;}
          }
      }
    }
    if (strchr(name, '.') != NULL) return symbol;
    { Interface *in;
      for (in = module->ast->interfaces; in; in = in->next)
          if (in->tok->source->module == module && strcmp(in->name, name) == 0) {
              symbol.kind = SYM_INTERFACE; symbol.contract = in; return symbol;
          }
    }
    { Model *m;
      for (m = module->ast->models; m; m = m->next)
          if (m->tok->source->module == module && strcmp(m->name, name) == 0) {
              symbol.kind = SYM_MODEL; symbol.model = m; return symbol;
          }
    }
    for (function = module->ast->funcs; function != NULL; function = function->next) {
        if (!function->model && function->tok->source->module == module && name_matches(function->name, function->name_len, name)) {
            symbol.kind = SYM_FUNCTION;
            symbol.function = function;
            return symbol;
        }
    }
    for (constant = module->ast->global_vars; constant != NULL; constant = constant->next) {
        if (constant->tok->source->module == module && name_matches(constant->name, constant->name_len, name)) {
            symbol.kind = SYM_CONSTANT;
            symbol.constant = constant;
            return symbol;
        }
    }
    return symbol;
}

static Symbol direct_symbol(Module *owner, const char *name)
{
    Symbol symbol = {0};
    ModuleImport *item;
    Module *standard;
    size_t length;
    standard = standard_module(name);
    if (standard != NULL) return namespace_symbol(standard);
    for (standard = modules; standard != NULL; standard = standard->next) {
        if (!standard->builtin) continue;
        length = strlen(standard->name);
        if (strncmp(name, standard->name, length) == 0 && name[length] == '.') {
            symbol = exported_symbol(standard, name + length + 1);
            if (symbol.kind != SYM_NONE) return symbol;
        }
    }
    {
        symbol = exported_symbol(owner, name);
        if (symbol.kind != SYM_NONE) return symbol;
    }
    for (item = owner->ast->imports; item != NULL; item = item->next) {
        length = strlen(item->name);
        if (strcmp(name, item->name) == 0) return namespace_symbol(item->module);
        if (strncmp(name, item->name, length) == 0 && name[length] == '.') {
            symbol = exported_symbol(item->module, name + length + 1);
            if (symbol.kind != SYM_NONE) return symbol;
        }
    }
    return symbol;
}

static int same_symbol(Symbol a, Symbol b)
{
    if (a.kind != b.kind) return 0;
    switch (a.kind) {
    case SYM_ENUM: return a.enumeration == b.enumeration;
    case SYM_MODEL: return a.model == b.model;
    case SYM_INTERFACE: return a.contract == b.contract;
    case SYM_MODULE: return a.module == b.module;
    case SYM_FUNCTION: return a.function == b.function;
    case SYM_CONSTANT: return a.constant == b.constant;
    case SYM_BUILTIN: return a.builtin == b.builtin;
    default: return 1;
    }
}

static void resolve_using(Using *u);
static int symbol_accessible(Symbol symbol,Module *owner);

static Symbol using_symbol(Using *u, const char *name)
{
    Symbol symbol = {0};
    const char *last;
    size_t length;
    if (u->alias != NULL) {
        length = (size_t)u->alias->len;
        if (strncmp(name, u->alias->loc, length) != 0 || (name[length] != '\0' && name[length] != '.')) return symbol;
        resolve_using(u);
        if (name[length] == '\0') return namespace_symbol(u->target_module);
        return exported_symbol(u->target_module, name + length + 1);
    }
    resolve_using(u);
    if (u->target_module != NULL) {
        Symbol candidate=exported_symbol(u->target_module,name);
        return symbol_accessible(candidate,u->tok->source->module)?candidate:symbol;
    }
    if(u->target_enum&&strchr(name,'.')){
        size_t n=strlen(u->target_enum->name);
        const char *short_name=strrchr(u->path,'.');short_name=short_name?short_name+1:u->path;n=strlen(short_name);
        if(!strncmp(name,short_name,n)&&name[n]=='.'){
            EnumMember *m=enum_find(u->target_enum,name+n+1);
            if(m){symbol.kind=SYM_CONSTANT;symbol.constant=m->symbol;}
        }
        return symbol;
    }
    if (strchr(name, '.') != NULL) return symbol;
    last = strrchr(u->path, '.');
    last = last != NULL ? last + 1 : u->path;
    if (strcmp(last, name) != 0) return symbol;
    if(u->target_enum){symbol.kind=SYM_ENUM;symbol.enumeration=u->target_enum;}
    else if (u->target_model != NULL) {
        symbol.kind = SYM_MODEL; symbol.model = u->target_model;
    } else if (u->target_interface != NULL) {
        symbol.kind = SYM_INTERFACE; symbol.contract = u->target_interface;
    } else if (u->target_function != NULL) {
        symbol.kind = SYM_FUNCTION;
        symbol.function = u->target_function;
    } else if (u->target_constant != NULL) {
        symbol.kind = SYM_CONSTANT;
        symbol.constant = u->target_constant;
    } else {
        symbol.kind = SYM_BUILTIN;
        symbol.builtin = u->target_builtin;
    }
    return symbol;
}

static void merge_symbol(Symbol *result, Symbol candidate, Token *site, const char *name)
{
    if (candidate.kind == SYM_NONE) return;
    if (result->kind != SYM_NONE && !same_symbol(*result, candidate)) error_tok(site, "using による名前 %s が衝突しています。修飾名を使ってください", name);
    *result = candidate;
}

static int symbol_accessible(Symbol symbol,Module *owner)
{
    Token *declaration=NULL;int private=0;
    switch(symbol.kind){
    case SYM_ENUM:private=symbol.enumeration->is_private;declaration=symbol.enumeration->tok;break;
    case SYM_MODEL:private=symbol.model->is_private;declaration=symbol.model->tok;break;
    case SYM_INTERFACE:private=symbol.contract->is_private;declaration=symbol.contract->tok;break;
    case SYM_FUNCTION:private=symbol.function->is_private;declaration=symbol.function->tok;break;
    case SYM_CONSTANT:private=symbol.constant->enum_member?symbol.constant->enum_member->owner->is_private:symbol.constant->is_private;declaration=symbol.constant->tok;break;
    default:break;
    }
    return !private || declaration->source->module==owner;
}
static void require_symbol_access(Symbol symbol,Module *owner,Token *site,const char *name)
{
    if(!symbol_accessible(symbol,owner))error_tok(site,"private の定義 %s は宣言したモジュールの外から参照できません",name);
}

static Symbol scoped_symbol(Module *owner, const char *name, Using *scope, Using *skip, Token *site)
{
    Symbol result = direct_symbol(owner, name);
    Using *u;
    /* 修飾された直接参照は using を経由しない。衝突は宣言の検証で報告する。 */
    if (result.kind != SYM_NONE) { require_symbol_access(result,owner,site,name);return result; }
    for (u = scope; u != NULL; u = u->vnext) {
        /* using の参照先は直接名か名前空間の別名から解決する。
         * 無関係な選択 using を再帰的に展開しない。 */
        if (u != skip && (skip == NULL || u->alias != NULL)) {
            Symbol candidate=using_symbol(u,name);
            if(u->alias)require_symbol_access(candidate,owner,site,name);
            if(symbol_accessible(candidate,owner))merge_symbol(&result,candidate,site,name);
        }
    }
    for (u = owner->ast->usings; u != NULL; u = u->next) {
        if (u != skip && (skip == NULL || u->alias != NULL)) {
            Symbol candidate=using_symbol(u,name);
            if(u->alias)require_symbol_access(candidate,owner,site,name);
            if(symbol_accessible(candidate,owner))merge_symbol(&result,candidate,site,name);
        }
    }
    return result;
}

static void resolve_using(Using *u)
{
    Symbol target;
    if (u->state == 2) return;
    if (u->state == 1) error_tok(u->path_tok, "using の参照が循環しています: %s", u->path);
    u->state = 1;
    target = scoped_symbol(u->tok->source->module, u->path, u->scope, u, u->path_tok);
    if (target.kind == SYM_NONE) error_tok(u->path_tok, "using の対象 %s はありません。必要な import と名前を確認してください", u->path);
    if (u->alias != NULL && target.kind != SYM_MODULE) error_tok(u->alias, "as の別名はモジュールにだけ指定できます: %s", u->path);
    u->target_enum = target.enumeration;
    u->target_model = target.model;
    u->target_interface = target.contract;
    u->target_module = target.module;
    u->target_function = target.function;
    u->target_constant = target.constant;
    if (target.kind == SYM_BUILTIN) u->target_builtin = target.builtin;
    u->state = 2;
}

static int import_root(Module *owner, const char *name)
{
    ModuleImport *item;
    Module *standard;
    for (standard = modules; standard != NULL; standard = standard->next) {
        const char *dot;
        size_t length;
        if (!standard->builtin) continue;
        dot = strchr(standard->name, '.');
        length = dot != NULL ? (size_t)(dot - standard->name) : strlen(standard->name);
        if (strlen(name) == length && memcmp(standard->name, name, length) == 0) return 1;
    }
    for (item = owner->ast->imports; item != NULL; item = item->next) {
        const char *dot = strchr(item->name, '.');
        size_t length = dot != NULL ? (size_t)(dot - item->name) : strlen(item->name);
        if (strlen(name) == length && memcmp(item->name, name, length) == 0) return 1;
    }
    return 0;
}

static void introduced_name(Using *u, const char *name, Symbol symbol)
{
    Module *owner = u->tok->source->module;
    if(symbol.kind==SYM_BUILTIN&&builtin_is_random(symbol.builtin)&&random_operation(symbol.builtin)!=0)error_tok(u->path_tok,"乱数の型名APIはモジュール別名を使ってください（using lib.random as random）");
    Symbol direct = direct_symbol(owner, name);
    Using *other;
    Var *local;
    if ((direct.kind != SYM_NONE && !same_symbol(direct, symbol)) ||
        (import_root(owner, name) && direct.kind == SYM_NONE)) {
        error_tok(u->path_tok, "using %s が導入する %s は、既存の名前と衝突しています", u->path, name);
    }
    for (local = u->locals; local != NULL; local = local->vnext) {
        if (name_matches(local->name, local->name_len, name)) error_tok(u->path_tok, "using %s の名前 %s は、%s:%d の変数と衝突しています", u->path, name, local->tok->source->filename, local->tok->line);
    }
    for (other = u->scope; other != NULL; other = other->vnext) {
        Symbol candidate = using_symbol(other, name);
        if (candidate.kind != SYM_NONE && !same_symbol(candidate, symbol)) {
            error_tok(u->path_tok, "using %s の名前 %s は、using %s (%s:%d) と衝突しています", u->path, name, other->path, other->tok->source->filename, other->tok->line);
        }
    }
    for (other = owner->ast->usings; other != NULL; other = other->next) {
        Symbol candidate;
        if (other == u) continue;
        candidate = using_symbol(other, name);
        if (candidate.kind != SYM_NONE && !same_symbol(candidate, symbol)) {
            error_tok(u->path_tok, "using %s の名前 %s は、using %s (%s:%d) と衝突しています", u->path, name, other->path, other->tok->source->filename, other->tok->line);
        }
    }
}

static void validate_using(Using *u)
{
    Module *target = u->target_module;
    Symbol symbol = {0};
    Function *fn;
    Var *v;
    if (u->alias != NULL) {
        char *name = copy_name(u->alias->loc, (size_t)u->alias->len);
        introduced_name(u, name, namespace_symbol(target));
        free(name);
    } else if (target == NULL) {
        const char *name = strrchr(u->path, '.');
        name = name != NULL ? name + 1 : u->path;
        introduced_name(u, name, using_symbol(u, name));
    } else if (target->builtin) {
        if (target->ast) {
            EnumDef *e; Model *m; Var *v;
            for(v=target->ast->global_vars;v;v=v->next)if(v->tok->source->module==target&&!v->is_private){symbol.kind=SYM_CONSTANT;symbol.constant=v;{char *name=copy_name(v->name,(size_t)v->name_len);introduced_name(u,name,symbol);free(name);}}
            for (e=target->ast->enums;e;e=e->next) if(e->tok->source->module==target) {
                symbol.kind=SYM_ENUM; symbol.enumeration=e; introduced_name(u,e->name,symbol);
            }
            for (m=target->ast->models;m;m=m->next) if(m->tok->source->module==target) {
                symbol.kind=SYM_MODEL; symbol.model=m; introduced_name(u,m->name,symbol);
            }
        }
        Module *child;
        size_t n = strlen(target->name);
        int b;
        for (child = modules; child != NULL; child = child->next) {
            if (child->builtin && strncmp(child->name, target->name, n) == 0 && child->name[n] == '.' && strchr(child->name + n + 1, '.') == NULL)
                introduced_name(u, child->name + n + 1, namespace_symbol(child));
        }
        for (b = 0; b < BI_COUNT; b++) {
            const char *ns = builtin_namespace((Builtin)b);
            if (ns == NULL || strcmp(ns, target->name) != 0) continue;
            symbol.kind = SYM_BUILTIN;
            symbol.builtin = b;
            introduced_name(u, builtin_name((Builtin)b), symbol);
        }
    } else {
        Model *m; Interface *in;EnumDef *e;
        for(e=target->ast->enums;e;e=e->next)if(e->tok->source->module==target&&(!e->is_private||target==u->tok->source->module)){symbol.kind=SYM_ENUM;symbol.enumeration=e;introduced_name(u,e->name,symbol);}
        for (in = target->ast->interfaces; in; in = in->next) if (in->tok->source->module == target && (!in->is_private || target==u->tok->source->module)) {
            symbol.kind = SYM_INTERFACE; symbol.contract = in; introduced_name(u, in->name, symbol);
        }
        for (m = target->ast->models; m; m = m->next) if (m->tok->source->module == target && (!m->is_private || target==u->tok->source->module)) {
            symbol.kind = SYM_MODEL; symbol.model = m; introduced_name(u, m->name, symbol);
        }
        for (fn = target->ast->funcs; fn != NULL; fn = fn->next) {
            if (fn->model || fn->tok->source->module != target || (fn->is_private && target!=u->tok->source->module)) continue;
            symbol.kind = SYM_FUNCTION;
            symbol.function = fn;
            {
                char *name = copy_name(fn->name, (size_t)fn->name_len);
                introduced_name(u, name, symbol);
                free(name);
            }
        }
        for (v = target->ast->global_vars; v != NULL; v = v->next) {
            if (v->tok->source->module != target || (v->is_private && target!=u->tok->source->module)) continue;
            symbol.kind = SYM_CONSTANT;
            symbol.constant = v;
            {
                char *name = copy_name(v->name, (size_t)v->name_len);
                introduced_name(u, name, symbol);
                free(name);
            }
        }
    }
}

static void validate_declaration(Token *site, Using *scope)
{
    Module *owner = site->source->module;
    char *name = copy_name(site->loc, (size_t)site->len);
    Using *u;
    if (import_root(owner, name)) error_tok(site, "名前 %s は import または組み込みモジュールの名前と衝突しています", name);
    for (u = scope; u != NULL; u = u->vnext) {
        if (using_symbol(u, name).kind != SYM_NONE) error_tok(site, "名前 %s は using %s と衝突しています", name, u->path);
    }
    for (u = owner->ast->usings; u != NULL; u = u->next) {
        Symbol symbol = using_symbol(u, name);
        /* 同じ宣言を自身の using として数えない。通常は導入時にも診断する。 */
        if (symbol.kind != SYM_NONE &&
            !(symbol.kind == SYM_FUNCTION && symbol.function->tok == site) &&
            !(symbol.kind == SYM_CONSTANT && symbol.constant->tok == site) &&
            !(symbol.kind == SYM_ENUM && symbol.enumeration->tok == site) &&
            !(symbol.kind == SYM_MODEL && symbol.model->tok == site) &&
            !(symbol.kind == SYM_INTERFACE && symbol.contract->tok == site)) {
            error_tok(site, "名前 %s は using %s と衝突しています", name, u->path);
        }
    }
    free(name);
}

void validate_module_names(Ast *program)
{
    Module *module;
    Function *fn;
    Var *v;
    for (module = program->modules; module != NULL; module = module->next) {
        Using *u;
        if (module->builtin) continue;
        for (u = module->ast->all_usings; u != NULL; u = u->all_next) resolve_using(u);
        for (u = module->ast->all_usings; u != NULL; u = u->all_next) validate_using(u);
    }
    { Interface *in; for (in = program->interfaces; in; in = in->next) {
        Function *method; validate_declaration(in->tok, NULL);
        for (method = in->methods; method; method = method->next) {
            Var *p; for (p = method->params; p; p = p->next) validate_declaration(p->tok, p->usings);
        }
      }
    }
    { EnumDef *e;for(e=program->enums;e;e=e->next)validate_declaration(e->tok,NULL); }
    { Model *m; for (m = program->models; m; m = m->next) validate_declaration(m->tok, NULL); }
    for (fn = program->funcs; fn != NULL; fn = fn->next) {
        if (!fn->model) validate_declaration(fn->tok, NULL);
        if (!fn->model && fn->tok->source->module != program->entry && token_matches(fn->tok, "main")) error_tok(fn->tok, "依存モジュールには main を定義できません");
        for (v = fn->params; v != NULL; v = v->next) validate_declaration(v->tok, v->usings);
        for (v = fn->locals; v != NULL; v = v->next) {
            if (v->name_len != 0) validate_declaration(v->tok, v->usings);
            if (v->is_implicit) {
                char *name = copy_name(v->name, (size_t)v->name_len);
                Symbol target = scoped_symbol(v->tok->source->module, name, v->usings, NULL, v->tok);
                if (target.kind != SYM_NONE)
                    error_tok(v->tok, "%s は既存の定数・関数・型・名前空間です。型名を省略した宣言で置き換えることはできません", name);
                free(name);
            }
        }
    }
    for (v = program->global_vars; v != NULL; v = v->next) validate_declaration(v->tok, NULL);
}

void resolve_module_name(Node *node)
{
    Module *owner = node->tok->source->module;
    char *name = node->name != NULL ? node->name : copy_name(node->tok->loc, (size_t)node->tok->len);
    Symbol symbol = scoped_symbol(owner, name, node->usings, NULL, node->tok);
    if (node->kind == ND_PRINT || node->kind == ND_INPUT) {
        int expected = node->kind == ND_PRINT ? BI_IO_PRINT : BI_IO_INPUT;
        if (symbol.kind == SYM_BUILTIN && symbol.builtin == expected) return;
        error_tok(node->tok, "%s は io.%s を参照していません。using io または選択 using を宣言し、名前空間を確認してください",
                  name, builtin_name((Builtin)expected));
    }
    /* Qualified constant aggregates (including forward declarations and using
     * aliases) share the ordinary field/method checker and its permissions. */
    if (symbol.kind == SYM_NONE && (node->kind == ND_VAR || node->kind == ND_CALL)) {
        char *dot, *last = strrchr(name, '.'); int has_constant = 0;
        for (dot = strchr(name, '.'); dot; dot = strchr(dot + 1, '.')) {
            char *prefix = copy_name(name, (size_t)(dot - name));
            Symbol base_symbol = scoped_symbol(owner, prefix, node->usings, NULL, node->tok);
            free(prefix);
            if (base_symbol.kind == SYM_CONSTANT) { has_constant = 1; break; }
        }
        if (has_constant && last && (node->kind == ND_CALL ||
            (strcmp(last + 1, "length") && strcmp(last + 1, "addr") && strcmp(last + 1, "value")))) {
            Node *base = allocate(sizeof(*base)); Token *member = node->tok;
            size_t component = 0; char *p;
            for (p = name; p <= last; p++) if (*p == '.') component++;
            while (component-- && member->next && member->next->next) member = member->next->next;
            *base = *node; base->kind = ND_VAR; base->name = copy_name(name, (size_t)(last - name));
            base->lhs = base->rhs = base->next = NULL; base->nargs = 0;
            resolve_module_name(base);
            if (node->kind == ND_CALL) { node->kind = ND_MODEL_METHOD; node->rhs = node->lhs; }
            else node->kind = ND_FIELD;
            node->lhs = base; node->name = copy_name(last + 1, strlen(last + 1)); node->tok = member;
            return;
        }
    }
    if (symbol.kind == SYM_NONE && node->kind == ND_VAR) {
        char *dot = strrchr(name, '.');
        if (dot != NULL && (strcmp(dot + 1, "length") == 0 || strcmp(dot + 1, "addr") == 0 || strcmp(dot + 1, "value") == 0)) {
            Node *base = allocate(sizeof(*base));
            *base = *node;
            base->name = copy_name(name, (size_t)(dot - name));
            base->next = NULL;
            resolve_module_name(base);
            if (strcmp(dot + 1, "value") == 0) {
                if (base->kind != ND_VAR) error_tok(node->tok, ".value は変数の直後に指定してください");
                node->var = base->var;
            } else {
                node->kind = strcmp(dot + 1, "length") == 0 ? ND_LENGTH : ND_ADDR;
                if (node->kind == ND_ADDR && base->kind != ND_VAR) error_tok(node->tok, ".addr は変数の直後に指定してください");
                node->lhs = base;
            }
            return;
        }
    }
    if (node->kind == ND_CALL) {
        if(symbol.kind==SYM_ENUM){node->kind=ND_ENUM_CONVERT;node->cmp_ty=symbol.enumeration->ty;return;}
        if (symbol.kind == SYM_INTERFACE) error_tok(node->tok, "interface は生成できません");
        if (symbol.kind == SYM_MODEL) {
            if (node->nargs) error_tok(node->tok, "モデルの初期化は 型 変数 = {フィールド=値} と書いてください");
            node->kind = ND_MODEL_INIT; node->cmp_ty = symbol.model->ty; return;
        }
        if (symbol.kind == SYM_FUNCTION) {
            node->func = symbol.function;
            return;
        }
        if (symbol.kind == SYM_BUILTIN) {
            node->kind = ND_BUILTIN;
            node->val = symbol.builtin;
            return;
        }
        if (symbol.kind == SYM_NONE) {
            const char *ns = builtin_namespace_of(name, (int)strlen(name));
            if (ns != NULL) error_tok(node->tok, "関数 %s は定義されていません（%s.%s と書くか、using %s.%s を宣言してください）", name, ns, name, ns, name);
        }
        error_tok(node->tok, "関数 %s は定義されていません。import と using を確認してください", name);
    }
    if (symbol.kind == SYM_CONSTANT) {
        node->var = symbol.constant;
        return;
    }
    error_tok(node->tok, "変数または定数 %s は宣言されていません。import と using を確認してください", name);
}

Type resolve_module_type(Token *tok, const char *name, Using *scope)
{
    Symbol symbol = scoped_symbol(tok->source->module, name, scope, NULL, tok);
    if(symbol.kind==SYM_ENUM)return symbol.enumeration->ty;
    if (symbol.kind == SYM_INTERFACE) return symbol.contract->ty;
    if (symbol.kind != SYM_MODEL) error_tok(tok, "モデル型 %s は定義されていません。import と using を確認してください", name);
    return symbol.model->ty;
}

int module_name_exists(Node *node)
{
    char *name=node->name?node->name:copy_name(node->tok->loc,(size_t)node->tok->len);
    return scoped_symbol(node->tok->source->module,name,node->usings,NULL,node->tok).kind!=SYM_NONE;
}
