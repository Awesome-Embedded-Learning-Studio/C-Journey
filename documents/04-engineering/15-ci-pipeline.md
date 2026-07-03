---
title: "把质量门拼成流水线:扩展本仓 ci.yml"
description: "前面几章一道一道立的门——build_examples 担保「每个示例能编」、sanitize 抓运行时 UB/内存、clang-tidy 抓编译时语义、gcov/lcov 量化测试盖到哪、frontmatter+markdownlint 守文档、clang-format 守代码风格——这一章把它们拼成一条 CI 流水线,看六道门是怎么在每次 push/PR 上并行各跑一份的。不重复阶段 0 第 16 章那次对「4-job 版 ci.yml」的逐行拆解,本章只讲扩展后的全景:逐 job 概览 build-examples(gcc/clang 矩阵)、sanitize(ASan+UBSan)、docs(frontmatter+markdownlint)、format-check(clang-format --dry-run --Werror)、static-analysis(clang-tidy)、coverage(gcov/lcov)每道门干啥,核心是分清「硬门 vs 报告模式」——build_examples/sanitize/format/clang-tidy 是硬门(失败断 CI),docs 的 frontmatter 也是硬门、markdownlint 同理,而 build_examples 脚本内部还有 KNOWN_LEGACY 双模式(projects 里的嵌入式 MCU/汇编老工程走 report 模式、失败只打日志不断 CI),coverage 这道门只生报告、不做阈值断言(是 advisory)。讲 concurrency:cancel-in-progress 让同分支新推送取消上一轮在跑的旧 CI,省排队。讲怎么加新门:照葫芦画瓢加一个 job——装工具、跑脚本、靠脚本退出码非 0 断 CI。真跑:本地用 /tmp/cj/p4ch15/gate_echo.sh 串起三道本地能跑的硬门(build_examples/clang_tidy_check/validate_frontmatter),贴退出码 0 的真实汇总。承接第 12 章(clang-tidy 门)+第 13 章(coverage 门)——本章把这两道和前几道一起拼进 ci.yml 全景。"
chapter: 4
order: 15
tags:
  - host
  - engineering
  - open-source
  - build
  - testing
difficulty: intermediate
reading_time_minutes: 13
platform: host
c_standard: [11]
prerequisites:
  - "阶段 0·第 16 章:GitHub Actions(那里逐行拆了原来的 4-job ci.yml,本章是它扩展到 6 道门后的全景,不重复逐行)"
  - "第 12 章:静态分析门(static-analysis 这道 CI 门就是它立的,本章把它拼进流水线)"
  - "第 13 章:覆盖率门(coverage 这道 CI 门就是它立的,本章把它拼进流水线)"
  - "阶段 0·第 10 章:Sanitizer 门禁(sanitize job 就是它,本章看它在流水线里的位置)"
related:
  - "阶段 0·第 17 章:格式化与质量门(format-check job 的 clang-format 详解)"
  - "第 10 章:ASan+UBSan 深入(sanitize job 的运行时检查)"
---

# 把质量门拼成流水线:扩展本仓 ci.yml

## 引言:门一道道立完了,谁来一次把它们都跑一遍

到这里,工程化阶段攒下的门已经够摆一条流水线了。`build_examples.py` 担保「每个示例都编得过」(gcc 和 clang 各编一遍)、`sanitize` 拿 ASan+UBSan 抓运行时的内存错和 UB、`clang-tidy` 抓编译时的语义毛病(reserved identifier、narrowing、缺括号)、`gcov/lcov` 把测试盖到了哪里量化成数字、`validate_frontmatter.py`+`markdownlint` 守文档的元信息和写法、`clang-format` 守代码风格。每一道在前面章节都单独立过、单独真跑过。

问题是——**谁来保证每次提交都把它们一道不漏地全跑一遍?** 阶段 0 第 16 章已经回答过这个问题:GitHub Actions,把质量门挂在 `push`/`pull_request` 上自动跑。那一章我们逐行拆了当时的 `ci.yml`——四道 job(`build-examples`/`sanitize`/`docs`/`format-check`)。可打那以后,本仓又立了两道新门:第 12 章的 `static-analysis`(clang-tidy)和第 13 章的 `coverage`(gcov/lcov)。于是 `ci.yml` 从四道扩到了**六道**。

本章干的就是把扩展后的全景讲清楚,**不重复阶段 0 第 16 章那次逐行拆解**——yml 的 `on`/`jobs`/`steps`/`runs-on`/`uses` 这些语法那里讲透了,这里只看「扩展后六道门怎么排、谁是硬门谁是报告、怎么再加一道」。读的是仓库里真实的 [.github/workflows/ci.yml](../../.github/workflows/ci.yml),本地用 `/tmp/cj/p4ch15/gate_echo.sh` 把三道本地能跑的硬门串起来真跑一遍,贴退出码 0 的汇总——这就是 GitHub 上那颗绿勾背后实际发生的事。

## 扩展后的全景:六道门怎么排

先把整张 `ci.yml` 的 job 一览摆出来,六道门之间默认是**并行**的(GitHub Actions 里同一 workflow 的多个 job 各起一台虚拟机、同时开跑),各自跑完自己的 step、各自的退出码决定自己那格是绿是红。读一遍当前的 ci.yml 全貌:

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

jobs:
  build-examples:   ...  # 矩阵 gcc/clang,跑 build_examples.py
  sanitize:         ...  # clang + -fsanitize=address,undefined 再编一遍
  docs:             ...  # validate_frontmatter.py + markdownlint
  format-check:     ...  # clang-format --dry-run --Werror
  static-analysis:  ...  # scripts/clang_tidy_check.py
  coverage:         ...  # cmake --coverage + ctest + lcov
```

`on` 那段定义触发条件:`push` 到 `main`(以及那个集成分支 `feat/integrate-old-c-code`)、对 `main` 发 `pull_request`——一推代码或一更新 PR,六道门就并行开跑。语法细节(why `runs-on: ubuntu-latest`、`uses: actions/checkout@v4` 是干嘛、matrix 怎么展开)阶段 0 第 16 章逐行拆过,这里不重复。

值得在全景这一层先说清的是 `concurrency` 那三行。它的 `group` 用 `ci-${{ github.ref }}` 把「同一个分支」的 CI 跑归成一组,`cancel-in-progress: true` 意思是——**同一个分支,如果你又推了新代码,就把上一轮还在跑的 CI 取消掉**。想象一下你对着一个 PR 连推三次 commit:第一次推完 CI 开跑、要跑几分钟;还没跑完你又推了第二次,这时候第一次那轮已经白跑了(代码已经不是最新),`cancel-in-progress` 就把它掐了、把算力让给第二次;第三次同理掐第二次。这样既省 GitHub Actions 的额度、也让 PR 页面上的 CI 状态始终对应最新一次推送。代价是——如果你那轮 CI 里跑到一半就被掐、有些 job 没跑完,你就看不到它们的完整结果,不过反正它们要被新一轮覆盖、这个代价可以接受。

## 逐 job 概览:每道门干啥、靠什么退出码裁决

六道门,逐个看它的「核心步骤」和「靠什么断 CI」。这一层的关键是分清**硬门**和**报告(advisory)**——硬门的脚本退出码非 0,job 整个失败、PR 上那格变红;报告门只生数据、不靠退出码断 CI。先说结论再展开:**`build-examples`、`sanitize`、`format-check`、`static-analysis` 是硬门,`docs` 的 frontmatter 部分是硬门(markdownlint 那步也一样靠 action 的退出码),`coverage` 是报告**。

### `build-examples`:gcc/clang 矩阵,担保「每个示例都编得过」

这是最核心的一道,用 `strategy.matrix` 把自己复制成 gcc、clang 两份并行各跑一次:

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

`matrix.cc: [gcc, clang]` 让这个 job 跑两份,`${{ matrix.cc }}` 分别替换成 `gcc` 和 `clang`(`echo "CC=..." >> $GITHUB_ENV` 把它写进环境变量,CMake 在 configure 时读到)。`fail-fast: false` 让 gcc 那份挂了不会立刻把 clang 那份也取消——两边都跑完,你能一次看全问题。最后一步 `python3 scripts/build_examples.py` 是裁决点:**它退出码非 0(有示例编不过),整个 job 失败、CI 红**。

这里有个值得单独拎出来的细节——`build_examples.py` 自己内部还有一套**双模式**(下一节展开):`examples/` 下的是硬门(任一失败 `sys.exit(1)`),而 `projects/` 下被标进 `KNOWN_LEGACY` 那批老工程是报告模式(失败只打日志、不影响退出码)。所以「`build-examples` 这道 job 是硬门」说的是它对 `examples/` 和「非遗留的 projects」是硬门,对遗留工程网开一面。

### `sanitize`:clang 带 `-fsanitize` 再编一遍,呼应第 10 章

这道门换 clang、带上第 10 章那套 `-fsanitize=address,undefined` 把示例再编一遍:

```yaml
  sanitize:
    name: Sanitizer(ASan + UBSan)
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - name: 安装构建工具
        run: sudo apt-get update && sudo apt-get install -y cmake ninja-build clang
      - name: 用 -fsanitize 编译 examples
        env:
          CC: clang
          CFLAGS: -fsanitize=address,undefined -fno-omit-frame-pointer -g
          LDFLAGS: -fsanitize=address,undefined
        run: python3 scripts/build_examples.py
```

`env:` 那几行给这一步注入 sanitizer 的 flags(`-fno-omit-frame-pointer` 保留帧指针、让 ASan 报错时的栈跟踪更准),CMake 读到 `CC`/`CFLAGS`/`LDFLAGS`、用 clang 带 sanitizer 去编译。裁决点和 `build-examples` 同一个脚本——`build_examples.py` 退出码非 0 即红。ci.yml 里那条注释诚实说明了它目前只覆盖 CMake 子项目(根 CMakeLists 统一后会把全部示例纳入)。

### `docs`:frontmatter + markdownlint,守文档

这道门是「文档也要过 CI」的落地,两个 step:

```yaml
  docs:
    name: 文档(frontmatter + markdownlint)
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - uses: actions/setup-python@v5
        with:
          python-version: '3.x'
      - name: 安装 PyYAML
        run: pip install pyyaml
      - name: 校验 frontmatter
        run: python3 scripts/validate_frontmatter.py
      - name: markdownlint
        uses: DavidAnson/markdownlint-cli2-action@v18
        with:
          globs: 'documents/**/*.md'
```

第一个裁决点是 `validate_frontmatter.py`——它扫 `documents/` 下所有 `.md`、解析每个文件顶上的 YAML 头、校验 `chapter`/`order`/`difficulty`/`platform`/`c_standard`/`tags` 这些字段合法,缺 `title` 或字段非法都 `sys.exit(1)`、断 CI。这一半是**硬门**。第二个裁决点是 `markdownlint-cli2-action`——这是个第三方 action(`DavidAnson/...@v18` 是版本号),对 `documents/**/*.md` 跑 markdownlint 检查 Markdown 写法(标题层级、列表缩进、行长),它自己的退出码非 0 同样让 job 失败,也是硬门。所以 `docs` 整道门两个 step 都是硬门。

### `format-check`:clang-format `--dry-run --Werror`,呼应第 17 章

守代码风格那道,很短:

```yaml
  format-check:
    name: clang-format(examples)
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - name: 安装 clang-format
        run: sudo apt-get update && sudo apt-get install -y clang-format
      - name: 检查 examples 格式
        run: |
          git ls-files 'examples/*.c' 'examples/*.h' | xargs clang-format --dry-run --Werror
```

最后一步是裁决点:`git ls-files` 列出 examples 下所有纳入 git 的 `.c`/`.h`,`xargs` 把它们喂给 `clang-format --dry-run --Werror`。`--dry-run` 只检查不改文件、`--Werror` 把「格式不合规」当错误——任一文件格式不对、退出码非 0、CI 红。注意它用 `git ls-files` 而不是 `find`,好处是只检查纳入版本控制的文件、不碰构建产物或临时垃圾。这一道和本仓根目录的 `.clang-format` 是一对:CI 这边只检查、本地该用 `clang-format -i` 改(阶段 0 第 17 章讲过)。硬门。

### `static-analysis`:clang-tidy,第 12 章立的语义门

这道是第 12 章新增的,跑仓库自己的 `clang_tidy_check.py`:

```yaml
  static-analysis:
    name: 静态分析(clang-tidy,examples)
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - name: 安装 clang-tidy + cmake
        run: sudo apt-get update && sudo apt-get install -y clang-tidy cmake ninja-build
      - name: 跑 clang-tidy(examples 硬门,阶段4·Ch12 引入)
        run: python3 scripts/clang_tidy_check.py
```

裁决点是 `clang_tidy_check.py`——它为每个 examples 子项目配 `compile_commands.json`、对每个 `.c` 跑 `clang-tidy -p`,**只要 stdout 里有 `warning:`、脚本就退出 1**(第 12 章逐行拆过这个机制)。job 名字和 step 名字里那两条注释「examples 硬门」「阶段4·Ch12 引入」是刻意的,告诉所有读 CI 的人这道门的来历和范围。硬门。注意一个潜在的「本地过、CI 红」来源:Ubuntu LTS 上的 `apt` 装的 clang-tidy 版本未必有本机的 22.1.6 新,check 集会随版本变——本地干净的 finding、换 CI 上更老或更新的 clang-tidy 可能冒出来。

### `coverage`:gcov/lcov,第 13 章立的量化门——这一道是报告

这是六道里唯一的**报告(advisory)门**,看一眼它的最后一步就明白为什么:

```yaml
  coverage:
    name: 覆盖率(gcov/lcov)
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - name: 安装工具
        run: sudo apt-get update && sudo apt-get install -y cmake ninja-build lcov
      - name: 编译+测试 clib(带 --coverage)
        run: |
          cmake -B build-cov -S projects/clib-utilities \
                -DCMAKE_C_FLAGS="--coverage -g" \
                -DCMAKE_EXE_LINKER_FLAGS="--coverage"
          cmake --build build-cov
          cd build-cov && ctest --output-on-failure && cd ..
      - name: 生成覆盖率报告
        run: |
          lcov --capture --directory build-cov --output-file coverage.info \
                --ignore-errors mismatch --rc geninfo_auto_base=1
          lcov --remove coverage.info '/usr/*' --output-file coverage.filtered.info
          lcov --summary coverage.filtered.info
```

拆开看它的两段语义。第一个 step(`编译+测试 clib`)里,`cmake` 带 `--coverage` 编 `projects/clib-utilities`、`cmake --build` 构建、`ctest --output-on-failure` 跑测试——这一步**是硬门**:测试挂了(`ctest` 退出码非 0),`&&` 链断、step 失败、CI 红。这合理,因为「测试得过」是正确性问题,不该妥协。

可第二个 step(`生成覆盖率报告`)不一样——它跑三条 `lcov` 命令,最后一条 `lcov --summary coverage.filtered.info` 只是**打印**覆盖率摘要(行覆盖百分之几、分支覆盖百分之几),**不做任何阈值断言**(没有「低于 80% 就失败」这种判断)。所以这一步的退出码只反映「lcov 跑没跑通」,不反映「覆盖率够不够高」——哪怕覆盖率只有 5%,只要 lcov 自己没报错、step 仍然退出 0、job 仍然绿。这就是「报告」的意思:**它把覆盖率数字暴露到 CI 日志和 GitHub 的 artifact 里供人看,但不拿这个数字挡合并**。第 13 章讲过为什么本仓暂不上覆盖率阈值——`clib-utilities` 的 `CCDynamicArray` 当前行覆盖才 42%(Unity 5 条用例只测了主路径),一上阈值 CI 立刻红、所有 PR 都过不去,得先把测试补上来。所以这道门的姿势是「先量化、让数字可见,等数字涨上来了再考虑收紧成阈值」。

把六道门的属性归一下:`build-examples`(硬门,矩阵双跑)、`sanitize`(硬门)、`docs`(硬门,frontmatter + markdownlint 两步都硬)、`format-check`(硬门)、`static-analysis`(硬门)、`coverage`(编译+测试那步硬、覆盖率数字本身是报告)。**前五道是「不达标就挡合并」的硬门,coverage 是「量化但不挡」的报告**——这种分层是真实工程 CI 的常态,不是所有检查都该一上来就硬挡。

## KNOWN_LEGACY 双模式:`build_examples.py` 内部的「报告 / 硬门」开关

刚才说 `build-examples` 这道 job 是硬门,其实它内部还藏着第二层「硬门 vs 报告」的划分——在 `scripts/build_examples.py` 里。这个脚本同时扫 `examples/` 和 `projects/` 两个目录,但**对两类目录的失败处理不一样**:

```python
KNOWN_LEGACY = {
    "projects/embedded-mcu",     # Keil 工程,宿主机无法构建
    "projects/os-from-scratch",  # bochs/汇编,需专属工具链与镜像
    "projects/tiny-c-stdlib",    # Visual Studio 工程
    "projects/song",             # 依赖外部 mplayer
}
```

这个集合列的是「`projects/` 下、目前根本编不过 CI 的老工程」——有的是 Keil 工程(只能在厂商 IDE 里编、宿主机没那套工具链)、有的是 bochs+汇编(要专属镜像)、有的是 Visual Studio 工程(走 MSBuild、不在 Linux CI 范围)、还有的依赖外部 `mplayer`。这些工程被标成 `KNOWN_LEGACY` 之后,`build_one(cl, fatal=...)` 里那个 `fatal` 参数就分叉了——`examples/` 全部传 `fatal=True`(失败就进 `fatal_fail` 列表、最后 `sys.exit(1)`),`projects/` 里**不在** `KNOWN_LEGACY` 的(比如已经整改完的 `clib-utilities`)也传 `fatal=True`,**在** `KNOWN_LEGACY` 里的传 `fatal=False`(失败进 `report_fail` 列表、只打印「已知问题、整改中」、不影响退出码)。

效果是这样的:`build_examples.py` 跑完,如果 `clib-utilities` 编挂了、`fatal_fail` 非空、脚本 `sys.exit(1)`、CI 红;但如果 `embedded-mcu` 编挂了(几乎必然挂,因为它压根不是 Linux 工程)、它进的是 `report_fail`、脚本照样 `sys.exit(0)`、CI 绿。这就是「同一道 job 里对不同子项目用不同裁决力度」——**新代码、整改完的代码走硬门;明知编不过的老工程走报告、留出口子、按优先级慢慢还债**。这套机制和上一章「clib 还在 clang-tidy 硬门外」是同一个思路:CI 不是一刀切,该硬的硬、该留余地的留余地,否则一开 CI 全红、所有 PR 卡死、谁也动不了。

怎么判断一个工程该不该从 `KNOWN_LEGACY` 里挪出去?标准就一条——**它能在 Linux CI 上用 gcc/clang 编过了、而且测试也过了**(像 `clib-utilities` 那样完成 GCC16/POSIX 整改),就从集合里删掉、转成硬门。脚本顶上那条注释「(clib-utilities 已完成 GCC16/POSIX 整改,转为硬门)」记录的就是这一笔——它曾经是 legacy、整改完挪出来了。

## 怎么加一道新门:照葫芦画瓢

理解了上面这套结构,「怎么加一道新门」就成了机械活。假设你想加一道——比如 `cppcheck`(第 12 章讲过、本仓还没上),步骤就三步。

第一步,先有裁决脚本。本仓的姿势是「写一个 `scripts/xxx_check.py`,内部决定什么叫失败、`sys.exit(1)` 或 `sys.exit(0)`」——`build_examples.py`、`clang_tidy_check.py`、`validate_frontmatter.py` 全是这个套路。为什么不用工具自己的退出码直接裁决?因为工具退出码的语义未必合你要的力度(比如 `clang-tidy` 自己的退出码在 `WarningsAsErrors: ''` 时永远是 0,得靠脚本解析 stdout 里的 `warning:` 才能裁决;`cppcheck` 得靠 `--error-exitcode=1` 显式设)。包一层 Python 脚本,把「什么叫失败」写在脚本里,比裸调工具可控。

第二步,在 `ci.yml` 里照着上面任意一道硬门抄一个 job。骨架是:`uses: actions/checkout@v4` 拉代码 → `apt-get install` 装工具 → `run: python3 scripts/xxx_check.py` 跑脚本。就这三步,step 之间串行、脚本退出码非 0 就让 job 失败。

第三步,想清楚它是硬门还是报告。如果是硬门,直接靠脚本退出码断 CI(像 `static-analysis`);如果是报告(像 `coverage` 那种只生数字、不挡合并),那就把最后一步写成「打印摘要」、不做阈值断言,job 失败条件只挂在「工具自己崩了」上,不挂在「指标不达标」上。

```python
#!/usr/bin/env python3
"""xxx_check.py —— 假想的新门示例(本章演示「照葫芦画瓢」用,非仓库真脚本)。
裁决逻辑写在这、退出码 0/1 决定 CI 红绿。"""
import subprocess
import sys


def main() -> int:
    # 假装调一个工具,解析它的输出,有 finding 就失败
    r = subprocess.run(
        ["cppcheck", "--enable=all", "--suppress=missingIncludeSystem",
         "--error-exitcode=1", "examples/"],
        text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
    )
    print(r.stdout)
    return r.returncode   # cppcheck 已经用 --error-exitcode=1 设了退出码,直接透传


if __name__ == "__main__":
    sys.exit(main())
```

读法:`subprocess.run` 调工具、`print(r.stdout)` 把输出透到 CI 日志(让 PR 上点开能看)、`return r.returncode` 把工具退出码当裁决结果、`sys.exit(main())` 把它变成脚本退出码。CI 那边 `run: python3 scripts/xxx_check.py` 就靠这个退出码断红绿。这是「照葫芦画瓢」的葫芦——本仓所有 `*_check.py` 都长这个样。

## 本地复现三道硬门:贴退出码 0 的汇总

GitHub Actions 跑在云端,本地没法完整触发它,但**CI 跑的命令就是 yml 里 `run:` 后面那些**——完全可以手动复现、确认退出码。我用一个脚本(`/tmp/cj/p4ch15/gate_echo.sh`,非仓库的一部分、只为本章演示)把三道本地能跑的硬门串起来,顺序对应 ci.yml 里 `build-examples`、`static-analysis`、`docs`(frontmatter 那半)三个 job 的核心命令:

```bash
#!/usr/bin/env bash
# gate_echo.sh —— 本章「本地复现三道门」的真跑脚本(非 ci.yml 的一部分,仅用于演示)
# 顺序对应 ci.yml 里 build-examples / static-analysis / docs 三道硬门的核心命令。
set -u
fail=0

echo '==> gate 1: build_examples.py(= ci.yml 的 build-examples job)'
python3 scripts/build_examples.py
rc=$?
echo "    build_examples exit=$rc"
[ $rc -eq 0 ] || fail=1

echo '==> gate 2: clang_tidy_check.py(= ci.yml 的 static-analysis job)'
python3 scripts/clang_tidy_check.py
rc=$?
echo "    clang_tidy_check exit=$rc"
[ $rc -eq 0 ] || fail=1

echo '==> gate 3: validate_frontmatter.py(= ci.yml 的 docs job 的一半)'
python3 scripts/validate_frontmatter.py
rc=$?
echo "    validate_frontmatter exit=$rc"
[ $rc -eq 0 ] || fail=1

echo "==== 汇总 fail=$fail ===="
exit $fail
```

在仓库根真跑一遍(gcc 16.1.1 / clang 22.1.6 本机双在线,这道脚本走的是 gcc 默认路径):

```text
$ bash /tmp/cj/p4ch15/gate_echo.sh
==> gate 1: build_examples.py(= ci.yml 的 build-examples job)
== examples/  (硬门) ==
[FATAL ] OK               examples/stage4-cmake-lib
[FATAL ] OK               examples/stage5-tcp-socket/SC1
[FATAL ] OK               examples/stage5-tcp-socket/SC2
[FATAL ] OK               examples/stage5-tcp-socket/SC3
[FATAL ] OK               examples/stage5-tcp-socket/SC4

== projects/  (非遗留=硬门 / 遗留=报告模式) ==
[FATAL ] OK               projects/clib-utilities

== 汇总 ==
examples 失败: 0    projects 报告失败: 0

✅ examples 全部构建通过。
    build_examples exit=0
==> gate 2: clang_tidy_check.py(= ci.yml 的 static-analysis job)
[OK]   examples/stage4-cmake-lib/src/mathlib.c

✅ clang-tidy 全部通过。
    clang_tidy_check exit=0
==> gate 3: validate_frontmatter.py(= ci.yml 的 docs job 的一半)
已校验 94 个文档。
✅ frontmatter 全部通过。
    validate_frontmatter exit=0
==== 汇总 fail=0 ====
$ echo $?
0
```

三道门退出码全是 0,汇总脚本整体也退出 0——**这就是 GitHub 上 PR 那几格变绿时,背后实际发生的事**。反过来,哪天哪个示例编不过、或 clang-tidy 冒出 finding、或哪篇文档 frontmatter 写错了,对应的脚本退出码变非 0、job 失败、PR 上那一格变红、合并按钮被禁掉(前提是分支保护里把「CI 必须绿」设成了合并前提)。这几行真跑输出还顺带暴露了两个信息:其一,`build_examples` 现在覆盖的不止第 12 章时那个文件——`stage4-cmake-lib`、`stage5-tcp-socket/SC1-4`、整改完的 `clib-utilities` 全编过了;其二,`clang_tidy_check` 目前还是只管 `examples/` 一级子目录(只列了 `mathlib.c`),`projects/` 在硬门外——这和第 12 章「clib 还在整改」的结论一致。

## 小结

CI 把前面几章一道道立的门拼成一条流水线,挂在每次 `push`/`pull_request` 上并行各跑一份,失败的那格在 PR 上变红、挡住合并。本仓 `.github/workflows/ci.yml` 现在六道 job:`build-examples`(gcc/clang 矩阵跑 `build_examples.py`,硬门)、`sanitize`(clang 带 `-fsanitize=address,undefined` 再编一遍,硬门)、`docs`(`validate_frontmatter.py` + markdownlint 两步都硬)、`format-check`(`clang-format --dry-run --Werror`,硬门)、`static-analysis`(`clang_tidy_check.py`,第 12 章立的硬门)、`coverage`(gcov/lcov,编译+测试那步硬、覆盖率数字本身只打印不挡合并,是报告)。这套分层是关键——不是所有检查都该一上来就硬挡,`coverage` 先量化、让数字可见、等数字涨上来再考虑收紧成阈值,`build_examples` 内部的 `KNOWN_LEGACY` 给明知己方编不过的老工程留报告出口子、按优先级慢慢还债。`concurrency.cancel-in-progress` 让同分支新推送取消上一轮在跑的旧 CI,省排队。加新门就三步:写个 `scripts/xxx_check.py` 决定什么叫失败、照着抄一个 job(装工具 + 跑脚本)、想清楚是硬门还是报告。本章用 `/tmp/cj/p4ch15/gate_echo.sh` 把三道本地能跑的硬门串起来真跑了一遍,三道退出码都是 0——这就是 GitHub 上那颗绿勾背后实际发生的事。这一章是工程化阶段「把质量自动化」这条线的收口:门立完了、也拼成流水线了,人可以忘、CI 不会。

## 参考资源

- **本仓活教材**:[`.github/workflows/ci.yml`](../../.github/workflows/ci.yml)(六道 job 的真实配置)、[`scripts/build_examples.py`](../../scripts/build_examples.py)(`KNOWN_LEGACY` 双模式在这)、[`scripts/clang_tidy_check.py`](../../scripts/clang_tidy_check.py)、[`scripts/validate_frontmatter.py`](../../scripts/validate_frontmatter.py)。
- **GitHub Actions 文档**:workflow 语法(`on`/`jobs`/`steps`/`runs-on`)、`strategy.matrix` 矩阵、`concurrency`(含 `cancel-in-progress`)、`DavidAnson/markdownlint-cli2-action`(第三方 action 的版本化复用)。
- **承接章节**:阶段 0 第 16 章(那里逐行拆了原来的 4-job ci.yml、本章是它扩到 6 道门后的全景)、第 12 章(static-analysis 这道门的来历)、第 13 章(coverage 这道门的来历)、阶段 0 第 10 章(sanitize job 的源头)、第 17 章(format-check job 的 clang-format 详解)。
