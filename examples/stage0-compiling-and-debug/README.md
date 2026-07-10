# 阶段 0 示例：编译流程、调试与库

> 对应 [ROADMAP](https://awesome-embedded-learning-studio.github.io/C-Journey/roadmap) · **阶段 0：开发环境与基础工具**
> 📖 先读知识点：[编译流程、GDB 与库](../../documents/00-dev-environment/1-compiling-and-debugging.md)

一组围绕“把 `.c` 变成可执行文件”全过程的小实验，覆盖预处理、汇编、链接、GDB 调试以及动 / 静态库的使用。

## 你会练到

- **编译四阶段**：预处理 → 编译 → 汇编 → 链接
- **查看中间产物**：`gcc -E`（预处理 `.i`）、`gcc -S`（汇编 `.s`）
- **GDB 调试**：断点、单步、查看变量、定位段错误
- **静态库 / 动态库**：制作与使用 `.a` / `.so`

## 目录

| 目录 | 主题 |
|---|---|
| `gdb_use/` | 用 GDB 调试含 `error.c` 的程序，练习断点与回溯 |
| `1/` | 多文件工程 + 编译中间产物（`.i` / `.s`）+ 反汇编观察 |
| `2/` | 动态库的生成与使用 |

## 如何使用

```bash
# 重新生成预处理 / 汇编产物
gcc -E function.c -o function.i
gcc -S function.c -o function.s

# GDB 调试
gcc -g gdb_use/*.c -o demo
gdb ./demo
```

> 目录中保留的 `.i` / `.s` 是编译中间产物示例，可用上面命令随时重新生成。

---
*整理自 2023–2024 学习存档，作为阶段 0 的配套练习。*
