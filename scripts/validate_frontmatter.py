#!/usr/bin/env python3
"""校验 documents/ 下所有 Markdown 的 frontmatter。

规则:
  - 每个文档必须有 frontmatter(YAML 头),且必含 title。
  - 其余字段(chapter / order / difficulty / platform / c_standard / tags)
    出现就必须合法;缺失仅作提示,不致命(便于逐步收紧)。
退出码:0 通过;1 有错误;2 缺 PyYAML。
依赖:PyYAML(pip install pyyaml)。
"""
import re
import sys
import pathlib

try:
    import yaml
except ImportError:
    print("ERROR: 需要 PyYAML。请 `pip install pyyaml`", file=sys.stderr)
    sys.exit(2)

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
from tags import (  # noqa: E402
    DIFFICULTIES, PLATFORM_TAGS, C_STANDARDS, CHAPTERS, EXTRA_CHAPTERS, is_valid_tag,
)

REPO = pathlib.Path(__file__).resolve().parent.parent
DOCS = REPO / "documents"
REQUIRED = ["title"]
RECOMMENDED = ["chapter", "order", "tags"]

errors = []
checked = 0


def parse_frontmatter(text):
    if not text.startswith("---"):
        return None
    m = re.match(r"^---\s*\n(.*?)\n---\s*\n?(.*)$", text, re.DOTALL)
    if not m:
        return None
    return yaml.safe_load(m.group(1)) or {}


def as_list(v):
    return v if isinstance(v, list) else [v]


for md in sorted(DOCS.rglob("*.md")):
    if md.name == "README.md":  # 导航/索引文档,不要求 frontmatter
        continue
    rel = md.relative_to(REPO)
    text = md.read_text(encoding="utf-8")
    try:
        fm = parse_frontmatter(text)
    except yaml.YAMLError as e:
        errors.append(f"{rel}: frontmatter YAML 解析失败: {e}")
        continue
    if fm is None:
        errors.append(f"{rel}: 缺少 frontmatter(需以 --- 开头的 YAML 头)")
        continue
    checked += 1

    for k in REQUIRED:
        if k not in fm:
            errors.append(f"{rel}: 缺少必填字段 {k}")

    ch = fm.get("chapter")
    if ch is not None and not (isinstance(ch, int) or ch in EXTRA_CHAPTERS):
        errors.append(f"{rel}: chapter={ch!r} 非法(应为整数阶段号 或 'advanced')")

    if "order" in fm and not isinstance(fm["order"], int):
        errors.append(f"{rel}: order 应为整数,实际 {fm['order']!r}")

    d = fm.get("difficulty")
    if d is not None and d not in DIFFICULTIES:
        errors.append(f"{rel}: difficulty={d!r} 非法(应为 {sorted(DIFFICULTIES)})")

    p = fm.get("platform")
    if p is not None and p not in PLATFORM_TAGS:
        errors.append(f"{rel}: platform={p!r} 非法(应为 {sorted(PLATFORM_TAGS)})")

    cs = fm.get("c_standard")
    if cs is not None:
        for v in as_list(cs):
            if v not in C_STANDARDS:
                errors.append(f"{rel}: c_standard 含非法值 {v!r}(应为 {sorted(C_STANDARDS)})")

    tags = fm.get("tags")
    if tags is not None:
        for t in as_list(tags):
            if not is_valid_tag(t):
                errors.append(f"{rel}: 未知标签 {t!r}(合法集合见 scripts/tags.py)")

print(f"已校验 {checked} 个文档。")
if errors:
    print(f"\n❌ 发现 {len(errors)} 处问题:")
    for e in errors:
        print(f"  - {e}")
    sys.exit(1)
print("✅ frontmatter 全部通过。")
