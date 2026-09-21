import DefaultTheme from 'vitepress/theme'
import { defineComponent, h } from 'vue'
import type { Theme } from 'vitepress'
import './custom.css'
import './article-code.css'
import './article-quote.css'
import './tags.css'
import { setupMermaid } from './mermaid-client'

/* 首页增强组件(HomeRoadmap.vue 已由 HomePath 接替,文件保留在 components/ 备回退) */
import HomeHeroVisual from './components/HomeHeroVisual.vue'
import HomePath from './components/HomePath.vue'
import HomeTipBanner from './components/HomeTipBanner.vue'
import ProofStrip from './components/ProofStrip.vue'

/* 全局组件(各章 .md 里能用 <ChapterNav>/<ChapterLink>) */
import ChapterNav from './components/ChapterNav.vue'
import ChapterLink from './components/ChapterLink.vue'

/* 在线编译器组件(各章 .md 里能用 <OnlineCompilerDemo>):
 * 调 godbolt API 让读者在浏览器里改 C 代码、点运行/汇编看输出。
 * 移植自 ~/Tutorial_AwesomeModernCPP,适配纯 C(cg161 编译器 + shiki c 高亮 + 内联 code prop) */
import OnlineCompilerDemo from './components/OnlineCompilerDemo.vue'

/* 布局增强(无模板,运行时注入 DOM) */
import FontSizeSwitcher from './components/FontSizeSwitcher.vue'
import ResizableSidebar from './components/ResizableSidebar.vue'

/* 阅读体验(2026-09 对齐 TAMCPP):
 * ReadingProgress 顶部阅读进度条;NavSpinner SPA 跳转两级加载反馈(角落小卡+慢网浮层);
 * MermaidLightbox 全屏看图模态(mermaid-client 给每张图挂 maximize 按钮触发) */
import ReadingProgress from './components/ReadingProgress.vue'
import NavSpinner from './components/NavSpinner.vue'
import MermaidLightbox from './components/MermaidLightbox.vue'
import { setupDevFakeLag } from './dev-fake-lag'

/* 标签体系(2026-09 对齐 TAMCPP):
 * DocTags 章尾主题标签徽章(数据由构建期 transformPageData 注入 frontmatter.topicTags);
 * TagExplorer /tags/ 标签探索页(数据经 frontmatter.tagsIndex 注入,SSR 即有内容)。 */
import DocTags from './components/DocTags.vue'
import TagExplorer from './components/TagExplorer.vue'

/* 教学动画(2026-09 对齐 TAMCPP Anim 系统):声明式 SVG 动画,
 * .md 里 <Anim id="..." /> 一行接入,数据自动收集(见 animations/README.md)。 */
import Anim from './components/Anim.vue'

/*
 * 主题:VitePress 默认主题 + 自定义 CSS + 标杆移植组件。
 * 首页用 Layout 插槽挂 Hero 终端动效 / ProofStrip / TipBanner / Roadmap;
 * 全局注册 ChapterNav / ChapterLink 供各章 .md 内联使用;
 * FontSizeSwitcher 挂在顶栏;ResizableSidebar 挂 layout-top 注入拖拽手柄。
 */
const Layout = defineComponent({
    setup() {
        setupMermaid()
        setupDevFakeLag()
        return () => h(DefaultTheme.Layout, null, {
            /* 可拖拽侧栏手柄(运行时注入,无视觉模板) + 阅读体验三件:
             * NavSpinner(跳转加载反馈) / ReadingProgress(阅读进度条) / MermaidLightbox(全屏看图) */
            'layout-top': () => [
                h(NavSpinner),
                h(ReadingProgress),
                h(ResizableSidebar),
                h(MermaidLightbox),
            ],
            /* Hero 区右侧:终端打字机动画(替换默认图片) */
            'home-hero-image': () => h(HomeHeroVisual),
            /* Hero 区移动端:ProofStrip 夹在标题与终端之间 */
            'home-hero-actions-after': () =>
                h('div', { class: 'proof-on-mobile' }, [h(ProofStrip)]),
            /* Hero 区下方:ProofStrip(桌面)+ 在线 C 编译器 demo(放最上面,一进门就能玩) */
            'home-hero-after': () =>
                h('div', { class: 'home-after-hero' }, [
                    h('div', { class: 'proof-on-desktop' }, [h(ProofStrip)]),
                    h('div', { class: 'home-compiler-demo-wrap', id: 'playground' }, [
                        h('h2', { class: 'home-compiler-demo-title' }, '亲手玩:C 在浏览器里跑'),
                        h('p', { class: 'home-compiler-demo-lead' },
                            '改改下面的代码、点「运行」看输出,或点「看 x86-64 汇编」看 C 编成什么样 —— 全程调 godbolt 公共 API,无需装任何东西。'),
                        h(OnlineCompilerDemo, {
                            title: '指针改值:隔着地址动另一个变量',
                            description: 'int* p = &n 让 p 指向 n;*p = 42 通过地址把 n 改成 42。把 42 改成别的数、或让 p 指向另一个变量,看 n 怎么变。',
                            allowRun: true,
                            allowX86Asm: true,
                            sourcePath: '/demos/hero_demo.c',
                        }),
                    ]),
                ]),
            /* Features 之前:提示横幅(给读者入口) */
            'home-features-before': () =>
                h('div', { class: 'home-pre-features' }, [h(HomeTipBanner)]),
            /* Features 之后:学习路径图(2026-09 由 HomePathGraph 接管旧 HomeRoadmap 卡片列表;
             * HomeRoadmap.vue 保留在 components/ 备回退,数据源见 home-path-data.ts) */
            'home-features-after': () => h('div', { class: 'home-after-features' }, [
                h(HomePath),
            ]),
            /* 顶栏右侧:字号切换器 */
            'nav-bar-content-after': () => h(FontSizeSwitcher),
            'nav-screen-content-after': () => h(FontSizeSwitcher),
            /* 章尾(评论位前):主题标签徽章,点击跳预筛好的 /tags?tag=X */
            'doc-footer-before': () => h(DocTags),
        })
    },
})

export default {
    extends: DefaultTheme,
    Layout,
    enhanceApp({ app }) {
        app.component('ChapterNav', ChapterNav)
        app.component('ChapterLink', ChapterLink)
        app.component('OnlineCompilerDemo', OnlineCompilerDemo)
        app.component('TagExplorer', TagExplorer)
        app.component('Anim', Anim)
    },
} satisfies Theme
