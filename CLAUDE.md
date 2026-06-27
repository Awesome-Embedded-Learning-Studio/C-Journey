# CLAUDE.md

本项目的 AI 协作约定以 [AGENTS.md](AGENTS.md) 为**唯一真相源**(vendor-neutral)。Claude 本身无额外约定 —— 遵守 AGENTS.md 即可。

与全局记忆一致,这几条在本仓库尤其强调:

- **永不 `git push`**(推送由维护者统一处理)。
- **commit 信息不带 `Co-Authored-By`**。
- 改动前先读懂上下文;改动后用脚本自检:
  - `python3 scripts/build_examples.py`
  - `python3 scripts/validate_frontmatter.py`
- 写 C 文档 / 示例前先读 `.claude/writing-style.md`;断言 C 行为前先写最小 `.c` 编译验证(见 AGENTS.md「C 专属铁律」)。
