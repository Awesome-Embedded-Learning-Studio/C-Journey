# 贡献指南

感谢参与 C-Journey!这是一份面向 C 学习者的工程化进阶教程。无论修一个错别字、补一个示例,还是写一整章,都欢迎。

## 仓库地图

| 路径 | 作用 |
|---|---|
| `documents/` | 各阶段知识点文档(按 `NN-stage/` 分阶段) |
| `examples/` | 配套可编译示例(按阶段) |
| `projects/` | 完整项目(clib-utilities、os-from-scratch 等,多为遗留代码,整改中) |
| `ROADMAP.md` | 8 阶段总路线 |
| `.claude/writing-style.md` | **写作风格契约(必读)** |
| `scripts/` | 质量门脚本(构建 / frontmatter 校验 / 标签源) |

## 开发环境

需要:`gcc`/`clang`、`cmake`、`ninja`、`python3` + `pyyaml`、`clang-format`。

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
python3 scripts/build_examples.py          # 编译所有 examples(硬门)
python3 scripts/validate_frontmatter.py    # 校验所有文档 frontmatter
git ls-files 'examples/*.c' 'examples/*.h' | xargs clang-format --dry-run --Werror
```

CI(`.github/workflows/ci.yml`)会在 push / PR 时跑同样的检查,本地先过能省一次往返。

## 提交与 PR 约定

- commit 信息用中文 + Conventional 前缀:`fix(stage1): ...`、`feat:`、`docs:`、`chore:`、`style:`。
- **commit 信息不要带 `Co-Authored-By`**(保持署名干净)。
- 推送由维护者统一处理,贡献者正常提 PR 即可。
- PR 走 `main` 分支,描述清楚改了什么、为什么。
- 大改动建议先开 Issue 对齐方向再动手。

## 遗留项目(`projects/`)

`projects/` 下是早期学习代码整合而来,部分需厂商 IDE(Keil / VS)、部分在 GCC14+ 下有待整改(如 clib 的若干类型问题)。动这些项目前请先开 Issue 讨论,避免破坏既有结构。

## 行为准则

参与本项目的每个人都应友善、尊重。详情见 [CODE_OF_CONDUCT.md](CODE_OF_CONDUCT.md)。

---
有问题先开 Issue 或 Discussion。
