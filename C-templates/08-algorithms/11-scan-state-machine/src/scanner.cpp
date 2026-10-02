/* scanner.cpp —— 练习模板 11 的实现（C++）
 *
 * 版权所有 (C) 2026 HiBer2007，保留所有权利。
 *
 * 本程序是《C 与 C++》教材的一部分，采用与教材文档相同的授权：
 * CC BY-NC-ND 4.0 加附加条款。全文见仓库根目录的 LICENSE 与 许可附加条款.md。
 *
 * 允许在保留本声明的前提下查看、编译、运行本程序用于学习；
 * 不允许二次分发，不允许商用，不允许演绎（修改后再分发），不允许移除署名。
 *
 * 本程序不提供任何担保。
 *
 * ------------------------------------------------------------------
 * 本模板的 4 处 TODO 全在这个文件里，按阶段编号：
 *
 *     阶段 1-1   scan_lines            行列号的累加规则（两处分支）
 *     阶段 2-1   step_switch           switch 版的转移
 *     阶段 3-1   kTable                状态表
 *     阶段 4-1   strip_json_comments   注释与字符串之间的状态迁移
 *
 * 每个 TODO 上面写明「要做什么」，下面的「判据」给出填完之后应当看到的数——
 * 那些数都由 src/main_cli.cpp 打印，逐个对得上才算做完。
 */
#include "scanner.hpp"

namespace scan {

/* ==================================================================
 * 阶段 1：一遍扫描，同时算出行号与列号
 * ================================================================== */

void scan_lines(const std::string &text,
                const std::vector<std::size_t> &offsets,
                std::vector<Pos> &marks,
                ScanStats &st)
{
    marks.assign(offsets.size(), Pos());
    st = ScanStats();

    Pos cur;                        /* 已给出：第 1 个字符停在第 1 行第 1 列 */
    std::size_t next = 0;

    for (std::size_t i = 0; i < text.size(); ++i) {
        /* 已给出：先把「停在第 i 个字符上」时的位置记下来，再读这个字符 */
        while (next < offsets.size() && offsets[next] == i) {
            marks[next] = cur;
            ++next;
        }

        ++st.chars;
        const char c = text[i];

        if (cur.col > st.max_col) {
            st.max_col = cur.col;
        }

        /* TODO（阶段 1-1）：
         * 下面两个分支各差一句：读掉当前这个字符之后，光标挪到哪里去。
         *   - 换行符把光标带到下一行的开头；
         *   - 其余字符仍旧留在本行，只是往后挪一格。
         * 行与列都从 1 起数，第 1 个字符停在第 1 行第 1 列。
         *
         * 判据（见《配置步骤.md》阶段 1）：chars 是 235、lines 是 9、
         * newlines 是 8、tabs 是 0、max col 是 50；偏移表里每一行的
         * (行, 列) 也各是一个定值，逐个对上。 */
        if (c == '\n') {
            ++st.newlines;
            /* 换行符这一步：行号与列号各变成多少 */
        } else {
            if (c == '\t') {
                ++st.tabs;
            }
            /* 其余字符这一步：两个量里哪一个变 */
        }
    }

    /* 已给出：字符走完之后，剩下的偏移（含 text.size() 那一个）都停在这里 */
    while (next < offsets.size()) {
        marks[next] = cur;
        ++next;
    }

    st.lines = cur.line;            /* 已给出：走完之后光标停在第几行 */
}

/* ==================================================================
 * 阶段 2：switch 版状态机
 * ================================================================== */

CharClass classify(char c)
{
    if (c == '-') {
        return CharClass::Minus;
    }
    if (c >= '0' && c <= '9') {
        return CharClass::Digit;
    }
    if (c == '.') {
        return CharClass::Dot;
    }
    return CharClass::Other;
}

const char *state_name(NumState s)
{
    switch (s) {
    case NumState::S0:
        return "S0";
    case NumState::S1:
        return "S1";
    case NumState::S2:
        return "S2";
    }
    return "??";
}

bool accepting(NumState s, bool last_was_digit)
{
    if (s == NumState::S1) {
        return true;
    }
    return s == NumState::S2 && last_was_digit;
}

bool step_switch(NumState from, char c, NumState &next)
{
    const CharClass cls = classify(c);      /* 已给出：先把字符归到一类 */

    /* TODO（阶段 2-1）：
     * 把「当前状态 × 这一类字符 → 下一个状态」写出来，一共十二个格子。
     * 三个状态各自的含义：
     *     S0   什么都还没读到，也在等一个开头的减号；
     *     S1   整数部分已经读到，可以在这里收尾；
     *     S2   小数点已经读到，还差一位小数。
     * 走不通的格子不要硬凑一个状态出来：那一步本来就不该发生，
     * 让函数返回假，机器停在原地——「最长的那一段」就是这么定下来的。
     *
     * 判据（见《配置步骤.md》阶段 2）：七个输入各自的 accepted、consumed、
     *       accepted prefix length 与状态序列逐行对上——`-12.5` 读进 5 个字符、
     *       停在 S2、最长前缀 5，`1.` 读进 2 个字符就停下、最长前缀 1、整段
     *       不接受。阶段 3 把同一批输入喂给状态表，序列要与这里逐位相同。 */
    (void)from;
    (void)cls;
    (void)next;
    return false;       /* 占位实现：一步也走不动 */
}

/* ==================================================================
 * 阶段 3：同一台机器，改成状态表驱动
 * ================================================================== */

/* 走不通的格子填这个值。表是数据，step_table 是驱动：
 * 驱动只做一次查表，怎么走全看表里填了什么。 */
const int kDead = -1;

/* TODO（阶段 3-1）：
 * 把这十二个格子填上。列的顺序与 CharClass 一致：
 *     Minus、Digit、Dot、Other
 * 每一行是一个状态；格子里填的是「读到这一类字符之后转到哪个状态」，
 * 走不通的格子填 kDead。填出来的表要和阶段 2 的转移说的是同一件事，
 * 一个字也不能差——那边是分支，这边是数据。
 *
 * 判据（见《配置步骤.md》阶段 3）：表的形状是 3 行 4 列，走不通的格子 7 个；
 *       同一批输入喂给两张实现，accepted、consumed、accepted prefix length
 *       与状态序列全部逐位相同。 */
const int kTable[3][4] = {
    /* Minus  Digit  Dot    Other */
    {    0,     0,     0,     0 },      /* S0 —— 占位实现：整表全填 0 */
    {    0,     0,     0,     0 },      /* S1 */
    {    0,     0,     0,     0 },      /* S2 */
};

bool step_table(NumState from, char c, NumState &next)
{
    /* 已给出：查一次表。表填好了，这个函数不必动。 */
    const int v = kTable[static_cast<int>(from)][static_cast<int>(classify(c))];
    if (v == kDead) {
        return false;
    }
    next = static_cast<NumState>(v);
    return true;
}

TableShape table_shape()
{
    TableShape s;
    s.states = static_cast<int>(sizeof(kTable) / sizeof(kTable[0]));
    s.classes = static_cast<int>(sizeof(kTable[0]) / sizeof(kTable[0][0]));
    s.cells = s.states * s.classes;
    return s;
}

int table_dead_cells()
{
    const TableShape sh = table_shape();
    int n = 0;
    for (int r = 0; r < sh.states; ++r) {
        for (int c = 0; c < sh.classes; ++c) {
            if (kTable[r][c] == kDead) {
                ++n;
            }
        }
    }
    return n;
}

/* 已给出：两个版本共用的驱动程序——一路读下去，每一步都记下来 */
namespace {

using StepFn = bool (*)(NumState, char, NumState &);

RunResult run_with(StepFn step, const std::string &input)
{
    RunResult r;
    NumState s = NumState::S0;
    bool last_was_digit = false;

    for (std::size_t i = 0; i < input.size(); ++i) {
        NumState next = NumState::S0;
        if (!step(s, input[i], next)) {
            break;                  /* 这一步走不通，机器停下 */
        }
        s = next;
        last_was_digit = classify(input[i]) == CharClass::Digit;

        Step st;
        st.index = i;
        st.ch = input[i];
        st.state = s;
        r.steps.push_back(st);

        if (accepting(s, last_was_digit)) {
            r.accepted_prefix = i + 1;
        }
    }

    r.stop_state = s;
    r.consumed = r.steps.size();
    r.accepted = r.consumed == input.size() && accepting(s, last_was_digit);
    return r;
}

} /* namespace */

RunResult run_switch(const std::string &input)
{
    return run_with(step_switch, input);
}

RunResult run_table(const std::string &input)
{
    return run_with(step_table, input);
}

bool same_states(const RunResult &a, const RunResult &b)
{
    if (a.steps.size() != b.steps.size()
        || a.stop_state != b.stop_state
        || a.consumed != b.consumed
        || a.accepted_prefix != b.accepted_prefix
        || a.accepted != b.accepted) {
        return false;
    }
    for (std::size_t i = 0; i < a.steps.size(); ++i) {
        if (a.steps[i].index != b.steps[i].index
            || a.steps[i].ch != b.steps[i].ch
            || a.steps[i].state != b.steps[i].state) {
            return false;
        }
    }
    return true;
}

/* ==================================================================
 * 阶段 4：剥掉 JSONC 里的注释
 * ================================================================== */

std::string strip_json_comments(const std::string &in)
{
    std::string out;
    out.reserve(in.size());     /* 已给出：注释换成空格，输出不会比输入长 */

    bool in_string = false;     /* 已给出：现在在不在一个字符串里面 */
    bool in_block = false;      /* 已给出：现在在不在块注释里面 */

    for (std::size_t i = 0; i < in.size(); ++i) {
        const char c = in[i];

        /* TODO（阶段 4-1）：
         * 循环体整段都要自己写。三处处境，每处都要想清楚：
         *
         *   在字符串里：什么字符会结束它？紧跟其后的那一个字符在什么情况下
         *       不算数（它可能是结束引号，也可能是另一个反斜杠）？
         *       注意 i 已经指到最后一个字符时，「后面那一个」还取不取得到。
         *   在块注释里：什么两连字符结束它？结束之前往输出里放什么？
         *       块注释可以跨行，跨行时换行符要留在原处。
         *   两处都不在：一个引号就打开字符串；一个斜杠要往后多看一个字符
         *       才能定下来；注释分两种，一种到行尾就断，一种要等到收尾的那
         *       两个字符。收尾的那两个字符本身也在注释里。
         *
         * 无论走到哪一步，注释里的每一个字符都要在输出里占掉同样多的位置，
         * 因此输出与输入长度始终相同，行列号不会因为剥注释而错位。
         *
         * 判据（见《配置步骤.md》阶段 4）：输出与参考文本逐字节相同，
         *       两者都是 235 字节，朴素版是 88 字节；末尾那一段单独喂进来，
         *       3 个字节进、3 个字节出。 */
        out.push_back(c);       /* 占位实现：原样抄过去，注释一个也不剥 */
    }

    (void)in_string;
    (void)in_block;
    return out;
}

std::string strip_naive(const std::string &in)
{
    /* 已给出：朴素做法。它只认字符，不认自己站在哪儿。 */
    std::string out;
    std::size_t i = 0;
    while (i < in.size()) {
        if (in[i] == '/' && i + 1 < in.size() && in[i + 1] == '/') {
            i += 2;
            while (i < in.size() && in[i] != '\n') {
                ++i;                /* 一路删到行尾，换行符留着 */
            }
            continue;
        }
        if (in[i] == '/' && i + 1 < in.size() && in[i + 1] == '*') {
            i += 2;
            while (i + 1 < in.size() && !(in[i] == '*' && in[i + 1] == '/')) {
                ++i;
            }
            i += 2;
            out.push_back(' ');
            continue;
        }
        out.push_back(in[i]);
        ++i;
    }
    return out;
}

/* ==================================================================
 * 已给出的工具
 * ================================================================== */

std::string escape_for_print(const std::string &s)
{
    static const char *const kHex = "0123456789ABCDEF";
    std::string r;
    for (std::size_t i = 0; i < s.size(); ++i) {
        const unsigned char u = static_cast<unsigned char>(s[i]);
        if (s[i] == '"') {
            r += "\\\"";
        } else if (s[i] == '\\') {
            r += "\\\\";
        } else if (u >= 32 && u < 127) {
            r += s[i];
        } else {
            r += "\\x";
            r += kHex[u >> 4];
            r += kHex[u & 0x0Fu];
        }
    }
    return r;
}

std::size_t count_differ(const std::string &a, const std::string &b)
{
    const std::size_t n = a.size() < b.size() ? a.size() : b.size();
    std::size_t k = 0;
    for (std::size_t i = 0; i < n; ++i) {
        if (a[i] != b[i]) {
            ++k;
        }
    }
    return k;
}

std::size_t count_substring(const std::string &hay, const std::string &needle)
{
    if (needle.empty()) {
        return 0;
    }
    std::size_t n = 0;
    for (std::size_t p = hay.find(needle); p != std::string::npos;
         p = hay.find(needle, p + 1)) {
        ++n;
    }
    return n;
}

} /* namespace scan */
