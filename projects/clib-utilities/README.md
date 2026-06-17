# 项目：C 工具库 (CCSTD UtilsLib)

> 对应 [ROADMAP](../../ROADMAP.md) · **阶段 4：工程化与模块化设计**
> 📖 先读知识点：[CMake 与模块化：让多文件工程不再靠手搓 gcc](../../documents/04-engineering/0-cmake-and-modules.md)

一个用 **CMake** 组织的可复用 C 基础库，演示模块化设计、头文件分离与测试组织方式——是阶段 4 推荐项目“C 基础库 clib”的实现雏形。

## 模块结构

```text
clib-utilities/
├── Basic_Utils/         # 基础工具（字符串、内存等）
│   ├── Includes/  Sources/
├── BasicDataStructure/  # 动态数组、链表等数据结构
├── SystemRelated/       # 系统相关（互斥锁、动态插件等）
├── test/                # 各模块测试用例
├── main.c               # 测试入口（选择启用哪个 test）
└── CMakeLists.txt
```

## 如何构建

```bash
mkdir build && cd build
cmake ..
make
./TEST_RES
```

> 在 `main.c` 里取消注释来选择运行哪个测试（`testDynamicArray` / `testMutex` / `testDynamicPlugin`）。

## 学习要点

- `.h` / `.c` 分离、include 目录与源码目录分离
- CMake 的 `GLOB_RECURSE` 收集源码、`target_include_directories` 设置头文件路径
- 模块化：每个子系统拥有独立的 `Includes/` + `Sources/`

---
*整理自 2023–2024 学习存档，作为阶段 4 工程化实践的起点。*
