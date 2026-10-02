/**
 * scan_lab.hpp —— 扫描与状态机：一个状态机的三种写法
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
 */

/**
 * 这台状态机只做一件事：把 JSONC 文本里的注释字节换成空格，别的字节一个不动。
 * 它没有预读、没有回溯，走一遍输入就出结果，因此输入与输出等长。
 *
 * 九个状态就是它的全部记忆，箭头上的字符是输入类：
 *
 *   普通 --/--> 待定斜杠 --/--> 行注释 --换行--> 普通
 *                   |
 *                   +--*--> 块注释 --*--> 块注释待定星 --/--> 普通
 *
 *   普通 --引号--> 双引号串 / 单引号串 --反斜杠--> 同名转义态 --任意字节--> 回到那个串
 *   双引号串 --双引号--> 普通        单引号串 --单引号--> 普通
 *
 * 注释里的换行一律原样保留，其余字节换成空格：这样处理前后长度相同，
 * 行列号也不会变，报错位置才能直接对着处理后的文本用。
 *
 * 报告里的每个数字都由这里的扫描器产出，重跑逐位相同：
 *
 *   bytes             扫描的字节数，等于输入长度
 *   steps             扫描步数，逐字节走一遍，也等于输入长度
 *   transitions       状态发生变化的步数，自环不算
 *   in_string_bytes   在字符串（含转义态）里读到的字节数
 *   out_string_bytes  其余状态里读到的字节数，两者之和等于 bytes
 *   blanked           被换成空格的字节数，只数输入里本来不是空格的那些
 *   cells_used        走到过的 (状态, 输入类) 格子数，满表 kStateCount × kClassCount
 *   class_hits        每个输入类被读到的次数
 *   state_entries     每个状态里读过多少个字节；末状态另算一次（空输入时普通状态为 1）
 *
 * 同一台机器有三种写法：switch、表驱动、函数指针表。三者在 63 个
 * (状态, 输入类) 组合上逐格相同，对同一份输入给出逐位相同的输出与相同的结构量，
 * 差别只在「加一个状态」与「加一个输入类」时要动几处。
 */
#ifndef SCAN_LAB_HPP
#define SCAN_LAB_HPP

#include <cstddef>
#include <string>
#include <vector>

namespace slab {

/* ================= 状态、输入类与动作 ================= */

/** 扫描器的状态 */
enum class ScanState : int {
    Normal = 0,     /**< 普通代码 */
    SlashPending,   /**< 刚读到一个斜杠，等下一个字节判断是不是注释开头 */
    DoubleString,   /**< 双引号字符串里 */
    SingleString,   /**< 单引号字符串里 */
    DoubleEscape,   /**< 双引号串里刚读到反斜杠，下一个字节一律原样收下 */
    SingleEscape,   /**< 单引号串里的同一种情形 */
    LineComment,    /**< 行注释里 */
    BlockComment,   /**< 块注释里 */
    BlockStar       /**< 块注释里刚读到星号，等一个斜杠 */
};

/** 输入类。逐字节分类，转移表只认类，不认具体是哪个字节 */
enum class CharClass : int {
    Slash = 0,   /**< 斜杠 */
    Star,        /**< 星号 */
    Quote2,      /**< 双引号 */
    Quote1,      /**< 单引号 */
    Backslash,   /**< 反斜杠 */
    Newline,     /**< 换行 */
    Other        /**< 其余全部字节 */
};

/** 一个位置上写什么。输出与输入等长，每个位置恰好写一个字节 */
enum class Action : int {
    WriteChar = 0,     /**< 原样写出当前字节 */
    WriteSpace,        /**< 写出一个空格，注释内容被抹掉 */
    WriteSpaceAndPrev  /**< 写出空格，并把上一个位置也改成空格（注释起始的两个字节） */
};

/** 状态 × 输入类 → 下一状态与动作。三种写法返回的都是这个结构 */
struct TableEntry {
    ScanState next = ScanState::Normal;
    Action action = Action::WriteChar;
};

inline bool operator==(const TableEntry &lhs, const TableEntry &rhs)
{
    return lhs.next == rhs.next && lhs.action == rhs.action;
}

inline bool operator!=(const TableEntry &lhs, const TableEntry &rhs)
{
    return !(lhs == rhs);
}

constexpr std::size_t kStateCount = 9;
constexpr std::size_t kClassCount = 7;
constexpr std::size_t kEntryCount = kStateCount * kClassCount;

const char *state_name(ScanState state);
const char *class_name(CharClass cls);
const char *action_name(Action action);

/** 逐字节分类。256 个字节值都能分类，落不进前六类的都算「其它」 */
CharClass classify(char byte);

/** 这个状态下读到的字节算不算在字符串里 */
bool is_in_string(ScanState state);

/** 这个状态算不算在注释里 */
bool is_in_comment(ScanState state);

/* ================= 同一个状态机的三种写法 ================= */

enum class MachineKind : int { Switch = 0, Table, Functions };

const char *machine_name(MachineKind kind);

/** 写法一：按状态分支，每个分支里再按输入类分支 */
TableEntry step_switch(ScanState state, CharClass cls);

/** 写法二：查一张 kStateCount × kClassCount 的二维表 */
TableEntry step_table(ScanState state, CharClass cls);

/** 写法三：每个状态一个处理函数，用函数指针表选函数 */
TableEntry step_functions(ScanState state, CharClass cls);

TableEntry step(ScanState state, CharClass cls, MachineKind kind);

/* ================= 扫描 ================= */

/** 一步扫描的记录，用来把状态转移打印出来 */
struct Step {
    std::size_t index = 0;                 /**< 字节下标 */
    char byte = 0;                         /**< 当前字节 */
    CharClass cls = CharClass::Other;      /**< 当前字节的输入类 */
    ScanState from = ScanState::Normal;    /**< 处理前的状态 */
    ScanState to = ScanState::Normal;      /**< 处理后的状态 */
    Action action = Action::WriteChar;     /**< 这一步写了什么 */
    bool in_string_before = false;         /**< 读这个字节时在不在字符串里 */
};

/** 一次扫描的全部结果。text 与输入等长 */
struct ScanResult {
    std::string text;
    std::size_t bytes = 0;
    std::size_t steps = 0;
    std::size_t transitions = 0;
    std::size_t in_string_bytes = 0;
    std::size_t out_string_bytes = 0;
    std::size_t blanked = 0;
    std::size_t blanked_line = 0;    /**< 其中行注释里换掉的 */
    std::size_t blanked_block = 0;   /**< 其中块注释里换掉的 */
    std::size_t cells_used = 0;
    std::vector<std::size_t> class_hits;                    /**< 长度 kClassCount */
    std::vector<std::size_t> state_entries;                 /**< 长度 kStateCount */
    std::vector<std::vector<std::size_t>> cells;            /**< kStateCount × kClassCount */
    ScanState final_state = ScanState::Normal;
    std::vector<Step> trace;                                /**< 前 trace_limit 步 */

    /** 输出文本与全部结构量都相同 */
    bool same_shape_as(const ScanResult &other) const;
};

/** 逐字节扫描一遍。trace_limit 是记录的步数上限，0 表示不记录 */
ScanResult scan(const std::string &text, MachineKind kind, std::size_t trace_limit);

/* ================= 朴素做法与预读式写法 ================= */

/** 朴素做法：先找块注释起点删到终点，再找行注释起点删到行尾。
    它不认识字符串，因此会把字符串里的斜杠当成注释开头 */
std::string strip_comments_naive(const std::string &text);

/** 预读式：遇到反斜杠就把下一个字节一起原样收下。
    with_guard 为假时不做 i + 1 < n 的判断，越界保护就是这一条判断 */
std::string copy_with_escape(const std::string &text, bool with_guard);

/* ================= 位置与样例文本 ================= */

struct Position {
    bool found = false;
    std::size_t offset = 0;
    std::size_t line = 1;
    std::size_t column = 1;
};

/** 找标记串的位置，行列号从 1 起算，换行符本身归上一行 */
Position find_marker(const std::string &text, const std::string &marker);

/** 行数 = 换行符个数，末尾若有没换行的内容再加一行；空文本算 0 行 */
std::size_t count_lines(const std::string &text);

/** 取第 line 行（从 1 起算），不含行尾的换行符；没有这一行时返回空串 */
std::string line_at(const std::string &text, std::size_t line);

/** 把一个字节写成看得见的形式：可打印字节原样，其余写成 \n \t \\ \0 或 \xNN */
std::string describe_byte(char byte);

/** 用竖线把一行夹起来，行尾的空格才看得见 */
std::string fenced(const std::string &line);

/** 主样例：JSONC 配置片段，含中文、网址、行注释、跨行块注释、转义引号与标记 */
const std::string &sample_jsonc();

/** 转义样例：转义引号、转义单引号、两个连在一起的反斜杠 */
const std::string &sample_escapes();

/** 末尾单个反斜杠：字符串没有闭合，越界保护就是为它准备的 */
const std::string &sample_tail_backslash();

/** 全是注释：两行都被换成空格，只剩换行 */
const std::string &sample_all_comment();

/** 状态转移样例：纯 ASCII，把九个状态都走到 */
const std::string &sample_trace();

/** 主样例里那个故意写错的标记 */
const std::string &marker_token();

/* ================= 报告与自测 ================= */

/** 自测结果。不打印，交给界面决定怎么显示 */
struct CheckResult {
    std::size_t total = 0;
    std::size_t passed = 0;
    std::size_t failed = 0;
    std::vector<std::string> lines;   /**< 每项一行，例如 "[通过] 3. ……" */

    bool all_passed() const { return failed == 0; }
    std::string summary() const;      /**< "23 项中 23 项通过，全部通过" */
};

/** 项目输出：三种写法、朴素做法、状态转移、长度与位置、越界保护五段。
    返回多行 UTF-8 文本，末尾不带多余换行；由界面层决定怎么显示 */
std::string build_report();

/** 逐项核对三种写法的一致性、剥注释的正确性与边界输入 */
CheckResult run_self_tests();

}   /* namespace slab */

#endif /* SCAN_LAB_HPP */
