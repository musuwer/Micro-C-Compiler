#include "microcc.h"

// ----------------------------- AST and parser -----------------------------
// Advanced requirement B:
//   The parser uses statement-level synchronization.  When a syntax error is
//   found, it reports the line/column/type of error, skips only to a safe
//   boundary such as ';', '}', or the beginning of the next statement, and then
//   continues parsing.  This allows one run to collect multiple parse errors.

const char *type_name(Type t) {
    switch (t) {
        case TY_INT: return "int";
        case TY_FLOAT: return "float";
        default: return "unknown";
    }
}

int parse_errors = 0;

static Node *new_node_at(NodeKind kind, int line, int col) {
    Node *n = (Node *)xmalloc(sizeof(Node));
    n->kind = kind;
    n->line = line;
    n->col = col;
    n->type = TY_UNKNOWN;
    n->slot = -1;
    return n;
}

static void node_add(Node *block, Node *child) {
    if (!block || !child) return;
    if (block->len == block->cap) {
        block->cap = block->cap ? block->cap * 2 : 8;
        block->items = (Node **)realloc(block->items, sizeof(Node *) * (size_t)block->cap);
        if (!block->items) die("out of memory");
    }
    block->items[block->len++] = child;
}

const char *node_kind_name(NodeKind k) {
    switch (k) {
        case ND_PROGRAM: return "Program";
        case ND_FUNC: return "Function";
        case ND_BLOCK: return "Block";
        case ND_DECL: return "Declaration";
        case ND_RETURN: return "Return";
        case ND_IF: return "If";
        case ND_WHILE: return "While";
        case ND_FOR: return "For";
        case ND_EXPR_STMT: return "ExpressionStatement";
        case ND_ASSIGN: return "Assign";
        case ND_BINARY: return "Binary";
        case ND_UNARY: return "Unary";
        case ND_INT: return "IntLiteral";
        case ND_FLOAT: return "FloatLiteral";
        case ND_VAR: return "Variable";
        case ND_EMPTY: return "Empty";
        default: return "Unknown";
    }
}

static Token *peek(void) { return &tokens.data[pos]; }
static Token *prev(void) { return &tokens.data[pos - 1]; }
static bool at_eof(void) { return peek()->kind == TK_EOF; }

static bool match(int kind) {
    if (peek()->kind == kind) { pos++; return true; }
    return false;
}

static bool is_type_keyword(int kind) { return kind == TK_KW_INT || kind == TK_KW_FLOAT; }

static bool is_stmt_start(int kind) {
    return kind == '{' || kind == ';' || kind == TK_KW_RETURN || kind == TK_KW_IF ||
           kind == TK_KW_WHILE || kind == TK_KW_FOR || is_type_keyword(kind);
}

static bool is_expr_boundary(int kind) {
    return kind == ';' || kind == ')' || kind == '}' || kind == TK_EOF;
}

static void report_expected(Token *t, const char *what) {
    fprintf(stderr, "parse error line %d col %d: expected %s, got %s '%s'\n",
            t->line, t->col, what, token_kind_name(t->kind), t->lexeme);
    parse_errors++;
}

// Synchronize after a statement-level error.  Do not consume the beginning of
// the next valid statement; this is what lets the parser continue collecting
// errors instead of cascading from one missing semicolon.
static void synchronize_stmt(void) {
    while (!at_eof()) {
        int k = peek()->kind;
        if (k == ';') { pos++; return; }
        if (k == '}' || is_stmt_start(k)) return;
        pos++;
    }
}

static bool expect_semicolon(void) {
    if (match(';')) return true;
    report_expected(peek(), "';'");
    synchronize_stmt();
    return false;
}

static bool expect_delim(int kind, const char *what) {
    if (match(kind)) return true;
    report_expected(peek(), what);

    // For missing ')' before a block or statement, keep the current token so the
    // caller can parse the body normally.
    if (peek()->kind == '{' || peek()->kind == '}' || peek()->kind == ';' || is_stmt_start(peek()->kind) || at_eof())
        return false;

    // Otherwise skip to the requested delimiter or a safe statement boundary.
    while (!at_eof()) {
        if (match(kind)) return false;
        if (peek()->kind == '{' || peek()->kind == '}' || peek()->kind == ';' || is_stmt_start(peek()->kind))
            return false;
        pos++;
    }
    return false;
}

static Type parse_type(void) {
    if (match(TK_KW_INT)) return TY_INT;
    if (match(TK_KW_FLOAT)) return TY_FLOAT;
    report_expected(peek(), "type keyword");
    if (!at_eof()) pos++;
    return TY_UNKNOWN;
}

static Node *parse_stmt(void);
static Node *parse_expr(void);

static Node *error_int_node(Token *t) {
    Node *n = new_node_at(ND_INT, t->line, t->col);
    n->ival = 0;
    n->type = TY_INT;
    return n;
}

static Node *parse_primary(void) {
    Token *t = peek();
    if (match(TK_INT_LITERAL)) {
        Node *n = new_node_at(ND_INT, t->line, t->col);
        n->ival = t->ival;
        n->type = TY_INT;
        return n;
    }
    if (match(TK_FLOAT_LITERAL)) {
        Node *n = new_node_at(ND_FLOAT, t->line, t->col);
        n->fval = t->fval;
        n->type = TY_FLOAT;
        return n;
    }
    if (match(TK_IDENT)) {
        Node *n = new_node_at(ND_VAR, t->line, t->col);
        n->name = xstrdup(t->lexeme);
        return n;
    }
    if (match('(')) {
        Node *n = parse_expr();
        expect_delim(')', "')'");
        return n;
    }

    fprintf(stderr, "parse error line %d col %d: expected expression, got %s '%s'\n",
            t->line, t->col, token_kind_name(t->kind), t->lexeme);
    parse_errors++;

    if (!is_expr_boundary(t->kind) && !is_stmt_start(t->kind)) pos++;
    return error_int_node(t);
}

static Node *parse_unary(void) {
    if (match('+')) return parse_unary();
    if (match('-')) {
        Token *op = prev();
        Node *n = new_node_at(ND_UNARY, op->line, op->col);
        strcpy(n->op, "-");
        n->rhs = parse_unary();
        return n;
    }
    if (match('!')) {
        Token *op = prev();
        Node *n = new_node_at(ND_UNARY, op->line, op->col);
        strcpy(n->op, "!");
        n->rhs = parse_unary();
        return n;
    }
    return parse_primary();
}

static Node *new_binary_at(const char *op, Node *lhs, Node *rhs, int line, int col) {
    Node *n = new_node_at(ND_BINARY, line, col);
    strncpy(n->op, op, sizeof(n->op) - 1);
    n->op[sizeof(n->op) - 1] = '\0';
    n->lhs = lhs;
    n->rhs = rhs;
    return n;
}

static Node *parse_multiplicative(void) {
    Node *node = parse_unary();
    for (;;) {
        if (match('*')) {
            Token *op = prev();
            node = new_binary_at("*", node, parse_unary(), op->line, op->col);
        } else if (match('/')) {
            Token *op = prev();
            node = new_binary_at("/", node, parse_unary(), op->line, op->col);
        } else if (match('%')) {
            Token *op = prev();
            node = new_binary_at("%", node, parse_unary(), op->line, op->col);
        } else {
            return node;
        }
    }
}

static Node *parse_additive(void) {
    Node *node = parse_multiplicative();
    for (;;) {
        if (match('+')) {
            Token *op = prev();
            node = new_binary_at("+", node, parse_multiplicative(), op->line, op->col);
        } else if (match('-')) {
            Token *op = prev();
            node = new_binary_at("-", node, parse_multiplicative(), op->line, op->col);
        } else {
            return node;
        }
    }
}

static Node *parse_relational(void) {
    Node *node = parse_additive();
    for (;;) {
        if (match('<')) {
            Token *op = prev();
            node = new_binary_at("<", node, parse_additive(), op->line, op->col);
        } else if (match('>')) {
            Token *op = prev();
            node = new_binary_at(">", node, parse_additive(), op->line, op->col);
        } else if (match(TK_LE)) {
            Token *op = prev();
            node = new_binary_at("<=", node, parse_additive(), op->line, op->col);
        } else if (match(TK_GE)) {
            Token *op = prev();
            node = new_binary_at(">=", node, parse_additive(), op->line, op->col);
        } else {
            return node;
        }
    }
}

static Node *parse_equality(void) {
    Node *node = parse_relational();
    for (;;) {
        if (match(TK_EQ)) {
            Token *op = prev();
            node = new_binary_at("==", node, parse_relational(), op->line, op->col);
        } else if (match(TK_NE)) {
            Token *op = prev();
            node = new_binary_at("!=", node, parse_relational(), op->line, op->col);
        } else {
            return node;
        }
    }
}

static Node *parse_logical_and(void) {
    Node *node = parse_equality();
    while (match(TK_AND)) {
        Token *op = prev();
        node = new_binary_at("&&", node, parse_equality(), op->line, op->col);
    }
    return node;
}

static Node *parse_logical_or(void) {
    Node *node = parse_logical_and();
    while (match(TK_OR)) {
        Token *op = prev();
        node = new_binary_at("||", node, parse_logical_and(), op->line, op->col);
    }
    return node;
}

static Node *parse_assignment(void) {
    Node *node = parse_logical_or();
    if (match('=')) {
        Token *op = prev();
        Node *n = new_node_at(ND_ASSIGN, op->line, op->col);
        n->lhs = node;
        n->rhs = parse_assignment();
        return n;
    }
    return node;
}

static Node *parse_expr(void) { return parse_assignment(); }

static Node *parse_decl(bool need_semicolon) {
    Token *start = peek();
    Type ty = parse_type();
    Token *name = NULL;
    if (match(TK_IDENT)) {
        name = prev();
    } else {
        report_expected(peek(), "identifier");
        if (!at_eof() && !is_expr_boundary(peek()->kind) && peek()->kind != '=' && !is_stmt_start(peek()->kind)) pos++;
    }

    Node *n = new_node_at(ND_DECL, name ? name->line : start->line, name ? name->col : start->col);
    n->type = ty;
    n->name = xstrdup(name ? name->lexeme : "__error_decl");
    if (match('=')) n->rhs = parse_expr();
    if (need_semicolon) expect_semicolon();
    return n;
}

static Node *parse_block(void) {
    Token *open = peek();
    expect_delim('{', "'{'");
    Node *block = new_node_at(ND_BLOCK, open->line, open->col);
    while (!at_eof()) {
        if (match('}')) return block;
        if (is_type_keyword(peek()->kind)) node_add(block, parse_decl(true));
        else node_add(block, parse_stmt());
    }
    fprintf(stderr, "parse error line %d col %d: expected '}' before end of file\n", open->line, open->col);
    parse_errors++;
    return block;
}

static Node *parse_stmt(void) {
    Token *t = peek();
    if (peek()->kind == '{') return parse_block();

    if (match(TK_KW_RETURN)) {
        Node *n = new_node_at(ND_RETURN, t->line, t->col);
        n->rhs = parse_expr();
        expect_semicolon();
        return n;
    }

    if (match(TK_KW_IF)) {
        Node *n = new_node_at(ND_IF, t->line, t->col);
        expect_delim('(', "'('");
        n->cond = parse_expr();
        expect_delim(')', "')'");
        n->then_branch = parse_stmt();
        if (match(TK_KW_ELSE)) n->else_branch = parse_stmt();
        return n;
    }

    if (match(TK_KW_WHILE)) {
        Node *n = new_node_at(ND_WHILE, t->line, t->col);
        expect_delim('(', "'('");
        n->cond = parse_expr();
        expect_delim(')', "')'");
        n->body = parse_stmt();
        return n;
    }

    if (match(TK_KW_FOR)) {
        Node *n = new_node_at(ND_FOR, t->line, t->col);
        expect_delim('(', "'('");
        if (match(';')) {
            n->init = NULL;
        } else if (is_type_keyword(peek()->kind)) {
            n->init = parse_decl(true);
        } else {
            n->init = new_node_at(ND_EXPR_STMT, peek()->line, peek()->col);
            n->init->rhs = parse_expr();
            expect_semicolon();
        }
        if (!match(';')) {
            n->cond = parse_expr();
            expect_semicolon();
        }
        if (!match(')')) {
            n->inc = parse_expr();
            expect_delim(')', "')'");
        }
        n->body = parse_stmt();
        return n;
    }

    if (match(';')) return new_node_at(ND_EMPTY, t->line, t->col);

    if (peek()->kind == '}' || at_eof()) return new_node_at(ND_EMPTY, t->line, t->col);

    Node *n = new_node_at(ND_EXPR_STMT, t->line, t->col);
    n->rhs = parse_expr();
    expect_semicolon();
    return n;
}

Node *parse_program(void) {
    Node *prog = new_node_at(ND_PROGRAM, 1, 1);
    Type ret = parse_type();
    Token *name = NULL;
    if (match(TK_IDENT)) {
        name = prev();
    } else {
        report_expected(peek(), "function name");
        if (!at_eof()) pos++;
    }
    expect_delim('(', "'('");
    expect_delim(')', "')'");
    Node *fn = new_node_at(ND_FUNC, name ? name->line : 1, name ? name->col : 1);
    fn->type = ret;
    fn->name = xstrdup(name ? name->lexeme : "__error_function");
    fn->body = parse_block();
    node_add(prog, fn);
    if (!at_eof()) {
        report_expected(peek(), "end of file");
        while (!at_eof()) pos++;
    }
    return prog;
}

// ----------------------------- AST JSON dump -----------------------------

static void indent(FILE *fp, int n) { for (int i = 0; i < n; i++) fputc(' ', fp); }

static int ast_json_next_id = 1;
static Node *ast_json_nodes[4096];
static int ast_json_node_count = 0;

static int ast_node_id(Node *n) {
    for (int i = 0; i < ast_json_node_count; i++) {
        if (ast_json_nodes[i] == n) return i + 1;
    }
    if (ast_json_node_count >= (int)(sizeof(ast_json_nodes) / sizeof(ast_json_nodes[0]))) {
        die("too many AST nodes for JSON dump");
    }
    ast_json_nodes[ast_json_node_count++] = n;
    return ast_json_next_id++;
}

static void dump_ast_node(FILE *fp, Node *n, int ind) {
    if (!n) { fputs("null", fp); return; }
    fprintf(fp, "{\n");
    indent(fp, ind + 2); fprintf(fp, "\"id\":%d,\n", ast_node_id(n));
    indent(fp, ind + 2); fprintf(fp, "\"kind\":"); json_escape(fp, node_kind_name(n->kind));
    fprintf(fp, ",\n");
    indent(fp, ind + 2); fprintf(fp, "\"line\":%d, \"col\":%d", n->line, n->col);
    if (n->name) { fprintf(fp, ",\n"); indent(fp, ind + 2); fprintf(fp, "\"name\":"); json_escape(fp, n->name); }
    if (n->op[0]) { fprintf(fp, ",\n"); indent(fp, ind + 2); fprintf(fp, "\"op\":"); json_escape(fp, n->op); }
    if (n->kind == ND_INT) { fprintf(fp, ",\n"); indent(fp, ind + 2); fprintf(fp, "\"value\":%ld", n->ival); }
    if (n->kind == ND_FLOAT) { fprintf(fp, ",\n"); indent(fp, ind + 2); fprintf(fp, "\"value\":%g", n->fval); }
    if (n->type != TY_UNKNOWN) { fprintf(fp, ",\n"); indent(fp, ind + 2); fprintf(fp, "\"type\":"); json_escape(fp, type_name(n->type)); }
    if (n->slot >= 0) { fprintf(fp, ",\n"); indent(fp, ind + 2); fprintf(fp, "\"slot\":%d", n->slot); }

    #define CHILD(field, label) if (n->field) { fprintf(fp, ",\n"); indent(fp, ind + 2); fprintf(fp, "\"%s\":", (label)); dump_ast_node(fp, n->field, ind + 2); }
    CHILD(lhs, "lhs")
    CHILD(rhs, "rhs")
    CHILD(cond, "condition")
    CHILD(then_branch, "then")
    CHILD(else_branch, "else")
    CHILD(init, "init")
    CHILD(inc, "step")
    CHILD(body, "body")
    #undef CHILD

    fprintf(fp, ",\n");
    indent(fp, ind + 2); fprintf(fp, "\"children\":[");
    bool first_child = true;
    #define CHILD_ITEM(child) if (child) { \
        fprintf(fp, "%s\n", first_child ? "" : ","); \
        indent(fp, ind + 4); \
        dump_ast_node(fp, child, ind + 4); \
        first_child = false; \
    }
    CHILD_ITEM(n->lhs)
    CHILD_ITEM(n->rhs)
    CHILD_ITEM(n->cond)
    CHILD_ITEM(n->then_branch)
    CHILD_ITEM(n->else_branch)
    CHILD_ITEM(n->init)
    CHILD_ITEM(n->inc)
    CHILD_ITEM(n->body)
    for (int i = 0; i < n->len; i++) {
        CHILD_ITEM(n->items[i])
    }
    #undef CHILD_ITEM
    if (!first_child) {
        fprintf(fp, "\n");
        indent(fp, ind + 2);
    }
    fprintf(fp, "]");
    fprintf(fp, "\n");
    indent(fp, ind); fprintf(fp, "}");
}

void dump_ast_json(const char *path, Node *root) {
    FILE *fp = open_out(path);
    ast_json_next_id = 1;
    ast_json_node_count = 0;
    dump_ast_node(fp, root, 0);
    fputc('\n', fp);
    fclose(fp);
}
