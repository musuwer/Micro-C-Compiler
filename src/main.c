#include "microcc.h"

#define MICROCC_VERSION "0.3-advanced-bc-icon-ui"

static void print_usage(const char *prog) {
    fprintf(stderr, "Micro C Compiler %s\n", MICROCC_VERSION);
    fprintf(stderr, "Usage: %s <file.mc> [options]\n", prog);
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "  --dump-tokens PATH    Export token stream JSON\n");
    fprintf(stderr, "  --dump-ast PATH       Export AST JSON\n");
    fprintf(stderr, "  --dump-symbols PATH   Export symbol table JSON\n");
    fprintf(stderr, "  --dump-tac PATH       Export TAC text\n");
    fprintf(stderr, "  --dump-bytecode PATH  Export VM bytecode text\n");
    fprintf(stderr, "  --run                 Execute generated VM bytecode\n");
    fprintf(stderr, "  --trace-vm            Execute VM with pc/instruction/stack trace\n");
    fprintf(stderr, "  --help                Show this help message\n");
    fprintf(stderr, "  --version             Show compiler version\n");
}

static bool need_value(int argc, char **argv, int i) {
    if (i + 1 < argc) return false;
    fprintf(stderr, "error: option %s requires a path argument\n", argv[i]);
    return true;
}

int main(int argc, char **argv) {
    const char *src_path = NULL;
    const char *out_tokens = NULL;
    const char *out_ast = NULL;
    const char *out_symbols = NULL;
    const char *out_tac = NULL;
    const char *out_bytecode = NULL;
    bool run = false;
    bool trace_vm = false;

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--help") || !strcmp(argv[i], "-h")) {
            print_usage(argv[0]);
            return 0;
        } else if (!strcmp(argv[i], "--version")) {
            printf("Micro C Compiler %s\n", MICROCC_VERSION);
            return 0;
        } else if (!strcmp(argv[i], "--dump-tokens")) {
            if (need_value(argc, argv, i)) return 1;
            out_tokens = argv[++i];
        } else if (!strcmp(argv[i], "--dump-ast")) {
            if (need_value(argc, argv, i)) return 1;
            out_ast = argv[++i];
        } else if (!strcmp(argv[i], "--dump-symbols")) {
            if (need_value(argc, argv, i)) return 1;
            out_symbols = argv[++i];
        } else if (!strcmp(argv[i], "--dump-tac")) {
            if (need_value(argc, argv, i)) return 1;
            out_tac = argv[++i];
        } else if (!strcmp(argv[i], "--dump-bytecode")) {
            if (need_value(argc, argv, i)) return 1;
            out_bytecode = argv[++i];
        } else if (!strcmp(argv[i], "--run")) {
            run = true;
        } else if (!strcmp(argv[i], "--trace-vm")) {
            trace_vm = true;
            run = true;
        } else if (argv[i][0] == '-') {
            fprintf(stderr, "error: unknown option %s\n", argv[i]);
            print_usage(argv[0]);
            return 1;
        } else if (!src_path) {
            src_path = argv[i];
        } else {
            fprintf(stderr, "error: multiple input files: %s and %s\n", src_path, argv[i]);
            return 1;
        }
    }

    if (!src_path) {
        print_usage(argv[0]);
        return 1;
    }

    const char *src = read_file(src_path);
    lex(src);
    if (lex_errors > 0) return 1;

    if (out_tokens) dump_tokens_json(out_tokens);

    Node *ast = parse_program();
    if (parse_errors > 0) return 1;

    // Semantic analysis runs before AST dumping so expression types and symbol
    // slots are available to both JSON output and the interactive Web frontend.
    analyze(ast);
    if (sem_errors > 0) return 1;

    if (out_ast) dump_ast_json(out_ast, ast);
    if (out_symbols) dump_symbols_json(out_symbols);
    if (out_tac) dump_tac(out_tac, ast);

    if (out_bytecode || run) {
        generate_bytecode(ast);
        if (codegen_errors > 0) return 1;
        if (out_bytecode) dump_bytecode(out_bytecode);
    }

    if (run) {
        long ret = run_vm(trace_vm);
        printf("Program return value: %ld\n", ret);
    }

    return 0;
}
