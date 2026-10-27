/* Interfaces are borrowed contracts. Only concrete specializations reach
 * resource analysis and code generation; contracts never imply ownership. */
static Node *allowed_interface_use;
static void check_interface_receiver(Node *n)
{
    Node *saved = allowed_interface_use;
    allowed_interface_use = n; check_expr(n); allowed_interface_use = saved;
}
static int implements_type(Type concrete, Type contract)
{
    Implementation *im;
    if (concrete == contract) return 1;
    if (!type_is_model(concrete)) return 0;
    for (im = type_model(concrete)->implements; im; im = im->next)
        if (im->ty == contract) return 1;
    return 0;
}
static void concrete_only(Type ty, Token *tok)
{
    if (type_is_interface(array_base(ty))) error_tok(tok, "interface は ref / ref readonly 引数にだけ使用できます");
}
static int same_dimensions(ArrayDim *a, ArrayDim *b)
{
    for (; a && b; a = a->next, b = b->next) if (a->limit != b->limit) return 0;
    return !a && !b;
}
static Function *interface_implementation(Model *m, Function *required)
{
    Function *fn;
    for (fn = cur_ast->funcs; fn; fn = fn->next)
        if (fn->model == m && !fn->is_internal && !fn->template_origin && !fn->generic_origin &&
            same_name(fn->name, fn->name_len, required->name, required->name_len)) return fn;
    return NULL;
}
static void check_interface_positions(Ast *ast)
{
    Interface *in, *other; Model *m; Function *fn; Var *v;
    for (in = ast->interfaces; in; in = in->next) {
        for (other = ast->interfaces; other != in; other = other->next)
            if (in->tok->source->module == other->tok->source->module && !strcmp(in->name, other->name))
                error_tok(in->tok, "interface %s が重複しています", in->name);
        for (m = ast->models; m; m = m->next)
            if (in->tok->source->module == m->tok->source->module && !strcmp(in->name, m->name))
                error_tok(in->tok, "interface 名がモデルと重複しています");
        if (find_global(in->tok->source->module, in->name, (int)strlen(in->name))) error_tok(in->tok, "interface 名が const と重複しています");
        for (fn = ast->funcs; fn; fn = fn->next)
            if (!fn->model && fn->tok->source->module == in->tok->source->module && same_name(fn->name, fn->name_len, in->name, (int)strlen(in->name)))
                error_tok(in->tok, "interface 名が関数と重複しています");
        for (fn = in->methods; fn; fn = fn->next) {
            Function *previous;
            for (previous = in->methods; previous != fn; previous = previous->next)
                if (same_name(previous->name, previous->name_len, fn->name, fn->name_len)) error_tok(fn->tok, "interface のメソッドが重複しています");
            concrete_only(fn->ret, fn->tok); check_dims(fn->return_dims);
            for (v = fn->params->next; v; v = v->next) { concrete_only(v->ty, v->tok); check_dims(v->dims); }
        }
    }
    for (m = ast->models; m; m = m->next) {
        ModelField *field; Implementation *im, *previous;
        for (field = m->fields; field; field = field->next) concrete_only(field->ty, field->tok);
        for (im = m->implements; im; im = im->next) {
            if (!type_is_interface(im->ty)) error_tok(im->tok, "implements の対象は interface にしてください");
            for (previous = m->implements; previous != im; previous = previous->next)
                if (previous->ty == im->ty) error_tok(im->tok, "implements の interface が重複しています");
        }
    }
    for (v = ast->global_vars; v; v = v->next) concrete_only(v->ty, v->tok);
    for (fn = ast->funcs; fn; fn = fn->next) {
        concrete_only(fn->ret, fn->tok);
        for (v = fn->locals; v; v = v->next) concrete_only(v->ty, v->tok);
        for (v = fn->params; v; v = v->next) {
            if (type_is_interface(v->ty) && v->is_ref && !fn->is_initializer) fn->is_template = 1;
            else concrete_only(v->ty, v->tok);
        }
    }
}
static void check_implementations(Ast *ast)
{
    Model *m;
    for (m = ast->models; m; m = m->next) {
        Implementation *im;
        for (im = m->implements; im; im = im->next) {
            Function *required;
            for (required = type_interface(im->ty)->methods; required; required = required->next) {
                Function *actual = interface_implementation(m, required); Var *a, *b; int valid;
                if (actual && (actual->is_private || actual->is_initializer))
                    error_tok(im->tok, "%s.%.*s は private または init のため interface を実装できません（契約: %s:%d、候補: %s:%d）",
                        m->name, actual->name_len, actual->name, required->tok->source->filename, required->tok->line,
                        actual->tok->source->filename, actual->tok->line);
                if (!actual)
                    error_tok(im->tok, "%s は interface %s の public メソッド %.*s を実装してください（契約: %s:%d）",
                        m->name, type_name(im->ty), required->name_len, required->name,
                        required->tok->source->filename, required->tok->line);
                if (actual->is_generic)
                    error_tok(actual->tok, "interface を実装するメソッドの引数には型を明示してください");
                valid = actual->ret == required->ret && same_dimensions(actual->return_dims, required->return_dims) &&
                    actual->param_count == required->param_count && (!required->is_readonly || actual->is_readonly);
                a = actual->params->next; b = required->params->next;
                for (; a && b; a = a->next, b = b->next)
                    if (a->ty != b->ty || a->is_ref != b->is_ref || a->is_readonly != b->is_readonly || !same_dimensions(a->dims, b->dims)) valid = 0;
                if (!valid) error_tok(im->tok, "%s.%.*s の型・引数・配列制約・readonly が interface %s と一致しません（契約: %s:%d、実装: %s:%d）",
                    m->name, actual->name_len, actual->name, type_name(im->ty), required->tok->source->filename,
                    required->tok->line, actual->tok->source->filename, actual->tok->line);
            }
        }
    }
}
