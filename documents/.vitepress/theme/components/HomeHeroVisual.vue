<script setup lang="ts">
import { ref, onMounted, onBeforeUnmount } from 'vue'
// 首页 Hero 终端动效:逐字打出 一段 C 程序 → 保存 → gcc+ASan 双跑 → 运行 的真实流程。
// 改编自 TAMCPP HomeHeroVisual(C++/std::print),换成 C-Journey 的纯 C + 指针主题。
type LineType = 'code' | 'cmd' | 'ok' | 'save' | 'out'
interface Line {
  text: string
  type: LineType
}

const script: Line[] = [
  { text: '#include <stdio.h>', type: 'code' },
  { text: 'int n = 5;', type: 'code' },
  { text: 'int *p = &n;', type: 'code' },
  { text: '*p = 42;  // 改的是 n', type: 'code' },
  { text: 'printf("%d\\n", n);', type: 'code' },
  { text: '', type: 'code' },
  { text: '\u{1F4BE}  saved pointer_demo.c', type: 'save' },
  { text: '$ gcc -std=c11 -Wall -Wextra pointer_demo.c', type: 'cmd' },
  { text: '$ gcc -std=c11 -fsanitize=address,undefined pointer_demo.c', type: 'cmd' },
  { text: '✓  双跑通过,无 UB', type: 'ok' },
  { text: '$ ./a.out', type: 'cmd' },
  { text: '42', type: 'out' },
]

const CHAR_MS = 28        // 每字毫秒
const LINE_MS = 80        // 行间停顿
const LOOP_MS = 2400      // 跑完后驻留再循环

const done = ref<Line[]>([])          // 已完成行
const partial = ref('')               // 当前正在打的局部文本
const partialType = ref<LineType>('code')
const finished = ref(false)           // 全部打完,驻留中
const reduced = ref(false)

let lineIdx = 0
let charIdx = 0
let timer: ReturnType<typeof setTimeout> | null = null

function reset() {
  done.value = []
  partial.value = ''
  partialType.value = 'code'
  finished.value = false
  lineIdx = 0
  charIdx = 0
  tick()
}

function tick() {
  const lines = script
  if (lineIdx >= lines.length) {
    finished.value = true
    timer = setTimeout(reset, LOOP_MS)
    return
  }
  const line = lines[lineIdx]
  const chars = Array.from(line.text)   // 按码点切,emoji 不会断
  if (charIdx <= chars.length) {
    partial.value = chars.slice(0, charIdx).join('')
    partialType.value = line.type
    charIdx++
    timer = setTimeout(tick, CHAR_MS)
  } else {
    done.value.push({ text: line.text, type: line.type })
    partial.value = ''
    lineIdx++
    charIdx = 0
    timer = setTimeout(tick, LINE_MS)
  }
}

onMounted(() => {
  reduced.value = !!window.matchMedia?.('(prefers-reduced-motion: reduce)').matches
  if (reduced.value) {
    // 减弱动效:一次性显示全部内容
    done.value = script.slice()
    finished.value = true
  } else {
    tick()
  }
})
onBeforeUnmount(() => {
  if (timer) clearTimeout(timer)
})
</script>

<template>
  <div class="hero-visual">
    <div class="terminal">
      <div class="terminal__bar">
        <span class="dot dot--red" />
        <span class="dot dot--yellow" />
        <span class="dot dot--green" />
        <span class="terminal__title">pointer_demo.c — zsh</span>
      </div>

      <div class="terminal__body">
        <div
          v-for="(ln, i) in done"
          :key="'d' + i"
          class="ln"
          :class="'ln--' + ln.type"
        >{{ ln.text }}</div>

        <div
          v-if="!finished"
          class="ln"
          :class="'ln--' + partialType"
        >{{ partial }}<span class="cursor">▋</span></div>

        <div v-else class="ln ln--prompt"><span class="cursor">▋</span></div>
      </div>
    </div>
  </div>
</template>

<style scoped>
.hero-visual {
  /* 固定宽度:容下最长代码行,gcc ASan 行约 56 字符;max-width:100% 保证窄屏不被遮。 */
  width: 540px;
  max-width: 100%;
  margin: 0 auto;
  animation: hero-fade-up 0.7s cubic-bezier(0.25, 0.46, 0.45, 0.94) both;
}

/* ── Terminal frame ────────────────────────────────────────── */
.terminal {
  position: relative;
  box-sizing: border-box;
  border: 1px solid rgba(99, 102, 241, 0.35);
  border-radius: 14px;
  overflow: hidden;
  background: linear-gradient(135deg, #1a1a2e 0%, #0f3460 100%);
  box-shadow:
    0 22px 56px rgba(15, 52, 96, 0.4),
    0 6px 14px rgba(0, 0, 0, 0.22);
  animation: terminal-glow 4s ease-in-out infinite;
}

.terminal__bar {
  display: flex;
  align-items: center;
  gap: 8px;
  padding: 13px 16px;
  background: rgba(0, 0, 0, 0.22);
  border-bottom: 1px solid rgba(255, 255, 255, 0.06);
}

.dot {
  width: 14px;
  height: 14px;
  border-radius: 50%;
  flex-shrink: 0;
}
.dot--red { background: #ff5f56; }
.dot--yellow { background: #ffbd2e; }
.dot--green { background: #27c93f; }

.terminal__title {
  margin-left: 10px;
  color: rgba(226, 232, 240, 0.55);
  font-family: var(--vp-font-family-mono);
  font-size: 13px;
  letter-spacing: 0.3px;
}

/* ── Body (typed lines) ────────────────────────────────────── */
.terminal__body {
  padding: 22px 22px 20px;
  font-family: var(--vp-font-family-mono);
  font-size: 14.5px;
  line-height: 1.75;
  /* 预留峰值高度 = 12 条脚本 + 1 收尾 prompt = 13 行,从开打就占满,避免边打字边撑高抖一下 */
  min-height: calc(23em + 42px);
}

.ln {
  white-space: pre;        /* 保留代码缩进 */
  min-height: 1.75em;
}

.ln--code { color: #cbd5e1; }
.ln--cmd { color: #42a5f5; }     /* shell prompt */
.ln--ok { color: #009688; }      /* 双跑通过 / brand teal */
.ln--save { color: #fbbf24; }    /* 保存提示 */
.ln--out { color: #e2e8f0; }     /* 程序输出 */
.ln--prompt { color: #009688; }

.cursor {
  display: inline-block;
  margin-left: 2px;
  color: #009688;
  animation: blink 1.05s step-end infinite;
}

/* ── Animations ────────────────────────────────────────────── */
@keyframes blink {
  0%, 50% { opacity: 1; }
  50.01%, 100% { opacity: 0; }
}

@keyframes terminal-glow {
  0%, 100% {
    box-shadow:
      0 22px 56px rgba(15, 52, 96, 0.4),
      0 6px 14px rgba(0, 0, 0, 0.22),
      0 0 0 0 rgba(99, 102, 241, 0);
  }
  50% {
    box-shadow:
      0 22px 56px rgba(15, 52, 96, 0.45),
      0 6px 14px rgba(0, 0, 0, 0.25),
      0 0 26px 3px rgba(99, 102, 241, 0.24);
  }
}

@keyframes hero-fade-up {
  from { opacity: 0; transform: translateY(22px); }
  to   { opacity: 1; transform: translateY(0); }
}

@media (prefers-reduced-motion: reduce) {
  .hero-visual,
  .terminal,
  .cursor {
    animation: none !important;
  }
}

@media (max-width: 639px) {
  .hero-visual {
    /* 手机端父容器(.image-container)按内容撑开,直接按视口约束。 */
    max-width: calc(100vw - 48px);
  }
  .terminal__body {
    font-size: 12px;
    padding: 16px 16px 14px;
    min-height: calc(23em + 30px);
  }
}
</style>
