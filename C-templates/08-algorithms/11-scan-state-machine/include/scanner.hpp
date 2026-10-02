/* scanner.hpp —— 练习模板 11 的核心接口（C++）
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
 * 扫描与状态机这一件事拆成 4 个阶段，每个阶段的实现写在 src/scanner.cpp 里：
 *
 *     阶段 1  scan_lines           一遍走完，同时算出每个位置的行号与列号
 *     阶段 2  step_switch          用 switch 写的玩具状态机
 *     阶段 3  kTable               同一台机器，改成状态表驱动
 *     阶段 4  strip_json_comments  剥掉 JSONC 里的注释
 *
 * 本模板只碰「一次遍历、边走边记住自己现在处在哪个状态」这一件事：
 * 用到的容器是 std::string 与 std::vector，不涉及数据结构的算法——
 * 那些属于 09-高阶数据结构 板块。
 */
#ifndef SCANNER_HPP
#define SCANNER_HPP

#include <cstddef>
#include <string>
#include <vector>

namespace scan {

/* ==================================================================
 * 阶段 1：一遍扫描，同时算出行号与列号
 * ================================================================== */

/* 一个位置：第几行、第几列，都从 1 起数。
 * 第 1 个字符停在第 1 行第 1 列。 */
struct Pos {
    int line = 1;
    int col = 1;
};

/* 一遍扫描同时记下来的几个数——它们都是结构量，重跑逐位相同。 */
struct ScanStats {
    long long chars = 0;      /* 走过了几个字符 */
    long long newlines = 0;   /* 其中换行符有几个 */
    long long tabs = 0;       /* 其中制表符有几个 */
    long long lines = 0;      /* 走完之后光标停在第几行 */
    long long max_col = 0;    /* 途中出现过的最大列号 */
};

/* 走一遍 text，把 offsets 里每个偏移处的 (行, 列) 填进 marks，
 * 同时把几个结构量填进 st。marks 会被调整成与 offsets 等长。
 * 偏移取 text.size() 时，记的是最后一个字符之后的那一个位置。
 *
 * 两处 TODO（阶段 1-1）都在这个函数里。 */
void scan_lines(const std::string &text,
                const std::vector<std::size_t> &offsets,
                std::vector<Pos> &marks,
                ScanStats &st);

/* ==================================================================
 * 阶段 2 与阶段 3：同一台玩具状态机，两种写法
 *
 * 这台机器认的是「由减号、数字、小数点拼出来的一个数」：
 *     S0   什么都还没读到（起点）
 *     S1   整数部分已经读到
 *     S2   小数点已经读到
 * 它从左往右读，走到走不通的那一步就停下，因此停下来时手里拿的是
 * 「能认出来的最长那一段」。
 * ================================================================== */

enum class NumState { S0 = 0, S1 = 1, S2 = 2 };

/* 字符类：转移按这几类写，不按具体字符写 */
enum class CharClass { Minus = 0, Digit = 1, Dot = 2, Other = 3 };

/* 已给出：把一个字符归到哪一类 */
CharClass classify(char c);

/* 已给出：状态的短名字，打印状态序列时用 */
const char *state_name(NumState s);

/* 已给出：这一步读完之后，能不能认为「到这里为止是一个完整的数」。
 * S1 是整数部分；S2 表示小数点已经出现，它后面还要再跟一个数字才算数，
 * 因此最后读进来的那一个是不是数字也要一起看。 */
bool accepting(NumState s, bool last_was_digit);

/* 每一步的记录：下标、读进来的字符、读完之后停在哪个状态 */
struct Step {
    std::size_t index = 0;
    char ch = 0;
    NumState state = NumState::S0;
};

/* 一次运行的完整结果。accepted 指整段输入都被读进来而且停得住；
 * accepted_prefix 指最长的那一段「读完之后可以收尾」的前缀有多长。 */
struct RunResult {
    std::vector<Step> steps;
    NumState stop_state = NumState::S0;
    std::size_t consumed = 0;
    std::size_t accepted_prefix = 0;
    bool accepted = false;
};

/* switch 版的一步：从 from 出发读到 c 之后，下一个状态写进 next。
 * 这一步走不通时返回假，next 不动。
 *
 * TODO（阶段 2-1）在这个函数里。 */
bool step_switch(NumState from, char c, NumState &next);

/* 表驱动的一步：查 src/scanner.cpp 里的状态表 kTable。
 * 表是数据、这个函数是驱动，两者分工写在那张表的上面。
 *
 * TODO（阶段 3-1）在 kTable 上，这个函数已经给出。 */
bool step_table(NumState from, char c, NumState &next);

/* 状态表的样子：几行、几列、一共几格 */
struct TableShape {
    int states = 0;
    int classes = 0;
    int cells = 0;
};

/* 已给出：读出 kTable 的形状，以及其中「走不通」的格子有几个 */
TableShape table_shape();
int table_dead_cells();

/* 已给出：用同一个驱动程序分别走 switch 版与表驱动版，把每一步都记下来 */
RunResult run_switch(const std::string &input);
RunResult run_table(const std::string &input);

/* 已给出：两次运行是不是逐位相同（步数、每一步、停在哪、结论都要一样） */
bool same_states(const RunResult &a, const RunResult &b);

/* ==================================================================
 * 阶段 4：剥掉 JSONC 里的注释
 * ================================================================== */

/* 把 in 里的注释剥掉，得到一个与 in 等长的结果：
 *     - 行注释与块注释里的每一个字符都换成一个空格；
 *     - 换行符原样保留，行与列的对应关系因此不会失效；
 *     - 字符串里的内容一个字也不动。
 *
 * TODO（阶段 4-1）在这个函数里。 */
std::string strip_json_comments(const std::string &in);

/* 已给出：朴素做法——见到两个斜杠就删到行尾，见到块注释的开头就删到结尾，
 * 完全不看自己是不是在字符串里。它会把 "https://example.com" 截断。 */
std::string strip_naive(const std::string &in);

/* 已给出：把字符串里不可打印的字节写成转义形式，便于逐字节核对 */
std::string escape_for_print(const std::string &s);

/* 已给出：a 与 b 从头对齐数下来，有多少个位置上的字符不一样 */
std::size_t count_differ(const std::string &a, const std::string &b);

/* 已给出：needle 在 hay 里出现过几次（允许重叠） */
std::size_t count_substring(const std::string &hay, const std::string &needle);

} /* namespace scan */

#endif /* SCANNER_HPP */
