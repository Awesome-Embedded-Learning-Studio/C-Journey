#!/usr/bin/env python3
"""构建并测试 examples/ 下所有 CMake 子项目 —— CI 质量门的核心。

- examples/**:硬门,任一失败则 CI 不通过(保证「每个示例都能编译」)。
- projects/**:报告模式,失败仅打印、不致命(遗留项目多为厂商 IDE 工程,
  正在按项目整改;清单见 KNOWN_LEGACY)。

退出码:0 examples 全过;1 examples 有失败;2 缺 cmake。
"""
import os
import sys
import shutil
import subprocess
import pathlib

REPO = pathlib.Path(__file__).resolve().parent.parent
BUILD_ROOT = pathlib.Path(os.environ.get("CJ_BUILD_ROOT", REPO / "_build_ci"))

# 遗留项目:报告模式(失败不阻塞 CI),逐个整改后从此处移除。
# (clib-utilities 已完成 GCC16/POSIX 整改,转为硬门。)
KNOWN_LEGACY = {
    "projects/embedded-mcu",     # Keil 工程,宿主机无法构建
    "projects/os-from-scratch",  # bochs/汇编,需专属工具链与镜像
    "projects/tiny-c-stdlib",    # Visual Studio 工程
    "projects/song",             # 依赖外部 mplayer
}

CMAKE = shutil.which("cmake")
CTEST = shutil.which("ctest")
GEN = "Ninja" if shutil.which("ninja") else "Unix Makefiles"


def run(cmd):
    return subprocess.run(cmd, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)


def build_one(cmakelists, fatal):
    src = cmakelists.parent
    rel = src.relative_to(REPO)
    bdir = BUILD_ROOT / rel
    if bdir.exists():
        shutil.rmtree(bdir)
    bdir.mkdir(parents=True)
    tag = "FATAL " if fatal else "report "

    r = run([CMAKE, "-S", str(src), "-B", str(bdir), "-G", GEN])
    if r.returncode != 0:
        print(f"[{tag}] CONFIGURE FAIL  {rel}")
        if fatal:
            print(r.stdout[-1500:])
        return False

    r = run([CMAKE, "--build", str(bdir)])
    if r.returncode != 0:
        print(f"[{tag}] BUILD FAIL      {rel}")
        if fatal:
            print(r.stdout[-1500:])
        return False

    if (bdir / "CTestTestfile.cmake").exists() and CTEST:
        r = run([CTEST, "--test-dir", str(bdir), "--output-on-failure"])
        if r.returncode != 0:
            print(f"[{tag}] TEST FAIL       {rel}")
            if fatal:
                print(r.stdout[-1500:])
            return False

    print(f"[{tag}] OK               {rel}")
    return True


def main():
    if not CMAKE:
        print("未找到 cmake,跳过构建。", file=sys.stderr)
        sys.exit(2)

    fatal_fail, report_fail = [], []

    print("== examples/  (硬门) ==")
    for cl in sorted((REPO / "examples").glob("**/CMakeLists.txt")):
        if not build_one(cl, fatal=True):
            fatal_fail.append(cl.parent.relative_to(REPO))

    print("\n== projects/  (非遗留=硬门 / 遗留=报告模式) ==")
    for cl in sorted((REPO / "projects").glob("**/CMakeLists.txt")):
        rel_s = str(cl.parent.relative_to(REPO))
        is_legacy = rel_s in KNOWN_LEGACY
        if not build_one(cl, fatal=not is_legacy):
            (report_fail if is_legacy else fatal_fail).append(cl.parent.relative_to(REPO))

    print("\n== 汇总 ==")
    print(f"examples 失败: {len(fatal_fail)}    projects 报告失败: {len(report_fail)}")
    if report_fail:
        print("  projects 已知问题(整改中,不阻塞 CI):")
        for f in report_fail:
            print(f"    - {f}")
    if fatal_fail:
        print(f"\n❌ examples 有 {len(fatal_fail)} 个失败,CI 不通过:")
        for f in fatal_fail:
            print(f"    - {f}")
        sys.exit(1)
    print("\n✅ examples 全部构建通过。")
    sys.exit(0)


if __name__ == "__main__":
    main()
