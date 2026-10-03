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

新规矩（2026-10-02 四条，2026-10-03 加真板跳过与两种节选标注写法；原因见 README 的
「已标节选的块不再报 PROBLEM」与「它查不出什么」）：

  - `AGENTS.local.md` 3.5 允许片段，前提是**明确标了节选**。标了的块跳过编译，
    在报告里单列 `[SKIP] 已标节选`，不再混进 `[PROBLEM]`；没标的照旧报 `[PROBLEM]`；
  - 编译命令写着「Linux 侧」或带 `-fsanitize=` 的块，本机 MinGW 缺 `-lasan`／
    `-lubsan` 运行库：先按 MinGW 编一次，编不过才送 WSL 再试。WSL 走
    `wsl -d Ubuntu`——本机默认发行版是 `kali-linux`，那里面没有编译器，
    不能用默认值；发行版名可用环境变量 `VERIFY_MEASURED_WSL` 换。
    本机没有 WSL 才跳过并说明原因；
  - 地址掩码不再吃 8 位以上的十进制数（`268435448`、`11649000.0`）。
    原先 `\b(?:0x)?[0-9a-fA-F]{8,16}\b` 会把它当地址，而吃不吃取决于
    计时值过没过 10^7，于是同一个块重跑一次就可能从「容差内漂移」翻成「MISMATCH」；
  - 正文按 `AGENTS.local.md` 3.12 第三节第 2 步把计时行改成**区间写法**
    （「约 2–12」）时，那一行**与紧跟在它下面的说明句**一起跳过逐行比对，
    报告里单列 `[SKIP] 区间写法`。区间本来就不会出现在程序输出里，
    逐行比对只会把它报成 `NOT-FOUND`——那既不是正文错，也不是程序错。
    护栏写在 `interval_line()` 里：判据是「**带「约」且形如区间的数值单元格**」，
    **不是**「这行有中文」「这行比较长」；把区间单元格挖掉之后行里还有别的数字的，
    照旧逐行比对（**写错的具体值仍然报 `MISMATCH`**），没有「约」前缀的单次值也照旧；
  - **真板程序**（2026-10-03 加）：编译命令是交叉工具链 `arm-none-eabi-gcc` 的块，
    输出要在 STM32F103C8 上经 semihosting 读回来。本机既没有这条工具链、也没有那块板子，
    照「本机没有 WSL 就跳过并写明原因」那条路处理：程序块报
    `[SKIP] …（需 ARM 交叉工具链与真板，本机跳过）`，它那几块输出（写着
    `STM32F103C8`／`Cortex-M3`）也跳过逐行比对；两者都不算 `[PROBLEM]`、不算不一致。
    判据只有两条：**块首部注释里的编译命令是那条交叉工具链**、
    **输出块自己写着芯片型号或内核**——不是「提到 Cortex-M 就跳过」这类宽判据；
  - **节选标注**多认两种写法（2026-10-03 加）：标注写在紧贴块上方／下方那一句正文里，
    位置在行尾（`……（下面是节选，第 1905 至 1918 行）：`）或行首（`（上面是节选，第 1807 行。）……`）。
    判据是**那句括号注本身**，别处散文里提一句节选位置对不上，照旧不算。

用法（在仓库根目录下跑）：
    python 工具/教材自检/verify_measured.py            # 默认查 README 与 A-00..A-03
    python 工具/教材自检/verify_measured.py --all09     # 查 09 板块全部 .md
    python 工具/教材自检/verify_measured.py --all08     # 查 08 板块全部 .md
    python 工具/教材自检/verify_measured.py --board 08  # 同上，写成板块号
    python 工具/教材自检/verify_measured.py --fresh     # 忽略缓存，全部重跑
09 的报告写到 临时/ops_lab/verify_measured_report.txt（与以前一致），
08 的写到 临时/ops_lab/verify_measured_report_08.txt（两份可以同时留着）；
控制台只打印末尾摘要，08 那次连板块名与扫描量一起打。

脚本查不出的（不是它没跑，是它看不见）：
  - 程序里硬编码的 printf：程序会照打，脚本无法判断那些数到底测没测过；
  - 正文散文里的数字：脚本只比对输出块，不比对句子；
  - 计时数字的跨机器可比性：只报比值，不断言对错；
  - 区间的宽窄是否如实：脚本只确认那行不是程序输出里的东西，判不了区间对不对。
"""

import argparse
import hashlib
import json
import os
import re
import subprocess
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
# 09 是原来那一条路、也是回归基线：板块目录与报告名与以前完全一致，一个字不动。
# 两个自测脚本靠改 BOARD／REPORT 这两个全局量把自己指到临时目录，因此 09 这条路
# 不在 main() 里重新赋值。
BOARD = os.path.join(ROOT, "09-高阶数据结构")
LAB = os.path.join(ROOT, "临时", "ops_lab")
CACHE = os.path.join(LAB, "_verify_cache")
WORK = os.path.join(LAB, "_verify_work")
REPORT = os.path.join(LAB, "verify_measured_report.txt")
# 08 是新加的入口（2026-10-03）：报告名带板块后缀，两次跑完两份报告都留着。
# 自测脚本要把它指到临时目录时，替换下面这两个量即可。
BOARD08 = os.path.join(ROOT, "08-一些散落的算法")
REPORT08_NAME = "verify_measured_report_08.txt"

DEFAULT_FILES = ["README.md",
                 "A-00-导读：数据结构是问题的形状.md",
                 "A-01-连续存储：array 与 vector.md",
                 "A-02-链式存储：list 与 forward_list.md",
                 "A-03-索引存储：map、set 与有序.md"]

FENCE = re.compile(r"^```(\S*)\s*$")
# 地址掩码。两侧不能用 \b，要用「不是词字符、也不是小数点」——
# 否则 8 位以上的十进制数（268435448 字节、11649000.0 ns）会被当成地址吃掉，
# 而吃不吃取决于数值有没有过 10^7，同一个块重跑一次就可能从「计时漂移在容差内」
# 翻成「MISMATCH」。真实地址（0x…、16 位十六进制）照旧认得出来。
ADDR = re.compile(r"(?<![\w.])(?:0x)?[0-9a-fA-F]{8,16}(?![\w.])")
NUM = re.compile(r"[-+]?\d+(?:\.\d+)?")
TIMING_HINT = re.compile(r"\bms\b|\bns\b|倍|MB\b|KB\b")
INTENTIONAL_FAIL = re.compile(r"故意编不过")
MSVC_BLOCK = re.compile(r"MSVC|cl\.exe|cl /")
TOLERANCE = 5.0

LABEL_LINE = re.compile(r"^`[^`]+`$")                    # `C++`、`实测数据` 这类标签行
EXCERPT_LINE = re.compile(r"^[（(].*节选.*[)）]$")         # 整行是一个带「节选」的括号注
# 紧贴在块上／下方的正文里写的同一句括号注（2026-10-03 加）。05 章引 libstdc++ 头文件
# 的那四段节选是这么标的：
#   上一行**以**它结尾   ``第一步与第三步在 `__sort` 里（下面是节选，第 1905 至 1918 行）：``
#   下一行**以**它开头   ``（上面是节选，第 1807 行。）基准从哪来由 `__unguarded…` 决定``
# 判据是**这句括号注本身**（「（下面是节选」「（上面是节选」这几个字 + 位置在行首／行尾），
# 不是「这行有中文」「块比较短」这类宽判据；别处散文里提一句节选（含 09 各章约定里
# 那句「凡是节选都写明（下面是节选）」）位置对不上，照旧不算。
EXCERPT_PHRASE = r"[（(](?:下面|上面)是节选[^）)]*[）)]"
EXCERPT_TAIL = re.compile(EXCERPT_PHRASE + r"[：:。，,]?$")
EXCERPT_HEAD = re.compile(r"^" + EXCERPT_PHRASE)
LINUX_HINT = re.compile(r"-fsanitize=|（Linux|\(Linux|Linux 侧|linux 侧")
# 真板程序（2026-10-03 加）：编译命令是交叉工具链 `arm-none-eabi-gcc`，
# 输出要靠 semihosting 从 STM32F103C8 上读回来。本机既没有这条工具链，
# 也没有那块板子——照「本机没有 WSL 就跳过并写明原因」那条路处理，归到 [SKIP]，
# 不算 [PROBLEM]，也不算不一致。判据只有一条：块首部注释里的编译命令是那条交叉
# 工具链（**不是**「这行提到 Cortex-M／STM32」这类宽判据）。
ARM_CC = re.compile(r"\barm-none-eabi-(?:gcc|g\+\+)")
# 真板输出块：输出自己写着芯片型号或内核，说明它来自那块板子（例如
# ``pid_board: STM32F103C8 / Cortex-M3 / HSI 8 MHz``）。本机跑不了，跳过逐行比对。
BOARD_OUT = re.compile(r"STM32F\d{3}|Cortex-M[0-9]|arm-none-eabi")

# 区间写法（`AGENTS.local.md` 3.12 第三节第 2 步）：计时量漂到 5 倍容差以外时，
# 正文把它改成**区间**，并写明这一行会随机器负载浮动。区间本来就不该出现在
# 程序输出里，逐行比对对它只会报 NOT-FOUND——那是约定与工具打架，不是正文错。
# 连接号认半角 `-` 与 `–`／`—`（正文用的是 `–`）。
INTERVAL = re.compile(r"约\s*[-+]?\d+(?:\.\d+)?\s*[–—-]\s*[-+]?\d+(?:\.\d+)?\s*"
                      r"(?:ms|ns|us|μs|倍)?")
# 区间行下面那句说明句的写法（只在紧跟着区间行时才认，见 interval_note()）
INTERVAL_NOTE = re.compile(r"随机器负载浮动|按区间给|不写单次值")
WSL_DISTRO = os.environ.get("VERIFY_MEASURED_WSL", "Ubuntu")


def read_lines(path):
    with open(path, encoding="utf-8", newline="") as f:
        return f.read().replace("\r\n", "\n").split("\n")


def excerpt_mark(lines, fence, close, body):
    """这个代码块标了节选吗。返回标记的位置（「块上方」／「块下方」／「块内首行」），
    没标返回空串。

    `AGENTS.local.md` 3.5 允许片段，前提是**明确标出**。扫描全库统计下来，
    实际写法有五种，位置不同但都算「已标」：

      1. 块上方单独一行   `` （下面是节选：只给数据成员与不变式） ``
      2. 块下方单独一行   `` （上面是节选：完整程序在附录 A。） ``
      3. 块内首行（注释） `` /* （下面是节选）C23 的检查算术 */ ``、`` // 节选自 x.c 第 10 行 ``
      4. 块上方那句正文**以**括号注结尾
         `` 第一步与第三步在 `__sort` 里（下面是节选，第 1905 至 1918 行）： ``
      5. 块下方那句正文**以**括号注开头
         `` （上面是节选，第 1807 行。）基准从哪来由 `__unguarded_partition_pivot` 决定 ``

    4、5 两种是 05 章引 libstdc++ 头文件时的写法：标注在，只是那句话前后还有别的字。
    判据只认这五种写法，**不用「块短于 N 行就放过」这类长度判据**——
    那会把没标的真碎片一起漏掉。
    """
    for step in range(4):                       # 1、4. 块上方
        k = fence - 1 - step
        if k < 0:
            break
        s = lines[k].strip()
        if not s or LABEL_LINE.match(s):
            continue
        if s.startswith("```"):
            break
        if EXCERPT_LINE.match(s):
            return "块上方"
        if EXCERPT_TAIL.search(s):
            return "块上方（写在那一句正文末尾）"
        break
    for step in range(4):                       # 2、5. 块下方
        k = close + 1 + step
        if k >= len(lines):
            break
        s = lines[k].strip()
        if not s or LABEL_LINE.match(s):
            continue
        if EXCERPT_LINE.match(s):
            return "块下方"
        if EXCERPT_HEAD.match(s):
            return "块下方（写在那一句正文开头）"
        break
    for raw in body[:3]:                        # 3. 块内首行
        s = raw.strip()
        if "节选" not in s:
            continue
        if s.startswith(("//", "/*", "*", "#", ";", "<!--")):
            return "块内首行"
    return ""


def interval_line(line):
    """整行是不是「区间写法」的计时行（`AGENTS.local.md` 3.12 第三节第 2 步）。

    判据两半，缺一不可：

      1. 行里**至少有一个**「约 A–B」形式的数值单元格（带「约」前缀，
         连接号是 `–`／`—`／`-`）；
      2. 把这些区间单元格**整段挖掉**之后，行里**再没有别的数字**。

    第 2 条是护栏：``遍历 11.925 约 1–9`` 这种混着写的行**不算**区间行 ——
    那个写错的具体值仍然会去逐行比对，仍然报 `MISMATCH`。

    判据里**没有**「这行有中文」「这行比较长」「带 ms／ns」这类宽判据：
    那些会把结构量行（节点大小、次数、字节数）一起放过，等于把检查关掉。
    """
    if not INTERVAL.search(line):
        return False
    return not NUM.search(INTERVAL.sub(" ", line))


def interval_note(line, after_interval):
    """区间行下面那句说明句：与区间行一起跳过。

    实测写法是「……重跑会随机器负载浮动，因此按区间给，不写单次值。」——
    它在程序输出里同样找不到，不跟着跳过就会一起报 `NOT-FOUND`。

    两个限制：只有**紧跟在区间行下面**时才算（`after_interval`），
    并且这句话本身**不含数字**。因此散文里提一句「随机器负载浮动」不会被放过，
    说明句也不会连带吃掉一行数据。
    """
    return bool(after_interval and INTERVAL_NOTE.search(line)
                and not NUM.search(line))


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
            programs.append({"file": name, "line": i + 2, "code": "\n".join(body),
                             "excerpt": excerpt_mark(lines, i, j, body)})
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
        if not ("g++" in line or ARM_CC.search(line)
                or re.search(r"\bcl(\.exe)?\s+/", line)):
            continue
        c = re.sub(r"^[^:：]*(?:编译|构建)[^:：]*[:：]\s*", "", line).strip()
        if ARM_CC.search(c):
            out.append(("arm", c))          # 交叉工具链：本机编不了，也跑不了
            continue
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


def cache_key(code, flags):
    return hashlib.sha1((code + "||" + flags).encode("utf-8")).hexdigest()[:16]


def cache_path(key):
    return os.path.join(CACHE, key + ".json")


def build_and_run(name, code, flags, fresh, timeout=900):
    key = cache_key(code, flags)
    cpath = cache_path(key)
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


def wsl_run(script, stdin=None, timeout=900):
    return subprocess.run(["wsl", "-d", WSL_DISTRO, "-e", "bash", "-c", script],
                          input=stdin, capture_output=True, text=True,
                          encoding="utf-8", errors="replace", timeout=timeout)


def wsl_build_and_run(name, code, flags, timeout=900):
    """Linux 侧构建：MinGW 不带 Sanitizer 运行库（`-lasan`／`-lubsan` 缺失），
    这一段只能在 Linux 上编。本机有 WSL，就把它送进去编、跑。

    返回 None 表示本机没有可用的 WSL（没装、发行版名不对），
    调用方按「跳过并说明」处理，不报 PROBLEM。

    源码走标准输入写进 `/tmp`，路径全 ASCII——避免中文仓库路径
    在 `wsl.exe` 这一层的编码问题。
    """
    key = cache_key(code, flags)
    d = "/tmp/verify_measured/" + key
    try:
        prep = wsl_run("mkdir -p %s && cat > %s/%s" % (d, d, name), code, timeout=120)
    except (OSError, subprocess.SubprocessError):
        return None
    if prep.returncode != 0:
        return None
    res = {"name": name, "flags": flags, "compile_ok": False, "compile_err": "",
           "stdout": "", "run_ok": False, "run_err": "", "via": "wsl"}
    try:
        cc = wsl_run("cd %s && g++ %s %s -o prog" % (d, flags, name), timeout=timeout)
        res["compile_err"], res["compile_ok"] = cc.stderr or "", cc.returncode == 0
        if res["compile_ok"]:
            r = wsl_run("cd %s && ./prog" % d, timeout=timeout)
            res["stdout"], res["run_ok"], res["run_err"] = \
                r.stdout or "", True, r.stderr or ""
    except subprocess.TimeoutExpired:
        res["run_err"] = "TIMEOUT"
    with open(cache_path(key), "w", encoding="utf-8") as f:
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
    global BOARD, REPORT
    ap = argparse.ArgumentParser()
    ap.add_argument("--all09", action="store_true",
                    help="查 09-高阶数据结构 板块全部章节（原行为）")
    ap.add_argument("--all08", action="store_true",
                    help="查 08-一些散落的算法 板块全部章节")
    ap.add_argument("--board", choices=("08", "09"),
                    help="与 --all08／--all09 等价，写成板块号")
    ap.add_argument("--fresh", action="store_true")
    args = ap.parse_args()

    if args.all08 and args.all09:
        ap.error("--all08 与 --all09 只能给一个")
    if args.board and args.all08 and args.board != "08":
        ap.error("--board %s 与 --all08 冲突" % args.board)
    if args.board and args.all09 and args.board != "09":
        ap.error("--board %s 与 --all09 冲突" % args.board)
    board = args.board or ("08" if args.all08 else "09")
    scan_all = bool(args.board or args.all08 or args.all09)
    if board == "08":
        # 换板块：目录与报告名各自独立。09 那一条路不在这里重新赋值——
        # 它的两个全局量就是原值（自测脚本也靠这一点把自己指到临时目录）。
        BOARD = BOARD08
        REPORT = os.path.join(LAB, REPORT08_NAME)

    ensure_dirs()
    files = sorted(f for f in os.listdir(BOARD) if f.endswith(".md")) if scan_all \
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
    n_excerpt, n_linux_skip, n_problem = 0, 0, 0
    n_board, n_board_out = 0, 0
    for p in programs:
        name = program_name(p["code"])
        if p["excerpt"]:
            # 3.5 允许片段，前提是标了节选。标了就跳过编译，不混进 PROBLEM
            n_excerpt += 1
            lines_out.append("[SKIP] %s:%d %s（已标节选：标注在%s，跳过编译）"
                             % (p["file"], p["line"], name, p["excerpt"]))
            continue
        expected_fail = bool(INTENTIONAL_FAIL.search(p["code"].split("\n")[0]))
        cmds = compile_commands(p["code"])
        if any(k == "arm" for k, _ in cmds):
            # 真板程序：编译命令是交叉工具链，输出要从 STM32F103C8 上经 semihosting 读回。
            # 本机没有这条工具链、也没有那块板子——跳过并写明原因，不报 PROBLEM。
            n_board += 1
            lines_out.append("[SKIP] %s:%d %s（需 ARM 交叉工具链与真板，本机跳过）"
                             % (p["file"], p["line"], name))
            continue
        gccs = [c for k, c in cmds if k == "gcc"] or ["-std=c++17 -O2"]
        if any(k == "msvc" for k, _ in cmds):
            lines_out.append("[跳过 MSVC 构建] %s:%d %s" % (p["file"], p["line"], name))
        head = "\n".join(p["code"].split("\n")[:8])
        for flags in dict.fromkeys(gccs):
            r = build_and_run(name, p["code"], flags, args.fresh)
            if not (r["compile_ok"] and r["run_ok"]) and LINUX_HINT.search(head + " " + flags):
                # 本机 MinGW 编不了的 Linux 侧构建：送进 WSL 再试一次
                w = wsl_build_and_run(name, p["code"], flags)
                if w is None:
                    n_linux_skip += 1
                    lines_out.append("[SKIP] %s:%d %s（需 Linux 侧工具链，"
                                     "本机没有可用的 WSL 发行版 %s，不判断）"
                                     % (p["file"], p["line"], name, WSL_DISTRO))
                    continue
                r = w
            if r["compile_ok"] and r["run_ok"]:
                tag = "OK(WSL)" if r.get("via") == "wsl" else "OK"
                index.add(r["stdout"], "%s(%s)" % (name, flags))
                if r["compile_err"].strip():          # 编译告警也要能对上
                    fail_index.add(r["compile_err"], "%s(编译告警)" % name)
            elif expected_fail and not r["compile_ok"]:
                tag = "EXPECT-FAIL"
                fail_index.add(r["compile_err"], "%s(编译错误)" % name)
            else:
                tag = "PROBLEM"
                n_problem += 1
                problems.append("%s:%d %s(%s) 编译=%s 运行=%s %s"
                                % (p["file"], p["line"], name, flags,
                                   r["compile_ok"], r["run_ok"],
                                   (r["compile_err"] or r["run_err"])[:200]))
            lines_out.append("[%s] %s:%d %s (%s)" % (tag, p["file"], p["line"], name, flags))

    lines_out += ["", "== 输出块逐行比对 =="]
    n_bad, n_interval = 0, 0
    interval_rows = []
    for o in outputs:
        if not o["marked"]:
            continue
        # MSVC 侧的输出块：本机不复现 MSVC 构建，跳过（只报一句）
        if MSVC_BLOCK.search(o.get("context", "") + "\n" + o["code"][:200]):
            lines_out.append("  [跳过 MSVC 输出块] %s:%d" % (o["file"], o["line"]))
            continue
        # 真板程序的输出块：输出自己写着芯片型号或内核（`STM32F103C8`／`Cortex-M3`），
        # 说明它来自那块板子。本机跑不了，跳过逐行比对，不报不一致。
        if BOARD_OUT.search(o.get("context", "") + "\n" + o["code"][:200]):
            n_board_out += 1
            lines_out.append("  [SKIP] 真板输出块 %s:%d（需 ARM 交叉工具链与真板，"
                             "本机跳过逐行比对）" % (o["file"], o["line"]))
            continue
        rows = []
        after_interval = False
        for k, dl in enumerate(o["code"].split("\n")):
            if not dl.strip():
                continue
            if interval_line(dl):
                # 3.12 第三节第 2 步：计时量漂到容差以外时正文改成区间。
                # 区间不来自程序输出，逐行比对只会报 NOT-FOUND，跳过。
                rows.append(("SKIP-区间写法", dl, "", "按 3.12 第三节第 2 步改成区间"))
                n_interval += 1
                interval_rows.append("%s:%d 正文: %s"
                                     % (o["file"], o["line"] + k, dl.strip()))
                after_interval = True
                continue
            if interval_note(dl, after_interval):
                rows.append(("SKIP-区间写法", dl, "", "区间行下面的说明句，随区间一起跳过"))
                n_interval += 1
                interval_rows.append("%s:%d 正文: %s"
                                     % (o["file"], o["line"] + k, dl.strip()))
                after_interval = False
                continue
            after_interval = False
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
        n_int = len([x for x in rows if x[0] == "SKIP-区间写法"])
        lines_out.append("  [%s] %s:%d（%d 行，%d 处不一致，%d 处计时漂移%s）"
                         % ("问题" if bad else "一致", o["file"], o["line"],
                            len(rows), len(bad), len(drift),
                            ("，%d 行区间写法" % n_int) if n_int else ""))
        for st, dl, src, note in rows:
            if st == "SKIP-区间写法":                 # 单列一类，不计入「不一致」
                lines_out.append("      %-11s 正文: %s（%s）" % (st, dl.strip(), note))
                continue
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

    lines_out += ["",
                  "== 区间写法、跳过逐行比对的行（SKIP，%d 行） ==" % n_interval,
                  "判据：带「约」且形如区间的数值单元格（约 A–B／约 A-B），以及紧跟在"
                  "区间行下面、",
                  "写着「随机器负载浮动」「按区间给」的说明句。区间不来自程序输出，"
                  "因此不算不一致。"]
    for s in interval_rows:
        lines_out.append("  [SKIP] 区间写法 %s" % s)

    lines_out += ["", "== 汇总 ==",
                  "不一致：%d 处（结构量必须逐位相同；计时量比值 > %.0fx 才算不一致）"
                  % (n_bad, TOLERANCE),
                  "计时漂移（在容差内，仅供参考）：%d 处" % len(drifts),
                  "无名／编不过的块（PROBLEM）：%d 处" % n_problem,
                  "已标节选、跳过编译的块（SKIP）：%d 个" % n_excerpt,
                  "需 Linux 侧工具链、本机跳过的块（SKIP）：%d 个" % n_linux_skip]
    if n_board or n_board_out:      # 两行只在 >0 时出现，09 的汇总因此一字未动
        lines_out += ["需 ARM 交叉工具链与真板、本机跳过的程序块（SKIP）：%d 个" % n_board,
                      "真板输出块、跳过逐行比对的（SKIP）：%d 个" % n_board_out]
    lines_out.append("区间写法、跳过逐行比对的行（SKIP）：%d 行" % n_interval)
    with open(REPORT, "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(lines_out) + "\n")
    tail_from = lines_out.index("== 汇总 ==") - 1
    if interval_rows:                      # 控制台也把新分类那几行打出来
        tail_from = min(tail_from, next(i for i, l in enumerate(lines_out)
                                        if l.startswith("== 区间写法")))
    head = lines_out[1:3] if board != "09" else []   # 08：连板块名与扫描量一起打
    try:                                   # 摘要里有编不出的字符时（管道/重定向下可能
        sys.stdout.reconfigure(errors="replace")     # 落到 GBK），别让整次自检在最后一步崩掉
    except (AttributeError, ValueError):
        pass
    print("\n".join(head + lines_out[tail_from:]))
    print("\n完整报告：%s" % REPORT)
    return 0


if __name__ == "__main__":
    sys.exit(main())
