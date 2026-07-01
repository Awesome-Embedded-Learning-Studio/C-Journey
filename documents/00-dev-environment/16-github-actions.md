---
title: "GitHub Actions：把质量门自动化挂在每次提交上"
description: "第 8 章开 -Werror、第 10 章上 sanitizer、第 12 章配 CMake、第 13 章 GDB——这些门和工具，靠人记得每次手动跑是不现实的。这一章讲 CI：用 GitHub Actions 把质量门挂到每次 push/pull_request 上自动跑，失败就让提交变红、挡住合并。我们逐行拆解本项目真实的 .github/workflows/ci.yml——四个 job：build-examples（gcc/clang 矩阵编译 examples 当硬门）、sanitize（clang -fsanitize=address,undefined 编译，呼应第 10 章）、docs（frontmatter + markdownlint）、format-check（clang-format --dry-run --Werror），讲清 on 触发、job/step/runs-on、matrix 矩阵、action 复用、concurrency 取消旧轮这些概念，并在本地把这几个 job 真跑一遍确认它们就是 CI 在做的事。"
chapter: 0
order: 16
tags:
  - host
  - open-source
  - build
difficulty: intermediate
reading_time_minutes: 12
platform: host
c_standard: [11]
prerequisites:
  - "第 10 章：Sanitizer 门禁（CI 的 sanitize job 就是它）"
  - "第 12 章：CMake 入门（CI 调 build_examples.py 编 CMake 子项目）"
  - "第 15 章：Git 工作流（CI 挂在 push/PR 事件上）"
related:
  - "第 8 章：警告旗标（-Werror 在 CI 里当硬门）"
  - "第 17 章：格式化与质量门（format-check job 的 clang-format 详解）"
---

# GitHub Actions：把质量门自动化挂在每次提交上

## 引言：靠人记得跑质量门，是不现实的

到这一章，我们已经攒下好几道「门」：`-Wall -Wextra -Werror` 让警告挡住编译、`-fsanitize` 让 UB 和内存错误现形、`build_examples.py` 保证每个示例都能编、`validate_frontmatter.py` 保证文档 frontmatter 合法。问题是——**你真的能保证每次提交前都手动把它们全跑一遍吗？** 一定会忘。忘了就有漏网的 bug 混进主线。

**CI**（Continuous Integration，持续集成）就是解决这个的：它在每次 `git push` 或发 Pull Request 时，**自动**在一台干净的机器上把你的质量门全跑一遍，任何一个失败就让那次提交「变红」、在 GitHub 上明晃晃地标出来、阻止它被合并进主线。本课程用 **GitHub Actions** 做 CI。这一章我们逐行拆解本项目真实的 [.github/workflows/ci.yml](../../.github/workflows/ci.yml)，看 CI 到底跑了什么、怎么跑的，并在本地把这些步骤亲手跑一遍。

## workflow 文件长什么样

GitHub Actions 的配置放在 `.github/workflows/` 目录下，一个 `.yml` 文件就是一个 workflow。本项目只有一个 `ci.yml`，它的骨架是：

```yaml
name: CI

on:
  push:
    branches: [main, feat/integrate-old-c-code]
  pull_request:
    branches: [main]

concurrency:
  group: ci-${{ github.ref }}
  cancel-in-progress: true
```

最顶上 `name: CI` 是这个 workflow 的名字。`on:` 是**触发条件**——`push` 到 `main`（以及那个集成分支）时触发、对 `main` 发 `pull_request` 时也触发。所以你一推代码、或一开/更新 PR，CI 就自动开跑，不用你手动按。`concurrency` 那段是个优化：`cancel-in-progress: true` 意思是「同一个分支，如果你又推了新代码，就把上一轮还在跑的 CI 取消掉」——避免你连推三次、后端排着跑三轮同样的活，浪费资源。

一个 workflow 由若干 **job** 组成（下面有四个），每个 job 在一台虚拟机上跑（`runs-on: ubuntu-latest`，就是 GitHub 提供的 Ubuntu 云主机），每个 job 又由若干 **step** 组成。job 之间默认并行，step 之间串行。

## 四个 job：本项目 CI 实际跑什么

ci.yml 里定义了四个 job，正好把前面几章的门串成一条防线。我们逐个看。

**第一个 `build-examples`：编译所有示例，当硬门。** 这是最核心的一道：

```yaml
  build-examples:
    name: 编译 examples (${{ matrix.cc }})
    runs-on: ubuntu-latest
    strategy:
      fail-fast: false
      matrix:
        cc: [gcc, clang]
    steps:
      - uses: actions/checkout@v4
      - name: 安装构建工具
        run: sudo apt-get update && sudo apt-get install -y cmake ninja-build
      - name: 选择编译器
        run: echo "CC=${{ matrix.cc }}" >> $GITHUB_ENV
      - name: 编译所有 examples(硬门)
        run: python3 scripts/build_examples.py
```

这里出现了一个新概念——**矩阵**（`strategy.matrix`）。`cc: [gcc, clang]` 让这个 job **复制成两份**，一份用 gcc、一份用 clang，并行各跑一次。也就是说「同一个代码，分别用 gcc 和 clang 各编译一遍」（<code v-pre>${{ matrix.cc }}</code> 会被替换成 `gcc` 或 `clang`），确保你的代码不会被「只在 gcc 下能编、换 clang 就挂」坑到。`fail-fast: false` 让其中一个失败时不立刻取消另一个（两个都跑完，你能一次看全两边的问题）。

step 这边：`uses: actions/checkout@v4` 是调用 GitHub 官方提供的 **action**（可复用的步骤，`@v4` 是版本号）把仓库代码拉到虚拟机上；接着 `apt-get install` 装构建工具；`echo "CC=..." >> $GITHUB_ENV` 把选定的编译器写进环境变量；最后 `python3 scripts/build_examples.py` 跑第 12 章那个脚本——**它退出码非 0（有示例编不过），整个 job 就失败、CI 就红**。这就是「硬门」的意思：不是警告，是直接挡住。

**第二个 `sanitize`：sanitizer 门（呼应第 10 章）。** 它换 clang，带 `-fsanitize` 再把示例全编一遍：

```yaml
  sanitize:
    name: Sanitizer(ASan + UBSan)
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - name: 安装构建工具
        run: ... && sudo apt-get install -y cmake ninja-build clang
      - name: 用 -fsanitize 编译 examples
        env:
          CC: clang
          CFLAGS: -fsanitize=address,undefined -fno-omit-frame-pointer -g
          LDFLAGS: -fsanitize=address,undefined
        run: python3 scripts/build_examples.py
```

`env:` 那几行给这一步设环境变量：`CC=clang`、`CFLAGS` 带上第 10 章的 `-fsanitize=address,undefined`（外加 `-fno-omit-frame-pointer` 保留帧指针、让 sanitizer 报错时的栈跟踪更准），再配 `LDFLAGS` 让链接也带上 sanitizer 运行时。这一步跑 `build_examples.py` 时，CMake 会读到这些环境变量、用 clang 带 sanitizer 去编译。**含越界、UAF、UB 的代码，到这一步会编译失败或被 sanitizer 拦下**——CI 就靠它把第 10 章那套运行期检查自动化了。（ci.yml 里那条注释也诚实说明了：目前 sanitizer 门覆盖的是 CMake 子项目，根 CMakeLists 统一后会把全部示例纳入。）

**第三、四个 `docs` 和 `format-check`：文档和格式门。** `docs` job 装好 Python，跑 `validate_frontmatter.py`（第 1 章起一直在用的 frontmatter 校验）和 `markdownlint`（检查 Markdown 写法）；`format-check` job 装 clang-format，对 examples 下的所有 `.c`/`.h` 跑 `clang-format --dry-run --Werror`——`--dry-run` 只检查不改动，`--Werror` 把「格式不合规」当错误，任何文件格式不对，退出码非 0、CI 红（这正好是下一章第 17 章的主题）。这俩 job 保证提交进来的代码和文档是干净、合规的。

## 本地把这些 job 跑一遍

GitHub Actions 跑在 GitHub 的云端机器上，我们本地没法完整触发它——但 CI 跑的命令就是上面 yml 里写的那些，完全可以手动复现，确认它们「就是 CI 在做的事」。我们模拟 docs、format-check、sanitize 三个 job 的核心步骤（前面章节其实已经跑过，这里串起来看）：

```text
$ python3 scripts/validate_frontmatter.py          # = docs job
✅ frontmatter 全部通过。

$ git ls-files 'examples/*.c' 'examples/*.h' | xargs clang-format --dry-run --Werror   # = format-check job
$ echo $?
0                                  ← 退出码 0,examples 格式全部合规

$ CC=clang CFLAGS="-fsanitize=address,undefined -g" python3 scripts/build_examples.py  # = sanitize job
✅ examples 全部构建通过。
```

三个步骤退出码都是 0——这就是你在 GitHub 上看到 CI 绿勾时，背后实际发生的事。反过来，如果哪次提交让某个示例编不过、或格式不对、或触发 sanitizer，对应的 job 就非 0 退出、CI 变红。

## 失败挡合并：把前面所有门串起来

理解了这四个 job，你就明白 CI 的意义了：它把第 8 章的 `-Werror`（藏在每个示例的编译旗标里）、第 10 章的 sanitizer、第 12 章的「每个示例都能编」、格式与文档合规，**全部自动化、每次提交都强制跑一遍**。在 GitHub 的分支保护里，你可以把「CI 必须绿」设成合并 PR 的前提条件——于是任何一道门没过，代码就合不进 `main`。人还是会忘、会偷懒，但 CI 不会。这一章是阶段 0「开发环境」的收尾：从工具链、编译流程、警告、标准与优化、sanitizer、make/CMake、GDB、Git，到这章的 CI，一条「让代码可靠地构建和验证」的链子就完整了。

## 小结

CI 用 GitHub Actions 把质量门挂到每次 `push`/`pull_request` 上自动跑，失败就红、挡住合并，省得靠人记得手动跑。配置在 `.github/workflows/*.yml`，骨架是 `name`/`on`(触发条件)/`jobs`(并行)/`steps`(串行)/`runs-on`(虚拟机)，还能用 `matrix`(如 gcc+clang 两份并行)、`uses: actions/...@vN`(复用现成 action)、`concurrency`(同分支新推送取消旧轮)。本项目 ci.yml 有四个 job：`build-examples`（gcc/clang 矩阵跑 `build_examples.py`，退出码非 0 即红，是硬门）、`sanitize`（clang 带 `-fsanitize=address,undefined -fno-omit-frame-pointer` 再编一遍，呼应第 10 章）、`docs`（`validate_frontmatter.py` + markdownlint）、`format-check`（`clang-format --dry-run --Werror`，呼应第 17 章）。我们在本地把这四个 job 的核心命令都复现了一遍、确认退出码 0 就是 GitHub 上那个绿勾。CI 的本质是把前面所有章节立起来的门自动化强制执行——任何一道没过就合不进主线，人可以忘、CI 不会。下一章我们把最后一道 format-check 展开讲透，看 clang-format 怎么统一代码风格、以及怎么在 CI 里和本地用同一份 `.clang-format` 守住它。

## 参考资源

- GitHub Actions 文档：workflow 语法（`on`/`jobs`/`steps`/`runs-on`）、`strategy.matrix` 矩阵、`actions/checkout`、`actions/setup-python`、`concurrency`
- 本项目 [.github/workflows/ci.yml](../../.github/workflows/ci.yml)（四个 job 的真实配置）
- 第 10 章：Sanitizer 门禁（CI 的 sanitize job）、第 12 章：CMake 入门（`build_examples.py` 编的 CMake 子项目）、第 8 章：警告旗标（`-Werror` 在编译旗标里）
- 第 17 章：格式化与质量门（format-check job 用的 clang-format 详解）
