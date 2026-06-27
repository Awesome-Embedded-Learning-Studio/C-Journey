# AGENTS.md

> 给所有 AI 编码助手(Agent)的统一入口。人类贡献者请看 [CONTRIBUTING.md](CONTRIBUTING.md)。

## 你在哪、这是什么

**C-Journey** —— 一个 C 语言工程化进阶教程仓库(**纯 C,不是 C++**)。目标:让学习者从语法走到能写可维护的 C 工程。8 个阶段:开发环境 → C 基础 → 指针内存 → 数据结构 → 工程化 → 系统编程 → 嵌入式 → 综合项目。

## 必读契约

1. **写作风格**:`.claude/writing-style.md` —— Part 1 硬规则必须遵守(frontmatter、文章骨架、C 代码风格、真实输出铁律);Part 2 是目标声音,尽量贴合。
2. **标签与 C 标准**:`scripts/tags.py` —— 单一真相源,frontmatter 的 `tags` / `c_standard` 取值都从这里来。
3. **质量门**:`scripts/build_examples.py`(编译 examples)+ `scripts/validate_frontmatter.py`(校验文档)。CI(`.github/workflows/ci.yml`)是最终硬门。

## C 专属铁律(比 C++ 更要命)

C 的未定义行为 / 实现定义行为极多。**断言任何 C 行为之前**:

1. 在 `/tmp/` 写最小 `.c`,
2. 用 `gcc -Wall -Wextra -std=cXX`(必要时加 `-fsanitize=undefined,address`)编译跑一遍,
3. 引用 ISO C 条款,

再下结论。**不要凭记忆断言 C 的行为**(尤其整型溢出、位移、严格别名、未初始化、大小端)。

## 不要做的事

- 不要 `git push`(推送由维护者统一处理)。
- commit 不要带 `Co-Authored-By`。
- 不要编造输出 / 汇编 / 地址(真实输出铁律)。
- 不要把 C++ 风格(`class` / `std::` / `cpp_standard` / `enum class` / 模板)写进本仓库的代码或文档。
- 不要大改 `projects/` 下的遗留项目(各自整改中);要动先开 Issue。

## 改了代码 / 文档之后

跑 `scripts/build_examples.py` 和 `scripts/validate_frontmatter.py`,本地过了再说完成。examples 代码用 `clang-format` 格式化。
