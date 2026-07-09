# Advanced Requirements B/C Completion Notes

## B. Parser error recovery

The parser now uses statement-level synchronization. When an error is detected,
it reports a diagnostic with line and column information, then skips only to a
safe boundary such as:

- `;`
- `}`
- the beginning of the next statement, including `if`, `while`, `for`, `return`,
  `{`, `int`, or `float`

This prevents one missing semicolon from destroying the rest of the parse and
allows the compiler to collect multiple syntax errors in one run.

Demo command:

```bash
./build/microcc examples/parse_recovery_demo.mc
```

Expected behavior: the compiler exits with a non-zero status and prints multiple
`parse error line X col Y: ...` messages.

## C. Interactive Web frontend

The Web frontend has been upgraded to show a complete compilation pipeline:

- Source code with line numbers
- Token table with `kind`, `lexeme`, `line`, `col`, `length`
- Interactive AST tree
- Symbol table
- TAC three-address code
- VM bytecode
- VM execution result

Clicking AST nodes, Token rows, or Symbol rows highlights the corresponding
source line. The page also includes summary cards for token count, AST node
count, symbol count, and VM return value.

Demo commands:

```bash
bash scripts/build_web_demo.sh
cd web
python3 -m http.server 8080
```

Open `http://localhost:8080` in a browser.
