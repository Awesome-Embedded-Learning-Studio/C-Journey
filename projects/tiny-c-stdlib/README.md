# 项目：迷你 C 标准库 (Tiny C Standard Library)

> 对应 [ROADMAP](../../ROADMAP.md) · **阶段 3：数据结构与算法基础** / **阶段 4：工程化与模块化设计**
> 📖 先读知识点：[用 C 造一个泛型容器：从 void* 到自己的 vector](../../documents/03-data-structures/0-generic-containers-in-c.md)

用 C 从零实现常见数据结构与字符串库，是“自己造轮子理解底层”的练习。对应阶段 3 的 **C STL Mini** 与阶段 4 的 **C 基础库**。

## 内容

### `CCSTDC_Tiny_Version/`（教学 / blog 版）
每个子目录是一个独立数据结构的实现：
- `CCSTDC_VectorsForBlog/` — 动态数组 Vector
- `CCSTDC_DRLinkList_Blog/` — 双向链表
- `CCSTDC_Stack_Blog/` — 栈
- `CCSTDC_Queue_Blog/` — 队列
- `CCSTDC_HashTable/` — 哈希表
- `CCSTDC_Tree/` — 树
- `CCSTDC_Utils_Blog/` — 工具函数

### `CCSTDLIB_Exportive/`
- `CString/` — 动态字符串实现（可导出版本）

## 学习要点

- 每个容器的完整生命周期：初始化、扩容、增删查改、销毁
- `void*` 泛型接口、回调函数、自定义释放函数
- 内存管理与错误处理

## 后续练习

以此为参考，对照 [ROADMAP](../../ROADMAP.md) 阶段 3，自己重写一份更完善的 `vector` / `hashmap`，并补上单元测试。

---
*整理自 2023–2024 学习存档。原项目曾混用 C++，本仓库仅保留纯 C 部分。*
