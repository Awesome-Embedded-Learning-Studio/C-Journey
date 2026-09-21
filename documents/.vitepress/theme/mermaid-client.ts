import { nextTick, onMounted } from 'vue'
import { subscribeAfterRouteChange } from './router-hooks'
import { openMermaidLightbox } from './mermaid-lightbox'

// 本地打包 mermaid(dynamic import,客户端懒加载,SSR 不引入)。
// 不走 CDN —— 避免 Edge Tracking Prevention 把跨域 CDN 脚本当 tracker 拦截,
// 也避开网络可达性问题。Vite 会把 mermaid 自动 code-split 到独立 chunk(故 config 里放宽 chunkSizeWarningLimit)。
// 配色呼应 C-Journey 锈橙主题:骨白底 / 锈橙强调 / 深炭文;暗色暖炭底 / 朱橙强调 / 暖灰文。
// (对齐 TAMCPP:2026-09 起 maximize 按钮 + 全屏 lightbox;路由钩子走 router-hooks 订阅器。)

let mermaidApi: any = null
let initialized = false

// 锈橙主题 themeVariables(呼应 custom.css :root)
const LIGHT_VARS = {
  fontSize: '15px',
  primaryColor: '#F7F3EC',
  primaryTextColor: '#2A2420',
  primaryBorderColor: '#C2410C',
  lineColor: '#8A7E72',
  secondaryColor: '#EFE8DC',
  tertiaryColor: '#FBF8F2',
  tertiaryBorderColor: '#D6CDBF',
  background: '#F7F3EC',
  mainBkg: '#F7F3EC',
  altBackground: '#EFE8DC',
  clusterBkg: '#EFE8DC',
  clusterBorder: '#D6CDBF',
}

const DARK_VARS = {
  fontSize: '15px',
  primaryColor: '#1F1812',
  primaryTextColor: '#E8D8CA',
  primaryBorderColor: '#FB7228',
  lineColor: '#7F756C',
  secondaryColor: '#241C15',
  tertiaryColor: '#17120E',
  tertiaryBorderColor: '#3D3225',
  background: '#17120E',
  mainBkg: '#1F1812',
  altBackground: '#241C15',
  clusterBkg: '#1F1812',
  clusterBorder: '#3D3225',
}

function isDark() {
  return typeof document !== 'undefined'
    && document.documentElement.classList.contains('dark')
}

async function ensureMermaid(): Promise<any> {
  if (mermaidApi) return mermaidApi
  mermaidApi = (await import('mermaid')).default
  return mermaidApi
}

async function initIfNeed() {
  const m = await ensureMermaid()
  if (initialized) return
  m.initialize({
    startOnLoad: false,
    securityLevel: 'loose',
    theme: 'base',
    flowchart: {
      htmlLabels: true,
      nodeSpacing: 50,
      rankSpacing: 50,
      padding: 15,
    },
    themeVariables: isDark() ? DARK_VARS : LIGHT_VARS,
  })
  initialized = true
}

async function renderMermaidDiagrams() {
  if (typeof window === 'undefined') return

  await initIfNeed()
  await nextTick()
  await new Promise<void>((r) => requestAnimationFrame(() => r()))

  const nodes = Array.from(
    document.querySelectorAll<HTMLElement>('.mermaid-diagram[data-rendered="false"]')
  )

  for (let i = 0; i < nodes.length; i++) {
    const el = nodes[i]
    const raw = el.dataset.mermaid
    if (!raw) continue

    const source = decodeURIComponent(raw)
    const id = `mermaid-${Date.now()}-${i}-${Math.random().toString(36).slice(2, 8)}`

    try {
      const m = await ensureMermaid()
      const { svg } = await m.render(id, source)
      el.innerHTML = svg
      el.dataset.rendered = 'true'
      attachMaximize(el, source)
    } catch (e) {
      el.dataset.rendered = 'error'
      el.innerHTML = `<pre class="mermaid-error">${escapeHtml(source)}</pre>`
      console.error('[mermaid] render failed for:\n' + source, e)
    }
  }
}

function escapeHtml(s: string) {
  return s.replaceAll('&', '&amp;').replaceAll('<', '&lt;').replaceAll('>', '&gt;')
    .replaceAll('"', '&quot;').replaceAll("'", '&#39;')
}

// ── maximize 按钮:每张图都挂(跟 GitHub 一样,所有图都可缩放),点开进全屏模态 ──

// Feather maximize-2 图标(四角向外箭头),currentColor 随主题。
const MAXIMIZE_ICON =
  '<svg viewBox="0 0 24 24" width="15" height="15" fill="none" stroke="currentColor" ' +
  'stroke-width="2" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true">' +
  '<polyline points="15 3 21 3 21 9"/><polyline points="9 21 3 21 3 15"/>' +
  '<line x1="21" y1="3" x2="14" y2="10"/><line x1="3" y1="21" x2="10" y2="14"/></svg>'

function attachMaximize(el: HTMLElement, source: string) {
  const svg = el.querySelector('svg')
  if (!svg) return
  el.classList.add('mermaid-diagram--zoomable')

  const btn = document.createElement('button')
  btn.type = 'button'
  btn.className = 'mermaid-maximize-btn'
  btn.setAttribute('aria-label', '放大查看图表')
  btn.title = '放大查看图表'
  btn.innerHTML = MAXIMIZE_ICON
  btn.addEventListener('click', () => {
    openMermaidLightbox({ svg, source, trigger: btn })
  })
  el.appendChild(btn)
}

export function setupMermaid() {
  // 用订阅器而非直接赋值 router.onAfterRouteChange:后者是单值属性,
  // 会被 ReadingProgress 等组件覆盖,导致 SPA 跳转后 mermaid 不渲染。
  // .catch 治「静默失败」:之前 onMounted 调用没接住 reject,加载失败时图直接消失无痕。
  onMounted(() => renderMermaidDiagrams().catch((e) => console.error('[mermaid] onMounted 渲染失败', e)))
  subscribeAfterRouteChange(() => renderMermaidDiagrams().catch((e) => console.error('[mermaid] 路由切换渲染失败', e)))
}
