# 阶段 1–2 示例：C 基础与内存观察

> 对应 [ROADMAP](../../ROADMAP.md) · **阶段 1：C 语言基础** / **阶段 2：指针、内存与数据布局**
> 📖 先读知识点：[指针、内存布局与位运算](../../documents/02-pointers-memory/0-pointers-memory-and-bitops.md)

用一组小程序观察变量存储、堆内存分配与位运算的底层效果（最初在 ARM 环境下练习，原理同样适用于桌面平台）。

## 目录

| 实验 | 主题 |
|---|---|
| `Exp1/var.c` | 变量的定义、存储与作用域 |
| `Exp2/heap.c` | `malloc` / `free` 与堆内存布局 |
| `Exp3/moveBits*.c` | 移位与位运算 |
| `Exp4/main.c` | 综合练习 |

## 如何使用

```bash
gcc Exp1/var.c -o var && ./var
gcc Exp2/heap.c -o heap && ./heap
```

## 学习要点

- 变量在栈上的地址分布；用 GDB 观察寄存器与内存
- `malloc` 返回的堆地址与栈地址的区别
- 移位运算 `<<` `>>` 与掩码技巧

---
*整理自 2023–2024 学习存档。*
