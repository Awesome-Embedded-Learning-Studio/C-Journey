import { defineConfig } from 'vitepress'
import fs from 'node:fs'
import path from 'node:path'
import { fileURLToPath } from 'node:url'
import { codeFoldPlugin } from './plugins/code-fold-plugin'
import { kbdPlugin } from './plugins/kbd-plugin'
import { mermaidPlugin } from './plugins/mermaid-plugin'
import { cppTemplateEscapePlugin } from './plugins/escape-cpp-templates'
import { viteCppEscape } from './plugins/vite-escape-cpp'

const __dirname = path.dirname(fileURLToPath(import.meta.url))
const docsRoot = path.resolve(__dirname, '..') /* documents/ */

/* 主线六阶段(0-5 已重写上线):目录名 + 显示名。侧栏自动读每阶段 NN-*.md、按 order 排、用 frontmatter title。
   阶段 6/7/进阶尚未重写,留在 srcExclude 不上线(见下)。*/
const stages = [
    { dir: '00-dev-environment', name: '阶段 0 · 开发环境与编译' },
    { dir: '01-c-basics', name: '阶段 1 · C 语言基底' },
    { dir: '02-pointers-memory', name: '阶段 2 · 指针与内存' },
    { dir: '03-data-structures', name: '阶段 3 · 数据结构与算法' },
    { dir: '04-engineering', name: '阶段 4 · 工程化与质量门' },
    { dir: '05-system-programming', name: '阶段 5 · 系统编程' },
]

function readTitle(filePath: string): string {
    const content = fs.readFileSync(filePath, 'utf8')
    const m = content.match(/^title:\s*"?(.+?)"?\s*$/m)
    return m ? m[1] : path.basename(filePath, '.md')
}

function buildSidebar() {
    return stages.map((stage) => {
        const stageDir = path.join(docsRoot, stage.dir)
        const files = fs
            .readdirSync(stageDir)
            .filter((f) => /^\d+-.*\.md$/.test(f))
            .sort((a, b) => parseInt(a, 10) - parseInt(b, 10))
        return {
            text: stage.name,
            collapsed: true, /* 默认折叠阶段、点击展开;当前阅读章所在阶段 VitePress 自动展开 */
            items: files.map((f) => ({
                text: readTitle(path.join(stageDir, f)),
                link: `/${stage.dir}/${f.replace(/\.md$/, '')}`,
            })),
        }
    })
}

export default defineConfig({
    title: 'C-Journey',
    description: '一份用 C 游历计算机世界的教程:主机系统全程真跑、引 ISO/IEC 9899;嵌入式浅尝指路,深做见 imx-forge',
    lang: 'zh-CN',
    lastUpdated: true,
    base: '/C-Journey/',
    cleanUrls: true,
    sitemap: {
        hostname: 'https://awesome-embedded-learning-studio.github.io/C-Journey',
    },
    /* 章节里有指向仓库根(scripts/、projects/、.clang-tidy 等)的相对链接 —— 它们在 GitHub
       仓库浏览时有效,但 Pages 站只 serve documents/,会 404。放行这类跳出 srcDir 的 ../../ 链接,
       保留站内链接的 dead-link 检查。(后续把这些改成 GitHub 绝对 URL 更彻底,见优化清单) */
    ignoreDeadLinks: [/\/\.\.\//],
    /* 阶段 6/7/进阶尚未重写,旧内容暂不上线;index.md/README 走首页 Hero 不参与侧栏 */
    srcExclude: [
        '06-embedded/**',
        '07-capstone/**',
        'advanced/**',
        '01-c-basics/index.md',
        'README.md',
    ],
    head: [
        /* favicon(锈橙 C) */
        ['link', { rel: 'icon', href: '/C-Journey/favicon.svg', type: 'image/svg+xml' }],
        /* theme-color 跟随系统亮/暗(锈橙主题:骨白 / 暖炭) */
        ['meta', { name: 'theme-color', media: '(prefers-color-scheme: light)', content: '#F7F3EC' }],
        ['meta', { name: 'theme-color', media: '(prefers-color-scheme: dark)', content: '#17120E' }],
        /* Open Graph / Twitter 社交分享(og:image 用栅格 PNG,平台兼容) */
        ['meta', { property: 'og:site_name', content: 'C-Journey' }],
        ['meta', { property: 'og:type', content: 'website' }],
        ['meta', { property: 'og:title', content: '用 C 游历计算机世界 · C-Journey' }],
        ['meta', { property: 'og:description', content: '用 C 游历计算机世界:主机系统全程真跑(gcc 16 + clang 22 + ASan/UBSan)、引 ISO/IEC 9899;嵌入式浅尝指路,深做见 imx-forge。' }],
        ['meta', { property: 'og:url', content: 'https://awesome-embedded-learning-studio.github.io/C-Journey/' }],
        ['meta', { property: 'og:image', content: 'https://awesome-embedded-learning-studio.github.io/C-Journey/og-image.png' }],
        ['meta', { property: 'og:locale', content: 'zh_CN' }],
        ['meta', { name: 'twitter:card', content: 'summary_large_image' }],
        ['meta', { name: 'twitter:image', content: 'https://awesome-embedded-learning-studio.github.io/C-Journey/og-image.png' }],
        /* 字号防闪:hydration 前读 localStorage('vp-font-size'),提前设 data-font-size。
           与 FontSizeSwitcher.vue 的 STORAGE_KEY 保持一致,缺省 normal。 */
        [
            'script',
            {},
            `(function(){try{var s=localStorage.getItem('vp-font-size')||'normal';if(s!=='xxsmall'&&s!=='small'&&s!=='normal'&&s!=='large'&&s!=='xxlarge'){s='normal';}document.documentElement.dataset.fontSize=s;}catch(e){}})()`,
        ],
    ],
    vite: {
        build: { chunkSizeWarningLimit: 5000 }, // mermaid 独立 chunk 较大,放宽告警阈
        plugins: [viteCppEscape()], // 第一道:Vite 预处理,把 prose 里的 <optimized out>/<stdio.h>/<main> 等转义,防 Vue 误当标签
    },
    markdown: {
        lineNumbers: true,
        theme: { light: 'github-light', dark: 'github-dark' },
        config(md) {
            cppTemplateEscapePlugin(md) // 第二道:markdown-it 渲染期再兜一次(双层保险)
            codeFoldPlugin(md)
            kbdPlugin(md)
            mermaidPlugin(md)
            // 内联反引号代码兜底加 v-pre:防 prose 里 `ci-${{ github.ref }}` 的 {{ }} 被 Vue
            // 当 mustache(CI/模板章节高频写 ${{ }} ;fenced 块 VitePress 已自动 v-pre,内联需自己加)
            const origInline = md.renderer.rules.code_inline
            md.renderer.rules.code_inline = function (...args) {
                const out = origInline
                    ? origInline.apply(md.renderer.rules, args)
                    : '<code>' + md.utils.escapeHtml(args[0][args[1]].content) + '</code>'
                return out.indexOf('v-pre') >= 0 ? out : out.replace(/^<code/, '<code v-pre')
            }
        },
    },
    vue: {
        template: {
            compilerOptions: {
                // 漏网之鱼:带 - 或 . 的标签当自定义元素放行,不告警
                // isCustomElement: (tag: string) => tag.includes('-') || tag.includes('.'),
            },
        },
    },
    themeConfig: {
        siteTitle: 'C-Journey',
        nav: [
            { text: '阶段 0 · 开发环境', link: '/00-dev-environment/01-toolchain-health-check' },
            { text: '阶段 1 · C 基底', link: '/01-c-basics/01-program-structure-and-compilation' },
            { text: '阶段 2 · 指针内存', link: '/02-pointers-memory/01-what-is-a-pointer' },
            { text: '阶段 3 · 数据结构', link: '/03-data-structures/01-singly-linked-list' },
            { text: '阶段 4 · 工程化', link: '/04-engineering/01-header-contracts' },
            { text: '阶段 5 · 系统编程', link: '/05-system-programming/01-file-io-and-fd' },
            { text: '更新日志', link: '/changelog/' },
        ],
        sidebar: buildSidebar(),
        search: {
            provider: 'local',
            options: {
                translations: {
                    button: { buttonText: '搜索', buttonAriaLabel: '搜索' },
                    modal: {
                        noResultsText: '无结果',
                        footer: { selectText: '选择', navigateText: '切换', closeText: '关闭' },
                    },
                },
            },
        },
        outline: { label: '本页内容', level: [2, 3] },
        docFooter: { prev: '上一页', next: '下一页' },
        lastUpdatedText: '最后更新',
        sidebarMenuLabel: '目录',
        returnToTopLabel: '回到顶部',
        darkModeSwitchLabel: '主题',
        socialLinks: [
            { icon: 'github', link: 'https://github.com/Awesome-Embedded-Learning-Studio/C-Journey' },
        ],
    },
})
