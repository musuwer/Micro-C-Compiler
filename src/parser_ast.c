#include "microcc.h"

// ----------------------------- AST and parser -----------------------------

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

static Token *expect(int kind, const char *what) {
    if (peek()->kind == kind) return &tokens.data[pos++];
    fprintf(stderr, "parse error at %d:%d: expected %s, got %s '%s'\n",
            peek()->line, peek()->col, what, token_kind_name(peek()->kind), peek()->lexeme);
    parse_errors++;
    return &tokens.data[pos++];
}

static bool is_type_keyword(int kind) { return kind == TK_KW_INT || kind == TK_KW_FLOAT; }

static Type parse_type(void) {
    if (match(TK_KW_INT)) return TY_INT;
    if (match(TK_KW_FLOAT)) return TY_FLOAT;
    fprintf(stderr, "parse error at %d:%d: expected type keyword\n", peek()->line, peek()->col);
    parse_errors++;
    pos++;
    return TY_UNKNOWN;
}

static Node *parse_stmt(void);
static Node *parse_expr(void);

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
        expect(')', "')'");
        return n;
    }
    fprintf(stderr, "parse error at %d:%d: expected expression, got %s '%s'\n",
            t->line, t->col, token_kind_name(t->kind), t->lexeme);
    parse_errors++;
    pos++;
    return new_node_at(ND_INT, t->line, t->col);
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

// Day3 parser update:
//   Expression parsing is now written as a clear precedence ladder.
//   From high to low: unary -> multiplicative -> additive -> relational
//   -> equality -> logical_and -> logical_or -> assignment.
//   Binary and assignment nodes also record the operator token position,
//   which makes AST JSON easier to explain and later easier to visualize.
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
    Type ty = parse_type();
    Token *name = expect(TK_IDENT, "identifier");
    Node *n = new_node_at(ND_DECL, name->line, name->col);
    n->type = ty;
    n->name = xstrdup(name->lexeme);
    if (match('=')) n->rhs = parse_expr();
    if (need_semicolon) expect(';', "';'");
    return n;
}

static Node *parse_block(void) {
    Token *open = expect('{', "'{' ");
    Node *block = new_node_at(ND_BLOCK, open->line, open->col);
    while (!at_eof() && !match('}')) {
        if (is_type_keyword(peek()->kind)) node_add(block, parse_decl(true));
        else node_add(block, parse_stmt());
    }
    return block;
}

static Node *parse_stmt(void) {
    Token *t = peek();
    if (peek()->kind == '{') return parse_block();

    if (match(TK_KW_RETURN)) {
        Node *n = new_node_at(ND_RETURN, t->line, t->col);
        n->rhs = parse_expr();
        expect(';', "';'");
        return n;
    }

    if (match(TK_KW_IF)) {
        Node *n = new_node_at(ND_IF, t->line, t->col);
        expect('(', "'('");
        n->cond = parse_expr();
        expect(')', "')'");
        n->then_branch = parse_stmt();
        if (match(TK_KW_ELSE)) n->else_branch = parse_stmt();
        return n;
    }

    if (match(TK_KW_WHILE)) {
        Node *n = new_node_at(ND_WHILE, t->line, t->col);
        expect('(', "'('");
        n->cond = parse_expr();
        expect(')', "')'");
        n->body = parse_stmt();
        return n;
    }

    if (match(TK_KW_FOR)) {
        Node *n = new_node_at(ND_FOR, t->line, t->col);
        expect('(', "'('");
        if (match(';')) {
            n->init = NULL;
        } else if (is_type_keyword(peek()->kind)) {
            n->init = parse_decl(true);
        } else {
            n->init = new_node_at(ND_EXPR_STMT, peek()->line, peek()->col);
            n->init->rhs = parse_expr();
            expect(';', "';'");
        }
        if (!match(';')) {
            n->cond = parse_expr();
            expect(';', "';'");
        }
        if (!match(')')) {
            n->inc = parse_expr();
            expect(')', "')'");
        }
        n->body = parse_stmt();
        return n;
    }

    if (match(';')) return new_node_at(ND_EMPTY, t->line, t->col);

    Node *n = new_node_at(ND_EXPR_STMT, t->line, t->col);
    n->rhs = parse_expr();
    expect(';', "';'");
    return n;
}

Node *parse_program(void) {
    Node *prog = new_node_at(ND_PROGRAM, 1, 1);
    Type ret = parse_type();
    Token *name = expect(TK_IDENT, "function name");
    expect('(', "'('");
    expect(')', "')'");
    Node *fn = new_node_at(ND_FUNC, name->line, name->col);
    fn->type = ret;
    fn->name = xstrdup(name->lexeme);
    fn->body = parse_block();
    node_add(prog, fn);
    expect(TK_EOF, "end of file");
    return prog;
}

// ----------------------------- AST JSON dump -----------------------------

static void indent(FILE *fp, int n) { for (int i = 0; i < n; i++) fputc(' ', fp); }

static void dump_ast_node(FILE *fp, Node *n, int ind) {
    if (!n) { fputs("null", fp); return; }
    fprintf(fp, "{\n");
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
    CHILD(cond, "cond")
    CHILD(then_branch, "then")
    CHILD(else_branch, "else")
    CHILD(init, "init")
    CHILD(inc, "inc")
    CHILD(body, "body")
    #undef CHILD

    if (n->len > 0) {
        fprintf(fp, ",\n");
        indent(fp, ind + 2); fprintf(fp, "\"children\":[\n");
        for (int i = 0; i < n->len; i++) {
            indent(fp, ind + 4);
            dump_ast_node(fp, n->items[i], ind + 4);
            fprintf(fp, "%s\n", i == n->len - 1 ? "" : ",");
        }
        indent(fp, ind + 2); fprintf(fp, "]");
    }
    fprintf(fp, "\n");
    indent(fp, ind); fprintf(fp, "}");
}

void dump_ast_json(const char *path, Node *root) {
    FILE *fp = open_out(path);
    dump_ast_node(fp, root, 0);
    fputc('\n', fp);
    fclose(fp);
}

