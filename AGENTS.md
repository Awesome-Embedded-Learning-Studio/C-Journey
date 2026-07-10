# AGENTS.md

> 给所有 AI 编码助手(Agent)的统一入口。人类贡献者请看 [CONTRIBUTING.md](CONTRIBUTING.md)。

## 你在哪、这是什么

**C-Journey** —— 脱胎于维护者自己学 C 时攒下的笔记、逐条真跑核实后公开的教程仓库(**纯 C,不是 C++**),定位是用 C 游历计算机世界:主机系统(工具链/内存/数据结构/工程化/系统编程)深做,嵌入式浅尝指路。仓库 canonical:`Awesome-Embedded-Learning-Studio/C-Journey`(本地 `origin` 指这;`Charliechen114514/C-Journey` 已转移、是死链,别用)。8 个阶段:开发环境 → C 基础 → 指针内存 → 数据结构 → 工程化 → 系统编程 → 嵌入式(浅尝)→ 综合项目。

## 当前状态(接手必读)

主线内容层重写**已完成**:**六阶段(0-5)84 章全部写完并上线**站点(锈橙主题 VitePress,源在 `documents/`)。阶段 6(嵌入式)/ 7(capstone)/ 进阶 = 尚未动笔,目录里不留占位稿。**新章写作暂停**。当前两项:

1. **维护者人工大优化**——逐章调优,见 `.claude/review-queue.md` 顶部「📋 人工大优化清单」;
2. **阶段 6/7/进阶定方向**——嵌入式按**深度**切:C-Journey 将来也只浅尝,深做交 [imx-forge](https://awesome-embedded-learning-studio.github.io/imx-forge/)/ST-Forge(见 memory `cjourney-portfolio-boundary`)。

**接手前先读**:

- `.claude/review-queue.md`(本地,**进度 + 优化清单 + 约定 + 执行坑的活真相源**,顶部「当前续接点」);
- `.claude/writing-style.md`(**声音规矩**:坑就地插、不套「踩坑预警」框 / 不开「常见坑」列表 / 不「坑N」枚举;几乎不用列表;小结走散文);
- **声音标杆**(Ch01-08,已审核定稿):`documents/00-dev-environment/01`–`08-*.md`;
- memory `cjourney-rewrite-execution`(跨会话续接主指针)、`cjourney-site-build`(站点构建 + escape/v-pre 坑)。

**过程约束**:每条 C 断言 gcc16+clang22+sanitizer 真跑、贴真实输出 + ISO 条款;每章过 `validate_frontmatter` + `build_examples` + `clang-format` + `clang_tidy_check`,改站点再过 `pnpm build:single`;commit 落 `main` 分支(**无 `next` 分支;永不 push、不带 Co-Authored-By**)。若开写新章,加 `> 🟡 状态:待审核(日期)` banner、更 review-queue。

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
