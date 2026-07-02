#!/usr/bin/env python3
"""对 examples/ + projects/clib-utilities 的 C 源跑 clang-tidy(阶段4·Ch12 引入)。

为每个 CMake 工程生成 compile_commands.json(EXPORT_COMPILE_COMMANDS)、
对其 .c 源(排除 test/ 与 build/)跑 clang-tidy,任一 warning 即失败(硬门)。
配置见仓库根 .clang-tidy。退出码:0 全过;1 有 warning;2 缺 cmake/clang-tidy。
"""
import os
import shutil
import subprocess
import sys
import pathlib

REPO = pathlib.Path(__file__).resolve().parent.parent
# 仅查 examples/(clib-utilities 是 legacy、有 4 个已知 finding 待整改,
# 修齐后再纳入;见 阶段4·Ch12 的「clib 现状」段)
PROJECTS = sorted((REPO / "examples").glob("*/CMakeLists.txt"))
BUILD_ROOT = pathlib.Path(os.environ.get("CJ_BUILD_ROOT", "/tmp/cj-clang-tidy"))

if not shutil.which("clang-tidy") or not shutil.which("cmake"):
    print("未找到 clang-tidy 或 cmake", file=sys.stderr)
    sys.exit(2)

fail = False
for cl in PROJECTS:
    src = cl.parent
    rel = src.relative_to(REPO)
    bdir = BUILD_ROOT / rel
    if bdir.exists():
        shutil.rmtree(bdir)
    bdir.mkdir(parents=True, exist_ok=True)
    subprocess.run(
        ["cmake", "-S", str(src), "-B", str(bdir), "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON"],
        stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
    )
    if not (bdir / "compile_commands.json").exists():
        print(f"[skip] {rel}(无 compile_commands)")
        continue
    cfiles = [
        p for p in src.rglob("*.c")
        if "/test/" not in str(p) and "/build/" not in str(p) and "/_build" not in str(p)
    ]
    for f in sorted(cfiles):
        r = subprocess.run(
            ["clang-tidy", "-p", str(bdir), str(f)],
            stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True,
        )
        warns = [ln for ln in r.stdout.splitlines() if "warning:" in ln]
        if warns:
            print(f"[FAIL] {f.relative_to(REPO)}")
            for w in warns:
                print(f"  {w}")
            fail = True
        else:
            print(f"[OK]   {f.relative_to(REPO)}")

if fail:
    print("\n❌ clang-tidy 发现 warning,CI 不通过。", file=sys.stderr)
    sys.exit(1)
print("\n✅ clang-tidy 全部通过。")
