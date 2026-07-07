#!/usr/bin/env bash
set -e

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT_DIR"

mkdir -p build web/data
make -s build/microcc

SOURCE_FILE="${1:-examples/ast_complex_demo.mc}"
if [ ! -f "$SOURCE_FILE" ]; then
  SOURCE_FILE="$(find examples -maxdepth 1 -name '*.mc' | sort | head -n 1)"
fi

if [ -z "$SOURCE_FILE" ] || [ ! -f "$SOURCE_FILE" ]; then
  echo "No .mc source file found under examples/." >&2
  exit 1
fi

./build/microcc "$SOURCE_FILE" \
  --dump-tokens build/tokens.json \
  --dump-ast build/ast.json \
  --dump-symbols build/symbols.json \
  --dump-tac build/out.tac \
  --dump-bytecode build/out.bc \
  --run > build/vm_output.txt

cp "$SOURCE_FILE" web/data/source.mc
cp build/tokens.json web/data/tokens.json
cp build/ast.json web/data/ast.json
cp build/symbols.json web/data/symbols.json
cp build/out.tac web/data/out.tac
cp build/out.bc web/data/out.bc
cp build/vm_output.txt web/data/vm_output.txt

echo "Web demo data generated in web/data from $SOURCE_FILE"
