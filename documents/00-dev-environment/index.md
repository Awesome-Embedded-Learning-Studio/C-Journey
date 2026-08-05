---
title: "阶段 0 · 开发环境与编译:18 章导读"
description: "阶段 0 的 18 章怎么读——它不是线性教材,是工具箱手册。三档分层:必读(读完就能动手写 C)/ 推荐(写几天就离不开)/ 进阶选读(撞墙或想深入时回查)。萌新走快速通道:Ch1 装好工具 → 跳阶段 1 写 C → 按需回查;折腾工程师从头线性读,是 C-Journey 的招牌体验。"
chapter: 0
order: 0
tags:
  - host
  - toolchain
difficulty: beginner
reading_time_minutes: 5
platform: host
c_standard: [11]
prerequisites: []
related:
  - "阶段 1 · C 语言基底:写 C 代码本身"
---

# 阶段 0 · 开发环境与编译:18 章导读

这一阶段讲的是「**写 C 之前,先把工具链弄通**」——gcc/clang 怎么编、make/cmake 怎么构建、gdb 怎么调、sanitizer 怎么抓错。可 18 章一口气读完,对萌新是劝退的:你可能一行 C 都没写,就被预处理、汇编、链接器顺序这些深度话题淹没。

所以这一阶段**不是线性教材,是工具箱手册**——按下面的分层挑着读,需要哪块回查哪块。

## 两条路径,挑一条

**🌱 萌新快速通道**(目标:两三章内写出能跑的 C):读完下面「必读」那两章,直接跳到 [阶段 1 · C 语言基底](../01-c-basics/) 去写 C(变量、控制流、函数、指针);写到需要构建、调试、排坑了,再回这里查「推荐」和「选读」。

**🔬 折腾工程师全读**(目标:把工具链彻底摸透):从头线性读到尾,这是 C-Journey 的招牌体验——命令行优先、亲手踩坑、贴真实输出。适合有别的语言经验、想真正搞懂 C 和底层的人。

## 🌱 必读 —— 读完就能动手写 C

- [01 工具链体检](./01-toolchain-health-check.md) —— 装好 gcc/clang/gdb/make/cmake、第一条 `gcc hello.c`、立「显式钉 -std」纪律(不钉会被 gcc 默认 C23、clang 默认 C17 坑)
- [03 编译四阶段全景](./03-save-temps-and-four-stages.md) —— 心智模型:`.c` 怎么一步步变成可执行(预处理 → 编译 → 汇编 → 链接);后面所有工具链章都是这四阶段的展开

## 🔧 推荐 —— 写了几天练习,这些很快就从「想用」变「离不开」

- [02 VSCode + Clangd](./02-vscode-clangd-setup.md) —— 把工具链接进编辑器,获得跳转/补全/实时诊断
- [09 警告旗标进阶](./09-warning-flags.md) —— `-Wall -Wextra -Werror`,让编译器替你抓低级错(萌新超需要)
- [10 标准与优化](./10-standards-and-optimization.md) —— 至少懂 `-std=c11` 要钉死、`-O` 在干嘛
- [12 make 入门](./12-make-basics.md) —— 文件一多,手敲 gcc 就烦了,make 替你管

## ⚗️ 进阶选读 —— 撞墙了 / 想深入了 / 做正经工程了,再按需回查

**编译器与链接器内部**(链接报错看不懂、想懂编译器在干嘛时):

- [04 预处理深入](./04-preprocessor-deep-dive.md) —— 宏、`#include`、条件编译的坑
- [05 编译阶段看汇编](./05-compile-to-assembly.md) —— 看懂 `-O` 对代码做了什么、ABI 是怎么回事
- [06 目标文件与符号](./06-object-files-and-symbols.md) —— `nm` 看 T/t/U/D/B,重定位
- [07 链接与静态库](./07-linking-and-static-libs.md) —— `undefined reference`、`multiple definition`、库顺序陷阱
- [08 动态库与 dlopen](./08-dynamic-libs-and-dlopen.md) —— `.so`、运行期加载

**抓错与排障**(程序崩了、出怪结果时):

- [11 Sanitizer 门禁](./11-sanitizer-gate.md) —— ASan/UBSan 当场抓内存错和 UB(神器,先知道存在就行)
- [14 GDB 基础](./14-gdb-basics.md) —— 段错误定位、断点单步、看变量
- [15 GDB 进阶](./15-gdb-advanced.md) —— 条件断点、watchpoint、core dump

**工程化与协作**(工程大了、要协作、上 CI 时):

- [13 CMake 入门](./13-cmake-basics.md) —— 工程大了 make 不够用,CMake 替你管跨平台构建
- [16 Git 工作流](./16-git-workflow.md) —— 版本控制基本功
- [17 GitHub Actions](./17-github-actions.md) —— CI 自动跑构建/测试/质量门
- [18 格式化与质量门](./18-format-and-quality-gate.md) —— clang-format 统一代码风格

## 学完之后

「必读 + 推荐」读完(6 章),你写 C 的工具链就齐了——能编、能跳转、能开警告、能构建、懂标准。接下来该真正写代码了:去 [阶段 1 · C 语言基底](../01-c-basics/) 学类型、运算、控制流、函数、指针。「选读」那 12 章不用急,它们是工具箱手册,等你写到对应场景(链接报错、程序崩、上 CI)再回来查就行。

> 全部代码 gcc 16 + clang 22 真编真跑(`-std=c11 -Wall -Wextra`),贴真实输出、引 ISO/IEC 9899 条款。
