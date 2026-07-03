#ifndef MICROCC_H
#define MICROCC_H

#include <ctype.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

// -----------------------------------------------------------------------------
// Shared utilities
// -----------------------------------------------------------------------------
static inline void die(const char *msg) {
    fprintf(stderr, "error: %s\n", msg);
    exit(1);
}

static inline void *xmalloc(size_t size) {
    void *p = malloc(size ? size : 1);
    if (!p) die("out of memory");
    memset(p, 0, size);
    return p;
}

static inline char *xstrndup(const char *s, size_t n) {
    char *p = (char *)xmalloc(n + 1);
    memcpy(p, s, n);
    p[n] = '\0';
    return p;
}

static inline char *xstrdup(const char *s) {
    return xstrndup(s, strlen(s));
}

static inline char *read_file(const char *path) {
    FILE *fp = fopen(path, "rb");
    if (!fp) {
        fprintf(stderr, "cannot open %s: %s\n", path, strerror(errno));
        exit(1);
    }
    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    rewind(fp);
    char *buf = (char *)xmalloc((size_t)size + 1);
    if (fread(buf, 1, (size_t)size, fp) != (size_t)size) die("failed to read file");
    fclose(fp);
    buf[size] = '\0';
    return buf;
}

static inline FILE *open_out(const char *path) {
    FILE *fp = fopen(path, "w");
    if (!fp) {
        fprintf(stderr, "cannot write %s: %s\n", path, strerror(errno));
        exit(1);
    }
    return fp;
}

static inline void json_escape(FILE *fp, const char *s) {
    fputc('"', fp);
    for (; *s; s++) {
        unsigned char c = (unsigned char)*s;
        if (c == '"') fputs("\\\"", fp);
        else if (c == '\\') fputs("\\\\", fp);
        else if (c == '\n') fputs("\\n", fp);
        else if (c == '\r') fputs("\\r", fp);
        else if (c == '\t') fputs("\\t", fp);
        else if (c < 32) fprintf(fp, "\\u%04x", c);
        else fputc(c, fp);
    }
    fputc('"', fp);
}

// -----------------------------------------------------------------------------
// Lexer public types
// -----------------------------------------------------------------------------
enum {
    TK_EOF = 256,
    TK_IDENT,
    TK_INT_LITERAL,
    TK_FLOAT_LITERAL,
    TK_KW_INT,
    TK_KW_FLOAT,
    TK_KW_IF,
    TK_KW_ELSE,
    TK_KW_WHILE,
    TK_KW_FOR,
    TK_KW_RETURN,
    TK_EQ,
    TK_NE,
    TK_LE,
    TK_GE,
    TK_AND,
    TK_OR
};
//add line/col
typedef struct {
    int kind;
    char *lexeme;
    long ival;
    double fval;
    int line;
    int col;
    int len;
} Token;

typedef struct {
    Token *data;
    int len;
    int cap;
} TokenVec;

extern TokenVec tokens;
extern int pos;
extern int lex_errors;

const char *token_kind_name(int kind);
void lex(const char *src);
void dump_tokens_json(const char *path);

// -----------------------------------------------------------------------------
// AST / parser public types
// -----------------------------------------------------------------------------
typedef enum { TY_INT, TY_FLOAT, TY_UNKNOWN } Type;
const char *type_name(Type t);

typedef enum {
    ND_PROGRAM,
    ND_FUNC,
    ND_BLOCK,
    ND_DECL,
    ND_RETURN,
    ND_IF,
    ND_WHILE,
    ND_FOR,
    ND_EXPR_STMT,
    ND_ASSIGN,
    ND_BINARY,
    ND_UNARY,
    ND_INT,
    ND_FLOAT,
    ND_VAR,
    ND_EMPTY
} NodeKind;

typedef struct Node Node;
struct Node {
    NodeKind kind;
    int line;
    int col;
    Type type;
    char *name;
    char op[4];
    long ival;
    double fval;
    int slot;
    Node *lhs;
    Node *rhs;
    Node *cond;
    Node *then_branch;
    Node *else_branch;
    Node *init;
    Node *inc;
    Node *body;
    Node **items;
    int len;
    int cap;
};

extern int parse_errors;
const char *node_kind_name(NodeKind k);
Node *parse_program(void);
void dump_ast_json(const char *path, Node *root);

// -----------------------------------------------------------------------------
// Semantic analysis
// -----------------------------------------------------------------------------
extern int sem_errors;
Type infer_expr_type(Node *n);
Type analyze(Node *n);
void dump_symbols_json(const char *path);

// -----------------------------------------------------------------------------
// Intermediate representation, bytecode and VM
// -----------------------------------------------------------------------------
extern int codegen_errors;
void dump_tac(const char *path, Node *root);
void generate_bytecode(Node *root);
void dump_bytecode(const char *path);
long run_vm(bool trace);

#endif
