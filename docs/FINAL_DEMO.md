# Final Demo Guide

## Files

- `web/index.html`: final visualization page.
- `web/app.js`: loads compiler output and renders source, tokens, AST, symbols, TAC and bytecode.
- `web/style.css`: page layout and highlighting styles.
- `scripts/build_web_demo.sh`: builds the compiler and writes demo data into `web/data/`.
- `web/data/source.mc`: source program displayed by the page.
- `web/data/tokens.json`: lexer output.
- `web/data/ast.json`: parser AST output with `id`, `kind`, `line` and `children`.
- `web/data/symbols.json`: semantic symbol table.
- `web/data/out.tac`: three-address code.
- `web/data/out.bc`: VM bytecode.

## Commands

```bash
make clean
make
bash scripts/run_demo.sh
bash scripts/run_tests.sh
bash scripts/build_web_demo.sh
cd web
python3 -m http.server 8080
```

Open `http://localhost:8080` in a browser.

## Presentation Order

1. Show the source program panel.
2. Show the token table and explain `kind`, `lexeme`, `line`, `col` and `length`.
3. Open the AST tree and click nodes to highlight source lines.
4. Show the symbol table with type, scope depth, declaration line, initialization and usage state.
5. Show TAC as the readable middle representation.
6. Show bytecode as the VM input.
7. Run the compiler with `--trace-vm` to show `pc`, instruction and stack state.
