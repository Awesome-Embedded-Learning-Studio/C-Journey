import DefaultTheme from 'vitepress/theme'
import type { Theme } from 'vitepress'
import './custom.css'

/* 主题:VitePress 默认主题 + 自定义 CSS(首页卡片六重 Hover 等,移植自 TAMCPP 标杆) */
export default {
    extends: DefaultTheme,
} satisfies Theme
