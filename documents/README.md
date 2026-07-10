# 学习文档（documents）

这里是 C-Journey 的全部知识点文档,按阶段分目录。主线六阶段(0-5)已经写完,共 84 章;阶段 6(嵌入式)/ 7(capstone)/ 进阶尚未动笔。

每章都按同一套规矩来:frontmatter 齐全、代码用 gcc 16 + clang 22 真编真跑(ASan/UBSan 抓 UB)、贴真实终端输出、引 ISO/IEC 9899 条款号。声音是「折腾工程师」——坑就地讲、不套预警框、小结走散文。完整规矩在 [.claude/writing-style.md](../.claude/writing-style.md),动手写之前先看一眼。

嵌入式这条线,这里只浅尝、开个门;真要写单片机和板级 Linux,去隔壁 [imx-forge](https://awesome-embedded-learning-studio.github.io/imx-forge/)、[ST-Forge](https://awesome-embedded-learning-studio.github.io/ST-Forge/)([Awesome-Embedded 工作室](https://awesome-embedded-learning-studio.github.io/Awesome-Embedded/)出品)。

> 新增或改完文档,本地过两道门再提交:`python3 scripts/validate_frontmatter.py`、`python3 scripts/build_examples.py`(详见 [CONTRIBUTING](../CONTRIBUTING.md))。

---

## 目录索引

### 阶段 0 · 开发环境与编译(17 章) — [00-dev-environment/](./00-dev-environment/)

- 01 [工具链体检](./00-dev-environment/01-toolchain-health-check.md) · 02 [编译四阶段全景(-save-temps)](./00-dev-environment/02-save-temps-and-four-stages.md) · 03 [预处理深入](./00-dev-environment/03-preprocessor-deep-dive.md)
- 04 [编译阶段看汇编](./00-dev-environment/04-compile-to-assembly.md) · 05 [目标文件与符号](./00-dev-environment/05-object-files-and-symbols.md) · 06 [链接与静态库](./00-dev-environment/06-linking-and-static-libs.md) · 07 [动态库与 dlopen](./00-dev-environment/07-dynamic-libs-and-dlopen.md)
- 08 [警告旗标进阶](./00-dev-environment/08-warning-flags.md) · 09 [标准与优化](./00-dev-environment/09-standards-and-optimization.md) · 10 [Sanitizer 门禁](./00-dev-environment/10-sanitizer-gate.md)
- 11 [make 入门](./00-dev-environment/11-make-basics.md) · 12 [CMake 入门](./00-dev-environment/12-cmake-basics.md)
- 13 [GDB 基础](./00-dev-environment/13-gdb-basics.md) · 14 [GDB 进阶](./00-dev-environment/14-gdb-advanced.md)
- 15 [Git 工作流](./00-dev-environment/15-git-workflow.md) · 16 [GitHub Actions](./00-dev-environment/16-github-actions.md) · 17 [格式化与质量门](./00-dev-environment/17-format-and-quality-gate.md)

### 阶段 1 · C 语言基底(13 章) — [01-c-basics/](./01-c-basics/) · [导读入口](./01-c-basics/index.md)

- 01 [程序结构与编译四阶段](./01-c-basics/01-program-structure-and-compilation.md) · 02 [整型家族与 sizeof](./01-c-basics/02-integer-types-and-sizeof.md) · 03 [整型提升、溢出与回绕](./01-c-basics/03-integer-promotion-overflow.md) · 04 [浮点、字符、常量与隐式转换](./01-c-basics/04-float-char-const-cast.md)
- 05 [运算符基础](./01-c-basics/05-operators-basics.md) · 06 [位运算与移位](./01-c-basics/06-bitwise-and-shift.md) · 07 [控制流](./01-c-basics/07-control-flow.md)
- 08 [函数](./01-c-basics/08-functions.md) · 09 [作用域、存储期与 static](./01-c-basics/09-scope-storage-static.md)
- 10 [数组](./01-c-basics/10-arrays.md) · 11 [C 字符串与不安全 libc](./01-c-basics/11-c-strings-and-libc.md) · 12 [基础 IO](./01-c-basics/12-basic-io.md) · 13 [结构体、联合、枚举与内存对齐](./01-c-basics/13-struct-union-enum.md)

### 阶段 2 · 指针与内存(12 章) — [02-pointers-memory/](./02-pointers-memory/)

- 01 [指针是什么](./02-pointers-memory/01-what-is-a-pointer.md) · 02 [指针算术](./02-pointers-memory/02-pointer-arithmetic.md) · 03 [用指针改调用者的变量](./02-pointers-memory/03-pointer-parameters.md) · 04 [const 限定](./02-pointers-memory/04-const-qualifier.md)
- 05 [指针、数组、字符串的统一视角](./02-pointers-memory/05-pointer-array-string.md) · 06 [动态内存入门 malloc/free](./02-pointers-memory/06-malloc-free-basics.md) · 07 [动态内存的坑(ASan 抓)](./02-pointers-memory/07-dynamic-memory-pitfalls.md)
- 08 [多级指针与指针数组](./02-pointers-memory/08-multi-level-pointers.md) · 09 [函数指针](./02-pointers-memory/09-function-pointers.md) · 10 [复杂声明与 typedef](./02-pointers-memory/10-complex-declarations-typedef.md)
- 11 [void* 与字节操作](./02-pointers-memory/11-void-ptr-and-byte-ops.md) · 12 [内存布局与生命周期](./02-pointers-memory/12-memory-layout.md)

### 阶段 3 · 数据结构与算法(12 章) — [03-data-structures/](./03-data-structures/)

- 01 [单链表](./03-data-structures/01-singly-linked-list.md) · 02 [双向链表](./03-data-structures/02-doubly-linked-list.md) · 03 [栈](./03-data-structures/03-stack.md) · 04 [队列](./03-data-structures/04-queue.md)
- 05 [动态数组](./03-data-structures/05-dynamic-array.md) · 06 [二叉树基础](./03-data-structures/06-binary-tree.md) · 07 [二叉搜索树 BST](./03-data-structures/07-bst.md) · 08 [哈希表](./03-data-structures/08-hash-table.md)
- 09 [排序入门 O(n²)](./03-data-structures/09-sorting-quadratic.md) · 10 [快排与归并](./03-data-structures/10-quicksort-mergesort.md) · 11 [二分查找](./03-data-structures/11-binary-search.md) · 12 [算法复杂度与大 O](./03-data-structures/12-big-o-complexity.md)

### 阶段 4 · 工程化与质量门(16 章) — [04-engineering/](./04-engineering/)

- 01 [头文件契约:include guard / ODR / inline](./04-engineering/01-header-contracts.md) · 02 [API 设计与不透明类型](./04-engineering/02-api-and-opaque-types.md) · 03 [错误处理三件套](./04-engineering/03-error-handling.md)
- 04 [make 深处:-MMD / -j 竞态](./04-engineering/04-make-deep.md) · 05 [CMake 工程化:target 语义](./04-engineering/05-cmake-engineering.md) · 06 [静态/动态库 + 链接顺序 + install/export](./04-engineering/06-libs-and-linking.md)
- 07 [测试:assert / Unity / CTest](./04-engineering/07-testing-with-unity.md) · 08 [Mock 与隔离:--wrap / weak](./04-engineering/08-mock-and-isolation.md)
- 09 [gdb 实战:多线程栈 / coredump](./04-engineering/09-gdb-multi-thread.md) · 10 [ASan+UBSan 深入:复现 CI 门](./04-engineering/10-sanitizer-deep.md) · 11 [valgrind 与 sanitizer 分工](./04-engineering/11-valgrind.md)
- 12 [静态分析门:clang-tidy / cppcheck](./04-engineering/12-static-analysis.md) · 13 [覆盖率门:gcov / lcov](./04-engineering/13-coverage.md) · 14 [性能剖析:clock_gettime / gprof / perf](./04-engineering/14-profiling.md)
- 15 [CI 流水线整合](./04-engineering/15-ci-pipeline.md) · 16 [工程化毕业项目:从能编到可信](./04-engineering/16-capstone.md)

### 阶段 5 · 系统编程(14 章) — [05-system-programming/](./05-system-programming/)

- 01 [文件 IO 与 fd](./05-system-programming/01-file-io-and-fd.md) · 02 [fork、写时复制与 stdio 陷阱](./05-system-programming/02-fork-cow-and-stdio-traps.md) · 03 [exec 与 wait](./05-system-programming/03-exec-and-wait.md) · 04 [守护进程与孤儿](./05-system-programming/04-daemons-and-orphans.md)
- 05 [信号:sigaction / async-signal-safe](./05-system-programming/05-signals.md) · 06 [pipe 与 FIFO](./05-system-programming/06-pipe-and-fifo.md) · 07 [共享内存与信号量](./05-system-programming/07-shm-and-semaphores.md)
- 08 [select](./05-system-programming/08-select.md) · 09 [poll 与 epoll](./05-system-programming/09-poll-and-epoll.md) · 10 [非阻塞 IO 与 reactor](./05-system-programming/10-nonblock-and-reactor.md)
- 11 [Socket TCP:客户端/服务端四件套](./05-system-programming/11-socket-tcp.md) · 12 [进阶 Socket:SIGPIPE / 消息边界](./05-system-programming/12-socket-advanced.md) · 13 [UDP 与 Unix 域套接字](./05-system-programming/13-udp-and-unix-domain.md) · 14 [getaddrinfo 与协议无关](./05-system-programming/14-getaddrinfo.md)

### 阶段 6 · 嵌入式(尚未写,浅尝定位)

> 嵌入式这条线 C-Journey 只浅尝、开个门;真要写单片机和板级 Linux,去隔壁 [imx-forge](https://awesome-embedded-learning-studio.github.io/imx-forge/)、[ST-Forge](https://awesome-embedded-learning-studio.github.io/ST-Forge/)。本阶段尚未动笔。

### 阶段 7 · 综合收官(尚未写,规划中)

> 综合收官项目尚未动笔,规划中。

---

## 配套实践

- [examples/](../examples/) — 按阶段的可编译示例(stage0 编译调试 / stage1 C 基础 / stage4 CMake 库工程 / stage5 TCP socket,均带 CMakeLists,CI 硬门编译)
- [projects/](../projects/) — 完整项目(clib-utilities、embedded-mcu、os-from-scratch、song、tiny-c-stdlib)
- [exercises/](../exercises/) — 练习题(按阶段规划中)
