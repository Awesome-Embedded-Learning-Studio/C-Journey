/**
 * 首页「学习路径图」种子数据(HomePathGraph.vue 消费)。
 * 移植自 TAMCPP home-path-data.ts,按 C-Journey 重画:
 * - 六条层带 = 六个阶段(严格线性主线一条 solid 蛇形穿行);
 * - tier 三档:core=C 语言本体(锈橙)/eng=造物与工程(琥珀)/domain=系统编程(绿);
 * - sup 支撑节点(点线,不进悬停高亮链):在线编译器/Sanitizer 门禁/练习/标签索引。
 * 种子坐标是模块常量、SSG 直出;客户端拖拽后的布局存 localStorage(key 带
 * HOME_GRAPH_REVISION,节点增删后递增版本号,旧布局自动失效回落种子)。
 */

export type PathNodeKind = 'root' | 'proj' | 'sup'
export type PathEdgeKind = 'solid' | 'dash' | 'dot'
export type PathSide = 'top' | 'right' | 'bottom' | 'left'
export type PathRouteCoord = number | 'from' | 'to'
/** done=已审核上线 doing=在建/持续更新 */
export type PathStatus = 'done' | 'doing'
/** 配色三档:core=锈橙(语言本体) / eng=琥珀(造物与工程) / domain=绿(系统编程) */
export type PathTier = 'core' | 'eng' | 'domain'

export interface PathNode {
  /** 节点短 id(边的 from/to 用它) */
  id: string
  name: string
  sub: string
  /** 种子坐标(中心点)与尺寸 */
  x: number
  y: number
  w: number
  h: number
  kind: PathNodeKind
  status: PathStatus
  tier: PathTier
  /** 站内路径(clean URL,组件侧 withBase) */
  href: string
  /** 左上角标 */
  badge: string
}

export interface PathEdge {
  from: string
  to: string
  /** solid = 主线路径;dash = 按需选修;dot = 支撑/索引(不进悬停高亮链) */
  kind: PathEdgeKind
  /**
   * 默认布局的布线提示。from/to 选节点锚点,via 把边送入层间或外围的固定轨道;
   * 坐标写 'from'/'to' 时跟随对应锚点,因此节点拖动后首尾段仍保持正交。
   */
  route?: {
    from: PathSide
    to: PathSide
    via?: Array<{ x: PathRouteCoord; y: PathRouteCoord }>
  }
}

export interface PathBand {
  label: string
  top: number
  bottom: number
}

/** 画布尺寸(SVG viewBox) */
export const VB_W = 1330
export const VB_H = 856

/** localStorage 布局 key 版本:节点增删/坐标或默认布线大改时 +1 */
export const HOME_GRAPH_REVISION = 1

export const HOME_PATH_BANDS: PathBand[] = [
  { label: '阶段 0 · 开发环境', top: 16, bottom: 104 },
  { label: '阶段 1 · C 语言基底', top: 138, bottom: 248 },
  { label: '阶段 2 · 指针与内存', top: 282, bottom: 392 },
  { label: '阶段 3 · 数据结构', top: 426, bottom: 546 },
  { label: '阶段 4 · 工程化', top: 580, bottom: 700 },
  { label: '阶段 5 · 系统编程', top: 734, bottom: 834 },
]

export const HOME_PATH_NODES: PathNode[] = [
  { id: 'start', name: '起点 · 开发环境', sub: 'gcc/clang · sanitizer · git', x: 340, y: 60, w: 210, h: 66, kind: 'root', status: 'done', tier: 'core', href: '/00-dev-environment/', badge: 'GO' },
  { id: 'play', name: '在线玩 C', sub: '浏览器里跑', x: 980, y: 60, w: 150, h: 46, kind: 'sup', status: 'done', tier: 'core', href: '/#playground', badge: 'DEM' },
  { id: 's1', name: '阶段 1 · C 基底', sub: '类型 · 运算 · 函数 · 结构体', x: 650, y: 193, w: 190, h: 58, kind: 'proj', status: 'done', tier: 'core', href: '/01-c-basics/01-program-structure-and-compilation', badge: 'S1' },
  { id: 's2', name: '阶段 2 · 指针与内存', sub: '指针算术 · malloc · 内存布局', x: 980, y: 337, w: 190, h: 58, kind: 'proj', status: 'done', tier: 'core', href: '/02-pointers-memory/01-what-is-a-pointer', badge: 'S2' },
  { id: 'asan', name: 'Sanitizer 门禁', sub: 'ASan/UBSan 护航', x: 340, y: 337, w: 160, h: 46, kind: 'sup', status: 'done', tier: 'core', href: '/00-dev-environment/11-sanitizer-gate', badge: 'SAN' },
  { id: 's3', name: '阶段 3 · 造数据结构', sub: '动态数组 · 链表 · 树 · 排序', x: 650, y: 486, w: 190, h: 58, kind: 'proj', status: 'done', tier: 'eng', href: '/03-data-structures/00-what-is-dynamic-array', badge: 'S3' },
  { id: 'exer', name: '练习与作业', sub: '252 题带参考答案', x: 990, y: 486, w: 150, h: 46, kind: 'sup', status: 'done', tier: 'eng', href: '/exercises/', badge: 'EXE' },
  { id: 's4', name: '阶段 4 · 工程化', sub: 'CMake · 测试 · 静态分析 · CI', x: 340, y: 640, w: 190, h: 58, kind: 'proj', status: 'done', tier: 'eng', href: '/04-engineering/01-header-contracts', badge: 'S4' },
  { id: 's5', name: '阶段 5 · 系统编程', sub: 'fd · fork · 信号 · epoll · socket', x: 665, y: 784, w: 210, h: 58, kind: 'proj', status: 'done', tier: 'domain', href: '/05-system-programming/01-file-io-and-fd', badge: 'S5' },
  { id: 'tags', name: '标签索引', sub: '按主题检索', x: 340, y: 784, w: 140, h: 46, kind: 'sup', status: 'done', tier: 'domain', href: '/tags', badge: 'IDX' },
  { id: 'proj', name: '实战项目', sub: 'tiny-c-stdlib 等', x: 980, y: 784, w: 150, h: 46, kind: 'sup', status: 'doing', tier: 'domain', href: '/roadmap', badge: 'PRJ' },
]

export const HOME_PATH_EDGES: PathEdge[] = [
  // 主线:起点 → 基底 → 指针 → 数据结构 → 工程化 → 系统编程(蛇形穿六带)
  { from: 'start', to: 's1', kind: 'solid', route: { from: 'bottom', to: 'top', via: [{ x: 'from', y: 121 }, { x: 'to', y: 121 }] } },
  { from: 's1', to: 's2', kind: 'solid', route: { from: 'bottom', to: 'top', via: [{ x: 'to', y: 265 }] } },
  { from: 's2', to: 's3', kind: 'solid', route: { from: 'bottom', to: 'top', via: [{ x: 'from', y: 409 }, { x: 'to', y: 409 }] } },
  { from: 's3', to: 's4', kind: 'solid', route: { from: 'bottom', to: 'top', via: [{ x: 'from', y: 563 }, { x: 'to', y: 563 }] } },
  { from: 's4', to: 's5', kind: 'solid', route: { from: 'bottom', to: 'top', via: [{ x: 'from', y: 710 }, { x: 'to', y: 710 }] } },
  // 选修:学完阶段 2 即可按需刷练习(dash,不占主线)
  { from: 's2', to: 'exer', kind: 'dash', route: { from: 'right', to: 'left' } },
  // 支撑/索引:不进悬停高亮链(dot)
  { from: 'start', to: 'play', kind: 'dot', route: { from: 'right', to: 'left', via: [{ x: 700, y: 60 }] } },
  { from: 's2', to: 'asan', kind: 'dot', route: { from: 'left', to: 'right' } },
  { from: 'start', to: 'tags', kind: 'dot', route: { from: 'left', to: 'top', via: [{ x: 120, y: 'from' }, { x: 120, y: 770 }, { x: 'to', y: 770 }] } },
  { from: 's5', to: 'proj', kind: 'dot', route: { from: 'right', to: 'left', via: [{ x: 900, y: 'from' }, { x: 900, y: 784 }] } },
]
