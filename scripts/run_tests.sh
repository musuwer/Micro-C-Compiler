#!/usr/bin/env bash
set -e
mkdir -p build
make -s build/microcc

pass=0
fail=0

run_ok() {
  local name="$1"
  local file="$2"
  local expect="$3"
  local out
  out=$(./build/microcc "$file" --run)
  if echo "$out" | grep -q "Program return value: $expect"; then
    echo "[PASS] $name => $expect"
    pass=$((pass + 1))
  else
    echo "[FAIL] $name"
    echo "$out"
    fail=$((fail + 1))
  fi
}

run_error() {
  local name="$1"
  local file="$2"
  local keyword="$3"
  set +e
  local out
  out=$(./build/microcc "$file" 2>&1)
  local code=$?
  set -e
  if [ $code -ne 0 ] && echo "$out" | grep -q "$keyword"; then
    echo "[PASS] $name reports $keyword"
    pass=$((pass + 1))
  else
    echo "[FAIL] $name"
    echo "$out"
    fail=$((fail + 1))
  fi
}

run_ok "basic control flow" examples/basic_demo.mc 13
run_ok "for loop" examples/for_demo.mc 10
run_ok "scope shadowing" examples/scope_demo.mc 8
run_ok "comments and logical expressions" examples/comment_logic_demo.mc 22
run_ok "expression precedence" examples/expression_demo.mc 16

./build/microcc examples/basic_demo.mc \
  --dump-tokens build/test_tokens.json \
  --dump-ast build/test_ast.json \
  --dump-symbols build/test_symbols.json \
  --dump-tac build/test_out.tac \
  --dump-bytecode build/test_out.bc > /dev/null
if [ -s build/test_tokens.json ] && [ -s build/test_ast.json ] && [ -s build/test_symbols.json ] && [ -s build/test_out.tac ] && [ -s build/test_out.bc ]; then
  echo "[PASS] compiler dump files generated"
  pass=$((pass + 1))
else
  echo "[FAIL] dump files missing"
  fail=$((fail + 1))
fi

./build/microcc examples/float_token_demo.mc --dump-tokens build/test_float_tokens.json --dump-symbols build/test_float_symbols.json > /dev/null
if grep -q "FLOAT_LITERAL" build/test_float_tokens.json && grep -q "float" build/test_float_symbols.json; then
  echo "[PASS] float token and symbol support"
  pass=$((pass + 1))
else
  echo "[FAIL] float token and symbol support"
  fail=$((fail + 1))
fi

./build/microcc examples/lexer_full_demo.mc --dump-tokens build/test_lexer_full_tokens.json --dump-ast build/test_lexer_full_ast.json > /dev/null
if grep -q "KW_FLOAT" build/test_lexer_full_tokens.json \
   && grep -q "FLOAT_LITERAL" build/test_lexer_full_tokens.json \
   && grep -q "&&" build/test_lexer_full_tokens.json \
   && grep -q "||" build/test_lexer_full_tokens.json \
   && grep -q "\"length\"" build/test_lexer_full_tokens.json \
   && grep -q "\"line\"" build/test_lexer_full_ast.json; then
  echo "[PASS] lexer full coverage and AST line mapping"
  pass=$((pass + 1))
else
  echo "[FAIL] lexer full coverage and AST line mapping"
  fail=$((fail + 1))
fi

./build/microcc examples/expression_demo.mc --dump-tokens build/test_expression_tokens.json --dump-ast build/test_expression_ast.json > /dev/null
if grep -q "==" build/test_expression_tokens.json \
   && grep -q "!" build/test_expression_tokens.json \
   && grep -q "<=\|>=\|<\|>" build/test_expression_tokens.json \
   && grep -q "\"op\":\"*\"" build/test_expression_ast.json \
   && grep -q "\"op\":\"+\"" build/test_expression_ast.json \
   && grep -q "\"op\":\"&&\"" build/test_expression_ast.json \
   && grep -q "\"op\":\"||\"" build/test_expression_ast.json; then
  echo "[PASS] expression token and AST precedence coverage"
  pass=$((pass + 1))
else
  echo "[FAIL] expression token and AST precedence coverage"
  fail=$((fail + 1))
fi

run_error "malformed float literal" examples/lex_error_bad_float.mc "malformed floating literal"
run_error "unclosed block comment" examples/lex_error_unclosed_comment.mc "unclosed block comment"

run_error "undeclared variable" examples/semantic_error_demo.mc "undeclared variable"
run_error "duplicate declaration" examples/duplicate_error_demo.mc "duplicate declaration"
run_error "type checking" examples/type_error_demo.mc "cannot assign float"

echo
printf "Test summary: %d passed, %d failed\n" "$pass" "$fail"
[ "$fail" -eq 0 ]
