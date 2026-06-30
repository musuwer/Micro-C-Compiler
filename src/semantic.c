#include "microcc.h"

// ----------------------------- semantic analysis -----------------------------

typedef struct {
    char *name;
    Type type;
    int depth;
    int slot;
    int line;
    bool active;
} Symbol;

static Symbol syms[512];
static int sym_count = 0;
static int active_depth = 0;
static int next_slot = 0;
int sem_errors = 0;

static Symbol *find_symbol(const char *name) {
    for (int i = sym_count - 1; i >= 0; i--)
        if (syms[i].active && !strcmp(syms[i].name, name)) return &syms[i];
    return NULL;
}

static Symbol *find_in_current_scope(const char *name) {
    for (int i = sym_count - 1; i >= 0; i--)
        if (syms[i].active && syms[i].depth == active_depth && !strcmp(syms[i].name, name)) return &syms[i];
    return NULL;
}

static Symbol *add_symbol(const char *name, Type type, int line) {
    if (sym_count >= (int)(sizeof(syms) / sizeof(syms[0]))) die("too many symbols");
    Symbol *s = &syms[sym_count++];
    s->name = xstrdup(name);
    s->type = type;
    s->depth = active_depth;
    s->slot = next_slot++;
    s->line = line;
    s->active = true;
    return s;
}

static void leave_scope(void) {
    for (int i = 0; i < sym_count; i++)
        if (syms[i].active && syms[i].depth == active_depth) syms[i].active = false;
    active_depth--;
}

static bool is_numeric(Type t) { return t == TY_INT || t == TY_FLOAT; }
Type analyze(Node *n);

static Type analyze_block(Node *n) {
    active_depth++;
    for (int i = 0; i < n->len; i++) analyze(n->items[i]);
    leave_scope();
    n->type = TY_UNKNOWN;
    return n->type;
}

Type analyze(Node *n) {
    if (!n) return TY_UNKNOWN;
    switch (n->kind) {
        case ND_PROGRAM:
            for (int i = 0; i < n->len; i++) analyze(n->items[i]);
            return TY_UNKNOWN;
        case ND_FUNC:
            if (strcmp(n->name, "main") != 0) {
                fprintf(stderr, "semantic error line %d: demo only supports function main()\n", n->line);
                sem_errors++;
            }
            analyze(n->body);
            return n->type;
        case ND_BLOCK:
            return analyze_block(n);
        case ND_DECL: {
            if (find_in_current_scope(n->name)) {
                fprintf(stderr, "semantic error line %d: duplicate declaration of '%s'\n", n->line, n->name);
                sem_errors++;
            } else {
                Symbol *s = add_symbol(n->name, n->type, n->line);
                n->slot = s->slot;
            }
            if (n->rhs) {
                Type rhs = analyze(n->rhs);
                if (n->type == TY_INT && rhs == TY_FLOAT) {
                    fprintf(stderr, "semantic error line %d: cannot assign float expression to int variable '%s'\n", n->line, n->name);
                    sem_errors++;
                }
            }
            return n->type;
        }
        case ND_RETURN:
            return analyze(n->rhs);
        case ND_IF:
            if (!is_numeric(analyze(n->cond))) {
                fprintf(stderr, "semantic error line %d: if condition must be numeric\n", n->line);
                sem_errors++;
            }
            analyze(n->then_branch);
            if (n->else_branch) analyze(n->else_branch);
            return TY_UNKNOWN;
        case ND_WHILE:
            if (!is_numeric(analyze(n->cond))) {
                fprintf(stderr, "semantic error line %d: while condition must be numeric\n", n->line);
                sem_errors++;
            }
            analyze(n->body);
            return TY_UNKNOWN;
        case ND_FOR:
            active_depth++;
            if (n->init) analyze(n->init);
            if (n->cond && !is_numeric(analyze(n->cond))) {
                fprintf(stderr, "semantic error line %d: for condition must be numeric\n", n->line);
                sem_errors++;
            }
            if (n->inc) analyze(n->inc);
            analyze(n->body);
            leave_scope();
            return TY_UNKNOWN;
        case ND_EXPR_STMT:
            return n->rhs ? analyze(n->rhs) : TY_UNKNOWN;
        case ND_ASSIGN: {
            if (!n->lhs || n->lhs->kind != ND_VAR) {
                fprintf(stderr, "semantic error line %d: left side of assignment must be a variable\n", n->line);
                sem_errors++;
                return TY_UNKNOWN;
            }
            Type lhs = analyze(n->lhs);
            Type rhs = analyze(n->rhs);
            n->type = lhs;
            n->slot = n->lhs->slot;
            if (lhs == TY_INT && rhs == TY_FLOAT) {
                fprintf(stderr, "semantic error line %d: cannot assign float expression to int variable '%s'\n", n->line, n->lhs->name);
                sem_errors++;
            }
            return n->type;
        }
        case ND_BINARY: {
            Type a = analyze(n->lhs);
            Type b = analyze(n->rhs);
            if (!is_numeric(a) || !is_numeric(b)) {
                fprintf(stderr, "semantic error line %d: binary operator '%s' expects numeric operands\n", n->line, n->op);
                sem_errors++;
            }
            if (!strcmp(n->op, "<") || !strcmp(n->op, ">") || !strcmp(n->op, "<=") || !strcmp(n->op, ">=") ||
                !strcmp(n->op, "==") || !strcmp(n->op, "!=") || !strcmp(n->op, "&&") || !strcmp(n->op, "||"))
                n->type = TY_INT;
            else
                n->type = (a == TY_FLOAT || b == TY_FLOAT) ? TY_FLOAT : TY_INT;
            return n->type;
        }
        case ND_UNARY:
            n->type = analyze(n->rhs);
            if (!strcmp(n->op, "!")) n->type = TY_INT;
            return n->type;
        case ND_INT:
            n->type = TY_INT;
            return n->type;
        case ND_FLOAT:
            n->type = TY_FLOAT;
            return n->type;
        case ND_VAR: {
            Symbol *s = find_symbol(n->name);
            if (!s) {
                fprintf(stderr, "semantic error line %d: undeclared variable '%s'\n", n->line, n->name);
                sem_errors++;
                n->type = TY_UNKNOWN;
                return n->type;
            }
            n->type = s->type;
            n->slot = s->slot;
            return n->type;
        }
        case ND_EMPTY:
            return TY_UNKNOWN;
    }
    return TY_UNKNOWN;
}

void dump_symbols_json(const char *path) {
    FILE *fp = open_out(path);
    fprintf(fp, "[\n");
    for (int i = 0; i < sym_count; i++) {
        fprintf(fp, "  {\"name\":"); json_escape(fp, syms[i].name);
        fprintf(fp, ", \"type\":"); json_escape(fp, type_name(syms[i].type));
        fprintf(fp, ", \"scope_depth\":%d, \"slot\":%d, \"decl_line\":%d}%s\n",
                syms[i].depth, syms[i].slot, syms[i].line, i == sym_count - 1 ? "" : ",");
    }
    fprintf(fp, "]\n");
    fclose(fp);
}

