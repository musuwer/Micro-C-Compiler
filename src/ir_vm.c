#include "microcc.h"

// ----------------------------- TAC generation -----------------------------

typedef struct {
    char **lines;
    int len;
    int cap;
} StrVec;

static void sv_push(StrVec *v, const char *s) {
    if (v->len == v->cap) {
        v->cap = v->cap ? v->cap * 2 : 128;
        v->lines = (char **)realloc(v->lines, sizeof(char *) * (size_t)v->cap);
        if (!v->lines) die("out of memory");
    }
    v->lines[v->len++] = xstrdup(s);
}

static void sv_printf(StrVec *v, const char *fmt, ...) {
    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    sv_push(v, buf);
}

static int temp_id = 0;
static int label_id = 0;
static StrVec tac;

static char *new_temp(void) {
    char buf[32];
    snprintf(buf, sizeof(buf), "t%d", temp_id++);
    return xstrdup(buf);
}

static char *new_label(void) {
    char buf[32];
    snprintf(buf, sizeof(buf), "L%d", label_id++);
    return xstrdup(buf);
}

static char *tac_expr(Node *n);
static void tac_stmt(Node *n);

static char *tac_expr(Node *n) {
    if (!n) return xstrdup("<null>");
    switch (n->kind) {
        case ND_INT: {
            char buf[64];
            snprintf(buf, sizeof(buf), "%ld", n->ival);
            return xstrdup(buf);
        }
        case ND_FLOAT: {
            char buf[64];
            snprintf(buf, sizeof(buf), "%g", n->fval);
            return xstrdup(buf);
        }
        case ND_VAR:
            return xstrdup(n->name);
        case ND_ASSIGN: {
            char *rhs = tac_expr(n->rhs);
            sv_printf(&tac, "%s = %s", n->lhs->name, rhs);
            return xstrdup(n->lhs->name);
        }
        case ND_UNARY: {
            char *rhs = tac_expr(n->rhs);
            char *t = new_temp();
            sv_printf(&tac, "%s = %s%s", t, n->op, rhs);
            return t;
        }
        case ND_BINARY: {
            char *a = tac_expr(n->lhs);
            char *b = tac_expr(n->rhs);
            char *t = new_temp();
            sv_printf(&tac, "%s = %s %s %s", t, a, n->op, b);
            return t;
        }
        default:
            return xstrdup("<expr>");
    }
}

static void tac_block(Node *n) {
    for (int i = 0; i < n->len; i++) tac_stmt(n->items[i]);
}

static void tac_stmt(Node *n) {
    if (!n) return;
    switch (n->kind) {
        case ND_PROGRAM:
            for (int i = 0; i < n->len; i++) tac_stmt(n->items[i]);
            break;
        case ND_FUNC:
            sv_printf(&tac, "function %s:", n->name);
            tac_stmt(n->body);
            sv_push(&tac, "end function");
            break;
        case ND_BLOCK:
            tac_block(n);
            break;
        case ND_DECL:
            sv_printf(&tac, "decl %s %s", type_name(n->type), n->name);
            if (n->rhs) {
                char *r = tac_expr(n->rhs);
                sv_printf(&tac, "%s = %s", n->name, r);
            }
            break;
        case ND_RETURN: {
            char *r = tac_expr(n->rhs);
            sv_printf(&tac, "return %s", r);
            break;
        }
        case ND_IF: {
            char *else_l = new_label();
            char *end_l = new_label();
            char *c = tac_expr(n->cond);
            sv_printf(&tac, "ifz %s goto %s", c, else_l);
            tac_stmt(n->then_branch);
            sv_printf(&tac, "goto %s", end_l);
            sv_printf(&tac, "%s:", else_l);
            if (n->else_branch) tac_stmt(n->else_branch);
            sv_printf(&tac, "%s:", end_l);
            break;
        }
        case ND_WHILE: {
            char *begin_l = new_label();
            char *end_l = new_label();
            sv_printf(&tac, "%s:", begin_l);
            char *c = tac_expr(n->cond);
            sv_printf(&tac, "ifz %s goto %s", c, end_l);
            tac_stmt(n->body);
            sv_printf(&tac, "goto %s", begin_l);
            sv_printf(&tac, "%s:", end_l);
            break;
        }
        case ND_FOR: {
            char *begin_l = new_label();
            char *end_l = new_label();
            if (n->init) tac_stmt(n->init);
            sv_printf(&tac, "%s:", begin_l);
            if (n->cond) {
                char *c = tac_expr(n->cond);
                sv_printf(&tac, "ifz %s goto %s", c, end_l);
            }
            tac_stmt(n->body);
            if (n->inc) tac_expr(n->inc);
            sv_printf(&tac, "goto %s", begin_l);
            sv_printf(&tac, "%s:", end_l);
            break;
        }
        case ND_EXPR_STMT:
            if (n->rhs) tac_expr(n->rhs);
            break;
        case ND_EMPTY:
            break;
        default:
            tac_expr(n);
            break;
    }
}

void dump_tac(const char *path, Node *root) {
    temp_id = 0;
    label_id = 0;
    tac.len = 0;
    tac_stmt(root);
    FILE *fp = open_out(path);
    fprintf(fp, "# Micro C three-address code\n");
    fprintf(fp, "# idx | instruction\n");
    for (int i = 0; i < tac.len; i++) fprintf(fp, "%03d | %s\n", i, tac.lines[i]);
    fclose(fp);
}

// ----------------------------- bytecode and VM -----------------------------

typedef enum {
    BC_PUSHI,
    BC_LOAD,
    BC_STORE,
    BC_POP,
    BC_ADD,
    BC_SUB,
    BC_MUL,
    BC_DIV,
    BC_MOD,
    BC_NEG,
    BC_NOT,
    BC_LT,
    BC_GT,
    BC_LE,
    BC_GE,
    BC_EQ,
    BC_NE,
    BC_AND,
    BC_OR,
    BC_JMP,
    BC_JZ,
    BC_RET,
    BC_HALT
} OpCode;

typedef struct {
    OpCode op;
    long arg;
    int line;
} Instr;

typedef struct {
    Instr *data;
    int len;
    int cap;
} Code;

static Code code;
int codegen_errors = 0;

static const char *op_name(OpCode op) {
    switch (op) {
        case BC_PUSHI: return "PUSHI";
        case BC_LOAD: return "LOAD";
        case BC_STORE: return "STORE";
        case BC_POP: return "POP";
        case BC_ADD: return "ADD";
        case BC_SUB: return "SUB";
        case BC_MUL: return "MUL";
        case BC_DIV: return "DIV";
        case BC_MOD: return "MOD";
        case BC_NEG: return "NEG";
        case BC_NOT: return "NOT";
        case BC_LT: return "LT";
        case BC_GT: return "GT";
        case BC_LE: return "LE";
        case BC_GE: return "GE";
        case BC_EQ: return "EQ";
        case BC_NE: return "NE";
        case BC_AND: return "AND";
        case BC_OR: return "OR";
        case BC_JMP: return "JMP";
        case BC_JZ: return "JZ";
        case BC_RET: return "RET";
        case BC_HALT: return "HALT";
        default: return "?";
    }
}

static int emit(OpCode op, long arg, int line) {
    if (code.len == code.cap) {
        code.cap = code.cap ? code.cap * 2 : 256;
        code.data = (Instr *)realloc(code.data, sizeof(Instr) * (size_t)code.cap);
        if (!code.data) die("out of memory");
    }
    int idx = code.len;
    code.data[code.len++] = (Instr){ op, arg, line };
    return idx;
}

static void patch(int idx, int target) { code.data[idx].arg = target; }

static void gen_expr(Node *n);
static void gen_stmt(Node *n);

static void ensure_int_codegen(Node *n) {
    if (n && n->type == TY_FLOAT) {
        fprintf(stderr, "codegen error line %d: VM demo currently executes integer expressions only\n", n->line);
        codegen_errors++;
    }
}

static void gen_expr(Node *n) {
    if (!n) { emit(BC_PUSHI, 0, 0); return; }
    ensure_int_codegen(n);
    switch (n->kind) {
        case ND_INT:
            emit(BC_PUSHI, n->ival, n->line);
            break;
        case ND_FLOAT:
            emit(BC_PUSHI, (long)n->fval, n->line);
            break;
        case ND_VAR:
            emit(BC_LOAD, n->slot, n->line);
            break;
        case ND_ASSIGN:
            gen_expr(n->rhs);
            emit(BC_STORE, n->lhs->slot, n->line);
            break;
        case ND_UNARY:
            gen_expr(n->rhs);
            if (!strcmp(n->op, "-")) emit(BC_NEG, 0, n->line);
            else if (!strcmp(n->op, "!")) emit(BC_NOT, 0, n->line);
            break;
        case ND_BINARY:
            gen_expr(n->lhs);
            gen_expr(n->rhs);
            if (!strcmp(n->op, "+")) emit(BC_ADD, 0, n->line);
            else if (!strcmp(n->op, "-")) emit(BC_SUB, 0, n->line);
            else if (!strcmp(n->op, "*")) emit(BC_MUL, 0, n->line);
            else if (!strcmp(n->op, "/")) emit(BC_DIV, 0, n->line);
            else if (!strcmp(n->op, "%")) emit(BC_MOD, 0, n->line);
            else if (!strcmp(n->op, "<")) emit(BC_LT, 0, n->line);
            else if (!strcmp(n->op, ">")) emit(BC_GT, 0, n->line);
            else if (!strcmp(n->op, "<=")) emit(BC_LE, 0, n->line);
            else if (!strcmp(n->op, ">=")) emit(BC_GE, 0, n->line);
            else if (!strcmp(n->op, "==")) emit(BC_EQ, 0, n->line);
            else if (!strcmp(n->op, "!=")) emit(BC_NE, 0, n->line);
            else if (!strcmp(n->op, "&&")) emit(BC_AND, 0, n->line);
            else if (!strcmp(n->op, "||")) emit(BC_OR, 0, n->line);
            break;
        default:
            fprintf(stderr, "codegen error line %d: unsupported expression node %s\n", n->line, node_kind_name(n->kind));
            codegen_errors++;
            emit(BC_PUSHI, 0, n->line);
            break;
    }
}

static void gen_block(Node *n) {
    for (int i = 0; i < n->len; i++) gen_stmt(n->items[i]);
}

static void gen_stmt(Node *n) {
    if (!n) return;
    switch (n->kind) {
        case ND_PROGRAM:
            for (int i = 0; i < n->len; i++) gen_stmt(n->items[i]);
            break;
        case ND_FUNC:
            gen_stmt(n->body);
            emit(BC_PUSHI, 0, n->line);
            emit(BC_RET, 0, n->line);
            break;
        case ND_BLOCK:
            gen_block(n);
            break;
        case ND_DECL:
            if (n->rhs) gen_expr(n->rhs);
            else emit(BC_PUSHI, 0, n->line);
            emit(BC_STORE, n->slot, n->line);
            emit(BC_POP, 0, n->line);
            break;
        case ND_RETURN:
            gen_expr(n->rhs);
            emit(BC_RET, 0, n->line);
            break;
        case ND_EXPR_STMT:
            if (n->rhs) {
                gen_expr(n->rhs);
                emit(BC_POP, 0, n->line);
            }
            break;
        case ND_IF: {
            gen_expr(n->cond);
            int jz = emit(BC_JZ, -1, n->line);
            gen_stmt(n->then_branch);
            int jmp = emit(BC_JMP, -1, n->line);
            patch(jz, code.len);
            if (n->else_branch) gen_stmt(n->else_branch);
            patch(jmp, code.len);
            break;
        }
        case ND_WHILE: {
            int begin = code.len;
            gen_expr(n->cond);
            int jz = emit(BC_JZ, -1, n->line);
            gen_stmt(n->body);
            emit(BC_JMP, begin, n->line);
            patch(jz, code.len);
            break;
        }
        case ND_FOR: {
            if (n->init) gen_stmt(n->init);
            int begin = code.len;
            int jz = -1;
            if (n->cond) {
                gen_expr(n->cond);
                jz = emit(BC_JZ, -1, n->line);
            }
            gen_stmt(n->body);
            if (n->inc) { gen_expr(n->inc); emit(BC_POP, 0, n->line); }
            emit(BC_JMP, begin, n->line);
            if (jz >= 0) patch(jz, code.len);
            break;
        }
        case ND_EMPTY:
            break;
        default:
            fprintf(stderr, "codegen error line %d: unsupported statement node %s\n", n->line, node_kind_name(n->kind));
            codegen_errors++;
            break;
    }
}

void generate_bytecode(Node *root) {
    gen_stmt(root);
    emit(BC_HALT, 0, 0);
}

void dump_bytecode(const char *path) {
    FILE *fp = open_out(path);
    for (int i = 0; i < code.len; i++) {
        fprintf(fp, "%04d  %-7s", i, op_name(code.data[i].op));
        switch (code.data[i].op) {
            case BC_PUSHI: case BC_LOAD: case BC_STORE: case BC_JMP: case BC_JZ:
                fprintf(fp, " %ld", code.data[i].arg);
                break;
            default:
                break;
        }
        fprintf(fp, "\n");
    }
    fclose(fp);
}

static long pop(long *stack, int *sp) {
    if (*sp <= 0) die("VM stack underflow");
    return stack[--(*sp)];
}

static void push(long *stack, int *sp, long v) {
    if (*sp >= 1024) die("VM stack overflow");
    stack[(*sp)++] = v;
}

static void trace_vm_state(int pc, Instr in, long *stack, int sp, int next_pc) {
    fprintf(stderr, "pc=%04d instr=%-7s", pc, op_name(in.op));
    switch (in.op) {
        case BC_PUSHI: case BC_LOAD: case BC_STORE: case BC_JMP: case BC_JZ:
            fprintf(stderr, " arg=%ld", in.arg);
            break;
        default:
            break;
    }
    fprintf(stderr, " stack=[");
    for (int i = 0; i < sp; i++) {
        fprintf(stderr, "%s%ld", i == 0 ? "" : ", ", stack[i]);
    }
    fprintf(stderr, "] next_pc=%04d\n", next_pc);
}

long run_vm(bool trace) {
    long stack[1024] = {0};
    long vars[512] = {0};
    int sp = 0;
    int pc = 0;
    while (pc >= 0 && pc < code.len) {
        int cur_pc = pc;
        Instr in = code.data[pc];
        pc++;
        long a, b;
        switch (in.op) {
            case BC_PUSHI: push(stack, &sp, in.arg); break;
            case BC_LOAD: push(stack, &sp, vars[in.arg]); break;
            case BC_STORE: vars[in.arg] = stack[sp - 1]; break;
            case BC_POP: (void)pop(stack, &sp); break;
            case BC_ADD: b = pop(stack, &sp); a = pop(stack, &sp); push(stack, &sp, a + b); break;
            case BC_SUB: b = pop(stack, &sp); a = pop(stack, &sp); push(stack, &sp, a - b); break;
            case BC_MUL: b = pop(stack, &sp); a = pop(stack, &sp); push(stack, &sp, a * b); break;
            case BC_DIV: b = pop(stack, &sp); a = pop(stack, &sp); if (b == 0) die("division by zero"); push(stack, &sp, a / b); break;
            case BC_MOD: b = pop(stack, &sp); a = pop(stack, &sp); if (b == 0) die("mod by zero"); push(stack, &sp, a % b); break;
            case BC_NEG: a = pop(stack, &sp); push(stack, &sp, -a); break;
            case BC_NOT: a = pop(stack, &sp); push(stack, &sp, !a); break;
            case BC_LT: b = pop(stack, &sp); a = pop(stack, &sp); push(stack, &sp, a < b); break;
            case BC_GT: b = pop(stack, &sp); a = pop(stack, &sp); push(stack, &sp, a > b); break;
            case BC_LE: b = pop(stack, &sp); a = pop(stack, &sp); push(stack, &sp, a <= b); break;
            case BC_GE: b = pop(stack, &sp); a = pop(stack, &sp); push(stack, &sp, a >= b); break;
            case BC_EQ: b = pop(stack, &sp); a = pop(stack, &sp); push(stack, &sp, a == b); break;
            case BC_NE: b = pop(stack, &sp); a = pop(stack, &sp); push(stack, &sp, a != b); break;
            case BC_AND: b = pop(stack, &sp); a = pop(stack, &sp); push(stack, &sp, a && b); break;
            case BC_OR: b = pop(stack, &sp); a = pop(stack, &sp); push(stack, &sp, a || b); break;
            case BC_JMP: pc = (int)in.arg; break;
            case BC_JZ: a = pop(stack, &sp); if (!a) pc = (int)in.arg; break;
            case BC_RET: {
                long ret = pop(stack, &sp);
                if (trace) trace_vm_state(cur_pc, in, stack, sp, -1);
                return ret;
            }
            case BC_HALT: {
                long ret = sp > 0 ? pop(stack, &sp) : 0;
                if (trace) trace_vm_state(cur_pc, in, stack, sp, -1);
                return ret;
            }
        }
        if (trace) trace_vm_state(cur_pc, in, stack, sp, pc);
    }
    die("VM program counter out of range");
    return 0;
}

