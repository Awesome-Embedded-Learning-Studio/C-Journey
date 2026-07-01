import DefaultTheme from 'vitepress/theme'
import { h } from 'vue'
import type { Theme } from 'vitepress'
import './custom.css'

/* 首页增强组件 */
import HomeHeroVisual from './components/HomeHeroVisual.vue'
import HomeRoadmap from './components/HomeRoadmap.vue'
import HomeTipBanner from './components/HomeTipBanner.vue'
import ProofStrip from './components/ProofStrip.vue'

/* 全局组件(各章 .md 里能用 <ChapterNav>/<ChapterLink>) */
import ChapterNav from './components/ChapterNav.vue'
import ChapterLink from './components/ChapterLink.vue'

/* 布局增强(无模板,运行时注入 DOM) */
import FontSizeSwitcher from './components/FontSizeSwitcher.vue'
import ResizableSidebar from './components/ResizableSidebar.vue'

/*
 * 主题:VitePress 默认主题 + 自定义 CSS + 标杆移植组件。
 * 首页用 Layout 插槽挂 Hero 终端动效 / ProofStrip / TipBanner / Roadmap;
 * 全局注册 ChapterNav / ChapterLink 供各章 .md 内联使用;
 * FontSizeSwitcher 挂在顶栏;ResizableSidebar 挂 layout-top 注入拖拽手柄。
 */
export default {
    extends: DefaultTheme,
    Layout() {
        return h(DefaultTheme.Layout, null, {
            /* 可拖拽侧栏手柄(运行时注入,无视觉模板) */
            'layout-top': () => h(ResizableSidebar),
            /* Hero 区右侧:终端打字机动画(替换默认图片) */
            'home-hero-image': () => h(HomeHeroVisual),
            /* Hero 区移动端:ProofStrip 夹在标题与终端之间 */
            'home-hero-actions-after': () =>
                h('div', { class: 'proof-on-mobile' }, [h(ProofStrip)]),
            /* Hero 区桌面端:ProofStrip 在终端下方 */
            'home-hero-after': () =>
                h('div', { class: 'proof-on-desktop' }, [h(ProofStrip)]),
            /* Features 之前:提示横幅(给读者入口) */
            'home-features-before': () =>
                h('div', { class: 'home-pre-features' }, [h(HomeTipBanner)]),
            /* Features 之后:四阶段路线图 */
            'home-features-after': () => h(HomeRoadmap),
            /* 顶栏右侧:字号切换器 */
            'nav-bar-content-after': () => h(FontSizeSwitcher),
            'nav-screen-content-after': () => h(FontSizeSwitcher),
        })
    },
    enhanceApp({ app }) {
        app.component('ChapterNav', ChapterNav)
        app.component('ChapterLink', ChapterLink)
    },
} satisfies Theme
