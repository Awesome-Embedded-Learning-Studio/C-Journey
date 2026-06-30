---
title: "格式化与质量门：用 .clang-format 守住代码外观"
description: "阶段 0 收尾章。代码格式不统一会让 code review 充满「这里少个空格」的噪音、还放大合并冲突。这一章讲怎么用 clang-format + 一份 .clang-format 配置文件当「格式真相源」、机器统一所有代码的外观。逐项解读本项目真实 .clang-format 的关键设置（4 空格、行宽 100、Attach 大括号、SortIncludes Never、短 if 一律展开），真跑一个故意写乱的 mesy.c：--dry-run --Werror 把每一处不合规标成 error（退出码 1）、clang-format 一键把它改整齐；再说清怎么把格式化挂进编辑器、git pre-commit、以及第 16 章 CI 的 format-check job；最后把阶段 0 的整套质量门（build_examples + validate_frontmatter + sanitizer + clang-format）收拢成一张全景图。"
chapter: 0
order: 17
tags:
  - host
  - toolchain
  - build
difficulty: beginner
reading_time_minutes: 12
platform: host
c_standard: [11]
prerequisites:
  - "第 16 章：GitHub Actions（CI 的 format-check job）"
  - "第 9 章：标准与优化（.clang-format 的指针风格）"
related:
  - "第 16 章：GitHub Actions（format-check 自动跑 clang-format）"
  - "AGENTS.md / .claude/writing-style.md（代码与文档两侧的规范）"
---

> 🟡 状态：待审核（2026-06-30）

# 格式化与质量门：用 .clang-format 守住代码外观

## 引言：为什么要在意「格式」这种小事

你可能会觉得「代码能跑就行，少个空格、大括号换不换行有什么关系」。关系在团队协作上：如果十个人各按各的习惯敲，code review 就会塞满「这里 `if(` 没空格」「那里大括号该换行」这种纯风格争论——既浪费时间，又掩盖了真正该被 review 的逻辑问题；而且风格不一致会让 `git diff` 在「没动逻辑只是换了个人写」的地方刷一大片，合并冲突凭空变多。

解法是**让机器统一格式**：定一份格式规则，所有代码过一遍工具自动对齐，人不用吵。C/C++ 这边的事实标准是 **clang-format**，规则写在 `.clang-format` 文件里——这个文件就是「格式的真相源」，所有人和工具都认它。这一章我们看本项目的 `.clang-format` 定了什么、clang-format 怎么用、怎么挂进日常和 CI，最后把阶段 0 的整套质量门收拢。

## `.clang-format`：格式的真相源

本项目的 `.clang-format` 放在仓库根目录，clang-format 会自动找到它。挑几个最影响外观的关键项解读：

```yaml
BasedOnStyle: LLVM          # 以 LLVM 风格为基础,下面逐项覆盖
IndentWidth: 4              # 缩进 4 个空格(不用 Tab)
ColumnLimit: 100            # 一行最多 100 字符,超了自动折行
BreakBeforeBraces: Attach   # 大括号「贴」在行尾(if/函数的 { 不另起一行)
PointerAlignment: Left      # 指针贴类型:'int*' 而不是 'int *'
SortIncludes: Never         # 不自动重排 #include 顺序(保教学顺序)
AllowShortIfStatementsOnASingleLine: Never   # 短 if 也强制展开成多行
```

每一项都直接决定代码长什么样：`IndentWidth: 4` 就是 4 空格缩进、`ColumnLimit: 100` 行宽 100（第 11 章的 Makefile、第 4 章的代码都遵守这个）；`Attach` 决定大括号跟在行尾（`if (...) {` 不换行）；`SortIncludes: Never` 特别值得说——它**不**自动按字母重排你的 `#include`，因为第 1 章讲过 include 顺序有讲究（对应头 → 标准库 → 项目头），机器重排会破坏教学意图，所以这里关掉。`AllowShortIfStatementsOnASingleLine: Never` 是「教学代码要可读」的体现——哪怕 `if (x) return;` 能写一行，也强制展开成带大括号的多行。

这里有个诚实的点要交代（和第 9 章发现并上报的一致）：`.clang-format` 定的 `PointerAlignment: Left` 是「指针贴类型」（`int* p`），但本项目部分文档的内嵌代码目前用的是 C 传统的「贴变量名」写法（`int *p`）。两者不完全一致，真相源是 `.clang-format`，文档侧在逐步统一——遇到对不上时以 `.clang-format` 和 examples 目录的实际代码为准。这种「配置和存量代码偶尔错位、以配置为准逐步收敛」在真实工程里很常见。

## clang-format 怎么用：检查、格式化、原地改

我们写一个故意乱来的 `mesy.c` 当靶子（缩进忽深忽浅、`if(x==42)` 没空格、大括号贴得乱七八糟）：

```c
#include <stdio.h>
int main(void){
int x=42;
    if(x==42){
        printf("%d\n",x);
    }
  return 0;
}
```

clang-format 有三种用法。第一种**检查**——只看、不改，报告哪些行不合规，这正是 CI 的 format-check job 用的：

```text
$ clang-format --dry-run --Werror mesy.c
mesy.c:2:15: error: code should be clang-formatted [-Wclang-format-violations]
int main(void){
              ^
mesy.c:3:6: error: code should be clang-formatted [-Wclang-format-violations]
int x=42;
     ^
...
$ echo $?
1                                   ← 退出码 1,有格式问题
```

`--dry-run` 是「只模拟不真改」，`--Werror` 把「格式不合规」当成 error（于是退出码非 0）——第 16 章那个 `format-check` job 靠的就是这一行：CI 里它退出码非 0，提交就红。第二种**看格式化结果**（输出到屏幕、不改文件）：

```text
$ clang-format mesy.c
#include <stdio.h>
int main(void) {
    int x = 42;
    if (x == 42) {
        printf("%d\n", x);
    }
    return 0;
}
```

对比一下原文件：缩进统一成 4 空格、`if(x==42)` 变成 `if (x == 42)`（`if` 后和运算符两边补空格）、大括号按 Attach 摆好——这就是 `.clang-format` 那几项规则落地的样子。第三种**原地改**：加 `-i`（`clang-format -i mesy.c`），它直接把文件改整齐，平时整理代码用这个。

## 把格式化挂进日常和 CI

光有工具不够，得让它「自动」跑、不用人记。三个挂法。第一，**编辑器**：VS Code、Vim、CLion 都能配成「保存时自动 clang-format」，你一按 Ctrl+S，文件就自动按 `.clang-format` 整齐——这是最省心的，写的时候就是合规的。第二，**git pre-commit hook**：在 `.git/hooks/pre-commit` 里挂一段，`git commit` 前自动对本次改动的 `.c`/`.h` 跑 clang-format，格式不对就拦下提交。第三，**CI 的 format-check job**（第 16 章讲过）：每次 push/PR 自动跑 `clang-format --dry-run --Werror`，任何格式不合规直接 CI 红、挡合并——这是最后一道、也是最硬的一道。三层从近到远（编辑器→本地提交→远端 CI）把格式守住，人几乎不需要手动操心格式。

## 阶段 0 质量门全景

到这，我们把阶段 0 攒下的所有质量门收拢成一张全景。本地自检时，你跑这几条就能复现 CI 的核心检查：

```text
$ python3 scripts/build_examples.py          # 1. 每个示例都能编译(硬门)
✅ examples 全部构建通过。
$ python3 scripts/validate_frontmatter.py    # 2. 文档 frontmatter 合法
✅ frontmatter 全部通过。
$ git ls-files 'examples/*.c' 'examples/*.h' | xargs clang-format --dry-run --Werror
$ echo $?                                     # 3. examples 代码格式合规
0
```

再配上第 10 章的 sanitizer 构建（`-fsanitize=address,undefined`）和第 8 章的 `-Werror`（藏在每个示例的编译旗标里），就构成了本项目的完整防线。这套门不只是「写完检查一下」，它在第 16 章的 CI 里**每次提交都自动强制执行**——任何一道没过，代码就合不进 `main`。文档这侧也有对应的「质量门」：[AGENTS.md](../../AGENTS.md) 和 `.claude/writing-style.md` 定了代码风格、C 铁律、写作声音，`validate_frontmatter.py` 校验 frontmatter 字段，markdownlint 查 Markdown 写法——代码和文档两侧都有自己的真相源和检查工具。

## 小结：阶段 0 收尾

clang-format 用一份 `.clang-format` 当格式的真相源，机器统一所有代码外观，把「风格之争」从人手里拿走交给机器。本项目 `.clang-format` 的关键项：4 空格缩进、行宽 100、Attach 大括号、`SortIncludes: Never`（不破坏 include 教学顺序）、短 if 强制展开；指针 `PointerAlignment: Left`（贴类型 `int*`），注意存量文档代码与此略有出入、以 `.clang-format` 为准逐步统一。clang-format 三种用法：`--dry-run --Werror` 只检查不改动（CI format-check 用它，退出码非 0 即红）、不带参数输出格式化结果、`-i` 原地改文件；把它挂进「编辑器保存时格式化 → pre-commit hook → CI」三层，格式就基本不用人操心。阶段 0 到此收尾，整套质量门是：`build_examples.py`（每个示例能编）+ `validate_frontmatter.py`（frontmatter 合法）+ `clang-format --dry-run --Werror`（格式合规）+ sanitizer（`-fsanitize`，第 10 章）+ `-Werror`（第 8 章，藏在编译旗标里），全由第 16 章的 CI 在每次提交自动强制。从第 1 章的工具链体检到这一章，开发环境与编译这条线就完整了——下一阶段我们要钻进 C 语言本身，从程序结构、整型家族、指针内存一路往下，那时你会反复用到这一阶段立起来的工具（gcc/clang 的旗标、make/CMake、GDB、sanitizer、CI）。

## 参考资源

- clang-format 文档与 `clang-format --dump-config`（所有配置项；本项目 [.clang-format](../../.clang-format)）
- `clang-format --help`：`--dry-run`/`-i`/`--Werror`/`--style=file`
- 第 16 章：GitHub Actions（CI 的 format-check job）、第 8 章：警告旗标（`-Werror`）、第 10 章：Sanitizer 门禁
- 本项目 [AGENTS.md](../../AGENTS.md)（代码风格与 C 铁律）、`.claude/writing-style.md`（文档写作规范，文档侧的「质量门」）
