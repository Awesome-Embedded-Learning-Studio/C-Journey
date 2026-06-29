# 学习文档（documents）

各阶段学习笔记与知识点文档，按 **阶段编号** 组织，与 [ROADMAP](../ROADMAP.md) 一一对应。**8 个阶段全部有内容，共 41 篇**。建议先读对应阶段的文档，再到 [examples](../examples/) / [projects](../projects/) 动手实践。

> 写作风格遵循 [.claude/writing-style.md](../.claude/writing-style.md)：frontmatter + 真实输出铁律 + 折腾工程师声音。新增 / 修改文档后请过门：`python3 scripts/validate_frontmatter.py` 与 markdownlint（见 [CONTRIBUTING](../CONTRIBUTING.md)）。

---

## 目录索引

### 阶段 0 · 开发环境与基础工具 — [00-dev-environment/](./00-dev-environment/)

- [Git 基础操作：给 C 代码上个后悔药](./00-dev-environment/git-basic-operations.md)
- [编译流程、GDB 与库：把 .c 变成可执行文件的每一步](./00-dev-environment/1-compiling-and-debugging.md)

### 阶段 1 · C 语言基础（20 章，从零到扎实） — [01-c-basics/](./01-c-basics/) · [导读入口](./01-c-basics/index.md)

- 01 [程序结构与编译基础](./01-c-basics/01-program-structure-and-compilation.md)
- 02A [数据类型基础：整数与内存](./01-c-basics/02A-data-types-basics.md) · 02B [浮点、字符、const 与类型转换](./01-c-basics/02B-float-char-const-cast.md)
- 03A [运算符基础](./01-c-basics/03A-operators-basics.md) · 03B [位运算与求值顺序](./01-c-basics/03B-bitwise-and-evaluation.md)
- 04 [控制流：选择与重复](./01-c-basics/04-control-flow.md)
- 05 [函数基础与参数传递](./01-c-basics/05-function-basics.md) · 06 [作用域与存储类别](./01-c-basics/06-scope-and-storage.md)
- 07A [指针入门：地址的世界](./01-c-basics/07A-pointer-essentials.md) · 07B [指针与数组、const 和空指针](./01-c-basics/07B-pointers-arrays-const.md)
- 08A [多级指针与声明读法](./01-c-basics/08A-multi-level-pointers.md) · 08B [restrict、不完整类型与结构体指针](./01-c-basics/08B-restrict-incomplete-types.md)
- 09 [函数指针与回调模式](./01-c-basics/09-function-pointers-and-callbacks.md)
- 10 [数组深入](./01-c-basics/10-arrays-deep-dive.md) · 11 [C 字符串与缓冲区安全](./01-c-basics/11-c-strings-and-buffer-safety.md)
- 12 [结构体与内存对齐](./01-c-basics/12-struct-and-memory-alignment.md) · 13 [联合体、枚举、位域与 typedef](./01-c-basics/13-union-enum-bitfield-typedef.md)
- 14 [动态内存管理](./01-c-basics/14-dynamic-memory.md) · 15 [预处理器与多文件工程](./01-c-basics/15-preprocessor-and-multifile.md) · 16 [文件 I/O 与标准库概览](./01-c-basics/16-file-io-and-stdlib.md)

### 阶段 2 · 指针、内存与数据布局 — [02-pointers-memory/](./02-pointers-memory/)

- [指针、内存布局与位运算](./02-pointers-memory/0-pointers-memory-and-bitops.md)
- [C 陷阱与坑：把「编译能过、运行就炸」的地方揪出来](./02-pointers-memory/1-c-pitfalls-and-traps.md)

### 阶段 3 · 数据结构与算法基础 — [03-data-structures/](./03-data-structures/)

- [用 C 造一个泛型容器：从 void* 到自己的 vector](./03-data-structures/0-generic-containers-in-c.md)
- [递归与调用栈：函数调自己的本质](./03-data-structures/1-recursion-and-call-stack.md)
- [动态数组 API 设计实战](./03-data-structures/2-dynamic-array.md)
- [用 C 手搓单链表：节点到一整套增删查转 API](./03-data-structures/3-linked-list.md)
- [算法分析与查找排序：用真实计时把 Big-O 变成手感](./03-data-structures/4-algorithm-analysis.md)

### 阶段 4 · 工程化与模块化设计 — [04-engineering/](./04-engineering/)

- [CMake 与模块化：让多文件工程不再靠手搓 gcc](./04-engineering/0-cmake-and-modules.md)
- [ASan 与 UBSan：让内存错误和未定义行为当场现形](./04-engineering/1-sanitizers-asan-and-ubsan.md)
- [符号与链接：读懂 undefined reference](./04-engineering/2-symbols-and-linking.md)
- [程序性能与剖析：把 C 程序的慢处逼出来](./04-engineering/3-performance-and-profiling.md)
- [CMake 进阶：target_*、PRIVATE/PUBLIC/INTERFACE、find_package 与 install](./04-engineering/4-cmake-in-depth.md)

### 阶段 5 · 系统编程 — [05-system-programming/](./05-system-programming/)

- [Socket 编程：用 TCP 从零写客户端/服务端](./05-system-programming/0-socket-programming.md)
- [进阶 Socket：地址复用、SIGPIPE、消息边界与并发服务端](./05-system-programming/1-advanced-socket.md)
- [stdio 与文件 IO：FILE 指针、缓冲与底层系统调用](./05-system-programming/2-stdio-and-file-io.md)

### 阶段 6 · 嵌入式 C 与硬件 — [06-embedded/](./06-embedded/)

- [裸机 C 入门：在 8051 上写一个会跑的时钟](./06-embedded/0-8051-bare-metal-basics.md)
- [Linux 字符设备驱动入门：从 hello-world 模块到用户态交互](./06-embedded/1-linux-driver-basics.md)

### 阶段 7 · 综合项目与开源协作 — [07-capstone/](./07-capstone/)

- [自制操作系统：从按电源到内核 main 的引导链实战](./07-capstone/0-hand-written-os.md)（对接 [projects/os-from-scratch](../projects/os-from-scratch/)）

### 进阶专题 — [advanced/](./advanced/)

- [从零写一个操作系统：通电到内核的接力赛](./advanced/0-os-from-scratch.md)

---

## 配套实践

- [examples/](../examples/) — 按阶段的可编译示例（编译调试 / C 基础 / TCP socket，均带 CMakeLists，CI 会编译）
- [projects/](../projects/) — 完整项目（clib-utilities、embedded-mcu、os-from-scratch、song、tiny-c-stdlib）
- [exercises/](../exercises/) — 练习题（按阶段规划中）
