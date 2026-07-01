#include "microcc.h"

// ----------------------------- lexer -----------------------------
// Day2 update:
//   1. Each token records line, column and length.
//   2. Lexical errors use unified diagnostics: lex error line X col Y: ...
//   3. Illegal characters, malformed floating literals and unclosed block
//      comments are detected before the parser runs.
// Day3 update:
//   1. Centralize logical/comparison operator recognition.
//   2. Ensure &&, ||, !, ==, !=, <= and >= all appear as explicit tokens
//      for expression-precedence parser tests.

TokenVec tokens;
int pos = 0;
int lex_errors = 0;

static void token_push(Token t) {
    if (tokens.len == tokens.cap) {
        tokens.cap = tokens.cap ? tokens.cap * 2 : 256;
        tokens.data = (Token *)realloc(tokens.data, sizeof(Token) * (size_t)tokens.cap);
        if (!tokens.data) die("out of memory");
    }
    tokens.data[tokens.len++] = t;
}

static void lex_error_at(int line, int col, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    fprintf(stderr, "lex error line %d col %d: ", line, col);
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    va_end(ap);
    lex_errors++;
}

const char *token_kind_name(int kind) {
    switch (kind) {
        case TK_EOF: return "EOF";
        case TK_IDENT: return "IDENT";
        case TK_INT_LITERAL: return "INT_LITERAL";
        case TK_FLOAT_LITERAL: return "FLOAT_LITERAL";
        case TK_KW_INT: return "KW_INT";
        case TK_KW_FLOAT: return "KW_FLOAT";
        case TK_KW_IF: return "KW_IF";
        case TK_KW_ELSE: return "KW_ELSE";
        case TK_KW_WHILE: return "KW_WHILE";
        case TK_KW_FOR: return "KW_FOR";
        case TK_KW_RETURN: return "KW_RETURN";
        case TK_EQ: return "==";
        case TK_NE: return "!=";
        case TK_LE: return "<=";
        case TK_GE: return ">=";
        case TK_AND: return "&&";
        case TK_OR: return "||";
        default: {
            static char buf[8];
            if (kind > 0 && kind < 128 && isprint(kind)) snprintf(buf, sizeof(buf), "%c", kind);
            else snprintf(buf, sizeof(buf), "#%d", kind);
            return buf;
        }
    }
}

static int keyword_kind(const char *s) {
    if (!strcmp(s, "int")) return TK_KW_INT;
    if (!strcmp(s, "float")) return TK_KW_FLOAT;
    if (!strcmp(s, "if")) return TK_KW_IF;
    if (!strcmp(s, "else")) return TK_KW_ELSE;
    if (!strcmp(s, "while")) return TK_KW_WHILE;
    if (!strcmp(s, "for")) return TK_KW_FOR;
    if (!strcmp(s, "return")) return TK_KW_RETURN;
    return TK_IDENT;
}

static Token make_token(int kind, const char *start, int len, int line, int col) {
    Token t = {0};
    t.kind = kind;
    t.lexeme = xstrndup(start, (size_t)len);
    t.line = line;
    t.col = col;
    t.len = len;
    return t;
}

static int two_char_operator_kind(char a, char b) {
    if (a == '=' && b == '=') return TK_EQ;
    if (a == '!' && b == '=') return TK_NE;
    if (a == '<' && b == '=') return TK_LE;
    if (a == '>' && b == '=') return TK_GE;
    if (a == '&' && b == '&') return TK_AND;
    if (a == '|' && b == '|') return TK_OR;
    return 0;
}

static bool is_single_char_token(char c) {
    return strchr("+-*/%(){};=<>!,", c) != NULL;
}

void lex(const char *src) {
    int line = 1, col = 1;
    const char *p = src;
    while (*p) {
        if (*p == ' ' || *p == '\t' || *p == '\r') { p++; col++; continue; }
        if (*p == '\n') { p++; line++; col = 1; continue; }

        // Single-line comment: skip until newline, but keep newline for line count.
        if (p[0] == '/' && p[1] == '/') {
            p += 2; col += 2;
            while (*p && *p != '\n') { p++; col++; }
            continue;
        }

        // Multi-line comment: track the opening position for clear diagnostics.
        if (p[0] == '/' && p[1] == '*') {
            int start_line = line;
            int start_col = col;
            p += 2; col += 2;
            bool closed = false;
            while (*p) {
                if (p[0] == '*' && p[1] == '/') { p += 2; col += 2; closed = true; break; }
                if (*p == '\n') { p++; line++; col = 1; }
                else { p++; col++; }
            }
            if (!closed) lex_error_at(start_line, start_col, "unclosed block comment");
            continue;
        }

        if (isalpha((unsigned char)*p) || *p == '_') {
            const char *start = p;
            int start_col = col;
            while (isalnum((unsigned char)*p) || *p == '_') { p++; col++; }
            int len = (int)(p - start);
            Token t = make_token(TK_IDENT, start, len, line, start_col);
            t.kind = keyword_kind(t.lexeme);
            token_push(t);
            continue;
        }

        if (isdigit((unsigned char)*p)) {
            const char *start = p;
            int start_col = col;
            bool is_float = false;
            while (isdigit((unsigned char)*p)) { p++; col++; }
            if (*p == '.') {
                is_float = true;
                p++; col++;
                while (isdigit((unsigned char)*p)) { p++; col++; }
            }
            if (*p == '.') {
                const char *bad_start = start;
                int bad_col = start_col;
                while (isdigit((unsigned char)*p) || *p == '.') { p++; col++; }
                lex_error_at(line, bad_col, "malformed floating literal '%.*s'", (int)(p - bad_start), bad_start);
                continue;
            }
            int len = (int)(p - start);
            Token t = make_token(is_float ? TK_FLOAT_LITERAL : TK_INT_LITERAL, start, len, line, start_col);
            if (is_float) t.fval = strtod(t.lexeme, NULL);
            else t.ival = strtol(t.lexeme, NULL, 10);
            token_push(t);
            continue;
        }

        int two_kind = two_char_operator_kind(p[0], p[1]);
        if (two_kind) {
            Token t = make_token(two_kind, p, 2, line, col);
            token_push(t);
            p += 2;
            col += 2;
            continue;
        }

        if (is_single_char_token(*p)) {
            Token t = make_token((unsigned char)*p, p, 1, line, col);
            token_push(t);
            p++;
            col++;
            continue;
        }

        lex_error_at(line, col, "unexpected character '%c'", *p);
        p++; col++;
    }
    Token eof = { .kind = TK_EOF, .lexeme = xstrdup(""), .line = line, .col = col, .len = 0 };
    token_push(eof);
}

void dump_tokens_json(const char *path) {
    FILE *fp = open_out(path);
    fprintf(fp, "[\n");
    for (int i = 0; i < tokens.len; i++) {
        Token *t = &tokens.data[i];
        fprintf(fp, "  {\"kind\":");
        json_escape(fp, token_kind_name(t->kind));
        fprintf(fp, ", \"lexeme\":");
        json_escape(fp, t->lexeme);
        fprintf(fp, ", \"line\":%d, \"col\":%d, \"length\":%d", t->line, t->col, t->len);
        if (t->kind == TK_INT_LITERAL) fprintf(fp, ", \"value\":%ld", t->ival);
        if (t->kind == TK_FLOAT_LITERAL) fprintf(fp, ", \"value\":%g", t->fval);
        fprintf(fp, "}%s\n", i == tokens.len - 1 ? "" : ",");
    }
    fprintf(fp, "]\n");
    fclose(fp);
}

