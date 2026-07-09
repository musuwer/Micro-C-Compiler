# Micro C Compiler 完善情况说明

## 本次完善目标

根据题目要求，本次重点完成：

1. 完善项目整体工程，使脚本、示例程序、Web 数据生成流程可以直接运行；
2. 完成进阶要求 B：错误恢复机制，遇到语法错误时继续分析并收集多处错误；
3. 完成进阶要求 C：交互式 Web 前端，可视化 AST 树形结构并高亮源码行；
4. 增加更多可正常编译运行的 MiniC 示例程序，用于验收和答辩演示。

## 基础功能完成情况

| 要求 | 当前状态 | 说明 |
|---|---|---|
| 词法分析器 | 已完成 | 支持关键字、标识符、整数/浮点常量、运算符、分隔符、单双行注释，输出 tokens.json。 |
| 语法分析器 | 已完成 | 采用递归下降法，支持声明、赋值、表达式、if-else、while、for、return、代码块。 |
| 语义分析 | 已完成 | 支持作用域、变量遮蔽、重复声明、未声明变量、int/float 类型检查和符号表输出。 |
| 中间代码 | 已完成 | 可输出 AST JSON 和 TAC 三地址码。 |
| 目标代码 | 已完成 | 生成自定义栈式 VM 字节码并执行，输出 Program return value。 |

## 进阶要求 B：错误恢复机制

修改文件：`src/parser_ast.c`

实现方式：

- 增加语句级同步恢复策略；
- 遇到错误后不会立刻停止，而是继续向后分析；
- 同步边界包括 `;`、`}`、`if`、`while`、`for`、`return`、`{`、`int`、`float`；
- 错误信息统一包含 `line`、`col` 和错误类型；
- 新增测试文件 `examples/parse_recovery_demo.mc`。

验证命令：

```bash
./build/microcc examples/parse_recovery_demo.mc
```

该命令会一次收集多处 `parse error`，例如缺少分号、缺少右括号等。

## 进阶要求 C：交互式 Web 前端

修改文件：

- `web/index.html`
- `web/app.js`
- `web/style.css`
- `scripts/build_web_demo.sh`

实现内容：

- 展示源代码并带行号；
- 展示 Token 表；
- 展示 AST 树形结构；
- 点击 AST 节点高亮对应源码行；
- 点击 Token 行和 Symbol 行定位源码；
- 展示符号表、TAC、VM 字节码和 VM 输出；
- 顶部统计 Token 数量、AST 节点数量、符号数量和 VM 返回值；
- `build_web_demo.sh` 会自动生成 `web/data/` 所需数据。

运行命令：

```bash
bash scripts/build_web_demo.sh
cd web
python3 -m http.server 8080
```

浏览器打开：

```text
http://localhost:8080
```

## 新增正常可运行示例程序

当前 `examples/` 下已整理出 11 个可正常编译并运行的程序：

| 文件 | 作用 | 预期返回值 |
|---|---|---:|
| `basic_demo.mc` | 基础控制流 | 13 |
| `for_demo.mc` | for 循环 | 10 |
| `scope_demo.mc` | 作用域遮蔽 | 8 |
| `comment_logic_demo.mc` | 注释与逻辑表达式 | 22 |
| `expression_demo.mc` | 表达式优先级 | 16 |
| `ast_complex_demo.mc` | 复杂 if/while/for | 20 |
| `while_demo.mc` | while 循环 | 15 |
| `if_else_demo.mc` | if-else 判断 | 42 |
| `nested_blocks_demo.mc` | 嵌套作用域 | 19 |
| `unary_logic_demo.mc` | 一元运算和逻辑非 | 9 |
| `empty_statement_demo.mc` | 空语句和 for 省略项 | 3 |

同时保留错误测试：

- `lex_error_bad_float.mc`
- `lex_error_unclosed_comment.mc`
- `parse_recovery_demo.mc`
- `semantic_error_demo.mc`
- `duplicate_error_demo.mc`
- `type_error_demo.mc`

## 自动测试结果

验证命令：

```bash
bash scripts/run_tests.sh
```

当前测试结果：

```text
Test summary: 22 passed, 0 failed
```

覆盖内容包括：

- 11 个正常程序运行；
- dump 文件生成；
- float Token 与符号表；
- AST 行映射；
- 表达式优先级；
- 控制流 AST 字段；
- 词法错误；
- 语法错误恢复；
- 语义错误；
- 重复声明错误；
- 类型检查错误。

## 注意事项

当前项目仍是教学型 MiniC 子集编译器，不是完整 C 编译器。当前不支持完整 C 语言的函数调用、递归函数、数组、指针、结构体和预处理器。VM 执行层主要执行整数表达式；float 目前主要用于词法识别、AST 表达、符号表记录和类型检查。
