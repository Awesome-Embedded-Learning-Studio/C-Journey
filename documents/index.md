---
layout: home

hero:
  name: C-Journey
  text: 用 C 游历计算机世界
  tagline: 脱胎于我学 C 时攒下的笔记,主机系统全程真跑(gcc 16 + clang 22 + ISO 9899)<br>嵌入式浅尝指路,深做见 <a href="https://awesome-embedded-learning-studio.github.io/imx-forge/" target="_blank">imx-forge</a>
  actions:
    - theme: brand
      text: 从阶段 0 开始
      link: /00-dev-environment/01-toolchain-health-check
    - theme: alt
      text: 直奔指针与内存
      link: /02-pointers-memory/01-what-is-a-pointer

features:
  - title: ISO 条款锚定
    details: 每个结论引用 ISO/IEC 9899:2011 条款,可追溯、可验证,不是「据说是」而是「标准说」。
  - title: Sanitizer 贯穿
    details: UBSan/ASan 当场抓未定义行为(溢出、越界、UAF、double-free、泄漏),写动态内存代码的必备护栏。
  - title: 六阶段、84 章
    details: 阶段0 开发环境(17)→ 阶段1 C 基底(13)→ 阶段2 指针与内存(12)→ 阶段3 数据结构与算法(12)→ 阶段4 工程化与质量门(16)→ 阶段5 系统编程(14)。
  - title: 每章可编译
    details: 文档里的每个代码块都能 gcc/clang 编译运行,输出和贴的一致——CI 硬门 build_examples 守着。
---
