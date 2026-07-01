<script setup lang="ts">
import { withBase } from 'vitepress'

// 首页提示横幅:给"不知道从哪开始"的读者一个明确入口。
// C-Journey 是严格线性的(阶段0→3),所以直接指向阶段 0 第一章。
const startLink = withBase('/00-dev-environment/01-toolchain-health-check')
</script>

<template>
  <div class="home-tip-banner">
    <span class="home-tip-icon">
      <svg xmlns="http://www.w3.org/2000/svg" width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round">
        <circle cx="12" cy="12" r="10" />
        <polygon points="16.24 7.76 14.12 14.12 7.76 16.24 9.88 9.88 16.24 7.76" />
      </svg>
    </span>
    <span class="home-tip-text">
      零基础或刚装好工具链？
      <a :href="startLink">从阶段 0 第 1 章「工具链健康检查」开始 →</a>
      <span class="home-tip-sub">。这份教程严格线性,每章真跑 gcc 16 + clang 22、贴 ISO 条款,别跳着读。</span>
    </span>
  </div>
</template>

<style scoped>
.home-tip-banner {
  max-width: 1152px;
  margin: -16px auto 32px;
  padding: 12px 20px;
  border-radius: 8px;
  display: flex;
  align-items: center;
  gap: 10px;
  background-color: var(--vp-tip-bg, rgba(63, 81, 181, 0.08));
  border: 1px solid var(--vp-tip-border, rgba(63, 81, 181, 0.2));
  color: var(--vp-tip-text, var(--vp-c-text-2));
  font-size: 14px;
  line-height: 1.6;
}

.home-tip-icon {
  flex-shrink: 0;
  color: var(--vp-tip-accent, #3f51b5);
  display: flex;
  align-items: center;
}

.home-tip-text a {
  color: var(--vp-tip-accent, #3f51b5);
  text-decoration: underline;
  text-underline-offset: 2px;
  font-weight: 600;
  margin-left: 2px;
}

.home-tip-text a:hover {
  color: var(--vp-tip-accent-hover, #303f9f);
}

.home-tip-sub {
  color: var(--vp-c-text-3);
}
</style>
