import type { PluginSimple } from 'markdown-it'

/**
 * 代码块语言标签文案化(移植自 TAMCPP code-label-plugin,C 适配)。
 * 不动 VitePress 的 copy/lang/pre 兄弟链,只改可见标签:
 *   c   → C      (语言名,配 article-code.css 左上角徽章)
 *   text → 终端   (本站约定:```text = $ 会话/工具输出的终端实录)
 * 其余语言(asm/makefile/cmake/yaml/bash...)保持原文。
 * 必须挂在 codeFoldPlugin 之后:折叠插件整段包 fence,本插件在最外层
 * 对最终 HTML 做字符串替换,折叠块内的语言徽章也能被替换到。
 */
export const codeLabelPlugin: PluginSimple = (md) => {
  const fence = md.renderer.rules.fence
  if (!fence) return
  md.renderer.rules.fence = (...args) => {
    const html = fence(...args)
    return html.replace(/(<span class="lang">)(c|text)(<\/span>)/g, (_, open, lang, close) => (
      open + (lang === 'c' ? 'C' : '终端') + close
    ))
  }
}
