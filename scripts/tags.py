"""C-Journey 单一标签源(single source of truth)。

所有 frontmatter 校验、文档索引、搜索过滤都从这里取标签集合。
**改标签只改这一个文件** —— 这是标杆仓库吃过「两份白名单各自漂移、最后弄坏 CI」
的教训。validate_frontmatter.py 等都从这里 import。

本集合已对齐 documents/ 现有文档实际使用的拼写,作为后续唯一标准。
"""

# 平台标签(描述代码运行目标)
PLATFORM_TAGS = {
    "host",   # 宿主机(非嵌入式)
    "stm32",  # STM32 / ARM Cortex-M
    "mcu51",  # 8051 单片机
    "8051",   # 8051(与 mcu51 等价,兼容历史文档)
}

# 主题标签(描述所属知识域;每个文档至少打一个)
TOPIC_TAGS = {
    # 工具链 / 工程(阶段 0 / 4)
    "toolchain",          # 编译器/工具链(gcc, clang)
    "build",              # 构建系统(make, cmake)
    "make",               # Makefile
    "cmake",              # CMake
    "gdb",                # GDB 调试
    "git",                # 版本控制
    "engineering",        # 工程化 / 模块化 / 库设计
    "testing",            # 测试
    "debug",              # 调试技巧
    "open-source",        # 开源协作 / CI
    # C 语言基础(阶段 1)
    "syntax",         # 语法基础
    "type",           # 类型系统
    "operator",       # 运算符
    "control-flow",   # 控制流
    "function",       # 函数
    "macro",          # 宏 / 预处理
    # 指针与内存(阶段 2)
    "pointers",         # 指针
    "memory",           # 内存管理 / 存储期 / 作用域
    "bit-manipulation",  # 位运算
    "volatile",         # volatile / 寄存器 / 内存模型
    # 数据结构与算法(阶段 3)
    "data-structures",  # 数组 / 链表 / 树 / 哈希等
    "generics",         # 泛型(C 的 void*/宏泛型)
    "algorithm",        # 算法
    "struct",           # 结构体 / 联合 / 枚举
    "state-machine",    # 状态机
    # 系统编程(阶段 5)
    "system-programming",  # 系统编程总称
    "networking",          # 网络
    "posix",               # POSIX API
    "socket",              # socket
    "ipc",                 # 进程间通信
    "thread",              # 线程
    "concurrency",         # 并发
    "file-io",             # 文件 IO
    "linker",              # 链接 / 库
    # OS / 高级
    "os",        # 操作系统 / 内核
    "asm",       # 汇编
    "boot",      # 启动 / 引导
    "advanced",  # 高级专题
    # 嵌入式(阶段 6)
    "embedded",    # 嵌入式通用
    "interrupts",  # 中断
    "driver",      # 驱动分层
    "meta",        # 元文档(项目本身的 changelog/roadmap 等)
}

# 全部合法标签
ALL_TAGS = PLATFORM_TAGS | TOPIC_TAGS

# 难度
DIFFICULTIES = {"beginner", "intermediate", "advanced"}

# 支持的 C 标准(c_standard frontmatter 取值)
C_STANDARDS = {89, 90, 99, 11, 17, 23}

# 课程阶段编号(0..7 为正式阶段);chapter 亦接受任意整数(供 advanced/misc 用)
CHAPTERS = {0, 1, 2, 3, 4, 5, 6, 7}
EXTRA_CHAPTERS = {"advanced"}


def is_valid_tag(tag) -> bool:
    """tag 可能是 str 也可能是 YAML 解析出的 int(如 8051),统一按 str 比较。"""
    return str(tag) in ALL_TAGS
