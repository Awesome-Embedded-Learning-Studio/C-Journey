<script setup lang="ts">
// 首页「学习路径」区块(挂载于 home-features-after)。
// 2026-09 对齐 TAMCPP:由 HomePathGraph 的 SVG 交互路径图接管旧 HomeRoadmap 的
// 阶段卡列表(旧组件保留在 components/ 备回退/取材);「严格线性」引导语保留。
// 图数据在 home-path-data.ts(六阶段蛇形主线 + 练习/标签/项目支撑点)。
import HomePathGraph from './HomePathGraph.vue'
</script>

<template>
  <section id="roadmap" class="home-path">
    <header class="hp-head">
      <h2 class="hp-title">🧭 学习路径</h2>
      <p class="hp-note">拖动节点布置 · 悬停追溯路径 · 点击直达</p>
    </header>

    <HomePathGraph />

    <p class="hp-next">
      严格线性:阶段 0 装好工具链和 sanitizer → 阶段 1 打 C 基底 → 阶段 2 啃指针与内存(ASan 全程护驾)→ 阶段 3 自己造数据结构 → 阶段 4 工程化(把零散 .c 变成可信工程)→ 阶段 5 系统编程(进程/IO/socket)。别跳着读。
    </p>
  </section>
</template>

<style scoped>
.home-path {
  max-width: 1152px;
  margin: 40px auto 56px;
  padding: 0 24px;
  scroll-margin-top: 80px;
  animation: hp-fade-up 0.7s cubic-bezier(0.25, 0.46, 0.45, 0.94) both;
}

.hp-head {
  display: flex;
  align-items: center;
  flex-wrap: wrap;
  gap: 10px 16px;
  margin-bottom: 16px;
}

.hp-title {
  margin: 0;
  font-size: 20px;
  font-weight: 700;
  line-height: 1.4;
  color: var(--vp-c-text-1);
}

.hp-note {
  margin: 0;
  font-size: 13px;
  color: var(--vp-c-text-3);
}

.hp-next {
  margin: 18px auto 0;
  max-width: 720px;
  font-size: 14px;
  line-height: 1.7;
  color: var(--vp-c-text-2);
  text-align: center;
}

@keyframes hp-fade-up {
  from {
    opacity: 0;
    transform: translateY(18px);
  }
  to {
    opacity: 1;
    transform: translateY(0);
  }
}

@media (prefers-reduced-motion: reduce) {
  .home-path {
    animation: none !important;
  }
}

@media (max-width: 639px) {
  .home-path {
    padding: 0 16px;
    margin: 28px auto 36px;
  }

  .hp-note {
    display: none;
  }
}
</style>
