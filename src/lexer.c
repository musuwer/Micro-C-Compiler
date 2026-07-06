#include "microcc.h"

/*
 * tokens:
 *   全局 Token 动态数组。
 *   词法分析器 lex() 会把源代码拆分成一个个 Token，
 *   然后依次存入 tokens 中。
 *
 * pos:
 *   当前读取 Token 的位置。
 *   词法分析阶段只负责生成 tokens；
 *   后面的语法分析 parser_ast.c 会通过 pos 来读取 Token。
 *
 * lex_errors:
 *   词法错误数量。
 *   例如非法字符、异常浮点数、未闭合多行注释等都会让 lex_errors++。
 */
TokenVec tokens;
int pos = 0;
int lex_errors = 0;

/*
 * token_push:
 *   将一个 Token 追加到 tokens 动态数组中。
 *
 * 为什么需要动态扩容？
 *   因为源代码长度不固定，Token 数量也不固定。
 *   初始容量设为 256；如果空间不够，就扩容为原来的 2 倍。
 *
 * 参数:
 *   t: 已经构造好的 Token。
 */
static void token_push(Token t) {
    if (tokens.len == tokens.cap) {
        tokens.cap = tokens.cap ? tokens.cap * 2 : 256;
        tokens.data = (Token *)realloc(tokens.data, sizeof(Token) * (size_t)tokens.cap);
        if (!tokens.data) die("out of memory");
    }

    tokens.data[tokens.len++] = t;
}

/*
 * lex_error_at:
 *   输出词法错误信息，并记录错误数量。
 *
 * 输出格式:
 *   lex error line 行号 col 列号: 错误原因
 *
 * 例如:
 *   lex error line 3 col 8: malformed floating literal '12.3.4'
 *
 * 为什么要带 line 和 col？
 *   后续老师检查或用户调试时，可以快速定位源代码中的错误位置。
 *
 * 参数:
 *   line: 错误所在行号
 *   col : 错误所在列号
 *   fmt : printf 风格的错误描述格式
 */
static void lex_error_at(int line, int col, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);

    fprintf(stderr, "lex error line %d col %d: ", line, col);
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");

    va_end(ap);
    lex_errors++;
}

/*
 * token_kind_name:
 *   将 Token 的 kind 编号转换成可读字符串。
 *
 * 作用:
 *   主要用于 dump_tokens_json() 输出 tokens.json。
 *
 * 例如:
 *   TK_IDENT       -> "IDENT"
 *   TK_KW_INT      -> "KW_INT"
 *   TK_INT_LITERAL -> "INT_LITERAL"
 *   '+'            -> "+"
 *
 * 注意:
 *   一些单字符 Token，例如 '+', '-', ';', '('，直接使用它们的 ASCII 值作为 kind。
 *   所以 default 分支中，如果 kind 是可打印字符，就直接输出该字符。
 */
const char *token_kind_name(int kind) {
    switch (kind) {
        case TK_EOF: return "EOF";

        /* 标识符和字面量 */
        case TK_IDENT: return "IDENT";
        case TK_INT_LITERAL: return "INT_LITERAL";
        case TK_FLOAT_LITERAL: return "FLOAT_LITERAL";

        /* 关键字 */
        case TK_KW_INT: return "KW_INT";
        case TK_KW_FLOAT: return "KW_FLOAT";
        case TK_KW_IF: return "KW_IF";
        case TK_KW_ELSE: return "KW_ELSE";
        case TK_KW_WHILE: return "KW_WHILE";
        case TK_KW_FOR: return "KW_FOR";
        case TK_KW_RETURN: return "KW_RETURN";

        /* 双字符运算符 */
        case TK_EQ: return "==";
        case TK_NE: return "!=";
        case TK_LE: return "<=";
        case TK_GE: return ">=";
        case TK_AND: return "&&";
        case TK_OR: return "||";

        /*
         * 单字符 Token 不单独写枚举，而是直接使用字符本身。
         * 例如:
         *   '+' 的 kind 就是 '+'
         *   ';' 的 kind 就是 ';'
         */
        default: {
            static char buf[8];

            if (kind > 0 && kind < 128 && isprint(kind)) {
                snprintf(buf, sizeof(buf), "%c", kind);
            } else {
                snprintf(buf, sizeof(buf), "#%d", kind);
            }

            return buf;
        }
    }
}

/*
 * keyword_kind:
 *   判断一个单词是关键字还是普通标识符。
 *
 * 词法分析时，只要读到字母或下划线开头的一串字符，
 * 一开始都会先当成“单词”。
 *
 * 然后用 keyword_kind() 判断：
 *   如果是 int / float / if / else / while / for / return，
 *   就返回对应的关键字 Token 类型；
 *
 *   如果不是关键字，例如 main、a、sum、total，
 *   就返回 TK_IDENT，表示普通标识符。
 *
 * 参数:
 *   s: 识别出来的单词字符串
 */
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

/*
 * make_token:
 *   创建一个 Token。
 *
 * 参数:
 *   kind : Token 类型
 *   start: Token 在源代码中的起始位置
 *   len  : Token 文本长度
 *   line : Token 起始行号
 *   col  : Token 起始列号
 *
 * 返回:
 *   一个初始化好的 Token。
 *
 * 注意:
 *   lexeme 使用 xstrndup 拷贝出来，而不是直接保存 start 指针。
 *   这样即使原始源代码处理结束，Token 仍然有自己的字符串内容。
 */
static Token make_token(int kind, const char *start, int len, int line, int col) {
    Token t = {0};

    t.kind = kind;
    t.lexeme = xstrndup(start, (size_t)len);
    t.line = line;
    t.col = col;
    t.len = len;

    return t;
}

/*
 * two_char_operator_kind:
 *   判断当前位置的两个字符是否组成双字符运算符。
 *
 * 支持的双字符运算符:
 *   ==   相等判断
 *   !=   不等判断
 *   <=   小于等于
 *   >=   大于等于
 *   &&   逻辑与
 *   ||   逻辑或
 *
 * 参数:
 *   a: 第一个字符
 *   b: 第二个字符
 *
 * 返回:
 *   如果匹配双字符运算符，返回对应 Token 类型；
 *   否则返回 0。
 */
static int two_char_operator_kind(char a, char b) {
    if (a == '=' && b == '=') return TK_EQ;
    if (a == '!' && b == '=') return TK_NE;
    if (a == '<' && b == '=') return TK_LE;
    if (a == '>' && b == '=') return TK_GE;
    if (a == '&' && b == '&') return TK_AND;
    if (a == '|' && b == '|') return TK_OR;

    return 0;
}

/*
 * is_single_char_token:
 *   判断一个字符是否是单字符 Token。
 *
 * 单字符 Token 包括:
 *   +  -  *  /  %     算术运算符
 *   (  )  {  }        分隔符
 *   ;  ,              分号和逗号
 *   =  <  >  !        单字符运算符
 *
 * 注意:
 *   ==、!=、<=、>=、&&、|| 这些是双字符运算符，
 *   会优先在 two_char_operator_kind() 中处理。
 */
static bool is_single_char_token(char c) {
    return strchr("+-*/%(){};=<>!,", c) != NULL;
}

/*
 * lex:
 *   词法分析核心函数。
 *
 * 作用:
 *   从头到尾扫描源代码 src，
 *   把源代码拆分成 Token 流，
 *   并保存到全局 tokens 数组中。
 *
 * 支持识别:
 *   1. 空白字符和换行
 *   2. 单行注释 //
 *   3. 多行注释
 *   4. 标识符和关键字
 *   5. 整型常量
 *   6. 浮点常量
 *   7. 双字符运算符
 *   8. 单字符运算符和分隔符
 *   9. 非法字符错误
 *
 * line 和 col:
 *   line 从 1 开始，表示当前行号。
 *   col  从 1 开始，表示当前列号。
 */
void lex(const char *src) {
    int line = 1;
    int col = 1;

    const char *p = src;

    while (*p) {
        /*
         * 处理普通空白字符:
         *   空格、Tab、回车都只会让列号增加。
         *
         * 注意:
         *   Windows 文本可能包含 \r\n，
         *   这里 \r 单独跳过，\n 再负责换行。
         */
        if (*p == ' ' || *p == '\t' || *p == '\r') {
            p++;
            col++;
            continue;
        }

        /*
         * 处理换行:
         *   行号 line 增加；
         *   列号 col 重置为 1。
         */
        if (*p == '\n') {
            p++;
            line++;
            col = 1;
            continue;
        }

        /*
         * 处理单行注释:
         *
         * 形式:
         *   // comment
         *
         * 做法:
         *   从 // 开始跳过，一直跳到换行符前。
         *   换行符不在这里消费，交给上面的 '\n' 分支处理，
         *   这样可以统一维护 line 和 col。
         */
        if (p[0] == '/' && p[1] == '/') {
            p += 2;
            col += 2;

            while (*p && *p != '\n') {
                p++;
                col++;
            }

            continue;
        }

        /*
         * 处理多行注释。
         *
         * 形式:
         *   以斜杠和星号开始，以星号和斜杠结束。
         *
         * 重点:
         *   多行注释内部可能有换行，
         *   因此必须在扫描注释内容时同步维护 line 和 col。
         *
         * 如果扫描到文件结束还没有遇到结束符，
         * 就报 unclosed block comment。
         */
        if (p[0] == '/' && p[1] == '*') {
            int start_line = line;
            int start_col = col;

            p += 2;
            col += 2;

            bool closed = false;

            while (*p) {
                if (p[0] == '*' && p[1] == '/') {
                    p += 2;
                    col += 2;
                    closed = true;
                    break;
                }

                if (*p == '\n') {
                    p++;
                    line++;
                    col = 1;
                } else {
                    p++;
                    col++;
                }
            }

            if (!closed) {
                lex_error_at(start_line, start_col, "unclosed block comment");
            }

            continue;
        }

        /*
         * 处理标识符或关键字:
         *
         * 标识符规则:
         *   以字母或下划线开头；
         *   后面可以跟字母、数字或下划线。
         *
         * 例如:
         *   a
         *   sum
         *   total_1
         *   _temp
         *
         * 关键字:
         *   int、float、if、else、while、for、return。
         */
        if (isalpha((unsigned char)*p) || *p == '_') {
            const char *start = p;
            int start_col = col;

            while (isalnum((unsigned char)*p) || *p == '_') {
                p++;
                col++;
            }

            int len = (int)(p - start);

            /*
             * 先创建成 TK_IDENT，
             * 再通过 keyword_kind() 判断是否应该改成关键字类型。
             */
            Token t = make_token(TK_IDENT, start, len, line, start_col);
            t.kind = keyword_kind(t.lexeme);

            token_push(t);
            continue;
        }

        /*
         * 处理数字字面量:
         *
         * 支持:
         *   整数: 123
         *   浮点数: 3.14
         *
         * 异常:
         *   12.3.4 会被识别为 malformed floating literal。
         */
        if (isdigit((unsigned char)*p)) {
            const char *start = p;
            int start_col = col;
            bool is_float = false;

            /*
             * 先扫描整数部分。
             */
            while (isdigit((unsigned char)*p)) {
                p++;
                col++;
            }

            /*
             * 如果遇到第一个小数点，说明这是浮点数。
             */
            if (*p == '.') {
                is_float = true;
                p++;
                col++;

                /*
                 * 扫描小数部分。
                 *
                 * 当前实现允许 12. 这种形式被识别为浮点数。
                 * 如果要更严格，可以要求小数点后必须至少有一个数字。
                 */
                while (isdigit((unsigned char)*p)) {
                    p++;
                    col++;
                }
            }

            /*
             * 如果后面又遇到一个小数点，
             * 说明浮点数字面量格式错误。
             *
             * 例如:
             *   12.3.4
             */
            if (*p == '.') {
                const char *bad_start = start;
                int bad_col = start_col;

                while (isdigit((unsigned char)*p) || *p == '.') {
                    p++;
                    col++;
                }

                lex_error_at(
                    line,
                    bad_col,
                    "malformed floating literal '%.*s'",
                    (int)(p - bad_start),
                    bad_start
                );

                continue;
            }

            int len = (int)(p - start);

            Token t = make_token(
                is_float ? TK_FLOAT_LITERAL : TK_INT_LITERAL,
                start,
                len,
                line,
                start_col
            );

            /*
             * 同时保存数值形式:
             *   整数保存到 ival；
             *   浮点数保存到 fval。
             *
             * 后续语法分析和语义分析可以直接使用这些值。
             */
            if (is_float) {
                t.fval = strtod(t.lexeme, NULL);
            } else {
                t.ival = strtol(t.lexeme, NULL, 10);
            }

            token_push(t);
            continue;
        }

        /*
         * 处理双字符运算符。
         *
         * 必须放在单字符运算符之前。
         *
         * 例如:
         *   如果先处理单字符，
         *   那么 == 会被错误拆成 '=' 和 '='。
         */
        int two_kind = two_char_operator_kind(p[0], p[1]);
        if (two_kind) {
            Token t = make_token(two_kind, p, 2, line, col);
            token_push(t);

            p += 2;
            col += 2;

            continue;
        }

        /*
         * 处理单字符 Token。
         *
         * 例如:
         *   + - * / % ( ) { } ; = < > ! ,
         *
         * 这些 Token 的 kind 直接使用字符本身。
         */
        if (is_single_char_token(*p)) {
            Token t = make_token((unsigned char)*p, p, 1, line, col);
            token_push(t);

            p++;
            col++;

            continue;
        }

        /*
         * 如果以上情况都不匹配，
         * 说明当前字符不是本语言支持的合法字符。
         *
         * 例如:
         *   @
         *   #
         *   中文符号
         */
        lex_error_at(line, col, "unexpected character '%c'", *p);

        p++;
        col++;
    }

    /*
     * 源代码扫描结束后，手动添加 EOF Token。
     *
     * EOF 的作用:
     *   让语法分析器知道 Token 流已经结束。
     */
    Token eof = {
        .kind = TK_EOF,
        .lexeme = xstrdup(""),
        .line = line,
        .col = col,
        .len = 0
    };

    token_push(eof);
}

/*
 * dump_tokens_json:
 *   将词法分析结果输出成 JSON 文件。
 *
 * 输出路径通常是:
 *   build/tokens.json
 *
 * 输出内容包括:
 *   kind    Token 类型
 *   lexeme  Token 原始文本
 *   line    行号
 *   col     列号
 *   length  Token 长度
 *   value   整数或浮点数的值
 *
 * 这个文件的作用:
 *   1. 用于调试词法分析结果；
 *   2. 用于期中/答辩展示；
 *   3. 后续 Web 前端可以读取 tokens.json，展示 Token 表格。
 */
/*
 * dump_tokens_json:
 *   Export the token stream to build/tokens.json for debugging and Web visualization.
 *
 * Day7 Web visualization format:
 *   [
 *     {
 *       "index": 0,
 *       "kind": "KW_INT",
 *       "lexeme": "int",
 *       "line": 1,
 *       "col": 1,
 *       "length": 3
 *     }
 *   ]
 *
 * The Web frontend can use:
 *   kind   -> token type column
 *   lexeme -> original source text
 *   line   -> source line mapping
 *   col    -> source column mapping
 *   length -> token highlight width
 */
void dump_tokens_json(const char *path) {
    FILE *fp = open_out(path);

    fprintf(fp, "[\n");

    for (int i = 0; i < tokens.len; i++) {
        Token *t = &tokens.data[i];

        fprintf(fp, "  {");

        /*
         * index:
         *   Stable token order in the token stream.
         *   This is useful for frontend table rendering and debugging.
         */
        fprintf(fp, "\"index\":%d", i);

        /*
         * kind:
         *   Human-readable token type, such as KW_INT, IDENT, INT_LITERAL.
         */
        fprintf(fp, ", \"kind\":");
        json_escape(fp, token_kind_name(t->kind));

        /*
         * lexeme:
         *   Original text of the token in source code.
         */
        fprintf(fp, ", \"lexeme\":");
        json_escape(fp, t->lexeme);

        /*
         * line / col / length:
         *   Source location information.
         *   These fields are required by the Web frontend to map tokens
         *   back to the source code and highlight the corresponding range.
         */
        fprintf(
            fp,
            ", \"line\":%d, \"col\":%d, \"length\":%d",
            t->line,
            t->col,
            t->len
        );

        /*
         * value:
         *   Numeric value for integer and floating-point literals.
         *   Other tokens do not need this field.
         */
        if (t->kind == TK_INT_LITERAL) {
            fprintf(fp, ", \"value\":%ld", t->ival);
        }

        if (t->kind == TK_FLOAT_LITERAL) {
            fprintf(fp, ", \"value\":%g", t->fval);
        }

        fprintf(fp, "}%s\n", i == tokens.len - 1 ? "" : ",");
    }

    fprintf(fp, "]\n");

    fclose(fp);
}
    FILE *fp = open_out(path);

    fprintf(fp, "[\n");

    for (int i = 0; i < tokens.len; i++) {
        Token *t = &tokens.data[i];

        fprintf(fp, "  {\"kind\":");
        json_escape(fp, token_kind_name(t->kind));

        fprintf(fp, ", \"lexeme\":");
        json_escape(fp, t->lexeme);

        fprintf(
            fp,
            ", \"line\":%d, \"col\":%d, \"length\":%d",
            t->line,
            t->col,
            t->len
        );

        /*
         * 如果是整数 Token，额外输出整数值。
         */
        if (t->kind == TK_INT_LITERAL) {
            fprintf(fp, ", \"value\":%ld", t->ival);
        }

        /*
         * 如果是浮点数 Token，额外输出浮点值。
         */
        if (t->kind == TK_FLOAT_LITERAL) {
            fprintf(fp, ", \"value\":%g", t->fval);
        }

        fprintf(fp, "}%s\n", i == tokens.len - 1 ? "" : ",");
    }

    fprintf(fp, "]\n");

    fclose(fp);
}