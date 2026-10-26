/* 添字は一度だけ評価する。右辺から戻ったら、変数の場所と保存した添字から
 * 最新の配列領域を解決し直す。可変配列の data の生ポインタを右辺越しに保持しない。
 */
typedef struct {
    Node *target;
    Node *indices[MAX_ARRAY_RANK + 1];
    int count;
    int bytes;
    int base;
    int text_last;
} PlacePlan;

static void place_load(PlacePlan *plan, Reg reg, int slot)
{
    x64_load(reg, RBP, -plan->base + 8 * slot);
}

static void place_save(PlacePlan *plan, int slot, Reg reg)
{
    x64_store(RBP, -plan->base + 8 * slot, reg);
}

static PlacePlan place_capture(Node *target)
{
    PlacePlan plan;
    Node *path[MAX_ARRAY_RANK + 1];
    Node *root = target;
    int i;
    memset(&plan, 0, sizeof(plan));
    plan.target = target;
    plan.text_last = target->kind == ND_INDEX && type_is_text(target->lhs->ty);
    while (root->kind == ND_INDEX) {
        if (plan.count >= MAX_ARRAY_RANK + 1) fatal("更新先の次元が多すぎます");
        path[plan.count++] = root;
        root = root->lhs;
    }
    plan.bytes = ((plan.count + 4) * 8 + 15) / 16 * 16;
    plan.base = emitting_function->stack_size + depth * 8 + plan.bytes;
    x64_sub_imm(RSP, plan.bytes);
    depth += plan.bytes / 8;
    gen_var_addr(root->var, root->tok, 1);
    place_save(&plan, 0, RAX);
    for (i = 0; i < plan.count; i++) {
        plan.indices[i] = path[plan.count - 1 - i]->rhs;
        gen_expr(plan.indices[i]);
        place_save(&plan, i + 1, RAX);
    }
    return plan;
}

static void place_release(PlacePlan *plan)
{
    x64_add_imm(RSP, plan->bytes);
    depth -= plan->bytes / 8;
}

/* 指定個数の添字までの「値の置き場」を返す。文字列の最後の添字は含めない。 */
static void place_resolve(PlacePlan *plan, int count)
{
    int i;
    place_load(plan, RAX, 0);
    for (i = 0; i < count; i++) {
        x64_load(RAX, RAX, 0);
        place_load(plan, RDI, i + 1);
        x64_call(array_slot_label);
    }
}

static void place_read(PlacePlan *plan)
{
    place_resolve(plan, plan->count - plan->text_last);
    x64_load(RAX, RAX, 0);
    if (plan->text_last) {
        place_load(plan, RDI, plan->count);
        x64_call(string_get_label);
    }
}

/* 添字は一度評価し、後続の引数と関数本体の実行中、祖先配列を固定する。 */
static void gen_element_ref(Node *node)
{
    PlacePlan plan = place_capture(node->lhs);
    int i;
    collection_context(node->tok);
    for (i = 0; i < plan.count; i++) {
        place_resolve(&plan, i);
        x64_load(RAX, RAX, 0);
        x64_store(RBP, -(int)node->val + i * 8, RAX);
        x64_load(RCX, RAX, 32);
        x64_add_imm(RCX, 1);
        x64_store(RAX, 32, RCX);
        place_load(&plan, RDI, i + 1);
        x64_call(array_slot_label); /* 範囲検査を後続の引数より先に行う */
    }
    place_resolve(&plan, plan.count);
    place_release(&plan);
}

static void gen_index_store(Node *node)
{
    PlacePlan plan = place_capture(node->lhs);
    int value = plan.count + 1;
    int old = plan.count + 2;
    collection_context(node->tok);
    place_read(&plan); /* 右辺より先に初回の範囲・UTF-8検査を行う */
    place_save(&plan, old, RAX);
    if (node->val == UPDATE_SET) gen_expr(node->rhs);
    else {
        node->init->val = plan.base - old * 8;
        gen_expr(node->cond);
    }
    place_save(&plan, value, RAX);
    collection_context(node->tok);
    if (plan.text_last) {
        place_resolve(&plan, plan.count - 1);
        x64_load(RAX, RAX, 0);
        place_load(&plan, RDI, plan.count);
        place_load(&plan, RSI, value);
        x64_call(string_replace_label);
        place_save(&plan, value, RAX);
        place_resolve(&plan, plan.count - 1);
    } else {
        place_resolve(&plan, plan.count);
        if (type_is_array(node->lhs->ty)) {
            x64_load(RAX, RAX, 0);
            array_require_unborrowed();
            x64_mov(RDI, RAX);
            x64_load(RDI, RDI, 16);
            place_load(&plan, RAX, value);
            x64_xor(RSI, RSI);
            x64_call(array_copy_label);
            place_save(&plan, value, RAX);
            place_resolve(&plan, plan.count);
        }
    }
    place_load(&plan, RDX, value);
    x64_store(RAX, 0, RDX);
    place_release(&plan);
}

static void gen_array_method(Node *node)
{
    PlacePlan plan = place_capture(node->lhs);
    collection_context(node->tok);
    place_read(&plan);
    if (node->rhs != NULL) {
        gen_expr(node->rhs);
        place_save(&plan, plan.count + 1, RAX);
    }
    collection_context(node->tok);
    place_resolve(&plan, plan.count);
    x64_load(RAX, RAX, 0);
    if (node->rhs != NULL) place_load(&plan, RDI, plan.count + 1);
    else if (node->val == AM_ADD) {
        Type element = array_element(node->lhs->ty);
        if (element == TY_STR) x64_lea_data(RDI, empty_str_offset);
        else if (element == TY_CHAR) x64_lea_data(RDI, zero_char_offset);
        else x64_xor(RDI, RDI); /* 0 / false / 初期子配列 */
    }
    x64_call(node->val == AM_ADD ? array_add_label : node->val == AM_REMOVE_AT ? array_remove_label : array_clear_label);
    place_release(&plan);
}

static void gen_array_literal(Node *node)
{
    Node *item;
    x64_xor(RAX, RAX);
    collection_shape(node->ty, NULL, RDI);
    x64_xor(RSI, RSI);
    collection_context(node->tok);
    x64_call(array_copy_label);
    push();
    for (item = node->lhs; item != NULL; item = item->next) {
        gen_expr(item);
        x64_mov(RDI, RAX);
        x64_load(RAX, RSP, 0);
        collection_context(node->tok);
        x64_call(array_add_label);
    }
    pop(RAX);
}
