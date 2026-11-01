/* Metadata access is effectful and returns an ordinary standard model. */
static void check_path_info_builtin(Node *node)
{
    Node *path=node->lhs,*follow=path?path->next:NULL;
    if(node->nargs<1||node->nargs>2)error_tok(node->tok,"io.path.info expects one or two arguments");
    if(path->kind==ND_REFARG)error_tok(path->tok,"info paths do not accept ref");
    check_expr(path);if(!type_is_text(path->ty))error_tok(path->tok,"info path must be str or char");
    if(is_const_expr(path)){
        Value v=eval_const(path);WideRequest r={0};
        r.op=W_PATH;r.aux=9;r.from=checking_target==TARGET_WINDOWS;r.input=v.s;r.length=v.len;r.base=(long long)v.len+4;
        r.buffer=malloc((size_t)r.base);if(!r.buffer)fatal("Cannot validate a metadata path");
        wide_execute(&r);free(r.buffer);
        if(r.status||!v.len)error_tok(path->tok,"Invalid info path: empty, NUL, or unsupported format");
    }
    if(!follow){follow=file_bulk_default(node,TY_BOOL,0,NULL);path->next=follow;node->nargs=2;}
    if(follow->kind==ND_REFARG)error_tok(follow->tok,"info follow_links does not accept ref");
    check_expr(follow);if(follow->ty!=TY_BOOL)error_tok(follow->tok,"info follow_links must be bool");
    node->ty=standard_type(cur_ast,"io.path","PathInfo");
}
