## 改动说明

<!-- 这个 PR 做了什么、为什么。大改动请先开 Issue 对齐方向。 -->

**相关 Issue**:#

## 类型

- [ ] 修 bug
- [ ] 新增示例 / 文档
- [ ] 改进现有内容
- [ ] 工程化 / CI / 工具
- [ ] 站点 / 主题
- [ ] 其他

## 自检

- [ ] `python3 scripts/build_examples.py` 通过(若改了 `examples/` 或 `projects/clib-utilities`)
- [ ] `python3 scripts/validate_frontmatter.py` 通过(若改了 `documents/`)
- [ ] `python3 scripts/clang_tidy_check.py` 通过(若改了 `examples/` 的 `.c`)
- [ ] examples 代码已 `clang-format` 过
- [ ] `pnpm build:single` 通过(若改了站点主题 / config / 章节正文)
- [ ] **文档贴了真实输出**(gcc/clang/ASan 实跑,未编造)
- [ ] 断言 C 行为处**引了 ISO/IEC 9899 条款**
- [ ] 代码**无 C++ 风格**(`class` / `std::` / `enum class` / 模板)
- [ ] 新文档 frontmatter 的 `tags` 取自 `scripts/tags.py`
- [ ] commit 信息未带 `Co-Authored-By`
