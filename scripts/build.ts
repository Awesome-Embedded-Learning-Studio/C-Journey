/**
 * C-Journey 增量构建脚本(移植自 Tutorial_AwesomeModernCPP/scripts/build.ts)。
 *
 * ┌─ 标杆 build.ts 在做什么 ────────────────────────────────────────┐
 * │ 17 个 VOLUME × (zh | en) = 最多 34 个独立 vitepress build 子任务,   │
 * │ 限并发跑(BUILD_CONCURRENCY,默认 4)。每个 volume 生成自己的临时     │
 * │ config.ts(只挂该 volume 的 sidebar)、symlink theme/plugins/public, │
 * │ 各自产出独立 dist,最后再合并 HTML / 合并搜索索引 / 统一 hash map。   │
 * │ 增量靠 .build-cache/manifest.json:每个 volume 一个 SHA256 内容      │
 * │ hash(build 脚本 + site/.vitepress + package.json + 该 volume 源    │
 * │ 目录的目录哈希),命中即 cp 缓存的 output,跳过 build。--force 清空。 │
 * └─────────────────────────────────────────────────────────────────┘
 *
 * ┌─ C-Journey 的现实 ─────────────────────────────────────────────┐
 * │ 53 章、单语、单 config(无 locale/en),全量 build 实测 ~188s。   │
 * │ 真正的「分段独立 build」要把 config 拆 4 份临时 config + 合并    │
 * │ 搜索索引 + 统一 hash map —— 工程成本远大于 188s 省下来的时间,    │
 * │ 且 4 段合起来还是覆盖整站,几乎拿不到并行收益。                   │
 * │                                                                  │
 * │ 因此本脚本选择「单次 vitepress build documents + 全站内容 hash   │
 * │ 增量缓存」:                                                      │
 * │   - 整站内容不变 → 直接复用上次的 dist,0 秒构建(真增量)。      │
 * │   - 任一文件变 → 正常 build,刷新缓存。                          │
 * │   - --force / --clean → 清缓存强制全量。                         │
 * │ 保留标杆的日志风格、hashDir/hashFile/manifest 机制、--force,     │
 * │ 为未来内容增长到「分段值得」的体量预留升级路径(见文末 TODO)。   │
 * └─────────────────────────────────────────────────────────────────┘
 *
 * 用法:
 *   pnpm build            # 正常增量
 *   pnpm build -- --force # 强制全量重建
 *   BUILD_CONCURRENCY=N … # 标杆保留;本脚本单次 build 不读它(占位)
 */

import { execFile } from 'child_process'
import {
  readdirSync, readFileSync, existsSync, writeFileSync,
  rmSync, mkdirSync, cpSync, statSync,
} from 'fs'
import { join, resolve, relative } from 'path'
import { createHash } from 'crypto'
import { createRequire } from 'module'

const require = createRequire(import.meta.url)

// ── CLI Flags ───────────────────────────────────────────────────

const FORCE_REBUILD = process.argv.includes('--force') || process.argv.includes('--clean')
/** 标杆用此值限制并行 volume 数;C-Journey 单次 build 暂不读它,留作未来分段化时的入口。 */
const CONCURRENCY = parseInt(process.env.BUILD_CONCURRENCY || '4', 10)

// ── Configuration ───────────────────────────────────────────────

/**
 * C-Journey 的「卷」= 6 个上线阶段(与 documents/.vitepress/config.ts 的 stages 对齐)。
 * 注意:documents/ 下除上线的六阶段(00-05)外,其余(index.md / README 等)被 config.ts 的
 * srcExclude 排除、不参与 build,因此这里也不纳入内容 hash。
 *
 * 标杆的 VOLUMES 是「每个 volume 独立 build 的单元」;这里则是「参与整站内容 hash
 * 的源目录集合」—— 语义被收窄了,因为不再分段独立 build。
 */
interface Stage {
  dir: string
  name: string
}
const STAGES: Stage[] = [
  { dir: '00-dev-environment',    name: '阶段 0 · 开发环境与编译' },
  { dir: '01-c-basics',           name: '阶段 1 · C 语言基底' },
  { dir: '02-pointers-memory',    name: '阶段 2 · 指针与内存' },
  { dir: '03-data-structures',    name: '阶段 3 · 数据结构与算法' },
  { dir: '04-engineering',        name: '阶段 4 · 工程化与质量门' },
  { dir: '05-system-programming', name: '阶段 5 · 系统编程' },
]

const PROJECT_ROOT = resolve(import.meta.dirname, '..')
const DOCUMENTS = join(PROJECT_ROOT, 'documents')
const VP_DIR = join(DOCUMENTS, '.vitepress')          // 现有 config / theme 所在地,保持不动
const DIST_FINAL = join(VP_DIR, 'dist')               // 与原 `vitepress build documents` 的产物路径一致
const CACHE_DIR = join(VP_DIR, '.build-cache')
const MANIFEST_PATH = join(CACHE_DIR, 'manifest.json')
/** 标杆把整站 dist 缓存到 .build-cache/output/<id>;这里只有一份,叫 output。 */
const CACHED_OUTPUT = join(CACHE_DIR, 'output')

const VITEPRESS_BIN = join(resolve(require.resolve('vitepress/package.json'), '..'), 'bin', 'vitepress.js')

// ── Logging(对齐标杆:[HH:MM:SS] 消息 + ═ 分步) ───────────────

function ts(): string {
  return new Date().toISOString().substring(11, 19)
}
function log(msg: string) { console.log(`[${ts()}] ${msg}`) }
function logStep(msg: string) {
  console.log(`\n[${ts()}] ${'═'.repeat(60)}`)
  log(`  ${msg}`)
  console.log(`[${ts()}] ${'═'.repeat(60)}`)
}
function memMB(): string {
  const m = process.memoryUsage()
  return `RSS=${(m.rss / 1024 / 1024).toFixed(0)}MB Heap=${(m.heapUsed / 1024 / 1024).toFixed(0)}/${(m.heapTotal / 1024 / 1024).toFixed(0)}MB`
}

// ── Helpers(直接照搬标杆) ─────────────────────────────────────

function ensureClean(dir: string) {
  if (existsSync(dir)) rmSync(dir, { recursive: true })
  mkdirSync(dir, { recursive: true })
}

/** 递归数 .md 数量,用于日志里报告「处理了 N 个文件」。 */
function countMdFiles(dir: string): number {
  let count = 0
  try {
    for (const e of readdirSync(dir, { withFileTypes: true })) {
      if (e.name.startsWith('.')) continue
      const full = join(dir, e.name)
      if (e.isDirectory()) count += countMdFiles(full)
      else if (e.name.endsWith('.md')) count++
    }
  } catch { /* ignore */ }
  return count
}

/**
 * 目录内容哈希:稳定排序后把「相对路径 + 文件字节」喂进 sha256,取前 16 位。
 * 与标杆 hashDir 完全一致 —— 跨全新 checkout 也能复现,不依赖 mtime/inode。
 */
function hashDir(dir: string): string {
  const h = createHash('sha256')
  function walk(d: string) {
    try {
      const entries = readdirSync(d, { withFileTypes: true }).sort((a, b) => a.name.localeCompare(b.name))
      for (const e of entries) {
        if (e.name.startsWith('.')) continue
        const full = join(d, e.name)
        if (e.isDirectory()) { walk(full); continue }
        h.update(`file:${relative(dir, full)}\n`)
        h.update(readFileSync(full))
        h.update('\n')
      }
    } catch { /* ignore */ }
  }
  walk(dir)
  return h.digest('hex').substring(0, 16)
}

function hashFile(path: string): string {
  const h = createHash('sha256')
  if (!existsSync(path)) return ''
  h.update(readFileSync(path))
  return h.digest('hex').substring(0, 16)
}

/**
 * 把所有「会影响 build 产物」的输入揉成一个 hash。
 * 标杆额外纳入 site/.vitepress(整段);这里把 config.ts、theme/、4 个阶段源目录、
 * documents/index.md、package.json、lockfile、本脚本自身都纳入 —— 任一变化即重建。
 */
function hashBuildInputs(): string {
  const h = createHash('sha256')
  const inputs: Array<[string, string]> = [
    ['config',     hashFile(join(VP_DIR, 'config.ts'))],
    ['theme',      hashDir(join(VP_DIR, 'theme'))],
    ['index',      hashFile(join(DOCUMENTS, 'index.md'))],
    ['package',    hashFile(join(PROJECT_ROOT, 'package.json'))],
    ['lockfile',   hashFile(join(PROJECT_ROOT, 'pnpm-lock.yaml'))],
    ['build-script', hashFile(join(PROJECT_ROOT, 'scripts', 'build.ts'))],
  ]
  for (const stage of STAGES) {
    const stageDir = join(DOCUMENTS, stage.dir)
    inputs.push([stage.dir, existsSync(stageDir) ? hashDir(stageDir) : ''])
  }
  for (const [label, value] of inputs) {
    h.update(`${label}:${value}\n`)
  }
  // 输出每个分项 hash,方便日志里看出「到底是谁变了」。
  const parts = inputs.map(([k, v]) => `${k}=${v}`)
  return `${h.digest('hex').substring(0, 16)}|${parts.join(' ')}`
}

// ── Manifest(对齐标杆结构) ────────────────────────────────────

interface ManifestEntry { hash: string; timestamp: string }
type Manifest = Record<string, ManifestEntry>

function readManifest(): Manifest {
  if (FORCE_REBUILD) {
    log('  --force: discarding build cache')
    if (existsSync(CACHE_DIR)) rmSync(CACHE_DIR, { recursive: true })
    return {}
  }
  if (!existsSync(MANIFEST_PATH)) return {}
  try { return JSON.parse(readFileSync(MANIFEST_PATH, 'utf-8')) } catch { return {} }
}

function writeManifest(manifest: Manifest) {
  mkdirSync(CACHE_DIR, { recursive: true })
  writeFileSync(MANIFEST_PATH, JSON.stringify(manifest, null, 2))
}

// ── Subprocess(对齐标杆:execFile + 转发 stdout/stderr) ───────

function execFileAsync(file: string, args: string[], opts?: { cwd?: string }): Promise<void> {
  return new Promise((resolve, reject) => {
    execFile(file, args, { cwd: opts?.cwd ?? PROJECT_ROOT }, (err, stdout, stderr) => {
      if (stdout) process.stdout.write(stdout)
      if (stderr) process.stderr.write(stderr)
      if (err) reject(err)
      else resolve()
    })
  })
}

// ── Main ────────────────────────────────────────────────────────

async function main() {
  logStep('C-Journey Build — VitePress single-build + content-hash cache')
  log(`  Project:     ${PROJECT_ROOT}`)
  log(`  Stages:      ${STAGES.map((s) => s.dir).join(', ')}`)
  log(`  Concurrency: ${CONCURRENCY} (占位:单次 build 不并行;未来分段化时启用)`)
  log(`  Force:       ${FORCE_REBUILD}`)
  log(`  Memory:      ${memMB()}`)
  const start = Date.now()

  const manifest = readManifest()

  // ── Step 1/3: 算输入 hash、判断是否命中缓存 ─────────────────
  logStep('Step 1/3: Hashing build inputs')

  const cacheKeyVerbose = hashBuildInputs()
  const [cacheKey, partsLine] = cacheKeyVerbose.split('|', 2)
  log(`  Input hash:  ${cacheKey}`)
  log(`  Components:  ${partsLine}`)

  const prev = manifest['site']
  const cached =
    !FORCE_REBUILD &&
    prev && prev.hash === cacheKey &&
    existsSync(CACHED_OUTPUT) &&
    // 缓存目录得真有内容(非空),否则视为脏缓存。
    readdirSync(CACHED_OUTPUT).length > 0

  const totalMd = STAGES.reduce(
    (n, s) => n + countMdFiles(join(DOCUMENTS, s.dir)), 0,
  ) + countMdFiles(join(DOCUMENTS, 'index.md'))

  if (cached) {
    // ── Step 2/3 (cached): 复用上次 dist ─────────────────────
    logStep('Step 2/3: Cache HIT — reusing previous dist (no VitePress build)')
    log(`  Last built:  ${prev!.timestamp}`)
    ensureClean(DIST_FINAL)
    cpSync(CACHED_OUTPUT, DIST_FINAL, { recursive: true })
    log(`  Restored ${totalMd} md → ${relative(PROJECT_ROOT, DIST_FINAL)}`)
  } else {
    // ── Step 2/3 (miss): 真正跑 vitepress build documents ────
    logStep(prev ? 'Step 2/3: Cache MISS — running full VitePress build'
                 : 'Step 2/3: No prior cache — running full VitePress build')
    if (prev) log(`  Why miss:   prev=${prev.hash} ≠ now=${cacheKey}`)
    log(`  Building ${totalMd} md files (root=documents)...`)

    ensureClean(DIST_FINAL)

    const t0 = Date.now()
    /**
     * 关键差异点(对齐标杆的设计意图):
     * 标杆为每个 volume 生成临时 config + symlink theme,再 `vitepress build <tmpSite>`。
     * C-Journey 的 config/theme 就在 documents/.vitepress/ 下、且 documents/ 本身就是
     * vitepress 的 root,所以直接 `vitepress build documents` 即可 —— 无需临时目录、
     * 无需 symlink。这是「选改动小、不破坏现有 config/theme 位置」的直接体现。
     */
    await execFileAsync(process.execPath, [VITEPRESS_BIN, 'build', 'documents'])
    const elapsed = ((Date.now() - t0) / 1000).toFixed(1)

    if (!existsSync(DIST_FINAL)) {
      throw new Error(`build 完成 but ${relative(PROJECT_ROOT, DIST_FINAL)} 不存在`)
    }
    log(`  ✓ built in ${elapsed}s (${totalMd} files, ${memMB()})`)

    // 把产物存进缓存(标杆 buildVolume 末尾的同款逻辑)。
    mkdirSync(CACHE_DIR, { recursive: true })
    if (existsSync(CACHED_OUTPUT)) rmSync(CACHED_OUTPUT, { recursive: true })
    cpSync(DIST_FINAL, CACHED_OUTPUT, { recursive: true })
  }

  // ── Step 3/3: 写 manifest + 汇总 ───────────────────────────
  logStep('Step 3/3: Finalizing')

  const newManifest: Manifest = {
    site: { hash: cacheKey, timestamp: new Date().toISOString() },
  }
  writeManifest(newManifest)

  let outputFiles = 0
  function countFiles(d: string) {
    for (const e of readdirSync(d, { withFileTypes: true })) {
      if (e.isDirectory()) countFiles(join(d, e.name))
      else outputFiles++
    }
  }
  if (existsSync(DIST_FINAL)) countFiles(DIST_FINAL)

  const elapsed = ((Date.now() - start) / 1000).toFixed(1)
  log(`\n  ═══ Build Summary ═══`)
  log(`  Status:   ✓ SUCCESS`)
  log(`  Mode:     ${cached ? 'cached (0 VitePress build)' : 'full build'}`)
  log(`  Time:     ${elapsed}s`)
  log(`  Output:   ${relative(PROJECT_ROOT, DIST_FINAL)} (${outputFiles} files)`)
  log(`  Memory:   ${memMB()}`)
  if (!cached) {
    log(`  Cached:   ${relative(PROJECT_ROOT, CACHED_OUTPUT)} (下次内容不变则秒级复用)`)
  }
  log(`  Tip:      用 --force 强制全量重建;改 config/theme/任一阶段 .md 会自动触发重建`)
}

main().catch((err) => {
  log('\n  BUILD FAILED')
  console.error(err)
  process.exit(1)
})

/* ────────────────────────────────────────────────────────────────
 * 未来升级路径(当内容体量让「分段独立 build」真的划算时):
 *
 * 1. 把 STAGES 升级成「每个 stage 一个独立 build 任务」:为每个 stage 生成临时
 *    config.ts(只挂该 stage 的 sidebar,参考标杆 generateVolumeConfig)、symlink
 *    documents/.vitepress/theme 到临时 site、并行 `vitepress build <tmpSite>`。
 * 2. 合并 dist:cpSync 每个 stage 的 output 到 DIST_FINAL(标杆 main 的 runParallel 段)。
 * 3. 合并搜索索引:标杆 mergeSearchIndexes / mergeSerializedSearchIndexes 整段搬过来
 *    (VitePress 的 local search 每个 build 各自产出,跨 build 必须合并)。
 * 4. 统一 hash map:标杆 unifyCrossVolumeData —— 多 build 的 HTML 里 __VP_HASH_MAP__
 *    各自只有本 volume 的条目,要让 SPA 路由跨 volume 跳转就必须合并成一份。
 *
 * 现在不做的理由:4 个 stage 合起来仍只占一次 build 的内存峰值,分段后总耗时基本
 * 不变(还要多算 4 次 config 加载 + 合并开销),收益 < 工程成本。等阶段数 ×2 或
 * 单 stage 章节数破百再考虑。
 * ────────────────────────────────────────────────────────────── */
