<script setup lang="ts">
import { withBase } from 'vitepress'

// 首页"学习路线图":展示 C-Journey 四阶段 + 进度状态。
// TAMCPP 原版用 mermaid 画图,C-Journey 没装 mermaid —— 改成纯 HTML/CSS 的阶段卡列表,
// 信息密度更高(每阶段带章数 + 简介 + 状态徽标),且零运行时依赖。
// 状态:done = 已审核标杆 / reviewing = 已写待审 / planned = 规划中。

type Status = 'done' | 'reviewing' | 'planned'

interface Stage {
  no: string
  name: string
  dir: string
  chapters: number
  desc: string
  status: Status
  link: string
}

const statusMeta: Record<Status, { mark: string; label: string; cls: string }> = {
  done:      { mark: '✓', label: '已审核 · 标杆', cls: 'rm-chip--done' },
  reviewing: { mark: '✦', label: '已写 · 待审核',  cls: 'rm-chip--doing' },
  planned:   { mark: '◇', label: '推进中',        cls: 'rm-chip--todo' },
}

const stages: Stage[] = [
  {
    no: '阶段 0',
    name: '开发环境与编译',
    dir: '00-dev-environment',
    chapters: 17,
    desc: '工具链体检、编译四阶段、预处理/汇编/目标文件、链接与动态库、警告体系、优化档、sanitizer、make/cmake、gdb、git、CI、clang-format。',
    status: 'done',
    link: '/00-dev-environment/01-toolchain-health-check',
  },
  {
    no: '阶段 1',
    name: 'C 语言基底',
    dir: '01-c-basics',
    chapters: 12,
    desc: '程序结构、整型与溢出、浮点字符、运算符、位运算、控制流、函数、作用域 static、数组、字符串、IO。',
    status: 'done',
    link: '/01-c-basics/01-program-structure-and-compilation',
  },
  {
    no: '阶段 2',
    name: '指针与内存',
    dir: '02-pointers-memory',
    chapters: 12,
    desc: '指针是什么、指针算术、改调用者、const 限定、统一视角、malloc/free、动态内存的坑(ASan 抓)、多级指针、函数指针、复杂声明、void*、内存布局。',
    status: 'done',
    link: '/02-pointers-memory/01-what-is-a-pointer',
  },
  {
    no: '阶段 3',
    name: '数据结构与算法',
    dir: '03-data-structures',
    chapters: 12,
    desc: '单链表、双向链表、栈、队列、动态数组、二叉树、BST、哈希表、排序入门、快排归并、二分查找、大 O。',
    status: 'done',
    link: '/03-data-structures/01-singly-linked-list',
  },
]
</script>

<template>
  <section id="roadmap" class="home-roadmap">
    <div class="home-roadmap__card">
      <h2 class="home-roadmap__title">📍 学习路线图</h2>

      <div class="home-roadmap__legend">
        <span
          v-for="(m, k) in statusMeta"
          :key="k"
          class="rm-chip"
          :class="m.cls"
        >
          <span class="rm-chip__mark">{{ m.mark }}</span>{{ m.label }}
        </span>
      </div>

      <div class="home-roadmap__stages">
        <a
          v-for="s in stages"
          :key="s.dir"
          class="rm-stage"
          :class="`rm-stage--${s.status}`"
          :href="withBase(s.link)"
        >
          <div class="rm-stage__head">
            <span class="rm-stage__no">{{ s.no }}</span>
            <span
              class="rm-stage__badge"
              :class="statusMeta[s.status].cls"
            >
              <span class="rm-stage__badge-mark">{{ statusMeta[s.status].mark }}</span>
              {{ statusMeta[s.status].label }}
            </span>
          </div>
          <h3 class="rm-stage__name">{{ s.name }}</h3>
          <p class="rm-stage__desc">{{ s.desc }}</p>
          <span class="rm-stage__meta">{{ s.chapters }} 章</span>
        </a>
      </div>

      <p class="home-roadmap__next">
        严格线性:阶段 0 装好工具链和 sanitizer → 阶段 1 打 C 基底 → 阶段 2 啃指针与内存(ASan 全程护驾)→ 阶段 3 自己造数据结构。别跳着读。
      </p>
    </div>
  </section>
</template>

<style scoped>
.home-roadmap {
  max-width: 1152px;
  margin: 40px auto 56px;
  padding: 0 24px;
  scroll-margin-top: 80px;
  animation: roadmap-fade-up 0.7s cubic-bezier(0.25, 0.46, 0.45, 0.94) both;
}

.home-roadmap__card {
  padding: 28px 32px;
  border: 1px solid var(--vp-c-divider);
  border-radius: 14px;
  background-color: var(--vp-c-bg);
  box-shadow:
    0 1px 3px rgba(0, 0, 0, 0.04),
    0 1px 2px rgba(0, 0, 0, 0.06);
  text-align: center;
}

.dark .home-roadmap__card {
  background-color: var(--vp-c-bg-elv);
  border-color: var(--vp-c-border);
  box-shadow:
    0 1px 3px rgba(0, 0, 0, 0.2),
    0 1px 2px rgba(0, 0, 0, 0.15);
}

.home-roadmap__title {
  margin: 0 0 18px;
  font-size: 20px;
  font-weight: 700;
  line-height: 1.4;
  color: var(--vp-c-text-1);
  border-top: 0;
  padding-top: 0;
}

/* ── Legend ─────────────────────────────────── */
.home-roadmap__legend {
  display: flex;
  flex-wrap: wrap;
  justify-content: center;
  gap: 12px;
  margin-bottom: 24px;
}

.rm-chip {
  display: inline-flex;
  align-items: center;
  gap: 8px;
  padding: 8px 16px;
  border-radius: 999px;
  border: 1px solid var(--vp-c-divider);
  background: var(--vp-c-bg-soft);
  font-size: 14px;
  font-weight: 500;
  line-height: 1;
  color: var(--vp-c-text-1);
  white-space: nowrap;
}

.rm-chip__mark {
  font-size: 14px;
  font-weight: 700;
}

.rm-chip--done .rm-chip__mark,
.rm-chip--done .rm-stage__badge-mark { color: var(--vp-c-green-1); }
.rm-chip--doing .rm-chip__mark,
.rm-chip--doing .rm-stage__badge-mark { color: #ffc107; }
.rm-chip--todo .rm-chip__mark,
.rm-chip--todo .rm-stage__badge-mark { color: var(--vp-c-text-3); }

/* ── Stage cards ────────────────────────────── */
.home-roadmap__stages {
  display: grid;
  grid-template-columns: repeat(2, 1fr);
  gap: 14px;
  margin-bottom: 24px;
  text-align: left;
}

.rm-stage {
  display: flex;
  flex-direction: column;
  gap: 8px;
  padding: 18px 20px;
  border: 1px solid var(--vp-c-divider);
  border-radius: 12px;
  background: var(--vp-c-bg);
  text-decoration: none !important;
  color: var(--vp-c-text-1) !important;
  transition: border-color 0.35s ease,
              box-shadow 0.35s ease,
              transform 0.35s ease;
}

.rm-stage:hover {
  border-color: var(--vp-c-brand-1);
  box-shadow: 0 10px 28px rgba(0, 0, 0, 0.1),
              0 4px 8px rgba(0, 0, 0, 0.06);
  transform: translateY(-3px);
}

.rm-stage__head {
  display: flex;
  align-items: center;
  justify-content: space-between;
  gap: 8px;
}

.rm-stage__no {
  font-size: 12px;
  font-weight: 700;
  letter-spacing: 0.06em;
  color: var(--vp-c-text-3);
  text-transform: uppercase;
}

.rm-stage__badge {
  display: inline-flex;
  align-items: center;
  gap: 5px;
  padding: 3px 10px;
  border-radius: 999px;
  border: 1px solid var(--vp-c-divider);
  background: var(--vp-c-bg-soft);
  font-size: 11.5px;
  font-weight: 600;
  line-height: 1;
  color: var(--vp-c-text-2);
}

.rm-stage__name {
  margin: 0;
  font-size: 17px;
  font-weight: 700;
  line-height: 1.4;
  color: var(--vp-c-text-1);
}

.rm-stage:hover .rm-stage__name {
  color: var(--vp-c-brand-1);
}

.rm-stage__desc {
  margin: 0;
  font-size: 13.5px;
  line-height: 1.7;
  color: var(--vp-c-text-2);
  flex: 1;
}

.rm-stage__meta {
  align-self: flex-start;
  font-size: 12px;
  font-weight: 600;
  color: var(--vp-c-brand-1);
  font-variant-numeric: tabular-nums;
}

.home-roadmap__next {
  margin: 0 auto;
  max-width: 720px;
  font-size: 14px;
  line-height: 1.7;
  color: var(--vp-c-text-2);
}

@keyframes roadmap-fade-up {
  from { opacity: 0; transform: translateY(18px); }
  to   { opacity: 1; transform: translateY(0); }
}

@media (prefers-reduced-motion: reduce) {
  .home-roadmap { animation: none !important; }
  .rm-stage { transition: none; }
}

@media (max-width: 639px) {
  .home-roadmap { padding: 0 16px; margin: 28px auto 36px; }
  .home-roadmap__card { padding: 22px 18px; }
  .home-roadmap__title { font-size: 18px; }
  .home-roadmap__stages { grid-template-columns: 1fr; }
  .rm-chip { font-size: 13px; padding: 7px 13px; }
  .rm-stage__name { font-size: 16px; }
}
</style>
