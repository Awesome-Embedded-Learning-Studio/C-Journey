// 标签索引页数据生成器:扫描 documents 树的 frontmatter,产出「标签 → 文章」索引。
// 移植自 TAMCPP config/tags-manifest.ts,按 C-Journey 单语站简化(去 locale/en 分支)。
// 数据源唯一——文章 frontmatter 的 tags 字段(白名单与分类在 scripts/tags.json,
// Python 校验器 scripts/tags.py 共读同一份)。
//
// 消费链路:config.ts 的 transformPageData 调 applyTagsPageData 注入——
//   - /tags 页:注入完整索引(frontmatter.tagsIndex),SSR 即有内容
//   - 普通文章页:注入 topicTags(滤掉平台标签后的主题标签),DocTags.vue 渲染章尾徽章
// dev 下改了文章标签需重启 dev server 才刷新(与 sidebar 行为一致);
// 扫描结果按进程 memoize,transformPageData 每页触发也不会重复扫全树。
//
// 附带构建期体检:白名单里从未被用到的主题标签打 warn,提醒清理死标签。

import { readdirSync, readFileSync } from 'node:fs'
import { basename, join, relative, sep } from 'node:path'
import { fileURLToPath } from 'node:url'
import matter from 'gray-matter'
import type { PageData } from 'vitepress'

const PROJECT_ROOT = fileURLToPath(new URL('../../', import.meta.url))
const DOCUMENTS = join(PROJECT_ROOT, 'documents')

// ── 标签分类(scripts/tags.json,与 Python 校验器共用) ────────

interface TaxonomyCategory {
  key: string
  zh: string
  en: string
  role: 'topic' | 'platform'
  tags: string[]
}

const TAXONOMY: { categories: TaxonomyCategory[] } = JSON.parse(
  readFileSync(join(PROJECT_ROOT, 'scripts', 'tags.json'), 'utf8'),
)
const TOPIC_CATEGORIES = TAXONOMY.categories.filter(c => c.role === 'topic')
const PLATFORM_TAGS = new Set(
  TAXONOMY.categories.filter(c => c.role === 'platform').flatMap(c => c.tags),
)
const DIFFICULTY_TAGS = ['beginner', 'intermediate', 'advanced']

// ── 卷名映射(顺序即结果列表的排序优先级,与 config.ts stages 对齐) ──

const VOLUME_ORDER = [
  '00-dev-environment',
  '01-c-basics',
  '02-pointers-memory',
  '03-data-structures',
  '04-engineering',
  '05-system-programming',
  'exercises',
  'changelog',
  'roadmap',
]

const VOLUME_LABELS: Record<string, string> = {
  '00-dev-environment': '阶段 0 · 开发环境与编译',
  '01-c-basics': '阶段 1 · C 语言基底',
  '02-pointers-memory': '阶段 2 · 指针与内存',
  '03-data-structures': '阶段 3 · 数据结构与算法',
  '04-engineering': '阶段 4 · 工程化与质量门',
  '05-system-programming': '阶段 5 · 系统编程',
  exercises: '练习与作业',
  changelog: '更新日志',
  roadmap: '路线图',
}

// ── 数据形状(TagExplorer.vue 消费) ─────────────────────────

export interface TagArticle {
  /** 标题 */
  t: string
  /** 站内链接(clean URL,不含 base,组件侧 withBase) */
  h: string
  /** 所属顶级目录 key(标签查 VOLUME_LABELS) */
  v: string
  /** beginner | intermediate | advanced(缺省则无难度筛选) */
  d?: string
  /** platform: host | stm32 | mcu51 | 8051 */
  p?: string
  /** 预估阅读分钟 */
  m?: number
  /** 主题标签(平台标签已剔除) */
  tg: string[]
}

export interface TagsIndex {
  /** 标签墙上展示的主题标签总数(仅 count>0) */
  tagCount: number
  articleCount: number
  volLabels: Record<string, string>
  categories: Array<{ key: string; label: string; labelEn: string; tags: Array<{ name: string; count: number }> }>
  articles: TagArticle[]
}

// ── 扫描 ────────────────────────────────────────────────────

/** 不算「文章」的文件:本页、404、README(index.md 单独判定,见 isArticleIndex) */
const SKIP_FILE_NAMES = new Set(['tags.md', '404.md', 'README.md'])
/** 不进扫描的目录:资源与公共目录 */
const SKIP_DIR_NAMES = ['public', 'images', 'demos']

function walkMd(dir: string, out: string[]) {
  let entries
  try {
    entries = readdirSync(dir, { withFileTypes: true })
  } catch {
    return
  }
  for (const e of entries) {
    if (e.name.startsWith('.')) continue
    const full = join(dir, e.name)
    if (e.isDirectory()) {
      if (SKIP_DIR_NAMES.includes(e.name)) continue
      walkMd(full, out)
    } else if (e.name.endsWith('.md') && !SKIP_FILE_NAMES.has(e.name)) {
      out.push(full)
    }
  }
}

/**
 * index.md 多数是导航页,但也有例外(如 changelog/index.md 是带完整 frontmatter
 * 的一站一文)。判别:带数字 chapter 的算文章,导航页(各阶段导读/首页)没有。
 */
function isArticleIndex(fm: Record<string, unknown>): boolean {
  return typeof fm.chapter === 'number'
}

function toArticle(absPath: string): TagArticle | null {
  // frontmatter 永远在文件头,截 16KB 足够,免去整读长文
  const raw = readFileSync(absPath, 'utf8').slice(0, 16384)
  let fm: Record<string, unknown>
  try {
    fm = matter(raw).data as Record<string, unknown>
  } catch {
    return null
  }
  const title = typeof fm.title === 'string' ? fm.title.trim() : ''
  if (!title) return null
  // 纯导航 index.md 不进文章列表
  if (basename(absPath) === 'index.md' && !isArticleIndex(fm)) return null

  const relWithExt = relative(DOCUMENTS, absPath).split(sep).join('/')
  // 目录 index 页的 URL 是目录本身(尾斜杠),普通页面去掉 .md
  const isDirIndex = relWithExt === 'index.md' || relWithExt.endsWith('/index.md')
  const rel = isDirIndex ? relWithExt.replace(/(^|\/)index\.md$/, '$1') : relWithExt.replace(/\.md$/, '')
  const href = `/${rel}`
  const volKey = rel.split('/')[0]

  const tags = Array.isArray(fm.tags) ? fm.tags.filter((t): t is string => typeof t === 'string') : []
  const topicTags = tags.filter(t => !PLATFORM_TAGS.has(t))

  const difficulty
    = typeof fm.difficulty === 'string' && DIFFICULTY_TAGS.includes(fm.difficulty)
      ? fm.difficulty
      : tags.find(t => DIFFICULTY_TAGS.includes(t))
  const platform
    = typeof fm.platform === 'string' && PLATFORM_TAGS.has(fm.platform)
      ? fm.platform
      : tags.find(t => PLATFORM_TAGS.has(t))

  const minutes = typeof fm.reading_time_minutes === 'number' ? fm.reading_time_minutes : undefined

  return {
    t: title,
    h: href,
    v: volKey,
    d: difficulty,
    p: platform,
    m: minutes,
    tg: topicTags,
  }
}

// 进程级 memoize:transformPageData 每页触发,只扫一次全树
let cachedIndex: TagsIndex | null = null

export function getTagsIndex(): TagsIndex {
  if (cachedIndex) return cachedIndex

  const files: string[] = []
  walkMd(DOCUMENTS, files)

  const volOrder = new Map(VOLUME_ORDER.map((k, i) => [k, i]))
  const articles = files
    .map(f => toArticle(f))
    .filter((a): a is TagArticle => a !== null)
    .sort((a, b) => {
      const va = volOrder.get(a.v) ?? VOLUME_ORDER.length
      const vb = volOrder.get(b.v) ?? VOLUME_ORDER.length
      return va !== vb ? va - vb : a.h.localeCompare(b.h, 'zh-CN')
    })

  // 标签计数(仅主题标签;墙只渲染 count>0 的)
  const counts = new Map<string, number>()
  for (const a of articles) for (const t of a.tg) counts.set(t, (counts.get(t) ?? 0) + 1)

  const categories = TOPIC_CATEGORIES.map(cat => ({
    key: cat.key,
    label: cat.zh,
    labelEn: cat.en,
    tags: cat.tags
      .map(name => ({ name, count: counts.get(name) ?? 0 }))
      .filter(t => t.count > 0)
      .sort((a, b) => b.count - a.count || a.name.localeCompare(b.name, 'zh-CN')),
  })).filter(cat => cat.tags.length > 0)

  const tagCount = categories.reduce((n, c) => n + c.tags.length, 0)

  // 体检:白名单里从未用到的主题标签——多半是改名残留,提醒清理
  const used = new Set(counts.keys())
  const dead = TOPIC_CATEGORIES.flatMap(c => c.tags).filter(t => !used.has(t))
  if (dead.length > 0) {
    console.warn(`[tags-manifest] 白名单主题标签从未被使用(${dead.length}): ${dead.join('、')} —— 考虑从 scripts/tags.json 清理`)
  }
  const untagged = articles.filter(a => a.tg.length === 0).length
  if (untagged > 0) {
    console.warn(`[tags-manifest] 有 ${untagged} 篇文章没有主题标签,不出现在任何标签过滤结果里`)
  }

  const index: TagsIndex = {
    tagCount,
    articleCount: articles.length,
    volLabels: VOLUME_LABELS,
    categories,
    articles,
  }
  cachedIndex = index
  return index
}

/** 供 config.ts 的 transformPageData 使用:给 tags 页注入索引数据,
 *  给文章页注入算好的主题标签(文章页底部徽章用)。 */
export function applyTagsPageData(page: PageData): void {
  if (page.relativePath === 'tags.md') {
    page.frontmatter.sidebar = false
    page.frontmatter.aside = false
    page.frontmatter.tagsIndex = getTagsIndex()
    return
  }
  // 文章页:客户端组件拿不到 tags.json(node 侧数据),构建期把「过滤掉
  // 平台标签后的主题标签」注入 frontmatter,单一数据源不破。
  // 没有主题标签的页面不注入,组件据此什么都不渲染。
  const tags = page.frontmatter.tags
  if (Array.isArray(tags)) {
    const topic = tags.filter(t => typeof t === 'string' && !PLATFORM_TAGS.has(t))
    if (topic.length > 0) {
      page.frontmatter.topicTags = topic
      page.frontmatter.tagsPageBase = '/tags'
    }
  }
}
