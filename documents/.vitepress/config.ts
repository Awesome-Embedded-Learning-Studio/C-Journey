import { defineConfig } from 'vitepress'
import fs from 'node:fs'
import path from 'node:path'
import { fileURLToPath } from 'node:url'

const __dirname = path.dirname(fileURLToPath(import.meta.url))
const docsRoot = path.resolve(__dirname, '..') /* documents/ */

/* 四阶段:目录名 + 显示名。侧栏自动读每阶段 NN-*.md、按 order 排、用 frontmatter title */
const stages = [
    { dir: '00-dev-environment', name: '阶段 0 · 开发环境与编译' },
    { dir: '01-c-basics', name: '阶段 1 · C 语言基底' },
    { dir: '02-pointers-memory', name: '阶段 2 · 指针与内存' },
    { dir: '03-data-structures', name: '阶段 3 · 数据结构与算法' },
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
    description: '一份亲手踩坑、贴真实输出的纯 C 系统编程教程',
    lang: 'zh-CN',
    lastUpdated: true,
    cleanUrls: true,
    /* 旧内容不上线(待 rewrite 或待补新章) */
    srcExclude: [
        '04-engineering/**',
        '05-system-programming/**',
        '06-embedded/**',
        '07-capstone/**',
        'advanced/**',
        '01-c-basics/index.md',
        'README.md', /* 旧导航(指向 04-07 等未上线内容);首页用 index.md Hero */
    ],
    head: [
        /* 字号防闪:hydration 前读 localStorage('vp-font-size'),提前设 data-font-size。
           与 FontSizeSwitcher.vue 的 STORAGE_KEY 保持一致,缺省 normal。 */
        [
            'script',
            {},
            `(function(){try{var s=localStorage.getItem('vp-font-size')||'normal';if(s!=='xxsmall'&&s!=='small'&&s!=='normal'&&s!=='large'&&s!=='xxlarge'){s='normal';}document.documentElement.dataset.fontSize=s;}catch(e){}})()`,
        ],
    ],
    themeConfig: {
        siteTitle: 'C-Journey',
        nav: [
            { text: '阶段 0 · 开发环境', link: '/00-dev-environment/01-toolchain-health-check' },
            { text: '阶段 1 · C 基底', link: '/01-c-basics/01-program-structure-and-compilation' },
            { text: '阶段 2 · 指针内存', link: '/02-pointers-memory/01-what-is-a-pointer' },
            { text: '阶段 3 · 数据结构', link: '/03-data-structures/01-singly-linked-list' },
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
            { icon: 'github', link: 'https://github.com/Charliechen114514/C-Journey' },
        ],
    },
})
