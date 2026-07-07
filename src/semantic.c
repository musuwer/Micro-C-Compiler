#include "microcc.h"

// ----------------------------- semantic analysis -----------------------------
// Day2 note: examples/lexer_full_demo.mc is a full lexer coverage program.
// The semantic analyzer accepts it as normal MiniC source, proving that tokens
// produced from comments, integer/float literals and logical operators can flow
// through parsing and semantic checking without breaking the compiler pipeline.
//
// Day3 update:
//   infer_expr_type(Node *n) is exposed as the expression type inference entry.
//   Statement checking still uses analyze(Node *n), while conditions,
//   assignments and expression statements call infer_expr_type() explicitly.
//   This keeps the semantic module ready for later int/float checking work.
// Day5 update:
//   1. Symbol table entries now track initialization and usage status.
//      These flags enable warnings for uninitialized variable use and help the
//      Web frontend display complete symbol information.
//   2. The symbols.json output includes "initialized" and "used" boolean fields.
//   3. Variable assignment marks the left-hand side as initialized.
//   4. Variable reference marks the symbol as used and warns if uninitialized.
//   5. Scope shadowing is verified through separate slot allocation per depth.
//

typedef struct {
    char *name;
    Type type;
    int depth;
    int slot;
    int line;
    bool active;
    bool initialized;
    bool used;       
} Symbol;

static Symbol syms[512];
static int sym_count = 0;
static int active_depth = 0;
static int next_slot = 0;
int sem_errors = 0;

// Day4 semantic update:
//   Control-flow conditions are checked through check_condition().  While the
//   ordinary expression inference logic is reused, this context string lets
//   diagnostics say whether an undeclared variable appears inside an if, while
//   or for condition.  It is intentionally small and local to this module so
//   later Day5/Day6 type-checking work can reuse the same inference entry.
static const char *condition_context = NULL;

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

static Symbol *add_symbol(const char *name, Type type, int line, bool has_init) {
    if (sym_count >= (int)(sizeof(syms) / sizeof(syms[0]))) die("too many symbols");
    Symbol *s = &syms[sym_count++];
    s->name = xstrdup(name);
    s->type = type;
    s->depth = active_depth;
    s->slot = next_slot++;
    s->line = line;
    s->active = true;
    s->initialized = has_init;
    s->used = false;    
    return s;
}

static void leave_scope(void) {
    for (int i = 0; i < sym_count; i++)
        if (syms[i].active && syms[i].depth == active_depth) syms[i].active = false;
    active_depth--;
}

static bool is_numeric(Type t) { return t == TY_INT || t == TY_FLOAT; }

static bool is_comparison_or_logic_op(const char *op) {
    return !strcmp(op, "<") || !strcmp(op, ">") || !strcmp(op, "<=") || !strcmp(op, ">=") ||
           !strcmp(op, "==") || !strcmp(op, "!=") || !strcmp(op, "&&") || !strcmp(op, "||");
}

Type analyze(Node *n);

Type infer_expr_type(Node *n) {
    if (!n) return TY_UNKNOWN;

    switch (n->kind) {
        case ND_ASSIGN: {
            if (!n->lhs || n->lhs->kind != ND_VAR) {
                fprintf(stderr, "semantic error line %d: left side of assignment must be a variable\n", n->line);
                sem_errors++;
                n->type = TY_UNKNOWN;
                return n->type;
            }
            Type lhs = infer_expr_type(n->lhs);
            Type rhs = infer_expr_type(n->rhs);
            if (n->lhs && n->lhs->kind == ND_VAR) {
                Symbol *s = find_symbol(n->lhs->name);
                if (s) s->initialized = true;
            }
            n->type = lhs;
            n->slot = n->lhs->slot;
            if (lhs == TY_INT && rhs == TY_FLOAT) {
                fprintf(stderr, "semantic error line %d: cannot assign float expression to int variable '%s'\n", n->line, n->lhs->name);
                sem_errors++;
            }
            return n->type;
        }
        case ND_BINARY: {
            Type a = infer_expr_type(n->lhs);
            Type b = infer_expr_type(n->rhs);
            if (!is_numeric(a) || !is_numeric(b)) {
                fprintf(stderr, "semantic error line %d: binary operator '%s' expects numeric operands\n", n->line, n->op);
                sem_errors++;
            }
            if (is_comparison_or_logic_op(n->op))
                n->type = TY_INT;
            else
                n->type = (a == TY_FLOAT || b == TY_FLOAT) ? TY_FLOAT : TY_INT;
            return n->type;
        }
        case ND_UNARY: {
            Type rhs = infer_expr_type(n->rhs);
            n->type = !strcmp(n->op, "!") ? TY_INT : rhs;
            if (!is_numeric(rhs)) {
                fprintf(stderr, "semantic error line %d: unary operator '%s' expects numeric operand\n", n->line, n->op);
                sem_errors++;
            }
            return n->type;
        }
        case ND_INT:
            n->type = TY_INT;
            return n->type;
        case ND_FLOAT:
            n->type = TY_FLOAT;
            return n->type;
        case ND_VAR: {
            Symbol *s = find_symbol(n->name);
            if (!s) {
                if (condition_context) {
                    fprintf(stderr, "semantic error line %d: undeclared variable '%s' in %s condition\n",
                            n->line, n->name, condition_context);
                } else {
                    fprintf(stderr, "semantic error line %d: undeclared variable '%s'\n", n->line, n->name);
                }
                sem_errors++;
                n->type = TY_UNKNOWN;
                return n->type;
            }
            s->used = true; 
            if (!s->initialized) {
                fprintf(stderr, "semantic warning line %d: variable '%s' used before initialization\n", n->line, n->name);
            }
            n->type = s->type;
            n->slot = s->slot;
            return n->type;
        }
        default:
            return TY_UNKNOWN;
    }
}

static Type analyze_block(Node *n) {
    active_depth++;
    for (int i = 0; i < n->len; i++) analyze(n->items[i]);
    leave_scope();
    n->type = TY_UNKNOWN;
    return n->type;
}

static Type check_condition(Node *cond, Node *owner, const char *kind) {
    const char *old_context = condition_context;
    condition_context = kind;
    Type ty = infer_expr_type(cond);
    condition_context = old_context;

    if (!is_numeric(ty)) {
        fprintf(stderr, "semantic error line %d: %s condition must be numeric\n", owner->line, kind);
        sem_errors++;
    }
    return ty;
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
                Symbol *s = add_symbol(n->name, n->type, n->line, n->rhs != NULL);
                n->slot = s->slot;
            }
            if (n->rhs) {
                Type rhs = infer_expr_type(n->rhs);
                if (n->type == TY_INT && rhs == TY_FLOAT) {
                    fprintf(stderr, "semantic error line %d: cannot assign float expression to int variable '%s'\n", n->line, n->name);
                    sem_errors++;
                }
            }
            return n->type;
        }
        case ND_RETURN:
            return infer_expr_type(n->rhs);
        case ND_IF:
            check_condition(n->cond, n, "if");
            analyze(n->then_branch);
            if (n->else_branch) analyze(n->else_branch);
            return TY_UNKNOWN;
        case ND_WHILE:
            check_condition(n->cond, n, "while");
            analyze(n->body);
            return TY_UNKNOWN;
        case ND_FOR:
            active_depth++;
            if (n->init) analyze(n->init);
            if (n->cond) check_condition(n->cond, n, "for");
            if (n->inc) infer_expr_type(n->inc);
            analyze(n->body);
            leave_scope();
            return TY_UNKNOWN;
        case ND_EXPR_STMT:
            return n->rhs ? infer_expr_type(n->rhs) : TY_UNKNOWN;
        case ND_ASSIGN:
        case ND_BINARY:
        case ND_UNARY:
        case ND_INT:
        case ND_FLOAT:
        case ND_VAR:
            return infer_expr_type(n);
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
        fprintf(fp, ", \"scopeDepth\":%d, \"slot\":%d, \"declLine\":%d",
                syms[i].depth, syms[i].slot, syms[i].line);
        fprintf(fp, ", \"scope_depth\":%d, \"decl_line\":%d",
                syms[i].depth, syms[i].line);
        fprintf(fp, ", \"initialized\":%s, \"used\":%s", 
                syms[i].initialized ? "true" : "false",
                syms[i].used ? "true" : "false");
        fprintf(fp, "}%s\n", i == sym_count - 1 ? "" : ",");
    }
    fprintf(fp, "]\n");
    fclose(fp);
}
