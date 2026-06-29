#include "microcc.h"

// ----------------------------- lexer -----------------------------

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

void lex(const char *src) {
    int line = 1, col = 1;
    const char *p = src;
    while (*p) {
        if (*p == ' ' || *p == '\t' || *p == '\r') { p++; col++; continue; }
        if (*p == '\n') { p++; line++; col = 1; continue; }

        // Single-line comment
        if (p[0] == '/' && p[1] == '/') {
            p += 2; col += 2;
            while (*p && *p != '\n') { p++; col++; }
            continue;
        }

        // Multi-line comment
        if (p[0] == '/' && p[1] == '*') {
            p += 2; col += 2;
            bool closed = false;
            while (*p) {
                if (p[0] == '*' && p[1] == '/') { p += 2; col += 2; closed = true; break; }
                if (*p == '\n') { p++; line++; col = 1; }
                else { p++; col++; }
            }
            if (!closed) {
                fprintf(stderr, "lex error: unclosed block comment at line %d\n", line);
                lex_errors++;
            }
            continue;
        }

        Token t = {0};
        t.line = line;
        t.col = col;

        if (isalpha((unsigned char)*p) || *p == '_') {
            const char *start = p;
            int start_col = col;
            while (isalnum((unsigned char)*p) || *p == '_') { p++; col++; }
            t.lexeme = xstrndup(start, (size_t)(p - start));
            t.kind = keyword_kind(t.lexeme);
            t.col = start_col;
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
            t.lexeme = xstrndup(start, (size_t)(p - start));
            t.kind = is_float ? TK_FLOAT_LITERAL : TK_INT_LITERAL;
            t.col = start_col;
            if (is_float) t.fval = strtod(t.lexeme, NULL);
            else t.ival = strtol(t.lexeme, NULL, 10);
            token_push(t);
            continue;
        }

        #define TWO(a,b,k) if (p[0] == (a) && p[1] == (b)) { \
            t.kind = (k); t.lexeme = xstrndup(p, 2); token_push(t); p += 2; col += 2; continue; }
        TWO('=', '=', TK_EQ)
        TWO('!', '=', TK_NE)
        TWO('<', '=', TK_LE)
        TWO('>', '=', TK_GE)
        TWO('&', '&', TK_AND)
        TWO('|', '|', TK_OR)
        #undef TWO

        const char *single = "+-*/%(){};=<>!,";
        if (strchr(single, *p)) {
            t.kind = (unsigned char)*p;
            t.lexeme = xstrndup(p, 1);
            token_push(t);
            p++; col++;
            continue;
        }

        fprintf(stderr, "lex error: unknown character '%c' at %d:%d\n", *p, line, col);
        lex_errors++;
        p++; col++;
    }
    Token eof = { .kind = TK_EOF, .lexeme = xstrdup(""), .line = line, .col = col };
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
        fprintf(fp, ", \"line\":%d, \"col\":%d", t->line, t->col);
        if (t->kind == TK_INT_LITERAL) fprintf(fp, ", \"value\":%ld", t->ival);
        if (t->kind == TK_FLOAT_LITERAL) fprintf(fp, ", \"value\":%g", t->fval);
        fprintf(fp, "}%s\n", i == tokens.len - 1 ? "" : ",");
    }
    fprintf(fp, "]\n");
    fclose(fp);
}

