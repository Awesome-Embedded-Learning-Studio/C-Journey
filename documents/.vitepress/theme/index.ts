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

/* 在线编译器组件(各章 .md 里能用 <OnlineCompilerDemo>):
 * 调 godbolt API 让读者在浏览器里改 C 代码、点运行/汇编看输出。
 * 移植自 ~/Tutorial_AwesomeModernCPP,适配纯 C(cg161 编译器 + shiki c 高亮 + 内联 code prop) */
import OnlineCompilerDemo from './components/OnlineCompilerDemo.vue'

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
            /* Features 之后:四阶段路线图 + 在线 C 编译器 demo(让访客一进门就能玩) */
            'home-features-after': () => h('div', { class: 'home-after-features' }, [
                h(HomeRoadmap),
                h('div', { class: 'home-compiler-demo-wrap' }, [
                    h('h2', { class: 'home-compiler-demo-title' }, '亲手玩:C 在浏览器里跑'),
                    h('p', { class: 'home-compiler-demo-lead' },
                        '改改下面的代码、点「运行」看输出,或点「看 x86-64 汇编」看 C 编成什么样 —— 全程调 godbolt 公共 API,无需装任何东西。'),
                    h(OnlineCompilerDemo, {
                        title: '指针改值:隔着地址动另一个变量',
                        description: 'int* p = &n 让 p 指向 n;*p = 42 通过地址把 n 改成 42。把 42 改成别的数、或让 p 指向另一个变量,看 n 怎么变。',
                        allowRun: true,
                        allowX86Asm: true,
                        code: '#include <stdio.h>\n\nint main(void) {\n    int n = 10;\n    int* p = &n;   /* p 指向 n */\n    *p = 42;       /* 通过指针改 n */\n    printf("n = %d\\n", n);\n    return 0;\n}\n',
                    }),
                ]),
            ]),
            /* 顶栏右侧:字号切换器 */
            'nav-bar-content-after': () => h(FontSizeSwitcher),
            'nav-screen-content-after': () => h(FontSizeSwitcher),
        })
    },
    enhanceApp({ app }) {
        app.component('ChapterNav', ChapterNav)
        app.component('ChapterLink', ChapterLink)
        app.component('OnlineCompilerDemo', OnlineCompilerDemo)
    },
} satisfies Theme
