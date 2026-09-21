"""C-Journey 单一标签源(single source of truth)—— 本文件是 tags.json 的薄加载器。

标签的分类/中英文名/角色全部在 **scripts/tags.json**(Python 与 TS 共读:
TS 侧见 documents/.vitepress/tags-manifest.ts)。本文件把 JSON 摊平成
validate_frontmatter.py 等下游习惯的集合名,**改标签只改 tags.json**。

难度/C 标准/章号不是标签,仍留在本文件。
"""

import json
from pathlib import Path

_DATA = json.loads(
    (Path(__file__).resolve().parent / "tags.json").read_text(encoding="utf-8")
)
_CATEGORIES = _DATA["categories"]

# 平台标签(描述代码运行目标;正交筛选器,不进标签墙)
PLATFORM_TAGS = {
    t for c in _CATEGORIES if c["role"] == "platform" for t in c["tags"]
}

# 主题标签(描述所属知识域;每个文档至少打一个,进标签墙)
TOPIC_TAGS = {
    t for c in _CATEGORIES if c["role"] == "topic" for t in c["tags"]
}

# 全部合法标签
ALL_TAGS = PLATFORM_TAGS | TOPIC_TAGS

# 难度
DIFFICULTIES = {"beginner", "intermediate", "advanced"}

# 支持的 C 标准(c_standard frontmatter 取值)
C_STANDARDS = {89, 90, 99, 11, 17, 23}

# 课程阶段编号(0..7 为正式阶段);chapter 亦接受任意整数(供 advanced/misc 用)
CHAPTERS = {0, 1, 2, 3, 4, 5, 6, 7}
EXTRA_CHAPTERS = {"advanced"}


def is_valid_tag(tag) -> bool:
    """tag 可能是 str 也可能是 YAML 解析出的 int(如 8051),统一按 str 比较。"""
    return str(tag) in ALL_TAGS
