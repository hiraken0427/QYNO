/* type.c に含める配列・代入先の型検査。上限は要素型から分離する。 */
static void check_dims(ArrayDim *dims)
{
    for (; dims != NULL; dims = dims->next) {
        if (dims->checked) continue;
        if (dims->bound != NULL) {
            Value v;
            check_expr(dims->bound);
            if (dims->bound->ty != TY_INT || !is_const_expr(dims->bound)) {
                error_tok(dims->tok, "配列の上限はコンパイル時に確定する非負の int にしてください");
            }
            v = eval_const(dims->bound);
            if (v.i < 0 || v.i > (LLONG_MAX - 64) / 8) {
                error_tok(dims->tok, "配列の上限が負か、必要な領域が int の範囲を超えます");
            }
            dims->limit = v.i;
        }
        dims->checked = 1;
    }
}

static void check_array_literal(Node *node, Type expected)
{
    Node *item;
    Node *next;
    Node head = {0};
    Node *tail = &head;
    Type element = type_is_array(expected) ? array_element(expected) : TY_NONE;
    if (element == TY_NONE) {
        for (item = node->lhs; item != NULL; item = item->next) {
            check_expr(item);
            if (element == TY_NONE) element = item->ty;
            else if (element != item->ty) {
                if (is_numeric(element) && is_numeric(item->ty)) {
                    if (element == TY_DOUBLE || item->ty == TY_DOUBLE) element = TY_DOUBLE;
                    else if (element == TY_FLOAT || item->ty == TY_FLOAT) element = TY_FLOAT;
                } else if (type_is_text(element) && type_is_text(item->ty)) element = TY_STR;
                else error_tok(item->tok, "配列の要素型と次元数をそろえてください");
            }
        }
        if (element == TY_NONE) {
            error_tok(node->tok, "空の配列リテラルには、宣言や引数などの要素型の文脈が必要です");
        }
    }
    for (item = node->lhs; item != NULL; item = next) {
        next = item->next;
        item->next = NULL;
        check_expected(item, element);
        tail = tail->next = coerce(item, element, item->tok, "配列の要素");
    }
    node->lhs = head.next;
    node->ty = array_type(element);
    if (array_rank(node->ty) > MAX_ARRAY_RANK) {
        error_tok(node->tok, "配列の次元数が上限を超えています");
    }
}

static void check_expected(Node *node, Type expected)
{
    if (node->kind == ND_ARRAY_LITERAL && type_is_array(expected)) check_array_literal(node, expected);
    else check_expr(node);
}

static ArrayDim *expression_dims(Node *node)
{
    if (node->kind == ND_VAR) return node->var->dims;
    if (node->kind == ND_CALL) return node->func->return_dims;
    if (node->kind == ND_ARRAY_NEW || node->kind == ND_ARRAY_COPY) return node->dims;
    if (node->kind == ND_INDEX && type_is_array(node->lhs->ty)) {
        ArrayDim *parent = expression_dims(node->lhs);
        return parent ? parent->next : NULL;
    }
    return NULL;
}

static long long expression_max_length(Node *node)
{
    ArrayDim *dims;
    if (node->kind == ND_ARRAY_LITERAL) return node->nargs;
    if (node->ty == TY_CHAR) return 1;
    dims = expression_dims(node);
    return dims ? dims->limit : -1;
}

/* ref はコピーで上限を付け直せないため、宣言先の制約を満たす参照先だけを許可。 */
static void check_ref_limits(Var *view, Var *target, Token *where)
{
    ArrayDim *required = view->dims;
    ArrayDim *actual = target->dims;
    if (!type_is_array(view->ty) || target->is_ref) return;
    while (required != NULL && actual != NULL) {
        if (required->checked && actual->checked && required->limit >= 0 &&
            (actual->limit < 0 || actual->limit > required->limit)) {
            error_tok(where, "ref 配列の上限が宣言された条件を満たしません");
        }
        required = required->next;
        actual = actual->next;
    }
}

static void validate_const_array(Node *where, Value source, ArrayDim *dims)
{
    Node *item;
    if (dims != NULL && dims->limit >= 0 && source.len > dims->limit) {
        const_error(where, "配列の要素数が代入先の上限を超えています");
    }
    if (type_is_array(array_element(source.ty))) {
        for (item = source.items; item != NULL; item = item->next) {
            validate_const_array(where, eval_const(item), dims != NULL ? dims->next : NULL);
        }
    }
}

static Node *copy_array(Node *source, ArrayDim *dims, int pad)
{
    Node *node = calloc(1, sizeof(*node));
    if (node == NULL) fatal("配列のコピーを作成できません");
    node->kind = ND_ARRAY_COPY;
    node->lhs = source;
    node->ty = source->ty;
    node->tok = source->tok;
    node->dims = dims;
    node->val = pad;
    if (is_const_expr(source)) validate_const_array(node, eval_const(source), dims);
    return node;
}

static Var *check_place(Node *place)
{
    Node *root = place;
    while (root->kind == ND_INDEX) {
        if (root != place && type_is_text(root->lhs->ty)) {
            error_tok(root->tok, "文字列の添字は更新先の最後に1回だけ書けます");
        }
        root = root->lhs;
    }
    if (root->kind != ND_VAR) {
        error_tok(root->tok, "更新先は変数、または変数から始まる添字にしてください");
    }
    check_expr(root);
    if (root->var->is_const) {
        error_tok(root->tok, "const の配列・文字列とその要素は変更できません");
    }
    return root->var;
}

static void check_method(Node *node)
{
    int allow = allow_void;
    int expected = node->val == AM_CLEAR ? 0 : 1;
    allow_void = 0;
    check_expr(node->lhs);
    if (!type_is_array(node->lhs->ty)) {
        error_tok(node->tok, "add・remove_at・clear は配列に使ってください");
    }
    check_place(node->lhs);
    if (node->val == AM_ADD && expression_max_length(node->lhs) == 0) {
        const_error(node, "上限0の配列には追加できません");
    }
    if (node->nargs != expected) {
        error_tok(node->tok, "この配列操作の引数は %d 個です", expected);
    }
    if (expected) {
        Type to = node->val == AM_ADD ? array_element(node->lhs->ty) : TY_INT;
        check_expected(node->rhs, to);
        node->rhs = coerce(node->rhs, to, node->rhs->tok, "配列操作の引数");
        if (node->val == AM_REMOVE_AT && is_const_expr(node->rhs) && eval_const(node->rhs).i < 0) {
            const_error(node, "配列の添字が範囲外です");
        }
        if (node->val == AM_REMOVE_AT && is_const_expr(node->rhs)) {
            long long limit = expression_max_length(node->lhs);
            if (limit >= 0 && eval_const(node->rhs).i >= limit) const_error(node, "配列の添字が範囲外です");
        }
    }
    node->ty = TY_VOID;
    if (!allow) error_tok(node->tok, "配列の追加・削除は値を返しません");
}

static void check_store(Node *node)
{
    Type destination;
    check_expr(node->lhs);
    if (node->lhs->kind == ND_LENGTH) {
        error_tok(node->tok, "length は読み取り専用です");
    }
    check_place(node->lhs);
    if (node->lhs->kind != ND_INDEX) {
        error_tok(node->tok, "length は読み取り専用です。代入できる添字を指定してください");
    }
    destination = type_is_text(node->lhs->lhs->ty) ? TY_CHAR : node->lhs->ty;
    if (node->val == UPDATE_SET) {
        check_expected(node->rhs, destination);
        node->rhs = coerce(node->rhs, destination, node->rhs->tok, "要素への代入");
        if (type_is_array(destination) && is_const_expr(node->rhs)) {
            validate_const_array(node, eval_const(node->rhs), expression_dims(node->lhs));
        }
    } else {
        Node *saved = calloc(1, sizeof(Node));
        Node *operation = calloc(1, sizeof(Node));
        if (saved == NULL || operation == NULL) fatal("添字の複合更新を作成できません");
        saved->kind = ND_SAVED;
        saved->ty = node->lhs->ty;
        saved->tok = node->lhs->tok;
        operation->kind = node->val == UPDATE_ADD || node->val == UPDATE_INC ? ND_ADD : ND_SUB;
        operation->lhs = saved;
        operation->rhs = node->rhs;
        operation->tok = node->tok;
        check_expr(operation);
        node->cond = coerce(operation, destination, node->tok, "要素の複合更新");
        node->init = saved;
    }
}

/* const 配列は AST の値として保持し、各内側配列を独立して作る。 */
static Value constant_array_build(Node *where, Value source, Type ty, ArrayDim *dims, int pad, long long *budget)
{
    Value result = {0};
    Node head = {0};
    Node *tail = &head;
    Node *item = source.items;
    long long n = source.len;
    long long i;
    Type element = array_element(ty);
    if (dims != NULL && dims->limit >= 0) {
        if (n > dims->limit) const_error(where, "配列の要素数が代入先の上限を超えています");
        if (pad) n = dims->limit;
    }
    if (n > *budget) const_error(where, "const 配列の展開が100万要素を超えています");
    *budget -= n;
    result.ty = ty;
    result.len = (int)n;
    for (i = 0; i < n; i++) {
        Value value = {0};
        value.ty = element;
        if (item != NULL) {
            value = eval_const(item);
            item = item->next;
        } else if (element == TY_STR) value.s = "";
        else if (element == TY_CHAR) { value.s = "\0"; value.len = 1; }
        if (type_is_array(element)) {
            value = constant_array_build(where, value, element, dims != NULL ? dims->next : NULL, pad, budget);
        }
        tail = tail->next = value_node(value, where->tok);
    }
    result.items = head.next;
    return result;
}

static Value constant_array(Node *where, Value source, Type ty, ArrayDim *dims, int pad)
{
    long long budget = 1000000;
    return constant_array_build(where, source, ty, dims, pad, &budget);
}
