# 贡献指南

感谢参与 C-Journey!无论改一个错别字、补一个示例,还是把您自己的学习笔记整理成一整章投上来,都热烈欢迎——别拘束,小贡献一样算数。

## 仓库地图

| 路径 | 作用 |
|---|---|
| `documents/` | 各阶段知识点文档(主线六阶段 84 章 + 嵌入式/capstone 待重写) |
| `examples/` | 配套可编译示例(按阶段) |
| `projects/` | 完整项目(clib-utilities 已整改进 CI 硬门;os-from-scratch 等为参考实现,改动前开 Issue) |
| `ROADMAP.md` | 8 阶段总路线(0-5 已上线,6-7 待重写) |
| `.claude/writing-style.md` | **写作风格契约(必读)** |
| `scripts/` | 质量门脚本(build_examples / validate_frontmatter / clang_tidy_check / tags) |

## 开发环境

需要:`gcc`(16+)、`clang`(22+)、`cmake`、`ninja`、`python3` + `pyyaml`、`clang-format`、`clang-tidy`;改文档站还需 Node + `pnpm`。

```bash
sudo apt install gcc clang cmake ninja-build clang-format python3-pip
pip install pyyaml pre-commit
pre-commit install   # 安装 git 钩子(可选但推荐)
```

## 如何加一个示例

1. 在 `examples/<stage>/` 下建目录,放 `.c` / `.h`。
2. 加一个最小 `CMakeLists.txt`(参考 `examples/stage5-tcp-socket/SC1/CMakeLists.txt`)。
3. 代码遵循 `.clang-format`:4 空格、行宽 100、Attach 大括号(同行开)、指针靠左。
4. 配套 `README.md` 简述本例练什么、怎么编译运行。

## 如何加 / 改文档

1. 放到 `documents/<NN-stage>/`,文件名 `{序号}-{kebab-case}.md`。
2. 必须有 frontmatter(`title` / `chapter` / `order` 必填),`tags` 取自 [`scripts/tags.py`](scripts/tags.py),`c_standard` ∈ {89,90,99,11,17,23}。完整规范见 [`.claude/writing-style.md`](.claude/writing-style.md)。
3. 遵循 **真实输出铁律**:贴真实编译 / 运行输出,不编造。C 的 UB 多,断言行为前先写最小 `.c` 用 `gcc -Wall -Wextra` 编译验证。

## 提交前自检

```bash
python3 scripts/build_examples.py          # 编译所有 examples(硬门,gcc+clang)
python3 scripts/validate_frontmatter.py    # 校验所有文档 frontmatter
python3 scripts/clang_tidy_check.py        # clang-tidy 跑 examples(硬门)
git ls-files 'examples/*.c' 'examples/*.h' | xargs clang-format --dry-run --Werror
pnpm build:single                          # 可选:VitePress 构建,catch Vue 级内容 bug(裸 <tag> / ${{ }} 误解析)
```

CI(`.github/workflows/ci.yml`)在 push / PR 时跑 **6 道门**:examples 双编译(gcc/clang 矩阵)、ASan+UBSan sanitize、clang-tidy 静态分析、gcov/lcov 覆盖率(报告门)、clang-format、frontmatter+markdownlint 文档校验。本地先过能省一次往返。

## 提交与 PR 约定

- commit 信息用中文 + Conventional 前缀:`fix(stage1): ...`、`feat:`、`docs:`、`chore:`、`style:`。
- **commit 信息不要带 `Co-Authored-By`**(保持署名干净)。
- 推送由维护者统一处理,贡献者正常提 PR 即可。
- PR 走 `main` 分支,描述清楚改了什么、为什么。
- 大改动建议先开 Issue 对齐方向再动手。

## 遗留项目(`projects/`)

`projects/` 下是早期学习代码整合而来。**`clib-utilities` 已完成整改**——`add_library` + CTest + Unity 测试框架,纳入 `build_examples.py` 硬门与覆盖率门,是阶段 4 工程化章节的活教材。其余(`os-from-scratch` / `embedded-mcu` / `song` / `tiny-c-stdlib`)为参考实现,部分需厂商 IDE 或仍在整改,动这些前请先开 Issue 讨论。

## 行为准则

参与本项目的每个人都应友善、尊重。详情见 [CODE_OF_CONDUCT.md](CODE_OF_CONDUCT.md)。

---
有问题先开 Issue 或 Discussion。
