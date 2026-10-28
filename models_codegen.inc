/* Model layout shares checked slot access with arrays, but its fixed fields
 * have individual types. All allocations use the existing tracing collector. */
static void gen_model_allocate(Model *m)
{
    x64_mov_imm(RDI, 40); x64_call(alloc_label);
    x64_store_imm(RAX, 0, m->field_count);
    x64_store_imm(RAX, 8, m->field_count);
    x64_store_imm(RAX, 16, 0);
    x64_store_imm(RAX, 24, 0);
    x64_store_imm(RAX, 32, 0);
    if (m->field_count) {
        push(); x64_mov_imm(RDI, m->field_count * 8); x64_call(alloc_label);
        x64_mov(RDX, RAX); pop(RAX); x64_store(RAX, 24, RDX);
    }
}
static void gen_model_init(Node *node)
{
    Node *item;
    gen_model_allocate(type_model(node->ty)); push();
    for (item = node->lhs; item; item = item->next) {
        gen_expr(item->lhs);
        if (node->constant_value && type_is_array(item->field->ty)) {
            collection_shape(item->field->ty, item->field->dims, RDI);
            x64_mov_imm(RSI, 1); x64_call(array_copy_label);
        }
        x64_load(RDX, RSP, 0); x64_load(RDX, RDX, 24);
        x64_store(RDX, item->field->index * 8, RAX);
    }
    pop(RAX);
}
static void gen_creation_context(Token *tok)
{
    if (emitting_function && emitting_function->creation_context)
        gen_load_var(emitting_function->creation_context, tok);
    else {
        collection_context(tok); x64_load(RAX, R14, COLLECTION_CONTEXT);
    }
}
static void gen_construction(Node *node)
{
    Node *item; int count = node->func->param_count, i;
    int bytes = ((count * 8 + 15) / 16) * 16;
    int base = emitting_function->stack_size + depth * 8 + bytes;
    x64_sub_imm(RSP, bytes); depth += bytes / 8;
    for (item = node->lhs; item; item = item->next) {
        gen_expr(item->lhs);
        x64_store(RBP, -base + item->var->parameter_index * 8, RAX);
    }
    gen_creation_context(node->tok); x64_store(RBP, -base, RAX);
    for (i = 0; i < count; i++) { x64_load(RAX, RBP, -base + i * 8); push(); }
    x64_call(node->func->label);
    x64_add_imm(RSP, count * 8 + bytes); depth -= count + bytes / 8;
}
static void gen_model_default(Model *m, Token *tok)
{
    gen_creation_context(tok); push(); x64_call(m->default_builder->label);
    x64_add_imm(RSP, 8); depth--;
}
static void model_copy_field(ModelField *f)
{
    if (type_is_model(f->ty)) x64_call(type_model(f->ty)->copy_label);
    else if (type_is_array(f->ty)) {
        collection_shape(f->ty, f->dims, RDI); x64_xor(RSI, RSI); x64_call(array_copy_label);
    }
}
static void gen_model_runtime(void)
{
    Model *m; Function scratch = {0};
    scratch.stack_size = 32; emitting_function = &scratch;
    for (m = emitting_models; m; m = m->next) {
        ModelField *f;
        int printed = 0;
        int from_value = rt_label("model.copy.value");
        int unequal = rt_label("model.unequal");
        depth = 0;
        rt_enter(m->copy_label, 4); rt_save(1, RAX);
        x64_test(RAX, RAX); x64_jcc(CC_NZ, from_value);
        collection_fail("未完了のモデルはコピーできません\n");
        x64_bind(from_value);
        gen_model_allocate(m); rt_save(2, RAX);
        for (f = m->fields; f; f = f->next) {
            rt_load(RAX, 1); x64_load(RAX, RAX, 24); x64_load(RAX, RAX, f->index * 8);
            model_copy_field(f);
            rt_load(RDX, 2); x64_load(RDX, RDX, 24); x64_store(RDX, f->index * 8, RAX);
        }
        rt_load(RAX, 2); rt_leave();
        rt_enter(m->equal_label, 4); rt_save(1, RAX); rt_save(2, RDI);
        for (f = m->fields; f; f = f->next) {
            rt_load(RAX, 1); x64_load(RAX, RAX, 24); x64_load(RAX, RAX, f->index * 8);
            rt_load(RDI, 2); x64_load(RDI, RDI, 24); x64_load(RDI, RDI, f->index * 8);
            if (type_is_model(f->ty) || type_is_array(f->ty) || type_is_text(f->ty)) {
                x64_call(type_is_model(f->ty) ? type_model(f->ty)->equal_label : type_is_array(f->ty) ? array_equal_label : str_eq_label);
                x64_test(RAX, RAX); x64_jcc(CC_Z, unequal);
            } else if (f->ty == TY_FLOAT || f->ty == TY_DOUBLE) {
                if (f->ty == TY_FLOAT) {
                    x64_movd_to_xmm(0, RAX); x64_cvtss2sd(0, 0);
                    x64_movd_to_xmm(1, RDI); x64_cvtss2sd(1, 1);
                } else { x64_movq_to_xmm(0, RAX); x64_movq_to_xmm(1, RDI); }
                x64_ucomisd(0, 1); x64_jcc(CC_P, unequal); x64_jcc(CC_NZ, unequal);
            } else { x64_cmp(RAX, RDI); x64_jcc(CC_NZ, unequal); }
        }
        x64_mov_imm(RAX, 1); rt_leave();
        x64_bind(unequal); x64_xor(RAX, RAX); rt_leave();
        rt_enter(m->print_label, 4); rt_save(1, RAX);
        gen_write_const(m->name); gen_write_const("{");
        for (f = m->fields; f; f = f->next) {
            if (f->is_private) continue;
            if (printed++) gen_write_const(", ");
            gen_write_const(f->name); gen_write_const("=");
            rt_load(RAX, 1); x64_load(RAX, RAX, 24); x64_load(RAX, RAX, f->index * 8);
            if (type_is_model(f->ty)) x64_call(type_model(f->ty)->print_label);
            else if (type_is_array(f->ty)) x64_call(array_print_label);
            else if (type_is_text(f->ty)) x64_call(quoted_string_label);
            else if (f->ty == TY_FILE) gen_write_const("<file>");
            else {
                if (f->ty == TY_FLOAT) gen_convert(TY_FLOAT, TY_DOUBLE);
                x64_call(f->ty == TY_BOOL ? print_bool_label : f->ty == TY_FLOAT ? print_float_label : f->ty == TY_DOUBLE ? print_double_label : integer_unsigned(f->ty) ? print_uint_label : print_int_label);
            }
        }
        gen_write_const("}"); rt_leave();
    }
    emitting_function = NULL;
}
