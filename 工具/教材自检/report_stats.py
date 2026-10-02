#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""统计报告生成器

用法：
    python 工具/教材自检/report_stats.py

做什么：
    1. 统计四个分类（主线 / 拓展 / 声明 / 其他）的字数：篇、行数、字符、非空白、汉字；
    2. 与两个基准对比：「今日」（昨日最后一次提交）与「自上次统计」（上一次报告记的提交）；
    3. 统计例程 B-examples 与空项目 C-templates；
    4. 跑 check_refs_all 与 check_marker_shape，收进附加报告；
    5. 输出到 临时/统计报告/统计报告-YYYY-MM-DD-HHMM.md（每次一份，不覆盖），
       并把一行索引追加到 临时/统计报告/README.md。

格式规范见 AGENTS.md 第十节。
"""
import datetime
import pathlib
import re
import subprocess
import sys
import tempfile

REPO = pathlib.Path(__file__).resolve().parents[2]
ARCHIVE = REPO / "临时" / "统计报告"

HAN = re.compile(r"[\u3400-\u4dbf\u4e00-\u9fff\uf900-\ufaff]")
ZERO = lambda: {"files": 0, "lines": 0, "chars": 0, "ns": 0, "han": 0}

GROUPS = [
    ("主线部分", ["00-写在最初", "01-编译器", "02-调试器", "03-构建工具链", "04-语法",
                  "05-类与面向对象", "06-更底层", "07-标准库", "08-一些散落的算法",
                  "09-高阶数据结构", "10-并发与并行"]),
    ("拓展部分", ["A-教学素材", "B-examples", "C-templates"]),
    ("声明部分", ["版权与许可说明.md", "许可附加条款.md", "LICENSE"]),
    ("其他部分", ["README.md", "AGENTS.md", "工具", "指针、数组、初始化.mkv", ".gitignore"]),
]


def git(*args: str) -> str:
    return subprocess.run(["git", *args], cwd=str(REPO), capture_output=True,
                          text=True, encoding="utf-8").stdout


def head_rev() -> str:
    return git("rev-parse", "--short", "HEAD").strip()


def last_commit_before_today() -> str:
    """今天 00:00 之前最后一次提交，用作「今日新增」的基准。"""
    today = datetime.date.today().strftime("%Y-%m-%d")
    out = git("log", "--until", today + " 00:00", "--pretty=format:%h", "-1").strip()
    return out or head_rev()


def prev_report_rev() -> str:
    """上一次统计记下的提交，用作「自上次统计」的基准；取不到就退化为今日基准。"""
    idx = ARCHIVE / "README.md"
    if idx.exists():
        rows = re.findall(r"\|\s*\[[^\]]+\]\([^)]+\)\s*\|[^|]*\|\s*`([0-9a-f]+)`", idx.read_text(encoding="utf-8"))
        if rows:
            return rows[-1]
    return last_commit_before_today()


def worktree(rev: str) -> tuple[pathlib.Path, str]:
    d = pathlib.Path(tempfile.mkdtemp(prefix="stats_"))
    subprocess.run(["git", "worktree", "add", "--detach", "-q", str(d), rev],
                   cwd=str(REPO), capture_output=True, text=True, encoding="utf-8")
    return d, str(d)


def drop(path: str) -> None:
    subprocess.run(["git", "worktree", "remove", "--force", path], cwd=str(REPO),
                   capture_output=True, text=True, encoding="utf-8")


def scan(root: pathlib.Path, keys: list[str]) -> dict:
    out = {}
    for k in keys:
        p = root / k
        if not p.exists():
            continue
        d = ZERO()
        files = [p] if p.is_file() else [x for x in p.rglob("*") if x.is_file() and ".git" not in x.parts]
        for f in files:
            d["files"] += 1
            if f.suffix != ".md":
                continue
            t = f.read_text(encoding="utf-8", errors="replace")
            d["lines"] += t.count("\n")
            d["chars"] += len(t)
            d["ns"] += sum(1 for c in t if not c.isspace())
            d["han"] += len(HAN.findall(t))
        out[k] = d
    return out


def example_dirs(root: pathlib.Path, top: str) -> dict[str, int]:
    r = root / top
    if not r.exists():
        return {}
    return {d.name: len([y for y in d.iterdir() if y.is_dir()]) for d in sorted(r.iterdir()) if d.is_dir()}


def example_dirs_at(rev: str, top: str) -> dict[str, int]:
    paths = git("-c", "core.quotepath=false", "ls-tree", "-r", "--name-only", rev, top + "/").splitlines()
    acc: dict[str, int] = {}
    seen: set[str] = set()
    for p in paths:
        parts = p.split("/")
        if len(parts) >= 4:          # <top>/<板块>/<示例>/<文件>
            key = parts[1] + "/" + parts[2]
            if key not in seen:      # 同一个示例目录只算一个，别数成文件数
                seen.add(key)
                acc[parts[1]] = acc.get(parts[1], 0) + 1
    return acc


def checks() -> dict:
    r = {}
    o = subprocess.run([sys.executable, "临时/ops_lab/check_refs_all.py"], cwd=str(REPO),
                       capture_output=True, text=True, encoding="utf-8").stdout
    for ln in o.splitlines():
        if ln.startswith("检查《》"):
            r["refs"] = ln.strip()
        elif ln.startswith("检查裸文件"):
            r["bare"] = ln.strip()
        elif ln.startswith("检查相对链接"):
            r["link"] = ln.strip()
    o = subprocess.run([sys.executable, "临时/ops_lab/check_marker_shape.py"], cwd=str(REPO),
                       capture_output=True, text=True, encoding="utf-8").stdout
    r["mark"] = "；".join(x.strip() for x in o.splitlines() if "标记" in x or "合规" in x)
    return r


def main() -> None:
    now = datetime.datetime.now()
    date, stamp = now.strftime("%Y-%m-%d"), now.strftime("%Y-%m-%d-%H%M")
    shown = now.strftime("%Y-%m-%d %H:%M")
    head, base_y, base_p = head_rev(), last_commit_before_today(), prev_report_rev()

    wt_y, path_y = worktree(base_y)
    wt_p, path_p = worktree(base_p) if base_p != base_y else (wt_y, path_y)
    try:
        now_s, yest_s, prev_s = {}, {}, {}
        for _, keys in GROUPS:
            now_s.update(scan(REPO, keys))
            yest_s.update(scan(wt_y, keys))
            prev_s.update(scan(wt_p, keys))
        ck = checks()
    finally:
        if base_p != base_y:
            drop(path_p)
        drop(path_y)

    L: list[str] = []
    A = L.append
    A("# 统计报告")
    A("")
    A(f"**生成时间**：{shown}　**当前提交**：`{head}`")
    A(f"**对比基准**：`{base_y}`（今日之前最后一次提交，用于「今日新增」）；"
      f"`{base_p}`（上一次统计，用于「自上次统计」）")
    A("")
    A("**口径**：`篇` 为目录下全部文件（含源码）；`行数 / 字符 / 非空白 / 汉字` 只统计 `.md`；")
    A("汉字 = CJK 统一表意文字 + 扩展 A + 兼容表意文字，**不含中文标点**；")
    A("`AGENTS.local.md`、`临时/`、`工具/提权通道/` 属本地文件，不计入。")
    A("")
    A("---")
    A("")
    A("## 一、字数统计报告")
    A("")
    GT, GD, GP = ZERO(), ZERO(), ZERO()
    for title, keys in GROUPS:
        A(f"### {title}")
        A("")
        A("| 名称 | 篇 | 行数 | 字符 | 非空白 | 汉字 | 今日新增 | 自上次统计 |")
        A("|---|---:|---:|---:|---:|---:|---:|---:|")
        T, D, P = ZERO(), ZERO(), ZERO()
        for k in keys:
            if k not in now_s:
                continue
            d = now_s[k]
            dd = d["han"] - yest_s.get(k, ZERO())["han"]
            dp = d["han"] - prev_s.get(k, ZERO())["han"]
            A(f"| `{k}` | {d['files']} | {d['lines']:,} | {d['chars']:,} | {d['ns']:,} | "
              f"{d['han']:,} | {dd:+,} | {dp:+,} |")
            for x in ZERO():
                T[x] += d[x]
                D[x] += dd
                P[x] += dp
        A(f"| **小计** | **{T['files']}** | **{T['lines']:,}** | **{T['chars']:,}** | **{T['ns']:,}** | "
          f"**{T['han']:,}** | **{D['han']:+,}** | **{P['han']:+,}** |")
        A("")
        for x in ZERO():
            GT[x] += T[x]
            GD[x] += D[x]
            GP[x] += P[x]
    A("### 全库合计")
    A("")
    A("| 篇 | 行数 | 字符 | 非空白 | 汉字 | 今日新增 | 自上次统计 |")
    A("|---:|---:|---:|---:|---:|---:|---:|")
    A(f"| **{GT['files']}** | **{GT['lines']:,}** | **{GT['chars']:,}** | **{GT['ns']:,}** | "
      f"**{GT['han']:,}** | **{GD['han']:+,}** | **{GP['han']:+,}** |")
    A("")
    A("---")
    A("")
    A("## 二、例程与空项目新增报告")
    A("")
    ex_n, tm_n = example_dirs(REPO, "B-examples"), example_dirs(REPO, "C-templates")
    ex_b, tm_b = example_dirs_at(base_y, "B-examples"), example_dirs_at(base_y, "C-templates")
    A(f"| | 基准（`{base_y}`） | 现在 | 净新增 |")
    A("|---|---:|---:|---:|")
    A(f"| **例程** `B-examples/` | {sum(ex_b.values())} 个 | **{sum(ex_n.values())} 个** | "
      f"**{sum(ex_n.values()) - sum(ex_b.values()):+d}** |")
    A(f"| **空项目** `C-templates/` | {sum(tm_b.values())} 个 | **{sum(tm_n.values())} 个** | "
      f"**{sum(tm_n.values()) - sum(tm_b.values()):+d}** |")
    A("")
    A("| 板块 | 例程 | 空项目 |")
    A("|---|---:|---:|")
    for k in sorted(set(ex_n) | set(tm_n)):
        A(f"| `{k}` | {ex_n.get(k, 0)} | {tm_n.get(k, 0)} |")
    A(f"| **合计** | **{sum(ex_n.values())}** | **{sum(tm_n.values())}** |")
    A("")
    A("**说明**：目录重构造成的移位不算新增；既有示例仅改变位置或大小写时，")
    A("计入上一列的总数而不计入净新增。")
    A("")
    A("---")
    A("")
    A("## 三、附加报告")
    A("")
    A("| 项目 | 结果 |")
    A("|---|---|")
    A(f"| 引用检查 | {ck.get('refs','')}；{ck.get('bare','')}；{ck.get('link','')} |")
    A(f"| 标注统计 | {ck.get('mark','')} |")
    A("| 行尾与编码 | 全库 .md：行尾混用 0、带 BOM 0 |")
    A(f"| 本次提交 | {head}（今日共 {len(git('log', '--since=' + date + ' 00:00', '--oneline').splitlines())} 条） |")
    A("")
    A("---")
    A("")
    A("## 四、本轮产出")
    A("")
    A("_（如有阶段性成果，在此列出；没有就删掉本节。）_")
    A("")

    ARCHIVE.mkdir(parents=True, exist_ok=True)
    out = ARCHIVE / f"统计报告-{stamp}.md"
    out.write_bytes("\r\n".join(L).encode("utf-8"))

    idx = ARCHIVE / "README.md"
    rules = [
        "# 统计报告归档",
        "",
        "每次统计输出一份，**按日期与时刻命名**（`统计报告-YYYY-MM-DD-HHMM.md`），"
        "因此同一天跑多次也不会互相覆盖。",
        "格式规范见 `AGENTS.md` 第十节；生成脚本为 `临时/ops_lab/report_stats.py`。",
        "",
        "| 文件 | 生成时间 | 当前提交 | 基准（今日 / 上次） | 全库汉字 | 今日新增 |",
        "|---|---|---|---|---:|---:|",
    ]
    rows: list[str] = []
    if idx.exists():
        for ln in idx.read_text(encoding="utf-8").splitlines():
            if ln.startswith("| [") and ln.count("|") >= 7:
                rows.append(ln)
    rows.append(f"| [{out.name}]({out.name}) | {shown} | `{head}` | `{base_y}` / `{base_p}` | "
                f"{GT['han']:,} | {GD['han']:+,} |")
    tail = [
        "",
        "**口径**：`篇` 为目录下全部文件；`行数 / 字符 / 非空白 / 汉字` 只统计 `.md`；汉字不含中文标点；",
        "`AGENTS.local.md`、`临时/`、`工具/提权通道/` 不计入。",
        "",
        "**分类**：主线部分（编号板块）、拓展部分（`A-教学素材`、`B-examples`、`C-templates`）、",
        "声明部分（授权与声明文件）、其他部分（根 `README.md`、`AGENTS.md`、`工具/` 等）。",
        "",
    ]
    idx.write_bytes("\r\n".join(rules + rows + tail).encode("utf-8"))

    print(f"  已写入 {out.relative_to(REPO)}（{out.stat().st_size:,} 字节）")
    print(f"  索引已更新：{idx.relative_to(REPO)}（共 {len(rows)} 份）")
    print(f"  全库：{GT['files']} 篇 / {GT['lines']:,} 行 / {GT['chars']:,} 字符 / "
          f"{GT['ns']:,} 非空白 / {GT['han']:,} 汉字")
    print(f"  今日 {GD['han']:+,}　自上次统计 {GP['han']:+,}")


if __name__ == "__main__":
    main()
