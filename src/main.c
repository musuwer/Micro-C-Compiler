#include "microcc.h"

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
        if (!strcmp(argv[i], "--dump-tokens") && i + 1 < argc) out_tokens = argv[++i];
        else if (!strcmp(argv[i], "--dump-ast") && i + 1 < argc) out_ast = argv[++i];
        else if (!strcmp(argv[i], "--dump-symbols") && i + 1 < argc) out_symbols = argv[++i];
        else if (!strcmp(argv[i], "--dump-tac") && i + 1 < argc) out_tac = argv[++i];
        else if (!strcmp(argv[i], "--dump-bytecode") && i + 1 < argc) out_bytecode = argv[++i];
        else if (!strcmp(argv[i], "--run")) run = true;
        else if (!strcmp(argv[i], "--trace-vm")) {
            trace_vm = true;
            run = true;
        }
        else if (!src_path) src_path = argv[i];
    }

    if (!src_path) {
        fprintf(stderr, "Usage: %s <file.mc> [options]\n", argv[0]);
        fprintf(stderr, "Options: --dump-tokens PATH --dump-ast PATH --dump-symbols PATH --dump-tac PATH --dump-bytecode PATH --run --trace-vm\n");
        return 1;
    }

    const char *src = read_file(src_path);
    lex(src);
    if (lex_errors > 0) return 1;

    if (out_tokens) dump_tokens_json(out_tokens);

    Node *ast = parse_program();
    if (parse_errors > 0) return 1;

    // Day6: run semantic analysis before dumping AST so expression types are available
    analyze(ast);
    if (sem_errors > 0 && !run) return 1;

    if (out_ast) dump_ast_json(out_ast, ast);

    if (out_symbols) dump_symbols_json(out_symbols);

    if (out_tac) dump_tac(out_tac, ast);

    generate_bytecode(ast);
    if (out_bytecode) dump_bytecode(out_bytecode);

    if (run) {
        long ret = run_vm(trace_vm);
        printf("Program return value: %ld\n", ret);
    }

    return 0;
}
