---
title: "Git 工作流：管住代码的版本与协作"
description: "前面讲完了编译、构建、调试，这一章管「代码本身的版本」。从一个临时仓库真跑 Git 工作流：搞清工作区/暂存区/版本库三个区（add/commit 在区间搬改动）、init/add/commit/log 的基础提交、git status 和 git diff 看改动、git switch -c 开 feature 分支、git merge --no-ff 合并，最后用 git log --graph 画出分叉与合并的历史。顺带说清 commit message 的约定写法（conventional commits：feat/fix/docs…）和本项目的几条硬约定——在 next 分支上写、永不 git push（推送由维护者统一处理）、commit 不带 Co-Authored-By——以及 .gitignore 该挡什么。"
chapter: 0
order: 15
tags:
  - host
  - git
difficulty: beginner
reading_time_minutes: 13
platform: host
c_standard: [11]
prerequisites:
  - "命令行基础（无强前置章节）"
related:
  - "第 16 章：GitHub Actions（把质量门挂在 Git 的 push/PR 上跑）"
  - "第 11 章：make 入门 / 第 12 章：CMake 入门（CI 里调用的构建工具）"
---

# Git 工作流：管住代码的版本与协作

## 引言：为什么 C 工程也要 Git

写代码写久了你一定会遇到这几件事：改着改着把原本能跑的代码改坏了、想退回去却不知道改之前长什么样；两个人同时改一个文件、合到一起时打架；线上出了问题、想知道「这行是谁什么时候改成这样的」。这些痛，**Git** 就是用来治的——它给你的代码拍快照、让你随时回到任何一个历史版本、让多人在不同分支上并行开发再合并。而且 Git 不只是「存代码」，本课程后面第 16 章的 CI（持续集成）就是挂在 Git 的提交/合并事件上自动跑的。这一章我们建一个临时仓库，把 Git 日常最常用的那一套真跑一遍。

为了不污染本项目的真实仓库，我们在 `/tmp` 下开一个全新的临时仓库来演示（下面所有命令都在它里面跑）：

```text
$ mkdir demo && cd demo
$ git init
$ git config user.email "demo@cj.dev"   # 临时仓库的提交身份
$ git config user.name "CJ Demo"
```

## 三个区：工作区 / 暂存区 / 版本库

学 Git 第一件事是搞清「改动到底放在哪」。Git 有三个区，你的改动在三区之间流动：

- **工作区**（working directory）：就是你眼里能看到的那些文件，你用编辑器改的就是它。
- **暂存区**（staging area / index）：`git add` 把工作区的改动「登记」到这里，是一个「准备提交」的候场区。
- **版本库**（repository，`.git/`）：`git commit` 把暂存区的内容固化成一个永久快照存到这里。

为什么中间要隔一个暂存区，不直接从工作区提交？因为它让你能**挑着提交**——你一次改了五个文件、其中三个属于功能 A、两个属于修 bug B，你可以只 `add` 三个、`commit` 成一个「功能 A」提交，再把另两个 `add`/`commit` 成「修 B」提交。没有暂存区，你只能一股脑全提交，历史就乱了。

## 基础流程：init/add/commit/log

我们放一个 `main.c` 进来，走一遍最基础的提交：

```c
#include <stdio.h>

int main(void) {
    printf("version 1\n");
    return 0;
}
```

```text
$ git add main.c          # 工作区 → 暂存区
$ git commit -m "feat: 初始版本,打印 version 1"   # 暂存区 → 版本库
[main bacd004] feat: 初始版本,打印 version 1
 1 file changed, 6 insertions(+)
 create mode 100644 main.c
$ git log --oneline
bacd004 feat: 初始版本,打印 version 1
```

`git add` 把 `main.c` 的当前内容放进暂存区，`git commit -m "..."` 把暂存区固化成一个提交（`-m` 直接给提交信息，不带 `-m` 会弹编辑器让你写长信息）。`git log --oneline` 把历史压成一行一条：前面那串 `bacd004` 是这个提交的短哈希（唯一标识），后面是提交信息。

这里要特别说一下**提交信息怎么写**。本课程和大多数现代项目用「约定式提交」（conventional commits）：开头一个类型词（`feat` 新功能、`fix` 修 bug、`docs` 文档、`refactor` 重构、`test` 测试、`style` 格式、`chore` 杂务），冒号后一句话说清改了什么。你回头看本项目的 `git log`，`feat(docs): ...`、`style(docs): ...` 就是这个套路。这种写法的好处是：别人（和工具）一眼能看出每个提交的性质，还能据此自动生成版本日志。别写 `update`、`修改` 这种信息量为零的提交信息——那等于没写。

## 看改动：git status 与 git diff

现在我们改一下 `main.c`（把 `version 1` 改成 `version 2`），但先不 add。两个最常用的「看」命令派上用场：

```text
$ git status
On branch main
Changes not staged for commit:
        modified:   main.c        ← 工作区改了,但还没 add 进暂存区
```

`git status` 告诉你「哪个文件改了、改动在哪个区」。然后 `git diff` 显示**工作区和暂存区之间**的具体差异（行级）：

```text
$ git diff
diff --git a/main.c b/main.c
--- a/main.c
+++ b/main.c
@@ -1,6 +1,6 @@
 #include <stdio.h>

 int main(void) {
-    printf("version 1\n");     ← 旧版本(-)
+    printf("version 2\n");     ← 新版本(+)
     return 0;
 }
```

`git diff` 用的是 unified diff 格式：`-` 开头是删掉的行、`+` 开头是新增的行，中间那几行是上下文。这是审查「我到底改了什么」最直接的工具——提交前养成 `git diff` 扫一眼的习惯，能挡掉很多「手滑把无关的也提交了」。注意 `git diff` 默认比的是「工作区 vs 暂存区」；如果你想看「暂存区 vs 最新提交」（即 add 了但还没 commit 的），用 `git diff --staged`。满意了就 `git add` + `git commit` 把它固化成第二个提交：

```text
$ git add main.c
$ git commit -m "feat: 升级到 version 2"
$ git log --oneline
393d39b feat: 升级到 version 2
bacd004 feat: 初始版本,打印 version 1
```

## 分支与合并：并行开发的根本

到目前为止历史是一条直线。Git 真正强大的地方是**分支**——你可以从主线分叉出去、在一个独立的分支上做实验性改动，主线完全不受影响，做完了再合回来。我们来开一个叫 `feature` 的分支：

```text
$ git switch -c feature        # 创建 feature 分支并切过去(-c = create)
Switched to a new branch 'feature'
```

在这个分支上我们把 `main.c` 改成 `version 3` 并提交（注意这些改动只发生在 `feature` 分支上，`main` 分支的 `version 2` 纹丝不动）：

```text
$ ...（改成 version 3）
$ git add main.c && git commit -m "feat: feature 分支 version 3"
```

现在把 `feature` 的工作合回主线。先切回 `main`，再 `merge`：

```text
$ git switch main
$ git merge --no-ff feature -m "Merge branch 'feature'"
Merge made by the 'ort' strategy.
 main.c | 2 +-
 1 file changed, 1 insertion(+), 1 deletion(-)
```

`git merge feature` 把 `feature` 分支的改动合并进当前分支（`main`）。`--no-ff`（no fast-forward）强制生成一个「合并提交」（merge commit），这样历史里会明确留下「这里发生过一次合并」。最后用 `git log --oneline --graph` 画出整个历史，分叉和合并看得一清二楚：

```text
$ git log --oneline --graph
*   6d569b7 Merge branch 'feature'
|\
| * 9d2900b feat: feature 分支 version 3
|/
* 393d39b feat: 升级到 version 2
* bacd004 feat: 初始版本,打印 version 1
```

看这个图：最底下是两次直线提交（`初始版本`、`version 2`）；然后从 `version 2` 分叉出 `feature` 分支（`9d2900b`），`feature` 完成后合回主线产生那个合并提交（`6d569b7`）。这种「开分支干活、干完合回主线」就是团队并行开发的标准节奏——每个人或每个功能一个分支，互不干扰，最后用 merge 汇总。

## 远程、协作，以及本项目的几条硬约定

上面演示的都是本地仓库。真实协作里，你还有一个**远程仓库**（GitHub/GitLab 上的那个仓库），本地和远程之间靠 `git push`（本地→远程）和 `git pull`（远程→本地）同步。这部分本项目有几条**必须遵守的硬约定**（来自 [AGENTS.md](https://github.com/Charliechen114514/C-Journey/blob/main/AGENTS.md)），你在这个仓库里干活前一定要知道：

- **在 `next` 分支上写，不在 `main` 上直接动**。`main` 是稳定主线，开发走 `next` 等分支。
- **永不 `git push`**——推送由维护者统一处理。你可以本地 `commit`，但不要自己推到远程。
- **commit 信息不带 `Co-Authored-By`** 之类的 AI 署名尾注，保持提交历史干净。

还有 `.gitignore`：它列出「哪些文件不该进版本库」。C 工程里编译产物（`*.o`、`*.so`、可执行文件）、构建目录（`build/`、`_build_ci/`）、编辑器临时文件都不该提交——它们能从源码重新生成，提交了只会污染历史。本项目还有一些「私有」文件（比如这份重写的审核队列 `.claude/review-queue.md`）也被 `.gitignore` 挡在外面，只存在于本机。新建工程时，把该挡的尽早写进 `.gitignore`，别等误提交了再补救（`git rm --cached 文件` 能把已跟踪的文件从版本库移除、但保留本地文件）。

## 小结

Git 用三个区管你的改动：工作区（编辑器里的文件）、暂存区（`git add` 登记的候场区）、版本库（`git commit` 固化的快照），隔一个暂存区是为了让你能挑着提交、把相关的改动归到同一个提交里。基础流程是 `git add`（工作区→暂存区）+ `git commit -m "信息"`（暂存区→版本库）+ `git log --oneline` 看历史，提交信息按约定式提交写（`feat:`/`fix:`/`docs:` 类型词 + 一句话，别写「update」这种）；`git status` 看改动在哪个区、`git diff` 看工作区 vs 暂存区的行级差异（`-` 删 `+` 增），提交前扫一眼能挡掉手滑。分支是并行开发的根本：`git switch -c 分支名` 开分支、在里面独立改、`git switch main` 切回主线、`git merge --no-ff 分支` 合并，`git log --oneline --graph` 画出分叉与合并的历史图。远程协作靠 push/pull，但本项目有硬约定：在 `next` 上写、**永不 push**（维护者统一推）、commit 不带 `Co-Authored-By`；编译产物、构建目录、私有文件都用 `.gitignore` 挡在版本库外。下一章我们把 Git 和 CI 接起来——看 GitHub Actions 怎么在你每次提交/合并时自动把构建、警告、sanitizer、测试全跑一遍。

## 参考资源

- `git --help` / Pro Git 书（`add`/`commit`/`log`/`status`/`diff`/`branch`/`switch`/`merge`/`push`/`pull`、暂存区概念）
- Conventional Commits 规范（`feat`/`fix`/`docs`/`refactor`/`test`/`style`/`chore` 类型词）
- 本项目 [AGENTS.md](https://github.com/Charliechen114514/C-Journey/blob/main/AGENTS.md)（`next` 分支、不 push、commit 不带 Co-Authored-By 的硬约定）
- 第 16 章：GitHub Actions（CI 挂在 Git 的提交/合并事件上）
