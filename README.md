# C-Journey

[![CI](https://github.com/Awesome-Embedded-Learning-Studio/C-Journey/actions/workflows/ci.yml/badge.svg)](https://github.com/Awesome-Embedded-Learning-Studio/C-Journey/actions/workflows/ci.yml)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](./LICENSE)
[![GitHub stars](https://img.shields.io/github/stars/Awesome-Embedded-Learning-Studio/C-Journey?style=social)](https://github.com/Awesome-Embedded-Learning-Studio/C-Journey)

一份纯 C 的系统编程教程,定位是「用 C 游历计算机世界」。主机系统这一路——工具链、内存、数据结构、工程化、系统编程——做深;嵌入式只尝一口、开个门,真要写单片机和板级 Linux,去隔壁 [imx-forge](https://awesome-embedded-learning-studio.github.io/imx-forge/)(i.MX6ULL,目前最完整)和 [ST-Forge](https://awesome-embedded-learning-studio.github.io/ST-Forge/)(STM32)。

每章代码都用 gcc 16 + clang 22 双编、ASan/UBSan 抓 UB,贴真实终端输出、引 ISO/IEC 9899 条款号。C 的未定义行为太多,凭记忆断言迟早翻车,所以每条都当场跑给你看,而不是「据说是」。在线站点在 <https://awesome-embedded-learning-studio.github.io/C-Journey/>,锈橙主题,浏览器里能直接改 C 代码、点运行看输出或看 x86-64 汇编(调 godbolt 公共 API,不用装东西)。

## 现在到哪了

主线六阶段已经全部写完上线,共 84 章:

| 阶段 | 章 | 内容 |
|---|---|---|
| 0 开发环境 | 17 | 工具链 / 编译四阶段 / 链接与动态库 / 警告体系 / sanitizer / make / cmake / gdb / Git / CI / clang-format |
| 1 C 基底 | 13 | 程序结构 / 类型与算术 / 运算符 / 控制流 / 函数 / 作用域 / 数组 / 字符串 / IO / 结构体联合枚举 |
| 2 指针与内存 | 12 | 指针算术 / 动态内存 / 函数指针 / void\* 与字节操作 / 内存六区布局 |
| 3 数据结构 | 12 | 链表 / 栈队列 / 动态数组 / 二叉树与 BST / 哈希表 / 查找排序 / 大 O |
| 4 工程化 | 16 | 头文件契约 / API / 错误处理 / CMake 工程化 / 库与链接 / 测试与 Mock / gdb / ASan 与 valgrind / 静态分析 / 覆盖率 / 性能剖析 / CI 流水线 |
| 5 系统编程 | 14 | 文件 IO / fork-exec / 守护进程 / 信号 / pipe 与共享内存 / select 与 epoll / 非阻塞 reactor / socket TCP/UDP / getaddrinfo |

阶段 6(嵌入式)和阶段 7(capstone)还是占位旧稿,没重写——深做交给 imx-forge / ST-Forge,这里只留浅尝的 stub。完整设计思路见 [ROADMAP](./ROADMAP.md)。

## 怎么读、怎么改

按阶段顺序读 `documents/`(阶段 1 有[导读](./documents/01-c-basics/index.md));配套可编译示例在 `examples/`,完整项目在 `projects/`。改了东西,本地过两道门再提 PR:

```bash
python3 scripts/build_examples.py        # 编译所有 examples(gcc + clang 硬门)
python3 scripts/validate_frontmatter.py  # 校验文档 frontmatter
```

环境搭建、加文档和示例的规范、CI 的六道门(编译 / sanitize / 静态分析 / 覆盖率 / format / 文档校验),都写在 [CONTRIBUTING](./CONTRIBUTING.md) 里。

## 仓库结构

```text
C-Journey/
├── documents/          # 84 章主线文档(阶段 0-5)+ 更新日志
├── examples/           # 可编译示例(C 基础 / CMake 库工程 / TCP socket,CI 硬门)
├── projects/           # clib-utilities(已整改,进 CI 硬门)+ os-from-scratch 等参考实现
├── scripts/            # 质量门脚本(build_examples / validate_frontmatter / clang_tidy_check / tags)
├── ROADMAP.md          # 完整路线图(0-5 已上线,6-7 待重写)
├── CONTRIBUTING.md     # 怎么贡献
└── .github/workflows/  # CI
```

## 贡献

Issue 和 PR 都热烈欢迎, 不必拘束，小到错别字，大到您的笔记提交,我们都热烈欢迎！更新日志在[站点上](https://awesome-embedded-learning-studio.github.io/C-Journey/changelog/),按里程碑记了每个阶段的收官。
