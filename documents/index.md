---
layout: home

hero:
  name: C-Journey
  text: 亲手踩坑的纯 C 系统编程教程
  tagline: 每章真跑 gcc 16 + clang 22、贴真实终端输出与 ISO 条款,用折腾工程师的声音带你走通
  actions:
    - theme: brand
      text: 从阶段 0 开始
      link: /00-dev-environment/01-toolchain-health-check
    - theme: alt
      text: 直奔指针与内存
      link: /02-pointers-memory/01-what-is-a-pointer

features:
  - title: 真实输出,绝不编造
    details: 每条 C 行为先在 /tmp 写最小 .c、gcc 16 + clang 22 双跑(-std=c11)、贴真实终端输出,UB 加 -fsanitize=address,undefined。
  - title: ISO 条款锚定
    details: 每个结论引用 ISO/IEC 9899:2011 条款,可追溯、可验证,不是「据说是」而是「标准说」。
  - title: Sanitizer 贯穿
    details: UBSan/ASan 当场抓未定义行为(溢出、越界、UAF、double-free、泄漏),写动态内存代码的必备护栏。
  - title: 折腾工程师腔
    details: 不是干巴巴的 API 文档,是技术探索记录 + 手把手带读者走完整旅程,允许吐槽、踩坑叙述、情绪。
  - title: 四阶段、53 章
    details: 阶段0 开发环境(17 章)→ 阶段1 C 基底(12)→ 阶段2 指针与内存(12)→ 阶段3 数据结构与算法(12)。
  - title: 每章可编译
    details: 文档里的每个代码块都能 gcc/clang 编译运行,输出和贴的一致——CI 硬门 build_examples 守着。
---
