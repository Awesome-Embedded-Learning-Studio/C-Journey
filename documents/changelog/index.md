---
title: "更新日志"
description: "C-Journey 的里程碑式变更——每个阶段的收官、基础设施、定位调整。日期为上线日期;详细提交见 commit 历史。"
chapter: 99
order: 0
tags:
  - host
  - meta
difficulty: beginner
reading_time_minutes: 5
platform: host
c_standard: [99, 11]
prerequisites: []
related: []
---

# 更新日志

C-Journey 起初是维护者自己学 C 时随手记的笔记,记着记着攒成了从工具链到系统编程的一条全程,逐条真跑核实后整理公开,就成了这份教程。

往后按里程碑往前走,不按语义化版本号。这里记每个阶段收官、基础设施大改、定位调整;逐条提交在 [commit 历史](https://github.com/Awesome-Embedded-Learning-Studio/C-Journey/commits/main)。

## 2026-07-03 · 定位落定 + 站点重设计 + 元信息收口

对外定位定成「**用 C 游历计算机世界**」——兑现项目名(游历 = journey),C 当载具而不是终点。叙事去掉了「亲手踩坑」(那是基本要求,当卖点反而诡异)。嵌入式改成按深度切:这里只浅尝、开个门,深做交给 [imx-forge](https://awesome-embedded-learning-studio.github.io/imx-forge/) 和 ST-Forge。

站点做了一轮重设计:锈橙 + 暖中性底主题(照 `~/anatomy_gui` 那套「单一强调色 + 暖中性底」公式),补齐了 favicon / og-image / robots.txt。搬进来三个 markdown-it 插件——超 20 行的长代码自动折叠(纯 CSS `:has()`,关 JS 也能展开)、`++Ctrl+C++` 渲染成键帽、mermaid 图本地打包且随主题变色。

顺手修了两个**预存的 build bug**——`vitepress build` 之前压根不在 CI 里、从没跑过,这次第一次构建 stage-4 内容才暴露。一个是正文里的 `<optimized out>`、`<stdio.h>` 被 Vue 当未闭合标签(加了 escape-cpp 插件兜);一个是内联反引号里的 `${{ github.ref }}` 被当 mustache(给内联 code 补了 `v-pre`)。

元信息也清了一遍:全仓的 `Charliechen114514/C-Journey`(仓库早转移到 org、是 404 死链)统一改成 canonical 的 `Awesome-Embedded-Learning-Studio/C-Journey`,连 `OnlineCompilerDemo` 抓源码的 rawBase 和 `branch: next`(早没这分支了)一起修了。README / CONTRIBUTING / ROADMAP / AGENTS / issue 和 PR 模板都刷到当前现实,GitHub 的 description 和 topics 也设好了。

## 2026-07-03 · 阶段 4/5 上线 + 清死文件

阶段 4(工程化)和阶段 5(系统编程)的 30 篇文章维护者初审通过,批量撤了 🟡 待审核 banner(审核完毕 = 无 banner,跟阶段 0-3 一致),`config.ts` 翻开关让 04/05 正式进站点侧栏。

清掉了 8 个 legacy 死文件(阶段 4/5 重写后没按「不留归档区」策略消化的旧稿)。其中三段独占内容——`find_package` 的 Module vs Config 模式、`SO_REUSEADDR` + `TIME_WAIT`、`setvbuf` + `feof` 坑——编译真跑核实后移植进了对应新章,没丢。导航(两份 README + 阶段 1 导读)重写,原来还指向重写前那些不存在的旧文件名。CI 顺手做了卫生:format-check 加注释、coverage 上传 artifact、过时 TODO 清掉、push 触发去掉半遗弃的特性分支。

## 2026-07-02 · 阶段 4 工程化收官(16/16)+ 阶段 5 系统编程收官(14/14)

阶段 4 把工程化整条链铺完:头文件契约(ODR / inline)、API 与不透明类型、错误处理、make 深处、CMake 工程化、静态/动态库 + install/export、测试(Unity)+ Mock、gdb 多线程、ASan/valgrind、静态分析门、覆盖率门、性能剖析、CI 流水线、毕业项目。

`clib-utilities` 这一轮做了真整改:`add_library` + CTest + Unity,纳进 `build_examples.py` 硬门和覆盖率门,成了阶段 4 的活教材。`ci.yml` 也加了 clang-tidy 静态分析门和 gcov/lcov 覆盖率门(基建先于章节落地)。

阶段 5 把主机系统编程走通:文件 IO 与 fd、fork + 写时复制、exec/wait、守护进程、信号、pipe、共享内存、select、poll/epoll、非阻塞 reactor、socket TCP、进阶 socket、UDP/Unix 域、getaddrinfo。

## 2026-07-01 · 阶段 0-3 全审 + 工作流成型

阶段 0(17 章)、阶段 1(13 章)、阶段 2(12 章)、阶段 3(12 章)重写完成、维护者审核完毕。「Agent 并发写初稿 + 主控串行总验证」的工作流也是这段时间成型的——核心验证(重跑每个代码块、核对输出块、核 ISO 条款 + 声音、ASan 复核动态内存)必须主控亲笔来。声音标杆 Ch01-08 定稿:坑就地插、几乎不用列表、小结走散文。

## 2026-05-27 · 项目启动

Initial commit。C-Journey 立项:一份纯 C、贴真实输出、引 ISO 条款的主机系统教程。
