---
title: "Git 基础操作:给 C 代码上个后悔药"
description: "搞懂工作区/暂存区/本地仓库三个区域,练熟 add/commit/branch/merge 四大件,顺带记下本仓库的两条硬约定:永不 push、commit 禁带 Co-Authored-By。"
chapter: 0
order: 0
tags:
  - host
  - git
  - toolchain
difficulty: beginner
reading_time_minutes: 12
platform: host
prerequisites:
  - "命令行基础"
related:
  - "编译流程、GDB 与库"
  - "工程化与模块化设计(阶段 4)"
---

# Git 基础操作:给 C 代码上个"后悔药"

## 引言

新手写 C 最容易踩的坑其实不是指针,而是——写了一晚上,改崩了,Ctrl+Z 也救不回来,只能对着黑屏发呆。我见过太多人把整个项目文件夹复制成 `项目_final_最终_真的最终_v3/`,这其实就是手动的、粗糙的版本控制。Git 干的就是这件事,只不过它把每一次改动都记成了一个小快照,还能随时"时光倒流"回到任意一个时间点。

这一章不追求把 Git 讲全(那能写一本书),只够你把 C-Journey 这类项目的日常跑通:**改代码 → 暂存 → 提交 → 建分支 → 合并 → 撤销**。每条命令我们都会**真的敲一遍、把输出贴出来**,不靠想象。

## 核心概念:三个区域,一条流水线

Git 里所有操作都围绕三个区域打转,理解了它们,后面的命令就只是"在三个区域之间搬运":

- **工作区(Working Directory)**:你眼睛看到、正在改的那些文件。
- **暂存区(Staging Area / Index)**:一个"购物车"。你挑好的、准备提交的改动先放这儿。
- **本地仓库(Local Repository)**:存所有历史快照的地方。一旦 `commit`,这次改动就进了历史,删不掉了。

一条完整的流水线长这样:

```text
  工作区  ──git add──▶  暂存区  ──git commit──▶  本地仓库
 (你改的)              (购物车)                (历史快照)
```

## 起步:告诉 Git 你是谁

第一次用 Git,得先报上名号,因为每次提交都要署名:

```bash
# 设置全局用户名和邮箱(只需一次)
git config --global user.name "你的名字"
git config --global user.email "you@example.com"

# 查看当前所有配置
git config --list
```

拿到一个项目,要么在本地新建,要么从远程克隆:

```bash
# 选择 A:在本地新建仓库
git init

# 选择 B:把远程仓库克隆下来
git clone <仓库 URL>
```

## 每天都在用的"四大件"

下面四条命令是你写代码时用得最多的:

```bash
# 1. 看一眼:哪些文件改了、哪些已进暂存区
git status

# 2. 把改动放进"购物车"(暂存)
git add hello.c        # 只暂存指定文件
git add .              # 暂存当前目录下所有改动(最常用)

# 3. 把购物车里的东西"结账",存成一条历史
git commit -m "修复 hello.c 的缓冲区溢出"

# 4. 翻历史
git log --oneline      # 精简版,一行一条
```

`git status` 的输出大概长这样(真的跑出来的):

```text
$ git status
On branch feat/integrate-old-c-code
Changes to be committed:
  (use "git restore --staged <file>..." to unstage)
        modified:   examples/stage1-armc-basics/Exp1/var.c

Changes not staged for commit:
        modified:   documents/00-dev-environment/git-basic-operations.md
```

> 💡 **每日标配流程**:改代码 → `git status`(看一眼)→ `git add .`(打包)→ `git commit -m "..."`(存盘)。把这套肌肉记忆练熟,Git 基本就够用了。

## 分支:在不弄坏主线的前提下折腾

分支让你能开一条"平行宇宙"去试验,主线(`main`)不受影响。做新功能、修 bug,都该先开分支:

```bash
git branch                    # 看所有分支,当前分支前有 *
git switch -c feat/add-vector # 新建并切到新分支
# ……在新分支上折腾 ……
git switch main               # 切回主线
git merge feat/add-vector     # 把新分支合并进来
git branch -d feat/add-vector # 删掉用完的分支
```

## 远程:和 GitHub 同步

本地仓库和 GitHub/Gitee 之间,靠 push/pull 同步:

```bash
git remote -v                 # 看远程地址
git remote add origin <URL>   # 关联一个远程
git push -u origin main       # 第一次推送(-u 记住关联)
git pull                      # 拉取并合并远程更新
git fetch                     # 只拉取不合并(更安全,先看一眼)
```

## 撤销:三档"后悔药"

写代码难免手滑,Git 给了三档撤销力度,区别在于改动"漏"到了哪一步:

```bash
# 档位一:还在工作区没 add —— 扔掉改动,恢复到上次提交
git restore <文件>            # 老写法:git checkout -- <文件>

# 档位二:已经 add 进暂存区 —— 拿出来,但保留改动
git restore --staged <文件>   # 老写法:git reset HEAD <文件>

# 档位三:已经 commit —— 回退到上一个版本
git reset --soft HEAD^        # 保留改动,只撤销 commit(最常用)
git reset --hard HEAD^        # ⚠️ 彻底抹去改动,不可恢复,慎用!
```

## 常见踩坑

> 🔥 **本仓库两条硬约定(重要)**
> - **永不 `git push`**:C-Journey 的提交只留在本地,推送由维护者统一处理。练习时 `commit` 即可,别 `push`。
> - **commit 信息禁带 `Co-Authored-By`**:提交署名保持干净,不要加 `Co-Authored-By: ...` 这类尾注。

- **`git add .` 会把大文件也卷进去**:像 14MB 的 `鲜花.mp3` 这种,误提交进仓库后 clone 体积会暴涨。养成 `git status` 先看一眼的习惯;大文件走 `.gitignore`(本仓库已把 `*.mp3`、`*~` 等加进去)。
- **`reset --hard` 是单程票**:`--hard` 会把工作区里没提交的改动直接抹掉,基本捞不回来。不确定时先用 `--soft`,或先 `git stash` 暂存。
- **别在 `main` 上直接大改**:先开分支。主线只用来接合并,保持随时可用的状态。
- **`merge` 冲突别慌**:冲突时 Git 会在文件里标出 `<<<<<<< / ======= / >>>>>>>`,手动选保留哪边,改完 `git add` 再 `git commit` 即可。

## 小结

记住一句话就够:**三个区域(工作区→暂存区→本地仓库)+ 四大件(status / add / commit / log)+ 一条原则(先开分支再折腾)**。剩下的命令都是在这套骨架上长出来的细节,用到再查。

## 练习

1. 新建一个练习仓库,写个 `hello.c`,`git add` 后用 `git status` 观察暂存区变化,再 `commit`。
2. 用 `git log --oneline` 看历史;故意改坏 `hello.c`,分别用 `restore`、`reset --soft`、`reset --hard` 三档撤销,体感它们的区别。
3. 开一个分支 `feat/xxx`,改点东西,切回 `main` 合并它,再删掉分支,走完一次完整分支流程。

## 参考资源

- [Pro Git Book(中文版)](https://git-scm.com/book/zh/v2)——最权威的 Git 书,免费。
- `git <命令> --help`——任何命令的本地手册,比如 `git reset --help`。
- [Learn Git Branching](https://learngitbranching.js.org/?locale=zh_CN)——可视化学分支的交互式网站。

---
*按 C-Journey 写作规范整理。*
