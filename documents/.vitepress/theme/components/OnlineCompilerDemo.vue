<template>
  <section ref="rootElement" class="online-compiler-demo">
    <div class="online-compiler-demo__header">
      <div>
        <p class="online-compiler-demo__eyebrow">Compiler Explorer</p>
        <h3>{{ title }}</h3>
        <p v-if="description" class="online-compiler-demo__description">
          {{ description }}
        </p>
      </div>
      <a
        v-if="sourcePath"
        class="online-compiler-demo__source"
        :href="sourceUrl"
        target="_blank"
        rel="noreferrer"
      >
        {{ sourcePath }}
      </a>
    </div>

    <!-- 默认态:只读源码 + "动手试一试" CTA。源码占满正文宽,不再两栏挤压 -->
    <div class="online-compiler-demo__inline">
      <div class="online-compiler-demo__source-view">
        <div
          v-if="highlightedHtml"
          class="online-compiler-demo__source-highlight"
          v-html="highlightedHtml"
        />
        <pre v-else class="online-compiler-demo__source-code"><code>{{ source }}</code></pre>
      </div>

      <p
        v-if="sourceLoadState === 'loading' || sourceLoadState === 'error'"
        class="online-compiler-demo__source-hint"
      >
        {{ sourceLoadState === 'error' ? '源码加载失败，可点上方源码链接查看' : '加载源码中…' }}
      </p>

      <button class="online-compiler-demo__cta" type="button" @click="openModal">
        <span class="online-compiler-demo__cta-text">动手试一试</span>
        <span class="online-compiler-demo__cta-arrow" aria-hidden="true">→</span>
      </button>
    </div>

    <!-- 浮层态:Teleport 到 body,灰色遮罩 + 比页面小一圈的卡片,完整 IDE -->
    <Teleport to="body">
      <div
        v-if="modalOpen"
        class="online-compiler-demo__overlay"
        @click.self="closeModal"
      >
        <div
          class="online-compiler-demo__modal"
          role="dialog"
          aria-modal="true"
          aria-labelledby="ocd-modal-title"
        >
          <div class="online-compiler-demo__modal-header">
            <div>
              <p class="online-compiler-demo__eyebrow">Compiler Explorer</p>
              <strong id="ocd-modal-title">{{ title }}</strong>
            </div>
            <button
              class="online-compiler-demo__modal-close"
              type="button"
              aria-label="关闭"
              @click="closeModal"
            >✕</button>
          </div>

          <div class="online-compiler-demo__split">
            <div class="online-compiler-demo__source-pane">
              <div v-if="!editorOpen" class="online-compiler-demo__source-view">
                <div
                  v-if="highlightedHtml"
                  class="online-compiler-demo__source-highlight"
                  v-html="highlightedHtml"
                />
                <pre v-else class="online-compiler-demo__source-code"><code>{{ source }}</code></pre>
              </div>
              <div v-else class="online-compiler-demo__editor-wrap">
                <div
                  ref="editorBackdropRef"
                  class="online-compiler-demo__source-highlight online-compiler-demo__editor-backdrop"
                  v-html="editorHighlightedHtml"
                />
                <textarea
                  ref="editorTextareaRef"
                  v-model="editorSource"
                  class="online-compiler-demo__textarea online-compiler-demo__editor-textarea"
                  spellcheck="false"
                  autocomplete="off"
                  autocorrect="off"
                  autocapitalize="off"
                  wrap="off"
                  @scroll="onEditorScroll"
                />
              </div>

              <p
                v-if="!editorOpen && (sourceLoadState === 'loading' || sourceLoadState === 'error')"
                class="online-compiler-demo__source-hint"
              >
                {{ sourceLoadState === 'error' ? '源码加载失败，可点上方源码链接查看' : '加载源码中…' }}
              </p>

              <div v-if="editorOpen" class="online-compiler-demo__editor-actions">
                <button
                  class="online-compiler-demo__button online-compiler-demo__button--secondary"
                  type="button"
                  :disabled="Boolean(activeAction)"
                  @click="resetEditor"
                >
                  还原源码
                </button>
                <button
                  class="online-compiler-demo__button online-compiler-demo__button--secondary"
                  type="button"
                  @click="closeEditor"
                >
                  保存并收起
                </button>
              </div>
            </div>

            <div class="online-compiler-demo__control-pane">
              <div class="online-compiler-demo__meta">
                <span v-for="action in actions" :key="action.id">
                  {{ action.label }}: {{ action.compiler }} {{ action.options }}
                </span>
              </div>

              <div class="online-compiler-demo__actions">
                <button
                  v-for="action in actions"
                  :key="action.id"
                  class="online-compiler-demo__button"
                  type="button"
                  :disabled="Boolean(activeAction)"
                  @click="compile(action)"
                >
                  <span v-if="activeAction === action.id">处理中...</span>
                  <span v-else>{{ action.label }}</span>
                </button>
                <button
                  v-if="actions.length"
                  class="online-compiler-demo__button online-compiler-demo__button--secondary"
                  type="button"
                  :disabled="Boolean(activeAction)"
                  @click="editorOpen ? closeEditor() : openEditor()"
                >
                  {{ editorOpen ? '只读预览' : '编辑源码' }}
                </button>
                <button
                  class="online-compiler-demo__button online-compiler-demo__button--secondary"
                  type="button"
                  :disabled="Boolean(activeAction)"
                  @click="optionsOpen = !optionsOpen"
                >
                  编译条件
                </button>
                <button
                  class="online-compiler-demo__button online-compiler-demo__button--secondary"
                  type="button"
                  :disabled="Boolean(activeAction)"
                  @click="openGodbolt"
                >
                  打开 Godbolt
                </button>
              </div>

              <div v-if="optionsOpen && actions.length" class="online-compiler-demo__options">
                <div class="online-compiler-demo__options-header">
                  <strong>编译条件</strong>
                  <span>运行、汇编和 Godbolt 外链都会使用当前设置</span>
                </div>
                <div class="online-compiler-demo__option-list">
                  <label
                    v-for="action in actions"
                    :key="action.id"
                    class="online-compiler-demo__option-row"
                  >
                    <span class="online-compiler-demo__option-label">{{ action.label }}</span>
                    <input
                      v-model.trim="actionSettings[action.id].compiler"
                      class="online-compiler-demo__input"
                      type="text"
                      autocomplete="off"
                      spellcheck="false"
                      placeholder="compiler id"
                    />
                    <textarea
                      v-model="actionSettings[action.id].options"
                      class="online-compiler-demo__options-textarea"
                      rows="2"
                      autocomplete="off"
                      spellcheck="false"
                      placeholder="compiler options"
                    />
                  </label>
                </div>
                <div class="online-compiler-demo__editor-actions">
                  <button
                    class="online-compiler-demo__button online-compiler-demo__button--secondary"
                    type="button"
                    :disabled="Boolean(activeAction)"
                    @click="resetCompileOptions"
                  >
                    还原编译条件
                  </button>
                  <button
                    class="online-compiler-demo__button online-compiler-demo__button--secondary"
                    type="button"
                    @click="optionsOpen = false"
                  >
                    收起编译条件
                  </button>
                </div>
              </div>

              <div v-if="result" class="online-compiler-demo__result">
                <div class="online-compiler-demo__result-header">
                  <strong>{{ result.title }}</strong>
                  <span>{{ result.compiler }} {{ result.options }}</span>
                </div>
                <pre><code>{{ result.text }}</code></pre>
              </div>
              <!-- 编译进行中(activeAction 置位、result 尚未返回):shimmer 骨架,避免结果区整块消失干等 -->
              <div v-else-if="activeAction" class="online-compiler-demo__result online-compiler-demo__result--skeleton">
                <div class="online-compiler-demo__result-header">
                  <span class="skel skel--title" />
                  <span class="skel skel--meta" />
                </div>
                <div class="online-compiler-demo__skel-lines">
                  <span
                    v-for="(w, i) in [92, 78, 86, 58, 90, 72, 95, 48]"
                    :key="i"
                    class="skel skel--line"
                    :style="{ width: w + '%' }"
                  />
                </div>
              </div>
            </div>
          </div>
        </div>
      </div>
    </Teleport>

    <p v-if="error" class="online-compiler-demo__error">
      {{ error }}
    </p>

    <noscript>
      <p class="online-compiler-demo__noscript">
        需要启用 JavaScript 才能运行示例或请求汇编输出；源码仍可通过上方链接查看。
      </p>
    </noscript>
  </section>
</template>

<script setup lang="ts">
import { withBase } from 'vitepress'
import { computed, onBeforeUnmount, onMounted, reactive, ref, useSlots, watch } from 'vue'

import { highlightC } from '../shiki'

type ActionId = 'run' | 'x86-asm'

interface DemoAction {
  id: ActionId
  label: string
  compiler: string
  options: string
  executorRequest: boolean
}

interface CompileResult {
  title: string
  compiler: string
  options: string
  text: string
}

interface ActionSetting {
  compiler: string
  options: string
}

/*
 * 三种喂源码方式(优先级从高到低):
 *   1. 默认插槽 —— .md 里 <OnlineCompilerDemo ...>C 代码...</OnlineCompilerDemo>,
 *      写多行最自然、不用管转义,VitePress 把标签间的文本当插槽内容传进来。推荐。
 *   2. code prop —— 内联字符串(:code='`...`' 反引号模板字符串),适合代码里没 / 的短片段。
 *   3. sourcePath —— 指向仓库里的 .c 文件(以后 push 了、想用单独 .c 文件时用)。
 * 都没给就用内置默认 demo。
 */
const props = withDefaults(defineProps<{
  title: string
  code?: string
  sourcePath?: string
  description?: string
  allowRun?: boolean
  allowX86Asm?: boolean
  runCompiler?: string
  runOptions?: string
  x86Compiler?: string
  x86Options?: string
  branch?: string
  rawBase?: string
}>(), {
  code: '',
  sourcePath: '',
  description: '',
  allowRun: false,
  allowX86Asm: false,
  // godbolt 的 C 编译器 ID:x86-64 gcc 16.1(cg161)——按 .c 后缀走 C。
  // 实测 -std=c11 -O2 编 .c 文件,godbolt 走 C 前端(不是 C++),与课程一致。
  runCompiler: 'cg161',
  // 运行默认带 sanitizer:UBSan 抓有符号溢出/越界等 UB,ASan 抓内存错。
  // 读者改代码点运行,能当场看到 sanitizer 报警——呼应课程「UB 当场抓」主线。
  runOptions: '-std=c11 -Wall -Wextra -O2 -fsanitize=address,undefined',
  x86Compiler: 'cg161',
  x86Options: '-std=c11 -O2',
  branch: 'main',
  rawBase: 'https://raw.githubusercontent.com/Awesome-Embedded-Learning-Studio/C-Journey',
})

// 内置默认 demo:仅当既没传 code、也没传 sourcePath 时兜底用。
const DEFAULT_DEMO_SOURCE = `#include <stdio.h>

int main(void) {
    int n = 10;
    int* p = &n;   /* p 指向 n */
    *p = 42;       /* 通过指针改 n */
    printf("n = %d\\n", n);
    return 0;
}
`

const source = ref('')
const activeAction = ref<ActionId | 'godbolt' | 'source' | ''>('')
const error = ref('')
const result = ref<CompileResult | null>(null)
const editorOpen = ref(false)
const highlightedHtml = ref('')
// 编辑态高亮(overlay 技巧):编辑框背后垫一层 shiki 高亮,文字透明,光标/选区由 textarea 接管
const editorHighlightedHtml = ref('')
const editorBackdropRef = ref<HTMLElement | null>(null)
const editorTextareaRef = ref<HTMLTextAreaElement | null>(null)
const optionsOpen = ref(false)
const editorSource = ref('')
const sourceLoadState = ref<'idle' | 'loading' | 'loaded' | 'error'>('idle')
const rootElement = ref<HTMLElement | null>(null)
const actionSettings = reactive<Record<ActionId, ActionSetting>>({
  run: { compiler: props.runCompiler, options: props.runOptions },
  'x86-asm': { compiler: props.x86Compiler, options: props.x86Options },
})

const slots = useSlots()

const hasInlineCode = computed(() => Boolean(props.code))
const hasSourcePath = computed(() => Boolean(props.sourcePath))

// 默认插槽:<OnlineCompilerDemo ...>多行 C 代码</OnlineCompilerDemo>。
// slots.default 是个渲染函数,调它拿到 vnode 数组;把文本节点拼成字符串。
// VitePress 把标签间的纯文本原样传进来(含换行),所以这是 .md 里写多行 C 最干净的方式。
const slotSource = computed(() => {
  if (!slots.default) return ''
  const extract = (vnode: any): string => {
    if (vnode == null) return ''
    if (typeof vnode === 'string') return vnode
    if (typeof vnode === 'number') return String(vnode)
    if (Array.isArray(vnode)) return vnode.map(extract).join('')
    if (typeof vnode === 'object') {
      if (typeof vnode.children === 'string') return vnode.children
      if (Array.isArray(vnode.children)) return vnode.children.map(extract).join('')
    }
    return ''
  }
  return extract(slots.default()).trim()
})
const hasSlot = computed(() => Boolean(slotSource.value))

const normalizedSourcePath = computed(() => props.sourcePath.replace(/^\/+/, ''))
const sourceUrl = computed(() => withBase(`/${normalizedSourcePath.value}`))
const rawSourceUrl = computed(() => `${props.rawBase}/${props.branch}/${normalizedSourcePath.value}`)

// 只读源码区做 shiki 高亮:源码一变(内联就绪 / 懒加载完成)就重新高亮。
// 高亮是异步的——未就绪时 template 先用纯文本 fallback,就绪后替换为着色 HTML。
watch(source, async (code) => {
  highlightedHtml.value = ''
  if (!code) return
  try {
    highlightedHtml.value = await highlightC(code)
  } catch {
    highlightedHtml.value = ''
  }
})

// 编辑态:边打边重新高亮(overlay backdrop 跟随 editorSource)
watch(editorSource, async (code) => {
  editorHighlightedHtml.value = ''
  if (!code) return
  try {
    editorHighlightedHtml.value = await highlightC(code)
  } catch {
    editorHighlightedHtml.value = ''
  }
})

// textarea 与 backdrop 滚动同步(两层重叠,滚动必须一致,否则错位)
function onEditorScroll() {
  const ta = editorTextareaRef.value
  const bd = editorBackdropRef.value
  if (!ta || !bd) return
  bd.scrollTop = ta.scrollTop
  bd.scrollLeft = ta.scrollLeft
}

const actions = computed<DemoAction[]>(() => {
  const available: DemoAction[] = []
  if (props.allowRun) {
    available.push({
      id: 'run',
      label: '运行',
      compiler: actionSettings.run.compiler,
      options: actionSettings.run.options,
      executorRequest: true,
    })
  }
  if (props.allowX86Asm) {
    available.push({
      id: 'x86-asm',
      label: '看 x86-64 汇编',
      compiler: actionSettings['x86-asm'].compiler,
      options: actionSettings['x86-asm'].options,
      executorRequest: false,
    })
  }
  return available
})

async function loadSource(): Promise<string> {
  if (editorOpen.value) {
    return editorSource.value
  }
  if (source.value) return source.value

  // 优先级 1:默认插槽(.md 里 <OnlineCompilerDemo>多行 C 代码</OnlineCompilerDemo>)
  if (hasSlot.value) {
    source.value = slotSource.value
    sourceLoadState.value = 'loaded'
    return source.value
  }

  // 优先级 2:内联 code prop(:code='`...`')
  if (hasInlineCode.value) {
    source.value = props.code
    sourceLoadState.value = 'loaded'
    return source.value
  }

  // 优先级 3:sourcePath(本地 withBase → GitHub raw)
  if (hasSourcePath.value) {
    sourceLoadState.value = 'loading'
    const localSource = await fetchText(sourceUrl.value)
    if (localSource.ok) {
      source.value = localSource.text
      sourceLoadState.value = 'loaded'
      return source.value
    }
    const rawSource = await fetchText(rawSourceUrl.value)
    if (rawSource.ok) {
      source.value = rawSource.text
      sourceLoadState.value = 'loaded'
      return source.value
    }
    sourceLoadState.value = 'error'
    throw new Error(`无法读取源码(本地: ${localSource.status}; GitHub raw: ${rawSource.status})`)
  }

  // 优先级 4:内置默认 demo
  source.value = DEFAULT_DEMO_SOURCE
  sourceLoadState.value = 'loaded'
  return source.value
}

async function fetchText(url: string): Promise<{ ok: true; text: string } | { ok: false; status: string }> {
  try {
    const response = await fetch(url)
    if (!response.ok) {
      return { ok: false, status: `${response.status} ${response.statusText}`.trim() }
    }
    return { ok: true, text: await response.text() }
  } catch (err) {
    return { ok: false, status: err instanceof Error ? err.message : String(err) }
  }
}

async function openEditor(): Promise<void> {
  activeAction.value = 'source'
  error.value = ''

  try {
    editorSource.value = await loadSource()
    editorOpen.value = true
  } catch (err) {
    error.value = err instanceof Error ? err.message : String(err)
  } finally {
    activeAction.value = ''
  }
}

function closeEditor(): void {
  // 收起编辑器前把编辑内容回写到源码缓存:只读预览就显示改过的代码,
  // 后续运行 / 汇编 / Godbolt 也会使用这份内容(loadSource 命中缓存即可)。
  source.value = editorSource.value
  editorOpen.value = false
}

// ── 浮层(模态):默认只显源码,点"动手试一试"弹完整 IDE ──
const modalOpen = ref(false)

function onModalEsc(e: KeyboardEvent) {
  if (e.key === 'Escape' && modalOpen.value) closeModal()
}

async function openModal(): Promise<void> {
  modalOpen.value = true
  document.body.style.overflow = 'hidden'
  window.addEventListener('keydown', onModalEsc)
  await openEditor() // 加载可编辑源码、置 editorOpen=true
}

function closeModal(): void {
  // closeEditor 把编辑内容回写源码缓存 + editorOpen=false
  closeEditor()
  modalOpen.value = false
  document.body.style.overflow = ''
  window.removeEventListener('keydown', onModalEsc)
}

onBeforeUnmount(() => {
  window.removeEventListener('keydown', onModalEsc)
  if (modalOpen.value) document.body.style.overflow = ''
})

async function resetEditor(): Promise<void> {
  activeAction.value = 'source'
  error.value = ''

  try {
    // 还原:清缓存后重新 load(slot/内联 code 会重新填回去;sourcePath 会重新 fetch)
    if (!hasSlot.value && !hasInlineCode.value) {
      source.value = ''
    }
    editorSource.value = await loadSource()
  } catch (err) {
    error.value = err instanceof Error ? err.message : String(err)
  } finally {
    activeAction.value = ''
  }
}

function resetCompileOptions(): void {
  actionSettings.run.compiler = props.runCompiler
  actionSettings.run.options = props.runOptions
  actionSettings['x86-asm'].compiler = props.x86Compiler
  actionSettings['x86-asm'].options = props.x86Options
}

function linesToText(value: unknown): string {
  if (!value) return ''
  if (typeof value === 'string') return stripAnsi(value)
  if (Array.isArray(value)) {
    return value.map((line) => {
      if (typeof line === 'string') return stripAnsi(line)
      if (line && typeof line === 'object' && 'text' in line) {
        return stripAnsi(String((line as { text: unknown }).text ?? ''))
      }
      return stripAnsi(String(line ?? ''))
    }).join('\n')
  }
  return stripAnsi(String(value))
}

function stripAnsi(value: string): string {
  return value.replace(/\x1b\[[0-?]*[ -/]*[@-~]/g, '')
}

function extractExecutionText(payload: any): string {
  const exec = payload.execResult ?? payload.executionResult ?? payload
  // godbolt executor 响应常把程序输出同时放在 execResult 和顶层(此时 exec===payload),
  // 每路只取第一份非空的,避免把同一份输出拼两遍。
  if (isCompilationFailure(payload, linesToText(payload.asm))) {
    const diag = gatherDiagnostics(payload)
    return diag
      ? `❌ 编译失败：\n${diag}`
      : '❌ 编译失败，但 Compiler Explorer 没有返回诊断信息。检查源码语法、编译器 id 与参数，或点「打开 Godbolt」看完整输出。'
  }
  const out = linesToText(exec.stdout) || linesToText(payload.stdout) || linesToText(payload.buildResult?.stdout)
  const err = linesToText(exec.stderr) || linesToText(payload.stderr) || linesToText(payload.buildResult?.stderr)
  const chunks = [out, err].filter(Boolean)

  if (exec.code !== undefined && exec.code !== 0) chunks.push(`exit code: ${exec.code}`)
  else if (payload.code !== undefined && payload.code !== 0) chunks.push(`exit code: ${payload.code}`)
  return chunks.join('\n').trim() || '(程序无输出)'
}

// 收集编译/运行诊断(execResult → 顶层 → buildResult,stderr 优先再 stdout),每路第一份非空避免重复
function gatherDiagnostics(payload: any): string {
  const exec = payload.execResult ?? payload.executionResult ?? payload
  const err = linesToText(exec.stderr) || linesToText(payload.stderr) || linesToText(payload.buildResult?.stderr)
  const out = linesToText(exec.stdout) || linesToText(payload.stdout) || linesToText(payload.buildResult?.stdout)
  return [err, out].filter(Boolean).join('\n')
}

function extractAsmText(payload: any): string {
  const asm = linesToText(payload.asm)
  if (isCompilationFailure(payload, asm)) {
    // 顶层与 buildResult 可能同源,每路只取第一份非空。
    const err = linesToText(payload.stderr) || linesToText(payload.buildResult?.stderr)
    const out = linesToText(payload.stdout) || linesToText(payload.buildResult?.stdout)
    const diag = [err, out].filter(Boolean).join('\n')
    return diag
      ? `❌ 编译失败：\n${diag}`
      : '❌ 编译失败，但 Compiler Explorer 没有返回诊断信息。检查源码语法、编译器 id 与参数，或点「打开 Godbolt」看完整输出。'
  }
  return (asm || 'Compiler Explorer 没有返回可显示的汇编输出。').trim()
}

function isCompilationFailure(payload: any, asm: string): boolean {
  return payload.code !== undefined && payload.code !== 0
    || payload.buildResult?.code !== undefined && payload.buildResult.code !== 0
    || asm.includes('<Compilation failed>')
}

async function compile(action: DemoAction): Promise<void> {
  activeAction.value = action.id
  error.value = ''
  result.value = null

  try {
    if (!action.compiler.trim()) {
      throw new Error(`${action.label} 缺少 compiler id`)
    }

    const currentSource = editorOpen.value ? editorSource.value : await loadSource()
    const response = await fetch(`https://godbolt.org/api/compiler/${action.compiler}/compile`, {
      method: 'POST',
      headers: {
        Accept: 'application/json',
        'Content-Type': 'application/json',
      },
      body: JSON.stringify({
        source: currentSource,
        options: {
          userArguments: action.options,
          compilerOptions: {
            executorRequest: action.executorRequest,
          },
          filters: {
            binary: false,
            commentOnly: true,
            demangle: true,
            directives: true,
            execute: action.executorRequest,
            intel: action.id === 'x86-asm',
            labels: true,
            libraryCode: false,
            trim: false,
          },
          executeParameters: {
            args: '',
            stdin: '',
          },
        },
      }),
    })

    if (!response.ok) {
      throw new Error(`Compiler Explorer 请求失败 (${response.status} ${response.statusText})`)
    }

    const payload = await response.json()
    result.value = {
      title: action.label,
      compiler: action.compiler,
      options: action.options,
      text: action.executorRequest ? extractExecutionText(payload) : extractAsmText(payload),
    }
  } catch (err) {
    error.value = err instanceof Error ? err.message : String(err)
  } finally {
    activeAction.value = ''
  }
}

async function openGodbolt(): Promise<void> {
  activeAction.value = 'godbolt'
  error.value = ''

  try {
    const currentSource = editorOpen.value ? editorSource.value : await loadSource()
    const state = buildClientState(currentSource)
    const encoded = encodeURIComponent(toBase64(JSON.stringify(state)))
    window.open(`https://godbolt.org/clientstate/${encoded}`, '_blank', 'noopener,noreferrer')
  } catch (err) {
    error.value = err instanceof Error ? err.message : String(err)
  } finally {
    activeAction.value = ''
  }
}

function buildClientState(currentSource: string) {
  const compilers = actions.value
    .filter((action) => !action.executorRequest)
    .map((action, index) => ({
      id: index + 1,
      compiler: action.compiler,
      options: action.options,
      filters: {
        binary: false,
        commentOnly: true,
        demangle: true,
        directives: true,
        intel: action.id === 'x86-asm',
        labels: true,
        libraryCode: false,
        trim: false,
      },
    }))

  const executors = actions.value
    .filter((action) => action.executorRequest)
    .map((action, index) => ({
      id: index + 1,
      compiler: action.compiler,
      options: action.options,
      arguments: '',
      stdin: '',
    }))

  return {
    sessions: [{
      id: 1,
      language: 'c',
      source: currentSource,
      filename: normalizedSourcePath.value.split('/').pop() || 'demo.c',
      compilers,
      executors,
    }],
  }
}

function toBase64(value: string): string {
  const bytes = new TextEncoder().encode(value)
  let binary = ''
  const chunkSize = 0x8000
  for (let i = 0; i < bytes.length; i += chunkSize) {
    const chunk = bytes.subarray(i, i + chunkSize)
    binary += String.fromCharCode(...chunk)
  }
  return btoa(binary)
}

// —— 懒加载:进入视口即加载主源码(仅 sourcePath 模式有网络开销;插槽/内联 code 立即可用) ——
async function ensureSourceLoaded(): Promise<void> {
  if (source.value) {
    sourceLoadState.value = 'loaded'
    return
  }
  if (hasSlot.value || hasInlineCode.value || !hasSourcePath.value) {
    // 插槽 / 内联 code / 默认 demo:无网络开销,直接 load
    try {
      await loadSource()
    } catch {
      sourceLoadState.value = 'error'
    }
    return
  }
  sourceLoadState.value = 'loading'
  try {
    await loadSource()
  } catch {
    sourceLoadState.value = 'error'
  }
}

let lazyObserver: IntersectionObserver | null = null

onMounted(() => {
  const target = rootElement.value
  if (!target) return

  const trigger = (entries: IntersectionObserverEntry[]) => {
    if (entries.some((entry) => entry.isIntersecting)) {
      lazyObserver?.disconnect()
      lazyObserver = null
      ensureSourceLoaded()
    }
  }

  if (typeof IntersectionObserver === 'undefined') {
    ensureSourceLoaded()
    return
  }

  lazyObserver = new IntersectionObserver(trigger, { rootMargin: '256px' })
  lazyObserver.observe(target)
})

onBeforeUnmount(() => {
  lazyObserver?.disconnect()
  lazyObserver = null
})
</script>
