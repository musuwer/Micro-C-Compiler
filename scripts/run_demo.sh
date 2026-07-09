#!/usr/bin/env bash
set -e
mkdir -p build
make -s build/microcc

./build/microcc examples/basic_demo.mc \
  --dump-tokens build/tokens.json \
  --dump-ast build/ast.json \
  --dump-symbols build/symbols.json \
  --dump-tac build/out.tac \
  --dump-bytecode build/out.bc \
  --run

./build/microcc examples/float_token_demo.mc \
  --dump-tokens build/float_tokens.json \
  --dump-ast build/float_ast.json \
  --dump-symbols build/float_symbols.json

printf '\nGenerated files:\n'
printf '  build/tokens.json       Lexer output\n'
printf '  build/ast.json          AST JSON\n'
printf '  build/symbols.json      Symbol table JSON\n'
printf '  build/out.tac           Three-address code\n'
printf '  build/out.bc            VM bytecode\n'
printf '  build/float_tokens.json Float literal lexer proof\n'
