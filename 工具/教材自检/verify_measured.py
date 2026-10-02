#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""verify_measured.py —— 把教材里的「实测数据」变成可重复检验的东西。

做法：
  1. 抽出每个 .md 里的 C/C++ 代码块，连同首部注释里写的编译命令，以及随后
     带 `实测数据` 标记的输出块；
  2. 真编译、真运行（首部注释里有几条命令就跑几个构建；标了「故意编不过」的
     按预期失败处理，改用编译错误信息比对）；
  3. 逐行比对：结构量（容器大小、分配次数、字节数、比较结果……）必须逐位相同；
     计时量按数量级判，比值超过 5 倍才报 MISMATCH；
     正文块是节选时，只要每一行都能在某个程序的输出里找到就算过。

用法（在仓库根目录下跑）：
    python 工具/教材自检/verify_measured.py            # 默认查 README 与 A-00..A-03
    python 工具/教材自检/verify_measured.py --all09     # 查该板块全部 .md
    python 工具/教材自检/verify_measured.py --fresh     # 忽略缓存，全部重跑
报告写到 临时/ops_lab/verify_measured_report.txt（控制台只打印末尾摘要）。

脚本查不出的（不是它没跑，是它看不见）：
  - 程序里硬编码的 printf：程序会照打，脚本无法判断它其实没测过；
  - 正文散文里的数字：脚本只比对输出块，不比对句子；
  - 计时数字的跨机器可比性：只报比值，不断言对错。
"""

import argparse
import hashlib
import json
import os
import re
import subprocess
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
BOARD = os.path.join(ROOT, "09-高阶数据结构")
LAB = os.path.join(ROOT, "临时", "ops_lab")
CACHE = os.path.join(LAB, "_verify_cache")
WORK = os.path.join(LAB, "_verify_work")
REPORT = os.path.join(LAB, "verify_measured_report.txt")

DEFAULT_FILES = ["README.md",
                 "A-00-导读：数据结构是问题的形状.md",
                 "A-01-连续存储：array 与 vector.md",
                 "A-02-链式存储：list 与 forward_list.md",
                 "A-03-索引存储：map、set 与有序.md"]

FENCE = re.compile(r"^```(\S*)\s*$")
ADDR = re.compile(r"\b(?:0x)?[0-9a-fA-F]{8,16}\b")
NUM = re.compile(r"[-+]?\d+(?:\.\d+)?")
TIMING_HINT = re.compile(r"\bms\b|\bns\b|倍|MB\b|KB\b")
INTENTIONAL_FAIL = re.compile(r"故意编不过")
MSVC_BLOCK = re.compile(r"MSVC|cl\.exe|cl /")
TOLERANCE = 5.0


def read_lines(path):
    with open(path, encoding="utf-8", newline="") as f:
        return f.read().replace("\r\n", "\n").split("\n")


def parse_markdown(path, name):
    lines = read_lines(path)
    programs, outputs = [], []
    i = 0
    while i < len(lines):
        m = FENCE.match(lines[i])
        if not m:
            i += 1
            continue
        lang = m.group(1).lower()
        j = i + 1
        body = []
        while j < len(lines) and not lines[j].startswith("```"):
            body.append(lines[j])
            j += 1
        if lang in ("cpp", "c"):
            programs.append({"file": name, "line": i + 2, "code": "\n".join(body)})
        elif lang in ("text", "txt", ""):
            k, marked = i - 1, False
            while k >= 0 and i - k <= 4:
                if lines[k].strip() == "`实测数据`":
                    marked = True
                    break
                k -= 1
            outputs.append({"file": name, "line": i + 2, "code": "\n".join(body),
                            "marked": marked,
                            "context": "\n".join(lines[max(0, i - 6):i])})
        i = j + 1
    return programs, outputs


def program_name(code):
    m = re.search(r"([A-Za-z_0-9]+\.(?:cpp|c))", code)
    return m.group(1) if m else "unnamed.cpp"


def compile_commands(code):
    head = "\n".join(code.split("\n")[:8])
    out = []
    for raw in head.split("\n"):
        line = raw.strip().lstrip("*").strip()
        if not ("g++" in line or re.search(r"\bcl(\.exe)?\s+/", line)):
            continue
        c = re.sub(r"^[^:：]*(?:编译|构建)[^:：]*[:：]\s*", "", line).strip()
        if re.search(r"\bcl(\.exe)?\s+/", c):
            out.append(("msvc", c))
            continue
        toks, i2, flags = c.split(), 0, []
        while i2 < len(toks):
            t = toks[i2]
            if t == "-o":
                i2 += 2
                continue
            if t.startswith("-") and not t.endswith(".cpp"):
                flags.append(t)
            i2 += 1
        out.append(("gcc", " ".join(flags) if flags else "-std=c++17 -O2"))
    return out


def ensure_dirs():
    os.makedirs(CACHE, exist_ok=True)
    os.makedirs(WORK, exist_ok=True)


def build_and_run(name, code, flags, fresh, timeout=900):
    key = hashlib.sha1((code + "||" + flags).encode("utf-8")).hexdigest()[:16]
    cpath = os.path.join(CACHE, key + ".json")
    if os.path.exists(cpath) and not fresh:
        with open(cpath, encoding="utf-8") as f:
            return json.load(f)
    d = os.path.join(WORK, key)
    os.makedirs(d, exist_ok=True)
    with open(os.path.join(d, name), "w", encoding="utf-8", newline="\n") as f:
        f.write(code + "\n")
    exe = name.replace(".cpp", "") + ".exe"
    cc = subprocess.run(["g++"] + flags.split() + [name, "-o", exe], cwd=d,
                        capture_output=True, text=True, encoding="utf-8",
                        errors="replace")
    res = {"name": name, "flags": flags, "compile_ok": cc.returncode == 0,
           "compile_err": cc.stderr or "", "stdout": "", "run_ok": False, "run_err": ""}
    if cc.returncode == 0:
        try:
            r = subprocess.run([os.path.join(d, exe)], cwd=d, capture_output=True,
                               text=True, encoding="utf-8", errors="replace",
                               timeout=timeout)
            res["stdout"], res["run_ok"], res["run_err"] = r.stdout or "", True, r.stderr or ""
        except subprocess.TimeoutExpired:
            res["run_err"] = "TIMEOUT"
    with open(cpath, "w", encoding="utf-8") as f:
        json.dump(res, f, ensure_ascii=False)
    return res


def norm_line(s):
    s = ADDR.sub("<ADDR>", s.strip())
    s = re.sub(r"[^\s:'\"]*include/c\+\+/", "<INCLUDE>/", s)      # 工具链 include 前缀
    s = re.sub(r"<源文件>", "<SRC>", s)
    s = re.sub(r"[\w./\\-]+\.(?:cpp|c)\b", "<SRC>", s)             # 源码文件名
    s = re.sub(r"(<SRC>|[\w./\\-]+):\d+:\d+", "<SRC>:<L:C>", s)    # 行:列
    s = re.sub(r"\s+", " ", s)
    return s


def shape(s):
    return re.sub(r"[-+]?\d+(?:\.\d+)?", "#", norm_line(s))


STRUCT_HINT = re.compile(r"次数|字节|sizeof|容量|capacity|size|相同|成功|命中|距离|分配|"
                         r"地址|抛出|内容|顺序|标志")
TIMING_HINT = re.compile(r"\bms\b|\bns\b|倍数")


def classify(line):
    """计时行：带 ms/ns/倍数，或者「只有数字、没有结构词」的表行。"""
    d = norm_line(line)
    if TIMING_HINT.search(d) and re.search(r"\d", d):
        return "timing"
    if STRUCT_HINT.search(d):
        return "struct"
    if len(NUM.findall(d)) >= 2:
        return "timing"
    return "struct"


def numbers(s):
    return [float(x) for x in NUM.findall(norm_line(s))]


class Index:
    """所有程序输出行的索引：精确归一化行 + 骨架。"""

    def __init__(self):
        self.exact = {}
        self.by_shape = {}

    def add(self, text, source):
        for raw in text.split("\n"):
            if not raw.strip():
                continue
            n = norm_line(raw)
            self.exact.setdefault(n, source)
            self.by_shape.setdefault(shape(raw), []).append((raw.strip(), source, n))

    def find(self, doc_line):
        d = norm_line(doc_line)
        if d in self.exact:
            return ("OK", d, self.exact[d], "")
        cands = self.by_shape.get(shape(doc_line), [])
        if not cands:
            return ("NOT-FOUND", d, "", "")
        dn = numbers(doc_line)
        if not dn:
            raw, source, _ = cands[0]
            return ("MISMATCH", d, raw, "骨架相同但数值读不出")
        # 多行同骨架时（例如同一程序两份构建的输出、计时表的不同规模行），
        # 先按行首那个数配对，再在剩下的候选里取数值最接近的一行。
        if float(dn[0]).is_integer():
            same = [c for c in cands
                    if (numbers(c[0]) or [1e18])[0] == dn[0]]
            if same:
                cands = same
        best, best_worst = None, float("inf")
        for raw, source, _ in cands:
            an = numbers(raw)
            if len(an) != len(dn):
                continue
            worst = 1.0
            for x, y in zip(dn, an):
                lo, hi = min(abs(x), abs(y)), max(abs(x), abs(y))
                r = (hi / lo) if lo > 0 else (float("inf") if hi > 0 else 1.0)
                worst = max(worst, r)
            if worst < best_worst:
                best, best_worst = (raw, source), worst
        if best is None:
            return ("MISMATCH", d, cands[0][0], "骨架相同但数值个数对不上")
        raw, source = best
        if classify(doc_line) == "timing" and best_worst <= TOLERANCE:
            return ("TIMING-DRIFT", d, raw, "最大比值 %.2fx" % best_worst)
        return ("MISMATCH", d, raw, "最大比值 %.2fx（超容差）" % best_worst)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--all09", action="store_true")
    ap.add_argument("--fresh", action="store_true")
    args = ap.parse_args()

    ensure_dirs()
    files = sorted(f for f in os.listdir(BOARD) if f.endswith(".md")) if args.all09 \
        else DEFAULT_FILES

    programs, outputs = [], []
    for f in files:
        p, o = parse_markdown(os.path.join(BOARD, f), f)
        programs += p
        outputs += o

    lines_out = ["== verify_measured.py 报告 ==",
                 "板块：%s" % BOARD,
                 "扫描：%d 个文件，%d 个 C/C++ 代码块，%d 个输出块"
                 % (len(files), len(programs), len(outputs)), ""]

    index, fail_index = Index(), Index()
    problems, drifts = [], []
    for p in programs:
        name = program_name(p["code"])
        expected_fail = bool(INTENTIONAL_FAIL.search(p["code"].split("\n")[0]))
        cmds = compile_commands(p["code"])
        gccs = [c for k, c in cmds if k == "gcc"] or ["-std=c++17 -O2"]
        if any(k == "msvc" for k, _ in cmds):
            lines_out.append("[跳过 MSVC 构建] %s:%d %s" % (p["file"], p["line"], name))
        for flags in dict.fromkeys(gccs):
            r = build_and_run(name, p["code"], flags, args.fresh)
            if r["compile_ok"] and r["run_ok"]:
                tag = "OK"
                index.add(r["stdout"], "%s(%s)" % (name, flags))
                if r["compile_err"].strip():          # 编译告警也要能对上
                    fail_index.add(r["compile_err"], "%s(编译告警)" % name)
            elif expected_fail and not r["compile_ok"]:
                tag = "EXPECT-FAIL"
                fail_index.add(r["compile_err"], "%s(编译错误)" % name)
            else:
                tag = "PROBLEM"
                problems.append("%s:%d %s(%s) 编译=%s 运行=%s %s"
                                % (p["file"], p["line"], name, flags,
                                   r["compile_ok"], r["run_ok"],
                                   (r["compile_err"] or r["run_err"])[:200]))
            lines_out.append("[%s] %s:%d %s (%s)" % (tag, p["file"], p["line"], name, flags))

    lines_out += ["", "== 输出块逐行比对 =="]
    n_bad = 0
    for o in outputs:
        if not o["marked"]:
            continue
        # MSVC 侧的输出块：本机不复现 MSVC 构建，跳过（只报一句）
        if MSVC_BLOCK.search(o.get("context", "") + "\n" + o["code"][:200]):
            lines_out.append("  [跳过 MSVC 输出块] %s:%d" % (o["file"], o["line"]))
            continue
        rows = []
        for dl in o["code"].split("\n"):
            if not dl.strip():
                continue
            st, d, src, note = index.find(dl)
            if st == "NOT-FOUND":
                st2, d2, src2, note2 = fail_index.find(dl)
                if st2 == "OK":
                    rows.append(("OK(编译错误)", dl, src2, ""))
                    continue
                if "<源文件>" in dl or "<MinGW>" in dl or "<SRC>" in d:
                    rows.append(("SKIP-占位符", dl, "", "编译错误信息里的路径已换成占位符"))
                    continue
            rows.append((st, dl, src, note))
        bad = [x for x in rows if x[0] in ("MISMATCH", "NOT-FOUND")]
        drift = [x for x in rows if x[0] == "TIMING-DRIFT"]
        n_bad += len(bad)
        lines_out.append("  [%s] %s:%d（%d 行，%d 处不一致，%d 处计时漂移）"
                         % ("问题" if bad else "一致", o["file"], o["line"],
                            len(rows), len(bad), len(drift)))
        for st, dl, src, note in rows:
            if st in ("MISMATCH", "NOT-FOUND"):
                lines_out.append("      %-9s 正文: %s" % (st, dl.strip()))
                lines_out.append("      %-9s 实际: %s   %s %s"
                                 % ("", src.strip() if src else "（全库输出里都没有）", note,
                                    ("← 来自 " + src.split("(")[0]) if src else ""))
                problems.append("%s:%d %s 正文「%s」→ 实际「%s」"
                                % (o["file"], o["line"], st, dl.strip(), src.strip()))
        for st, dl, src, note in drift:
            lines_out.append("      %-9s 正文: %s | 实际: %s | %s"
                             % ("漂移", dl.strip(), src.strip(), note))
            drifts.append((o["file"], o["line"], dl.strip(), src.strip(), note))

    lines_out += ["", "== 汇总 ==",
                  "不一致：%d 处（结构量必须逐位相同；计时量比值 > %.0fx 才算不一致）"
                  % (n_bad, TOLERANCE),
                  "计时漂移（在容差内，仅供参考）：%d 处" % len(drifts)]
    with open(REPORT, "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(lines_out) + "\n")
    print("\n".join(lines_out[-14:]))
    print("\n完整报告：%s" % REPORT)
    return 0


if __name__ == "__main__":
    sys.exit(main())
