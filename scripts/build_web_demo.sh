#!/usr/bin/env bash
set -e

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT_DIR"

mkdir -p build web/data web/data/demos web/data/diagnostics
make -s build/microcc

# Normal programs used by the interactive Web frontend.  These are all expected
# to compile and execute on the VM, so the selector can switch between complete
# Source/Token/AST/Symbol/TAC/Bytecode/VM datasets.
NORMAL_SOURCES=(
  "examples/basic_demo.mc|Basic control flow|13"
  "examples/for_demo.mc|For loop|10"
  "examples/scope_demo.mc|Scope shadowing|8"
  "examples/comment_logic_demo.mc|Comments and logical expressions|22"
  "examples/expression_demo.mc|Expression precedence|16"
  "examples/ast_complex_demo.mc|Complex control-flow AST|20"
  "examples/while_demo.mc|While loop|15"
  "examples/if_else_demo.mc|If-else logic|42"
  "examples/nested_blocks_demo.mc|Nested blocks|19"
  "examples/unary_logic_demo.mc|Unary and logical not|9"
  "examples/empty_statement_demo.mc|Empty statements and for clauses|3"
  "examples/logic_chain_demo.mc|Logic chain demo|31"
  "examples/nested_loop_demo.mc|Nested loop demo|18"
  "examples/accumulate_even_demo.mc|Accumulation demo|30"
)

slug_of() {
  basename "$1" .mc
}

generate_dataset() {
  local source_file="$1"
  local title="$2"
  local expect="$3"
  local out_dir="$4"
  local slug
  slug="$(slug_of "$source_file")"

  mkdir -p "$out_dir"
  ./build/microcc "$source_file" \
    --dump-tokens "$out_dir/tokens.json" \
    --dump-ast "$out_dir/ast.json" \
    --dump-symbols "$out_dir/symbols.json" \
    --dump-tac "$out_dir/out.tac" \
    --dump-bytecode "$out_dir/out.bc" \
    --run > "$out_dir/vm_output.txt"

  cp "$source_file" "$out_dir/source.mc"
  cat > "$out_dir/manifest.json" <<MANIFEST
{
  "id": "$slug",
  "title": "$title",
  "source": "$source_file",
  "expectedReturn": $expect,
  "featureB": "parser error recovery with multi-error collection",
  "featureC": "interactive AST tree with source-line and token-range highlighting"
}
MANIFEST
}

# Add a few extra runnable programs if they do not exist yet.  They are kept
# small and within the currently supported MiniC subset.
cat > examples/logic_chain_demo.mc <<'MC'
int main() {
    int a = 3;
    int b = 7;
    int c = 0;
    if ((a < b && b != 0) || c) {
        return a + b * 4;
    }
    return 0;
}
MC

cat > examples/nested_loop_demo.mc <<'MC'
int main() {
    int i = 0;
    int total = 0;
    while (i < 3) {
        int j = 0;
        while (j < 3) {
            total = total + i + j;
            j = j + 1;
        }
        i = i + 1;
    }
    return total;
}
MC

cat > examples/accumulate_even_demo.mc <<'MC'
int main() {
    int i = 0;
    int total = 0;
    for (i = 0; i <= 10; i = i + 1) {
        if (i % 2 == 0) {
            total = total + i;
        }
    }
    return total;
}
MC

# Pick a default source. If the caller passes an existing .mc file, use it;
# otherwise use the complex AST demo.
DEFAULT_SOURCE="${1:-examples/ast_complex_demo.mc}"
if [ ! -f "$DEFAULT_SOURCE" ]; then
  DEFAULT_SOURCE="examples/ast_complex_demo.mc"
fi
DEFAULT_TITLE="Default demo"
DEFAULT_EXPECT=20
for entry in "${NORMAL_SOURCES[@]}"; do
  IFS='|' read -r src title expect <<< "$entry"
  if [ "$src" = "$DEFAULT_SOURCE" ]; then
    DEFAULT_TITLE="$title"
    DEFAULT_EXPECT="$expect"
  fi
done

generate_dataset "$DEFAULT_SOURCE" "$DEFAULT_TITLE" "$DEFAULT_EXPECT" "web/data"

# Build all selectable datasets and the selector manifest.
examples_json="web/data/examples.json"
printf '[\n' > "$examples_json"
first=1
for entry in "${NORMAL_SOURCES[@]}"; do
  IFS='|' read -r src title expect <<< "$entry"
  [ -f "$src" ] || continue
  slug="$(slug_of "$src")"
  generate_dataset "$src" "$title" "$expect" "web/data/demos/$slug"
  if [ "$first" -eq 0 ]; then printf ',\n' >> "$examples_json"; fi
  first=0
  printf '  {"id":"%s", "title":"%s", "source":"%s", "expected":%s}' "$slug" "$title" "$src" "$expect" >> "$examples_json"
done
printf '\n]\n' >> "$examples_json"

# Advanced requirement B: persist parser diagnostics for frontend display.
set +e
./build/microcc examples/parse_recovery_demo.mc > web/data/diagnostics/parse_recovery_demo.txt 2>&1
set -e

cat > web/data/diagnostics/README.txt <<'TXT'
This directory stores captured compiler diagnostics for Web demonstration.
parse_recovery_demo.txt shows advanced requirement B: collecting multiple parse errors in one run.
TXT

echo "Web demo data generated in web/data."
echo "Selectable demos generated under web/data/demos/."
echo "Run: cd web && python3 -m http.server 8080"
