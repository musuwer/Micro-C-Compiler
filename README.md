<div align="center">

# Micro C Compiler
**Tiny C-Subset Compiler | AI-assisted C Programming Practice Project**

<br/>

![C](https://img.shields.io/badge/C-C17-blue)
![Build](https://img.shields.io/badge/Build-Make-success)
![Status](https://img.shields.io/badge/Status-Developing-orange)
![Platform](https://img.shields.io/badge/Platform-Linux%20%7C%20WSL2-lightgrey)

</div>

---

## Day 8 Final Demo

This folder is the Day 8 final integration package based on the Day 7 compiler
outputs. It includes the compiler pipeline plus a native HTML/CSS/JavaScript
visualization page.

### Build and generate Web data

```bash
make clean
make
bash scripts/build_web_demo.sh
```

The script writes the files required by the Web page into `web/data/`:

```text
source.mc
tokens.json
ast.json
symbols.json
out.tac
out.bc
vm_output.txt
```

### Run the Web page

```bash
cd web
python3 -m http.server 8080
```

Then open:

```text
http://localhost:8080
```

The page displays source code, Token table, AST tree, symbol table, TAC and VM
bytecode. Clicking an AST node highlights the matching source line.

### VM trace

```bash
./build/microcc examples/ast_complex_demo.mc --trace-vm
```

The trace prints `pc`, current instruction, stack state and next `pc`.

---

## 📌 项目简介

**Micro C Compiler** 是一个面向教学场景的微型 C 语言编译器项目，目标是在两周实践周期内完成一个可以运行、可以演示、可以解释的 C 子集编译流程。

项目参考了 chibicc、9cc 等教学型 C 编译器的分阶段设计思想，但当前实现采用精简自实现方案，重点覆盖：词法分析、递归下降语法分析、语义分析、中间代码生成、自定义虚拟机字节码生成与执行。

本项目不是完整 C 编译器，当前主要支持教学用 MiniC 语法子集，便于小组成员分阶段开发、测试和答辩讲解。

---

## ✨ 已实现功能概览（持续更新）

### 基础要求覆盖情况

| 课程基础要求 | 当前实现情况 | 对应输出/证据 |
|---|---|---|
| 词法分析器 | 识别关键字、标识符、整型常量、浮点常量、运算符、分隔符，支持单行/多行注释 | `build/tokens.json`、`build/float_tokens.json` |
| 语法分析器 | 基于递归下降法，支持赋值、算术/逻辑表达式、`if-else`、`while`、`for` | `build/ast.json` |
| 语义分析 | 支持作用域管理、符号表、重复声明检测、未声明变量检测、简单 `int/float` 类型检查 | `build/symbols.json`、错误示例输出 |
| 中间代码生成 | 输出 AST JSON 和 TAC 三地址码 | `build/ast.json`、`build/out.tac` |
| 目标代码生成 | 生成自定义栈式 VM 字节码，并可实际执行 | `build/out.bc`、`Program return value: 13` |

### 当前支持的 MiniC 语法

- 变量声明：`int a;`、`float rate;`
- 初始化声明：`int a = 1;`
- 赋值语句：`a = a + 1;`
- 算术表达式：`+ - * / %`
- 关系表达式：`< <= > >= == !=`
- 逻辑表达式：`&& || !`
- 分支语句：`if-else`
- 循环语句：`while`、`for`
- 复合语句块：`{ ... }`
- 返回语句：`return expr;`
- 单行注释：`// comment`
- 多行注释：`/* comment */`

---

## 🧱 编译流程

```text
MiniC 源代码 .mc
        ↓
词法分析 Lexer
        ↓
Token 流 tokens.json
        ↓
递归下降 Parser
        ↓
AST 抽象语法树 ast.json
        ↓
语义分析 Semantic Analyzer
        ↓
符号表 symbols.json
        ↓
TAC 三地址码 out.tac
        ↓
VM 字节码 out.bc
        ↓
虚拟机执行 Program return value
```

---

## 📁 项目结构

```text
micro-c-compiler/
├── src/
│   ├── main.c                     # 总控入口：串联四个核心模块
│   ├── lexer.c                    # 成员A模块：词法分析与 Token JSON 输出
│   ├── parser_ast.c               # 成员B模块：递归下降语法分析与 AST JSON 输出
│   ├── semantic.c                 # 成员C模块：符号表、作用域与语义检查
│   └── ir_vm.c                    # 成员D模块：TAC、字节码生成与 VM 执行
├── include/
│   └── microcc.h                  # 公共数据结构、函数声明与工具函数
├── examples/
│   ├── basic_demo.mc              # 综合演示：for + while + if-else + return
│   ├── for_demo.mc                # for 循环测试
│   ├── scope_demo.mc              # 作用域与变量遮蔽测试
│   ├── comment_logic_demo.mc      # 注释与逻辑表达式测试
│   ├── float_token_demo.mc        # 浮点常量词法识别测试
│   ├── semantic_error_demo.mc     # 未声明变量错误测试
│   ├── duplicate_error_demo.mc    # 重复声明错误测试
│   └── type_error_demo.mc         # 类型检查错误测试
├── scripts/
│   ├── run_demo.sh                # 一键演示脚本
│   └── run_tests.sh               # 自动测试脚本
├── docs/
│   ├── PROJECT_SCOPE.md           # 项目范围说明
│   ├── ENVIRONMENT.md             # 推荐开发环境
│   ├── DEVELOPMENT_PLAN_8_DAYS.md # 8 天阶段开发计划
│   ├── DAILY_GIT_PLAN.md          # 每日 Git 提交建议
│   ├── MODULE_SPLIT.md            # 四模块拆分与成员职责说明
│   └── REPORT_POINTS.md           # 实验报告可写内容
├── Makefile
└── README.md
```

---

## 🛠️ 开发环境

推荐使用：

```text
Windows 11 + WSL2 Ubuntu + VS Code + Git
```

安装依赖：

```bash
sudo apt update
sudo apt install -y build-essential make git
```

不建议第一阶段直接使用 Visual Studio 原生 MSVC 编译，因为当前项目采用 `Makefile + gcc + bash` 的方式，WSL2 环境更接近 Linux，配置更简单。

---

## 🚀 编译与运行

### 1. 编译项目

```bash
make
```

生成可执行文件：

```text
build/microcc
```

### 2. 一键运行 Demo

```bash
bash scripts/run_demo.sh
```

运行成功后应看到：

```text
Program return value: 13
```

同时生成：

```text
build/tokens.json       词法分析结果
build/ast.json          AST JSON
build/symbols.json      符号表
build/out.tac           三地址码
build/out.bc            VM 字节码
build/float_tokens.json 浮点常量识别结果
```

### 3. 运行测试脚本

```bash
bash scripts/run_tests.sh
```

正常结果：

```text
Test summary: 9 passed, 0 failed
```

### 4. 手动运行单个文件

```bash
./build/microcc examples/basic_demo.mc \
  --dump-tokens build/tokens.json \
  --dump-ast build/ast.json \
  --dump-symbols build/symbols.json \
  --dump-tac build/out.tac \
  --dump-bytecode build/out.bc \
  --run
```

---

## 🧪 示例程序

```c
int main() {
    int a = 1;
    int sum = 0;

    for (int i = 0; i < 5; i = i + 1) {
        sum = sum + i;
    }

    while (a < 3) {
        sum = sum + a;
        a = a + 1;
    }

    if (sum > 10 && a == 3) {
        return sum;
    } else {
        return 0;
    }
}
```

该程序返回值为：

```text
13
```

---

## 👤 用户 ID / 成员姓名对照

> 第四位成员请按实际情况补充，便于课程验收和 Git 记录追溯。

- `musuwer`：柴承源 / Chai
- `kokomily`：刘健松 / Liu
- `git-li-hub`：李嘉杰 / Li
- `zz-zz823`：张周伟 / Zhou

---

## 🧩 模块分工与职责

> 当前代码已经拆分为“四个核心模块 + 一个主控入口”，便于四位成员分别维护、测试和扩展。

| 成员方向 | 负责文件 | 主要职责 | 可继续扩展内容 |
|---|---|---|---|
| 成员 A：Lexer & Project Base | `src/lexer.c`、`include/microcc.h`、`Makefile` | 词法分析、Token 类型、关键字/常量/注释识别、Token JSON 输出 | 增加字符串常量、字符常量、更多词法错误提示 |
| 成员 B：Parser & AST | `src/parser_ast.c` | 递归下降语法分析、AST 节点设计、AST JSON 输出 | 增加函数调用、数组语法、更多语法错误恢复 |
| 成员 C：Semantic Analyzer | `src/semantic.c` | 符号表、作用域管理、未声明变量、重复声明、类型检查 | 增加函数符号表、返回值类型检查、作用域可视化 |
| 成员 D：IR / VM / Tests | `src/ir_vm.c`、`scripts/run_tests.sh` | TAC 生成、字节码生成、虚拟机执行、自动测试 | 增加更多 VM 指令、运行时错误处理、后续 Qt 前端调用 |
| 集成入口 | `src/main.c` | 读取参数，串联 Lexer → Parser → Semantic → TAC → Bytecode → VM | 增加 `--dump-all`、前端调用接口、版本信息 |

---

## 🗓️ 项目进展日志（每日记录）

<details open>
<summary><b>点击展开 / 收起日志</b></summary>

### 2026-06-29
- 初始化 Micro C Compiler 项目结构；
- 完成最小可运行编译器 Demo；
- 支持 Lexer、Parser、Semantic Analyzer、TAC、Bytecode、VM 基础闭环；
- 添加 `basic_demo.mc`、`float_token_demo.mc`、`semantic_error_demo.mc` 示例；
- 运行 `scripts/run_demo.sh`，得到 `Program return value: 13`。
