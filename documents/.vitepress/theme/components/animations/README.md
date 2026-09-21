# C-Journey 教学动画

声明式 SVG 教学动画:YAML DSL 编译成 JSON 数据 + 共用播放器,文章里一行接入。
系统移植自 TAMCPP(AnimPlayer 与数据格式同源,播放器零 npm 依赖、SSG 安全)。

## 文章侧用法

```md
<Anim id="pointer-arithmetic" />
```

- `id` = `generated/data/**/<id>.json` 的文件名,全局唯一;分组目录不影响引用。
- 必须写 PascalCase `<Anim … />`;`<anim>` 小写会被 VitePress 当 customElement 静默不渲染。
- 组件已全局注册(theme/index.ts),页面零 import。

## 目录约定

```
theme/components/
├── Anim.vue                    # 入口(id → 数据),import.meta.glob 自动收集
└── animations/
    ├── README.md               # 本文件(本仓接入约定)
    └── generated/              # 编译产物,整体勿手改
        ├── AnimPlayer.vue      # 共用播放器(步进/倍速/懒播放,SSG 安全)
        └── data/<stage>/<id>.json   # 一条动画一份(分组=阶段,如 stage2)
```

## 新增一条动画(内容生产)

写 YAML DSL → 用 animy_maker 编译(工具仓在 TAMCPP 的
`.claude/tools/animy_maker`,PYTHONPATH 复用):

```bash
PYTHONPATH=/home/charliechen/Tutorial_AwesomeModernCPP/.claude/tools/animy_maker \
  python3 -m animation_maker compile scene.yaml --backend web \
  -o documents/.vitepress/theme/components/animations/generated
```

DSL 的 `group:` 字段决定 data/ 下的子目录。播放器已存在时编译器默认跳过
(`--force-player` 才覆盖)。TAMCPP 的 vol1/pointer-arithmetic 即纯 C 语义
(int 数组 + 指针步进),本仓首条动画直接复用了它的数据。

## 配色说明

数据 JSON 内嵌 palette,当前沿用 TAMCPP 的深色「放映幕布」风(深底 + 语义色),
像深色代码块一样嵌在浅色页面里,视觉自洽;若将来要统一成锈橙暖色,
改数据的 palette 字段即可,播放器不用动。
