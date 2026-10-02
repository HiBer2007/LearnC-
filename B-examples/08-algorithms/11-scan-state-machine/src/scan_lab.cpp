/**
 * scan_lab.cpp —— 扫描与状态机：一个状态机的三种写法
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

#include "scan_lab.hpp"

#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

namespace slab {

/* ================= 名字与分类 ================= */

const char *state_name(ScanState state)
{
    switch (state) {
    case ScanState::Normal:       return u8"普通";
    case ScanState::SlashPending: return u8"待定斜杠";
    case ScanState::DoubleString: return u8"双引号串";
    case ScanState::SingleString: return u8"单引号串";
    case ScanState::DoubleEscape: return u8"双引号转义";
    case ScanState::SingleEscape: return u8"单引号转义";
    case ScanState::LineComment:  return u8"行注释";
    case ScanState::BlockComment: return u8"块注释";
    case ScanState::BlockStar:    return u8"块注释待定星";
    }
    return u8"未知状态";
}

const char *class_name(CharClass cls)
{
    switch (cls) {
    case CharClass::Slash:     return u8"斜杠";
    case CharClass::Star:      return u8"星号";
    case CharClass::Quote2:    return u8"双引号";
    case CharClass::Quote1:    return u8"单引号";
    case CharClass::Backslash: return u8"反斜杠";
    case CharClass::Newline:   return u8"换行";
    case CharClass::Other:     return u8"其它";
    }
    return u8"未知类";
}

const char *action_name(Action action)
{
    switch (action) {
    case Action::WriteChar:        return u8"原样写出";
    case Action::WriteSpace:       return u8"写空格";
    case Action::WriteSpaceAndPrev: return u8"写空格并回改上一格";
    }
    return u8"未知动作";
}

const char *machine_name(MachineKind kind)
{
    switch (kind) {
    case MachineKind::Switch:    return u8"switch 写法";
    case MachineKind::Table:     return u8"表驱动写法";
    case MachineKind::Functions: return u8"函数指针表写法";
    }
    return u8"未知写法";
}

CharClass classify(char byte)
{
    switch (byte) {
    case '/':  return CharClass::Slash;
    case '*':  return CharClass::Star;
    case '"':  return CharClass::Quote2;
    case '\'': return CharClass::Quote1;
    case '\\': return CharClass::Backslash;
    case '\n': return CharClass::Newline;
    default:   return CharClass::Other;
    }
}

bool is_in_string(ScanState state)
{
    return state == ScanState::DoubleString || state == ScanState::SingleString ||
           state == ScanState::DoubleEscape || state == ScanState::SingleEscape;
}

bool is_in_comment(ScanState state)
{
    return state == ScanState::LineComment || state == ScanState::BlockComment ||
           state == ScanState::BlockStar;
}

/* ================= 写法二：转移表 ================= */

namespace {

/** 状态 × 输入类 → 下一状态与动作。行按 ScanState，列按 CharClass。
    读表的方式：kTable[当前状态][输入类]。
    这张表就是这台机器的全部定义，另外两种写法必须与它逐格相同。 */
const TableEntry kTable[kStateCount][kClassCount] = {
    /* 普通 */ {
        {ScanState::SlashPending, Action::WriteChar},   /* 斜杠：先记下来，等下一个字节 */
        {ScanState::Normal,       Action::WriteChar},   /* 星号 */
        {ScanState::DoubleString, Action::WriteChar},   /* 双引号：进串 */
        {ScanState::SingleString, Action::WriteChar},   /* 单引号：进串 */
        {ScanState::Normal,       Action::WriteChar},   /* 反斜杠在串外只是普通字节 */
        {ScanState::Normal,       Action::WriteChar},   /* 换行 */
        {ScanState::Normal,       Action::WriteChar},   /* 其它 */
    },
    /* 待定斜杠 */ {
        {ScanState::LineComment,  Action::WriteSpaceAndPrev},  /* 两条斜杠：行注释开头 */
        {ScanState::BlockComment, Action::WriteSpaceAndPrev},  /* 斜杠加星号：块注释开头 */
        {ScanState::DoubleString, Action::WriteChar},          /* 上一格那个斜杠是普通字节 */
        {ScanState::SingleString, Action::WriteChar},
        {ScanState::Normal,       Action::WriteChar},
        {ScanState::Normal,       Action::WriteChar},
        {ScanState::Normal,       Action::WriteChar},
    },
    /* 双引号串 */ {
        {ScanState::DoubleString, Action::WriteChar},   /* 串里的斜杠不动 */
        {ScanState::DoubleString, Action::WriteChar},
        {ScanState::Normal,       Action::WriteChar},   /* 同种引号：出串 */
        {ScanState::DoubleString, Action::WriteChar},   /* 另一种引号只是普通字节 */
        {ScanState::DoubleEscape, Action::WriteChar},   /* 反斜杠：下一个字节要转义 */
        {ScanState::DoubleString, Action::WriteChar},   /* 换行照旧保留 */
        {ScanState::DoubleString, Action::WriteChar},
    },
    /* 单引号串 */ {
        {ScanState::SingleString, Action::WriteChar},
        {ScanState::SingleString, Action::WriteChar},
        {ScanState::SingleString, Action::WriteChar},
        {ScanState::Normal,       Action::WriteChar},
        {ScanState::SingleEscape, Action::WriteChar},
        {ScanState::SingleString, Action::WriteChar},
        {ScanState::SingleString, Action::WriteChar},
    },
    /* 双引号转义 */ {
        {ScanState::DoubleString, Action::WriteChar},   /* 转义后的字节一律原样收下 */
        {ScanState::DoubleString, Action::WriteChar},
        {ScanState::DoubleString, Action::WriteChar},   /* 转义引号不结束串 */
        {ScanState::DoubleString, Action::WriteChar},
        {ScanState::DoubleString, Action::WriteChar},   /* 两个反斜杠连在一起 */
        {ScanState::DoubleString, Action::WriteChar},
        {ScanState::DoubleString, Action::WriteChar},
    },
    /* 单引号转义 */ {
        {ScanState::SingleString, Action::WriteChar},
        {ScanState::SingleString, Action::WriteChar},
        {ScanState::SingleString, Action::WriteChar},
        {ScanState::SingleString, Action::WriteChar},
        {ScanState::SingleString, Action::WriteChar},
        {ScanState::SingleString, Action::WriteChar},
        {ScanState::SingleString, Action::WriteChar},
    },
    /* 行注释 */ {
        {ScanState::LineComment, Action::WriteSpace},   /* 注释里的字节换成空格 */
        {ScanState::LineComment, Action::WriteSpace},
        {ScanState::LineComment, Action::WriteSpace},
        {ScanState::LineComment, Action::WriteSpace},
        {ScanState::LineComment, Action::WriteSpace},
        {ScanState::Normal,      Action::WriteChar},    /* 换行保留，注释到头 */
        {ScanState::LineComment, Action::WriteSpace},
    },
    /* 块注释 */ {
        {ScanState::BlockComment, Action::WriteSpace},
        {ScanState::BlockStar,    Action::WriteSpace},  /* 星号：等一个斜杠 */
        {ScanState::BlockComment, Action::WriteSpace},
        {ScanState::BlockComment, Action::WriteSpace},
        {ScanState::BlockComment, Action::WriteSpace},
        {ScanState::BlockComment, Action::WriteChar},   /* 换行保留 */
        {ScanState::BlockComment, Action::WriteSpace},
    },
    /* 块注释待定星 */ {
        {ScanState::Normal,       Action::WriteSpace},  /* 星号加斜杠：注释结束 */
        {ScanState::BlockStar,    Action::WriteSpace},  /* 连着两个星号 */
        {ScanState::BlockComment, Action::WriteSpace},
        {ScanState::BlockComment, Action::WriteSpace},
        {ScanState::BlockComment, Action::WriteSpace},
        {ScanState::BlockComment, Action::WriteChar},   /* 换行之后星号与斜杠不再相邻 */
        {ScanState::BlockComment, Action::WriteSpace},
    },
};

}   /* namespace */

TableEntry step_table(ScanState state, CharClass cls)
{
    const std::size_t s = static_cast<std::size_t>(state);
    const std::size_t c = static_cast<std::size_t>(cls);
    if (s >= kStateCount || c >= kClassCount) {
        return TableEntry{ScanState::Normal, Action::WriteChar};
    }
    return kTable[s][c];
}

/* ================= 写法一：switch ================= */

TableEntry step_switch(ScanState state, CharClass cls)
{
    switch (state) {
    case ScanState::Normal:
        switch (cls) {
        case CharClass::Slash:     return TableEntry{ScanState::SlashPending, Action::WriteChar};
        case CharClass::Star:      return TableEntry{ScanState::Normal, Action::WriteChar};
        case CharClass::Quote2:    return TableEntry{ScanState::DoubleString, Action::WriteChar};
        case CharClass::Quote1:    return TableEntry{ScanState::SingleString, Action::WriteChar};
        case CharClass::Backslash: return TableEntry{ScanState::Normal, Action::WriteChar};
        case CharClass::Newline:   return TableEntry{ScanState::Normal, Action::WriteChar};
        case CharClass::Other:     return TableEntry{ScanState::Normal, Action::WriteChar};
        }
        break;
    case ScanState::SlashPending:
        switch (cls) {
        case CharClass::Slash:     return TableEntry{ScanState::LineComment, Action::WriteSpaceAndPrev};
        case CharClass::Star:      return TableEntry{ScanState::BlockComment, Action::WriteSpaceAndPrev};
        case CharClass::Quote2:    return TableEntry{ScanState::DoubleString, Action::WriteChar};
        case CharClass::Quote1:    return TableEntry{ScanState::SingleString, Action::WriteChar};
        case CharClass::Backslash: return TableEntry{ScanState::Normal, Action::WriteChar};
        case CharClass::Newline:   return TableEntry{ScanState::Normal, Action::WriteChar};
        case CharClass::Other:     return TableEntry{ScanState::Normal, Action::WriteChar};
        }
        break;
    case ScanState::DoubleString:
        switch (cls) {
        case CharClass::Slash:     return TableEntry{ScanState::DoubleString, Action::WriteChar};
        case CharClass::Star:      return TableEntry{ScanState::DoubleString, Action::WriteChar};
        case CharClass::Quote2:    return TableEntry{ScanState::Normal, Action::WriteChar};
        case CharClass::Quote1:    return TableEntry{ScanState::DoubleString, Action::WriteChar};
        case CharClass::Backslash: return TableEntry{ScanState::DoubleEscape, Action::WriteChar};
        case CharClass::Newline:   return TableEntry{ScanState::DoubleString, Action::WriteChar};
        case CharClass::Other:     return TableEntry{ScanState::DoubleString, Action::WriteChar};
        }
        break;
    case ScanState::SingleString:
        switch (cls) {
        case CharClass::Slash:     return TableEntry{ScanState::SingleString, Action::WriteChar};
        case CharClass::Star:      return TableEntry{ScanState::SingleString, Action::WriteChar};
        case CharClass::Quote2:    return TableEntry{ScanState::SingleString, Action::WriteChar};
        case CharClass::Quote1:    return TableEntry{ScanState::Normal, Action::WriteChar};
        case CharClass::Backslash: return TableEntry{ScanState::SingleEscape, Action::WriteChar};
        case CharClass::Newline:   return TableEntry{ScanState::SingleString, Action::WriteChar};
        case CharClass::Other:     return TableEntry{ScanState::SingleString, Action::WriteChar};
        }
        break;
    case ScanState::DoubleEscape:
        switch (cls) {
        case CharClass::Slash:     return TableEntry{ScanState::DoubleString, Action::WriteChar};
        case CharClass::Star:      return TableEntry{ScanState::DoubleString, Action::WriteChar};
        case CharClass::Quote2:    return TableEntry{ScanState::DoubleString, Action::WriteChar};
        case CharClass::Quote1:    return TableEntry{ScanState::DoubleString, Action::WriteChar};
        case CharClass::Backslash: return TableEntry{ScanState::DoubleString, Action::WriteChar};
        case CharClass::Newline:   return TableEntry{ScanState::DoubleString, Action::WriteChar};
        case CharClass::Other:     return TableEntry{ScanState::DoubleString, Action::WriteChar};
        }
        break;
    case ScanState::SingleEscape:
        switch (cls) {
        case CharClass::Slash:     return TableEntry{ScanState::SingleString, Action::WriteChar};
        case CharClass::Star:      return TableEntry{ScanState::SingleString, Action::WriteChar};
        case CharClass::Quote2:    return TableEntry{ScanState::SingleString, Action::WriteChar};
        case CharClass::Quote1:    return TableEntry{ScanState::SingleString, Action::WriteChar};
        case CharClass::Backslash: return TableEntry{ScanState::SingleString, Action::WriteChar};
        case CharClass::Newline:   return TableEntry{ScanState::SingleString, Action::WriteChar};
        case CharClass::Other:     return TableEntry{ScanState::SingleString, Action::WriteChar};
        }
        break;
    case ScanState::LineComment:
        switch (cls) {
        case CharClass::Slash:     return TableEntry{ScanState::LineComment, Action::WriteSpace};
        case CharClass::Star:      return TableEntry{ScanState::LineComment, Action::WriteSpace};
        case CharClass::Quote2:    return TableEntry{ScanState::LineComment, Action::WriteSpace};
        case CharClass::Quote1:    return TableEntry{ScanState::LineComment, Action::WriteSpace};
        case CharClass::Backslash: return TableEntry{ScanState::LineComment, Action::WriteSpace};
        case CharClass::Newline:   return TableEntry{ScanState::Normal, Action::WriteChar};
        case CharClass::Other:     return TableEntry{ScanState::LineComment, Action::WriteSpace};
        }
        break;
    case ScanState::BlockComment:
        switch (cls) {
        case CharClass::Slash:     return TableEntry{ScanState::BlockComment, Action::WriteSpace};
        case CharClass::Star:      return TableEntry{ScanState::BlockStar, Action::WriteSpace};
        case CharClass::Quote2:    return TableEntry{ScanState::BlockComment, Action::WriteSpace};
        case CharClass::Quote1:    return TableEntry{ScanState::BlockComment, Action::WriteSpace};
        case CharClass::Backslash: return TableEntry{ScanState::BlockComment, Action::WriteSpace};
        case CharClass::Newline:   return TableEntry{ScanState::BlockComment, Action::WriteChar};
        case CharClass::Other:     return TableEntry{ScanState::BlockComment, Action::WriteSpace};
        }
        break;
    case ScanState::BlockStar:
        switch (cls) {
        case CharClass::Slash:     return TableEntry{ScanState::Normal, Action::WriteSpace};
        case CharClass::Star:      return TableEntry{ScanState::BlockStar, Action::WriteSpace};
        case CharClass::Quote2:    return TableEntry{ScanState::BlockComment, Action::WriteSpace};
        case CharClass::Quote1:    return TableEntry{ScanState::BlockComment, Action::WriteSpace};
        case CharClass::Backslash: return TableEntry{ScanState::BlockComment, Action::WriteSpace};
        case CharClass::Newline:   return TableEntry{ScanState::BlockComment, Action::WriteChar};
        case CharClass::Other:     return TableEntry{ScanState::BlockComment, Action::WriteSpace};
        }
        break;
    }
    return TableEntry{ScanState::Normal, Action::WriteChar};
}

/* ================= 写法三：函数指针表 ================= */

namespace {

using StateHandler = TableEntry (*)(CharClass);

TableEntry handle_normal(CharClass cls)
{
    switch (cls) {
    case CharClass::Slash:  return TableEntry{ScanState::SlashPending, Action::WriteChar};
    case CharClass::Quote2: return TableEntry{ScanState::DoubleString, Action::WriteChar};
    case CharClass::Quote1: return TableEntry{ScanState::SingleString, Action::WriteChar};
    default:                return TableEntry{ScanState::Normal, Action::WriteChar};
    }
}

TableEntry handle_slash_pending(CharClass cls)
{
    switch (cls) {
    case CharClass::Slash:  return TableEntry{ScanState::LineComment, Action::WriteSpaceAndPrev};
    case CharClass::Star:   return TableEntry{ScanState::BlockComment, Action::WriteSpaceAndPrev};
    case CharClass::Quote2: return TableEntry{ScanState::DoubleString, Action::WriteChar};
    case CharClass::Quote1: return TableEntry{ScanState::SingleString, Action::WriteChar};
    default:                return TableEntry{ScanState::Normal, Action::WriteChar};
    }
}

TableEntry handle_double_string(CharClass cls)
{
    switch (cls) {
    case CharClass::Quote2:    return TableEntry{ScanState::Normal, Action::WriteChar};
    case CharClass::Backslash: return TableEntry{ScanState::DoubleEscape, Action::WriteChar};
    default:                   return TableEntry{ScanState::DoubleString, Action::WriteChar};
    }
}

TableEntry handle_single_string(CharClass cls)
{
    switch (cls) {
    case CharClass::Quote1:    return TableEntry{ScanState::Normal, Action::WriteChar};
    case CharClass::Backslash: return TableEntry{ScanState::SingleEscape, Action::WriteChar};
    default:                   return TableEntry{ScanState::SingleString, Action::WriteChar};
    }
}

TableEntry handle_double_escape(CharClass)
{
    return TableEntry{ScanState::DoubleString, Action::WriteChar};
}

TableEntry handle_single_escape(CharClass)
{
    return TableEntry{ScanState::SingleString, Action::WriteChar};
}

TableEntry handle_line_comment(CharClass cls)
{
    if (cls == CharClass::Newline) {
        return TableEntry{ScanState::Normal, Action::WriteChar};
    }
    return TableEntry{ScanState::LineComment, Action::WriteSpace};
}

TableEntry handle_block_comment(CharClass cls)
{
    if (cls == CharClass::Star) {
        return TableEntry{ScanState::BlockStar, Action::WriteSpace};
    }
    if (cls == CharClass::Newline) {
        return TableEntry{ScanState::BlockComment, Action::WriteChar};
    }
    return TableEntry{ScanState::BlockComment, Action::WriteSpace};
}

TableEntry handle_block_star(CharClass cls)
{
    if (cls == CharClass::Slash) {
        return TableEntry{ScanState::Normal, Action::WriteSpace};   /* 注释结束 */
    }
    if (cls == CharClass::Newline) {
        return TableEntry{ScanState::BlockComment, Action::WriteChar};
    }
    if (cls == CharClass::Star) {
        return TableEntry{ScanState::BlockStar, Action::WriteSpace};
    }
    return TableEntry{ScanState::BlockComment, Action::WriteSpace};
}

/** 函数指针表。下标与 ScanState 一一对应 */
const StateHandler kHandlers[kStateCount] = {
    handle_normal,
    handle_slash_pending,
    handle_double_string,
    handle_single_string,
    handle_double_escape,
    handle_single_escape,
    handle_line_comment,
    handle_block_comment,
    handle_block_star,
};

}   /* namespace */

TableEntry step_functions(ScanState state, CharClass cls)
{
    const std::size_t s = static_cast<std::size_t>(state);
    if (s >= kStateCount) {
        return TableEntry{ScanState::Normal, Action::WriteChar};
    }
    return kHandlers[s](cls);
}

TableEntry step(ScanState state, CharClass cls, MachineKind kind)
{
    switch (kind) {
    case MachineKind::Switch:    return step_switch(state, cls);
    case MachineKind::Table:     return step_table(state, cls);
    case MachineKind::Functions: return step_functions(state, cls);
    }
    return step_table(state, cls);
}

/* ================= 扫描 ================= */

bool ScanResult::same_shape_as(const ScanResult &other) const
{
    return text == other.text && bytes == other.bytes && steps == other.steps &&
           transitions == other.transitions && in_string_bytes == other.in_string_bytes &&
           out_string_bytes == other.out_string_bytes && blanked == other.blanked &&
           blanked_line == other.blanked_line && blanked_block == other.blanked_block &&
           cells_used == other.cells_used && final_state == other.final_state &&
           class_hits == other.class_hits && state_entries == other.state_entries &&
           cells == other.cells;
}

namespace {

/** 换掉的那个字节算在行注释还是块注释里 */
void note_comment_blank(ScanResult *result, ScanState owner)
{
    if (owner == ScanState::LineComment) {
        ++result->blanked_line;
    } else if (owner == ScanState::BlockComment || owner == ScanState::BlockStar) {
        ++result->blanked_block;
    }
}

}   /* namespace */

ScanResult scan(const std::string &text, MachineKind kind, std::size_t trace_limit)
{
    ScanResult result;
    result.text.assign(text.size(), ' ');
    result.bytes = text.size();
    result.class_hits.assign(kClassCount, 0);
    result.state_entries.assign(kStateCount, 0);
    result.cells.assign(kStateCount, std::vector<std::size_t>(kClassCount, 0));

    ScanState state = ScanState::Normal;

    for (std::size_t i = 0; i < text.size(); ++i) {
        const char byte = text[i];
        const CharClass cls = classify(byte);
        const TableEntry entry = step(state, cls, kind);
        const ScanState from = state;
        const ScanState next = entry.next;

        ++result.class_hits[static_cast<std::size_t>(cls)];
        ++result.cells[static_cast<std::size_t>(from)][static_cast<std::size_t>(cls)];
        ++result.state_entries[static_cast<std::size_t>(from)];   /* 在这个状态里读过字节 */

        if (result.trace.size() < trace_limit) {
            Step record;
            record.index = i;
            record.byte = byte;
            record.cls = cls;
            record.from = from;
            record.to = next;
            record.action = entry.action;
            record.in_string_before = is_in_string(from);
            result.trace.push_back(record);
        }

        if (is_in_string(from)) {
            ++result.in_string_bytes;
        } else {
            ++result.out_string_bytes;
        }

        /* 被换掉的字节算在哪个注释里：注释开头那一步按下一状态归类。
           注释开头要回改上一格，那一个字节也算换掉的，因此按字节数，不按步数 */
        if (entry.action != Action::WriteChar) {
            const ScanState owner = (entry.action == Action::WriteSpaceAndPrev) ? next : from;
            if (byte != ' ') {
                ++result.blanked;
                note_comment_blank(&result, owner);
            }
            if (entry.action == Action::WriteSpaceAndPrev && i > 0 && text[i - 1] != ' ') {
                ++result.blanked;
                note_comment_blank(&result, owner);
            }
        }

        switch (entry.action) {
        case Action::WriteChar:
            result.text[i] = byte;
            break;
        case Action::WriteSpace:
            result.text[i] = ' ';
            break;
        case Action::WriteSpaceAndPrev:
            result.text[i] = ' ';
            if (i > 0) {
                result.text[i - 1] = ' ';   /* 注释开头那一格已经写出去了，回改成空格 */
            }
            break;
        }

        if (next != from) {
            ++result.transitions;
        }
        state = next;
        ++result.steps;
    }

    result.final_state = state;
    ++result.state_entries[static_cast<std::size_t>(state)];   /* 末状态也算进去过一次 */
    for (std::size_t s = 0; s < kStateCount; ++s) {
        for (std::size_t c = 0; c < kClassCount; ++c) {
            if (result.cells[s][c] != 0) {
                ++result.cells_used;
            }
        }
    }
    return result;
}

/* ================= 朴素做法与预读式写法 ================= */

std::string strip_comments_naive(const std::string &text)
{
    std::string out = text;

    /* 第一趟：块注释。找起点删到终点，中间跨几行一起删掉 */
    for (;;) {
        const std::size_t open = out.find("/*");
        if (open == std::string::npos) {
            break;
        }
        const std::size_t close = out.find("*/", open + 2);
        if (close == std::string::npos) {
            out.erase(open);
            break;
        }
        out.erase(open, close + 2 - open);
    }

    /* 第二趟：行注释。找两条斜杠，删到行尾 */
    for (;;) {
        const std::size_t start = out.find("//");
        if (start == std::string::npos) {
            break;
        }
        const std::size_t end = out.find('\n', start);
        out.erase(start, (end == std::string::npos ? out.size() : end) - start);
    }
    return out;
}

std::string copy_with_escape(const std::string &text, bool with_guard)
{
    std::string out;
    out.reserve(text.size());
    std::size_t i = 0;
    while (i < text.size()) {
        if (text[i] == '\\') {
            out.push_back(text[i]);
            if (with_guard) {
                if (i + 1 < text.size()) {
                    out.push_back(text[i + 1]);
                    i += 2;
                } else {
                    ++i;            /* 末尾单个反斜杠：收下它就结束，不再往下取 */
                }
            } else {
                out.push_back(text[i + 1]);   /* 下标已经走到 size() 上 */
                i += 2;
            }
            continue;
        }
        out.push_back(text[i]);
        ++i;
    }
    return out;
}

/* ================= 位置与样例文本 ================= */

Position find_marker(const std::string &text, const std::string &marker)
{
    Position pos;
    const std::size_t at = text.find(marker);
    if (at == std::string::npos || marker.empty()) {
        return pos;
    }
    pos.found = true;
    pos.offset = at;
    for (std::size_t i = 0; i < at; ++i) {
        if (text[i] == '\n') {
            ++pos.line;
            pos.column = 1;
        } else {
            ++pos.column;
        }
    }
    return pos;
}

std::size_t count_lines(const std::string &text)
{
    if (text.empty()) {
        return 0;
    }
    std::size_t lines = 0;
    for (const char byte : text) {
        if (byte == '\n') {
            ++lines;
        }
    }
    if (text.back() != '\n') {
        ++lines;
    }
    return lines;
}

std::string line_at(const std::string &text, std::size_t line)
{
    if (line == 0) {
        return std::string();
    }
    std::size_t current = 1;
    std::size_t begin = 0;
    for (std::size_t i = 0; i <= text.size(); ++i) {
        if (i == text.size() || text[i] == '\n') {
            if (current == line) {
                return text.substr(begin, i - begin);
            }
            if (i == text.size()) {
                break;
            }
            ++current;
            begin = i + 1;
        }
    }
    return std::string();
}

std::string describe_byte(char byte)
{
    const unsigned char value = static_cast<unsigned char>(byte);
    switch (value) {
    case '\n': return "\\n";
    case '\r': return "\\r";
    case '\t': return "\\t";
    case '\\': return "\\\\";
    case '\0': return "\\0";
    default:   break;
    }
    if (value >= 0x20 && value <= 0x7E) {
        return std::string(1, static_cast<char>(value));
    }
    static const char *const digits = "0123456789ABCDEF";
    std::string out = "\\x";
    out.push_back(digits[(value >> 4) & 0x0F]);
    out.push_back(digits[value & 0x0F]);
    return out;
}

std::string fenced(const std::string &line)
{
    return "|" + line + "|";
}

const std::string &sample_jsonc()
{
    /* 样例写在源码里，不读外部文件。逐行拼接而不用原始字符串：
       源码文件的行尾一旦被换成 CRLF，原始字符串里就会多出回车字节，
       字节数与列号会跟着变。 */
    static const std::string text =
        u8"{\n"
        u8"  \"name\": \"扫描与状态机\",\n"
        u8"  \"home\": \"https://example.com/docs//index.html\",  // 网址里的两条斜杠不是注释\n"
        u8"  /* 块注释第一行：里面的 // 与 /* 都不算数\n"
        u8"     块注释第二行，等号 == 与括号 () 都算注释内容 */\n"
        u8"  \"note\": \"他说：\\\"注释要小心\\\"\",\n"
        u8"  \"path\": \"C:\\\\tmp\\\\out.txt\",\n"
        u8"  \"timeout\": @@   // 这里故意留了一处写错的标记\n"
        u8"}\n";
    return text;
}

const std::string &sample_escapes()
{
    static const std::string text =
        u8"\"a\\\"b\" // 引号被转义，字符串到这里才结束\n"
        u8"'c\\'d'   // 单引号串里的转义\n"
        u8"\"两\\\\个反斜杠\"\n";
    return text;
}

const std::string &sample_tail_backslash()
{
    static const std::string text = u8"\"abc\\";
    return text;
}

const std::string &sample_all_comment()
{
    static const std::string text = u8"// 整行注释\n/* 整块注释 */\n";
    return text;
}

const std::string &sample_trace()
{
    static const std::string text =
        u8"\"a\\\"b\" /*c*/ //d\n"
        u8"'a\\'b'\n"
        u8"\"e//f\"\n";
    return text;
}

const std::string &marker_token()
{
    static const std::string token = u8"@@";
    return token;
}

/* ================= 报告 ================= */

namespace {

/** 表格按显示宽度对齐：CJK 与全角标点算 2 列，其余算 1 列 */
bool is_wide_codepoint(unsigned int cp)
{
    return (cp >= 0x1100 && cp <= 0x115F) || cp == 0x2329 || cp == 0x232A ||
           (cp >= 0x2E80 && cp <= 0xA4CF && cp != 0x303F) ||
           (cp >= 0xAC00 && cp <= 0xD7A3) || (cp >= 0xF900 && cp <= 0xFAFF) ||
           (cp >= 0xFE30 && cp <= 0xFE6F) || (cp >= 0xFF00 && cp <= 0xFF60) ||
           (cp >= 0xFFE0 && cp <= 0xFFE6) || (cp >= 0x20000 && cp <= 0x3FFFD);
}

std::size_t display_width(const std::string &text)
{
    std::size_t width = 0;
    for (std::size_t i = 0; i < text.size();) {
        const unsigned char lead = static_cast<unsigned char>(text[i]);
        std::size_t length = 1;
        unsigned int cp = lead;
        if (lead >= 0xF0) {
            length = 4;
            cp = lead & 0x07u;
        } else if (lead >= 0xE0) {
            length = 3;
            cp = lead & 0x0Fu;
        } else if (lead >= 0xC0) {
            length = 2;
            cp = lead & 0x1Fu;
        }
        for (std::size_t k = 1; k < length && i + k < text.size(); ++k) {
            cp = (cp << 6) | (static_cast<unsigned char>(text[i + k]) & 0x3Fu);
        }
        width += is_wide_codepoint(cp) ? 2 : 1;
        i += length;
    }
    return width;
}

std::string pad_right(const std::string &text, std::size_t width)
{
    const std::size_t shown = display_width(text);
    if (shown >= width) {
        return text;
    }
    return text + std::string(width - shown, ' ');
}

std::string percent(std::size_t part, std::size_t whole)
{
    std::ostringstream os;
    if (whole == 0) {
        os << "0.00%";
        return os.str();
    }
    os << std::fixed << std::setprecision(2)
       << (100.0 * static_cast<double>(part) / static_cast<double>(whole)) << "%";
    return os.str();
}

std::string position_text(const Position &pos)
{
    if (!pos.found) {
        return u8"没找到";
    }
    std::ostringstream os;
    os << u8"第 " << pos.line << u8" 行第 " << pos.column << u8" 列";
    return os.str();
}

std::string state_list()
{
    std::string out;
    for (std::size_t i = 0; i < kStateCount; ++i) {
        if (i != 0) {
            out += u8"、";
        }
        out += state_name(static_cast<ScanState>(i));
    }
    return out;
}

std::string class_list()
{
    std::string out;
    for (std::size_t i = 0; i < kClassCount; ++i) {
        if (i != 0) {
            out += u8"、";
        }
        out += class_name(static_cast<CharClass>(i));
    }
    return out;
}

void append_three_forms(std::ostringstream &os)
{
    struct NamedSample {
        const char *label;
        const std::string *text;
    };
    const NamedSample samples[] = {
        {u8"主样例 JSONC，含中文与两种注释", &sample_jsonc()},
        {u8"转义样例，转义引号与转义单引号", &sample_escapes()},
        {u8"末尾单个反斜杠，字符串没闭合", &sample_tail_backslash()},
        {u8"全是注释，两行", &sample_all_comment()},
    };

    os << u8"一、同一个状态机的三种写法\n";
    os << u8"  状态 " << kStateCount << u8" 个：" << state_list() << "\n";
    os << u8"  输入类 " << kClassCount << u8" 个：" << class_list() << "\n";
    os << u8"  转移表 " << kStateCount << u8" 行 × " << kClassCount << u8" 列 = " << kEntryCount << u8" 格\n";
    os << u8"  同一份输入，三种写法各跑一遍，比输出、比结构量\n";

    for (const NamedSample &sample : samples) {
        const ScanResult base = scan(*sample.text, MachineKind::Switch, 0);
        os << u8"  样例：" << sample.label << u8"，" << base.bytes << u8" 字节、"
           << count_lines(*sample.text) << u8" 行\n";
        os << "    " << pad_right(u8"写法", 18) << pad_right(u8"输出长度", 12) << pad_right(u8"扫描步数", 12)
           << pad_right(u8"状态转移次数", 16) << pad_right(u8"走到的格子", 14) << u8"逐位相同\n";
        for (int m = 0; m < 3; ++m) {
            const MachineKind kind = static_cast<MachineKind>(m);
            const ScanResult result = scan(*sample.text, kind, 0);
            const bool same = result.same_shape_as(base);
            os << "    " << pad_right(machine_name(kind), 18)
               << pad_right(std::to_string(result.text.size()), 12)
               << pad_right(std::to_string(result.steps), 12)
               << pad_right(std::to_string(result.transitions), 16)
               << pad_right(std::to_string(result.cells_used), 14)
               << (m == 0 ? u8"基准" : (same ? u8"是" : u8"否")) << "\n";
        }
    }

    os << u8"  加一个状态、加一个输入字符，各要改几处：\n";
    os << "    " << pad_right(u8"写法", 20) << pad_right(u8"加一个状态要改", 30)
       << u8"加一个要单独处理的输入字符要改\n";
    os << "    " << pad_right(u8"switch 写法", 20) << pad_right(u8"1 个 case，里面 7 个分支", 30)
       << u8"9 个 case 各补 1 个分支\n";
    os << "    " << pad_right(u8"表驱动写法", 20) << pad_right(u8"1 行，7 格", 30) << u8"9 行各补 1 格\n";
    os << "    " << pad_right(u8"函数指针表写法", 20) << pad_right(u8"1 个处理函数加 1 个数组项", 30)
       << u8"9 个处理函数各补 1 个分支\n";
    os << u8"    三种写法共用：加状态要动 ScanState 与 state_name 两处，\n";
    os << u8"      加输入字符要动 classify 一处\n";
    os << u8"    新字符落在已有的类里（例如 @ 与 # 都归入「其它」）：三种写法都是 0 处，一行不用改\n";
    os << u8"    三种写法都不预读下一个字节，所以都没有「越界保护」这一行要写\n";
    os << "\n";
}

void append_escapes(std::ostringstream &os);

void append_naive(std::ostringstream &os)
{
    const std::string &text = sample_jsonc();
    const std::string naive = strip_comments_naive(text);
    const ScanResult machine = scan(text, MachineKind::Switch, 0);
    const Position in_pos = find_marker(text, marker_token());
    const Position naive_pos = find_marker(naive, marker_token());
    const Position machine_pos = find_marker(machine.text, marker_token());

    os << u8"二、剥注释：朴素做法错在哪\n";
    os << u8"  输入 " << text.size() << u8" 字节、" << count_lines(text) << u8" 行\n";
    os << u8"  朴素做法：先找块注释起点删到终点，再找行注释起点删到行尾\n";
    os << u8"    输出 " << naive.size() << u8" 字节、" << count_lines(naive) << u8" 行，网址那一行被截成\n";
    os << u8"      " << fenced(line_at(naive, 3)) << "\n";
    os << u8"    它找到的头一处两条斜杠在 https:// 里，网址连同后面的真注释一起被删掉\n";
    os << u8"    输出里还剩 example.com："
       << (naive.find("example.com") != std::string::npos ? u8"是" : u8"否") << "\n";
    os << u8"  状态机做法：逐字节扫描，只认状态\n";
    os << u8"    输出 " << machine.text.size() << u8" 字节、" << count_lines(machine.text)
       << u8" 行，网址那一行是\n";
    os << u8"      " << fenced(line_at(machine.text, 3)) << "\n";
    os << u8"    它知道自己在双引号串里，串里的两条斜杠一个字节都不动\n";
    os << u8"    输出里还剩 example.com："
       << (machine.text.find("example.com") != std::string::npos ? u8"是" : u8"否") << "\n";
    os << u8"    标记 " << marker_token() << u8" 的位置：原文 " << position_text(in_pos)
       << u8"，处理后 " << position_text(machine_pos) << u8"，朴素处理后 " << position_text(naive_pos) << "\n";
    append_escapes(os);
}

/** 转义样例：把每一行的原文与处理结果并排摆出来。
    串里的转义引号不结束串，单引号串里的斜杠也不动，被换掉的只有真注释。 */
void append_escapes(std::ostringstream &os)
{
    const std::string &text = sample_escapes();
    const ScanResult result = scan(text, MachineKind::Switch, 0);

    os << u8"  转义样例 " << text.size() << u8" 字节、"
       << count_lines(text) << u8" 行\n";
    for (std::size_t line = 1; line <= count_lines(text); ++line) {
        os << u8"    原文   " << fenced(line_at(text, line)) << "\n";
        os << u8"    处理后 " << fenced(line_at(result.text, line)) << "\n";
    }
    os << u8"    三行里只有两处真注释被换成空格，共 " << result.blanked
       << u8" 个字节，字符串部分一个字节没动\n";
    os << "\n";
}

void append_trace(std::ostringstream &os)
{
    const std::string &text = sample_trace();
    const ScanResult result = scan(text, MachineKind::Switch, text.size());

    os << u8"三、状态转移看得见\n";
    os << u8"  样例 " << text.size() << u8" 字节、纯 ASCII，一个字节就是一个字符：\n";
    for (std::size_t line = 1; line <= count_lines(text); ++line) {
        os << "    " << fenced(line_at(text, line)) << "\n";
    }
    os << u8"  逐字节走一遍，" << result.steps << u8" 步全列出来：\n";
    os << "    " << pad_right(u8"i", 5) << pad_right(u8"字节", 8) << pad_right(u8"类", 10)
       << pad_right(u8"状态(前)", 16) << pad_right(u8"串内", 8) << pad_right(u8"动作", 22)
       << u8"状态(后)\n";
    for (const Step &record : result.trace) {
        os << "    " << pad_right(std::to_string(record.index), 5)
           << pad_right(describe_byte(record.byte), 8)
           << pad_right(class_name(record.cls), 10)
           << pad_right(state_name(record.from), 16)
           << pad_right(record.in_string_before ? u8"是" : u8"否", 8)
           << pad_right(action_name(record.action), 22)
           << state_name(record.to) << "\n";
    }
    std::size_t states_visited = 0;
    for (std::size_t s = 0; s < kStateCount; ++s) {
        if (result.state_entries[s] != 0) {
            ++states_visited;
        }
    }
    os << u8"  " << result.steps << u8" 步里状态转移 " << result.transitions << u8" 次，走到 "
       << kStateCount << u8" 个状态里的 " << states_visited << u8" 个、" << kEntryCount
       << u8" 格里的 " << result.cells_used << u8" 格\n";
    os << u8"  扫描器按字节走：一个汉字在 UTF-8 里占三个字节，就算三步，\n";
    os << u8"    所以第一节的表里 " << sample_jsonc().size() << u8" 字节也就是 "
       << sample_jsonc().size() << u8" 步\n";
    os << "\n";
}

void append_positions(std::ostringstream &os)
{
    const std::string &text = sample_jsonc();
    const ScanResult machine = scan(text, MachineKind::Switch, 0);
    const std::string naive = strip_comments_naive(text);
    const Position in_pos = find_marker(text, marker_token());
    const Position machine_pos = find_marker(machine.text, marker_token());
    const Position naive_pos = find_marker(naive, marker_token());
    const bool same_position = in_pos.found && machine_pos.found && in_pos.line == machine_pos.line &&
                               in_pos.column == machine_pos.column;

    os << u8"四、注释换成空格：长度不变，位置信息不失效\n";
    os << u8"  主样例 " << text.size() << u8" 字节、" << count_lines(text) << u8" 行\n";
    os << u8"  处理后 " << machine.text.size() << u8" 字节、" << count_lines(machine.text)
       << u8" 行，字节数相同：" << (machine.text.size() == text.size() ? u8"是" : u8"否")
       << u8"，行数相同：" << (count_lines(machine.text) == count_lines(text) ? u8"是" : u8"否") << "\n";
    os << u8"  块注释跨两行，里面的换行原样保留，行号才不会整体前移\n";
    os << u8"  被换成空格的字节 " << machine.blanked << u8" 个，占全部的 "
       << percent(machine.blanked, machine.bytes) << u8"：块注释 " << machine.blanked_block
       << u8" 个、行注释 " << machine.blanked_line << u8" 个\n";
    os << u8"  在字符串里读到的字节 " << machine.in_string_bytes << u8" 个，字符串外 "
       << machine.out_string_bytes << u8" 个，两者之和 " << (machine.in_string_bytes + machine.out_string_bytes)
       << u8" 等于总字节数\n";
    os << u8"  块注释第二行处理后长这样，竖线之间全是空格，长度与原文一样：\n";
    os << "    " << fenced(line_at(machine.text, 5)) << "\n";
    os << u8"  标记 " << marker_token() << u8" 的位置：原文 " << position_text(in_pos) << u8"，处理后 "
       << position_text(machine_pos) << u8"，相同：" << (same_position ? u8"是" : u8"否") << "\n";
    os << u8"  对照：朴素做法删掉字节，输出 " << naive.size() << u8" 字节、" << count_lines(naive)
       << u8" 行，标记跑到 " << position_text(naive_pos) << "\n";
    os << "\n";
}

void append_overrun(std::ostringstream &os)
{
    const std::string &text = sample_tail_backslash();
    const ScanResult machine = scan(text, MachineKind::Switch, 0);
    const std::string guarded = copy_with_escape(text, true);
    const std::string unguarded = copy_with_escape(text, false);

    os << u8"五、末尾单个反斜杠：越界保护是干什么的\n";
    os << u8"  输入 \"abc\\ 共 " << text.size() << u8" 字节，末字节是反斜杠 0x5C，双引号没有闭合\n";
    os << u8"  状态机：走了 " << machine.steps << u8" 步、状态转移 " << machine.transitions
       << u8" 次，末状态是" << state_name(machine.final_state) << "\n";
    os << u8"    输出 " << machine.text.size() << u8" 字节，与输入逐位相同："
       << (machine.text == text ? u8"是" : u8"否") << u8"，最后一个字节还是那个反斜杠\n";
    os << u8"    它只看当前字节，从不预读下一个字节，所以没有越界的机会\n";
    os << u8"  预读式写法：遇到反斜杠就把下一个字节一起收下\n";
    os << u8"    带保护 i + 1 < n：输出 " << guarded.size() << u8" 字节，与输入逐位相同："
       << (guarded == text ? u8"是" : u8"否") << "\n";
    os << u8"    不带保护：输出 " << unguarded.size() << u8" 字节，比输入多 "
       << (unguarded.size() - text.size()) << u8" 字节，多出来的是 "
       << (unguarded.empty() ? std::string(u8"没有") : describe_byte(unguarded.back())) << "\n";
    os << u8"      下标 i + 1 已经走到 size() 上，取回来的是 std::string 的结尾空字符\n";
    os << u8"      这一版把空字符也写进输出，长度一变，后面的位置全错\n";
    os << u8"      输入若是 const char*，这一次读取就是真的越界\n";
}

}   /* namespace */

std::string build_report()
{
    std::ostringstream os;
    append_three_forms(os);
    append_naive(os);
    append_trace(os);
    append_positions(os);
    append_overrun(os);
    return os.str();
}

/* ================= 自测 ================= */

namespace {

class Checks {
public:
    void expect(bool ok, const std::string &what)
    {
        ++total_;
        if (ok) {
            ++passed_;
            lines_.push_back(u8"[通过] " + std::to_string(total_) + ". " + what);
        } else {
            ++failed_;
            lines_.push_back(u8"[失败] " + std::to_string(total_) + ". " + what);
        }
    }

    void expect_eq_size(std::size_t got, std::size_t want, const std::string &what)
    {
        if (got == want) {
            expect(true, what);
        } else {
            expect(false, what + u8"（得到 " + std::to_string(got) + u8"，应为 " +
                              std::to_string(want) + u8"）");
        }
    }

    CheckResult finish() const
    {
        CheckResult result;
        result.total = total_;
        result.passed = passed_;
        result.failed = failed_;
        result.lines = lines_;
        return result;
    }

private:
    std::size_t total_ = 0;
    std::size_t passed_ = 0;
    std::size_t failed_ = 0;
    std::vector<std::string> lines_;
};

std::vector<std::string> all_samples()
{
    return std::vector<std::string>{sample_jsonc(), sample_escapes(), sample_tail_backslash(),
                                    sample_all_comment(), sample_trace()};
}

}   /* namespace */

std::string CheckResult::summary() const
{
    std::ostringstream os;
    os << total << u8" 项中 " << passed << u8" 项通过";
    if (failed == 0) {
        os << u8"，全部通过";
    } else {
        os << u8"，" << failed << u8" 项不符";
    }
    return os.str();
}

CheckResult run_self_tests()
{
    Checks checks;
    const std::vector<std::string> samples = all_samples();

    /* 1 三种写法在整张转移表上一致 */
    bool grid_same = true;
    for (std::size_t s = 0; s < kStateCount; ++s) {
        for (std::size_t c = 0; c < kClassCount; ++c) {
            const ScanState state = static_cast<ScanState>(s);
            const CharClass cls = static_cast<CharClass>(c);
            const TableEntry by_switch = step_switch(state, cls);
            if (by_switch != step_table(state, cls) || by_switch != step_functions(state, cls)) {
                grid_same = false;
            }
        }
    }
    checks.expect(grid_same, u8"三种写法在 " + std::to_string(kEntryCount) +
                                  u8" 个 (状态, 输入类) 组合上返回同一份转移");

    /* 2 三种写法跑同一份输入的输出与结构量 */
    bool machines_same = true;
    for (const std::string &sample : samples) {
        const ScanResult by_switch = scan(sample, MachineKind::Switch, 0);
        if (!by_switch.same_shape_as(scan(sample, MachineKind::Table, 0)) ||
            !by_switch.same_shape_as(scan(sample, MachineKind::Functions, 0))) {
            machines_same = false;
        }
    }
    checks.expect(machines_same, u8"三种写法在五个样例上输出逐位相同、结构量也相同");

    /* 3 转移次数的定义 */
    bool transition_rule = true;
    for (const std::string &sample : samples) {
        const ScanResult result = scan(sample, MachineKind::Switch, sample.size());
        std::size_t changed = 0;
        for (const Step &record : result.trace) {
            if (record.to != record.from) {
                ++changed;
            }
        }
        if (result.trace.size() != result.steps || changed != result.transitions) {
            transition_rule = false;
        }
    }
    checks.expect(transition_rule, u8"状态转移次数等于逐步状态序列里相邻不同的步数");

    /* 4 长度与步数 */
    bool length_rule = true;
    for (const std::string &sample : samples) {
        const ScanResult result = scan(sample, MachineKind::Switch, 0);
        if (result.text.size() != sample.size() || result.steps != sample.size() ||
            result.bytes != sample.size()) {
            length_rule = false;
        }
    }
    checks.expect(length_rule, u8"五个样例处理前后字节数相同，扫描步数等于字节数");

    /* 5 行数 */
    bool line_rule = true;
    for (const std::string &sample : samples) {
        const ScanResult result = scan(sample, MachineKind::Switch, 0);
        if (count_lines(result.text) != count_lines(sample)) {
            line_rule = false;
        }
    }
    checks.expect(line_rule, u8"五个样例处理前后行数相同：注释里的换行原样保留");

    /* 6 改动只落在注释上 */
    bool only_comments = true;
    for (const std::string &sample : samples) {
        const ScanResult result = scan(sample, MachineKind::Switch, 0);
        for (std::size_t i = 0; i < sample.size(); ++i) {
            if (result.text[i] != sample[i] && !(sample[i] != ' ' && result.text[i] == ' ')) {
                only_comments = false;
            }
        }
    }
    checks.expect(only_comments, u8"处理结果与原文的差异只出现在「原文不是空格、结果是空格」的位置");

    /* 7 串内与串外的字节数配平，两类注释换掉的字节数之和等于总数 */
    bool split_rule = true;
    for (const std::string &sample : samples) {
        const ScanResult result = scan(sample, MachineKind::Switch, 0);
        if (result.in_string_bytes + result.out_string_bytes != result.bytes ||
            result.blanked_line + result.blanked_block != result.blanked) {
            split_rule = false;
        }
    }
    checks.expect(split_rule, u8"字符串内外字节数配平，行注释与块注释换掉的字节数之和等于被换掉的总数");

    /* 8—9 主样例的标记位置 */
    const ScanResult jsonc = scan(sample_jsonc(), MachineKind::Switch, 0);
    const Position in_pos = find_marker(sample_jsonc(), marker_token());
    const Position out_pos = find_marker(jsonc.text, marker_token());
    checks.expect(in_pos.found && out_pos.found && in_pos.line == out_pos.line &&
                      in_pos.column == out_pos.column && in_pos.line == 8 && in_pos.column == 14,
                  u8"主样例的标记在原文与处理后都是第 8 行第 14 列");

    /* 10—11 朴素做法 */
    const std::string naive = strip_comments_naive(sample_jsonc());
    const Position naive_pos = find_marker(naive, marker_token());
    checks.expect(naive.find("example.com") == std::string::npos &&
                      naive.size() < sample_jsonc().size() &&
                      count_lines(naive) < count_lines(sample_jsonc()),
                  u8"朴素做法把 https:// 里的两条斜杠当成注释，网址被删、输出变短、行数也变少");
    checks.expect(naive_pos.found && naive_pos.line != in_pos.line,
                  u8"朴素做法之后标记的行号变了：位置信息失效");

    /* 12—13 字符串里的斜杠 */
    const std::string quoted = u8"\"a//b\"";
    const ScanResult quoted_run = scan(quoted, MachineKind::Switch, 0);
    const std::string single_quoted = u8"'a//b'";
    const ScanResult single_run = scan(single_quoted, MachineKind::Switch, 0);
    checks.expect(quoted_run.text == quoted && quoted_run.blanked == 0 && quoted_run.transitions == 2,
                  u8"双引号串里的两条斜杠不算注释：输出不变、替换 0 次、转移 2 次");
    checks.expect(single_run.text == single_quoted && single_run.blanked == 0,
                  u8"单引号串里的两条斜杠同样不算注释");

    /* 14 转义引号 */
    const std::string escape_input = u8"\"a\\\"b\" // c";
    const ScanResult escape_run = scan(escape_input, MachineKind::Switch, 0);
    checks.expect(escape_run.text.size() == escape_input.size() && escape_run.blanked == 3 &&
                      escape_run.text.compare(0, 6, u8"\"a\\\"b\"") == 0,
                  u8"转义引号不结束字符串：串原样保留，只有真注释的 3 个字节换成空格");

    /* 15 行注释 */
    const ScanResult line_run = scan(u8"//x\n", MachineKind::Switch, 0);
    checks.expect(line_run.text == u8"   \n" && line_run.blanked == 3 && line_run.transitions == 3,
                  u8"行注释的三个字节换成空格，换行留着，状态回到普通");

    /* 16 块注释不嵌套 */
    const ScanResult nested_run = scan(u8"/* /* */x", MachineKind::Switch, 0);
    checks.expect(nested_run.text == u8"        x" && nested_run.blanked == 6,
                  u8"块注释不嵌套：头一个 */ 就结束注释，只留下末尾的 x");

    /* 17—19 末尾单个反斜杠 */
    const ScanResult tail_run = scan(sample_tail_backslash(), MachineKind::Switch, 0);
    checks.expect(tail_run.text == sample_tail_backslash() &&
                      tail_run.final_state == ScanState::DoubleEscape && tail_run.steps == 5,
                  u8"末尾单个反斜杠：状态机走满 5 步不越界，末状态是双引号转义");
    checks.expect(copy_with_escape(sample_tail_backslash(), true) == sample_tail_backslash(),
                  u8"预读式带保护 i + 1 < n：末尾反斜杠不再往下取，输出与输入相同");
    const std::string unguarded = copy_with_escape(sample_tail_backslash(), false);
    checks.expect(unguarded.size() == sample_tail_backslash().size() + 1 && !unguarded.empty() &&
                      unguarded.back() == '\0',
                  u8"预读式不带保护：输出比输入多一个字节，多出来的是空字符");

    /* 20 空输入 */
    const ScanResult empty_run = scan(std::string(), MachineKind::Switch, 0);
    checks.expect(empty_run.steps == 0 && empty_run.transitions == 0 && empty_run.text.empty() &&
                      empty_run.final_state == ScanState::Normal && empty_run.cells_used == 0,
                  u8"空输入：0 步、0 次转移、输出为空、停在普通状态");

    /* 21 全是注释 */
    const ScanResult all_comment = scan(sample_all_comment(), MachineKind::Switch, 0);
    bool blanks_and_newlines = true;
    for (const char byte : all_comment.text) {
        if (byte != '\n' && byte != ' ') {
            blanks_and_newlines = false;
        }
    }
    checks.expect(blanks_and_newlines && count_lines(all_comment.text) == 2,
                  u8"全是注释的样例：除换行以外的字节都成了空格，行数不变");

    /* 22 分类函数覆盖全部字节值 */
    bool every_byte_classified = true;
    bool class_seen[kClassCount] = {};
    for (int value = 0; value < 256; ++value) {
        const CharClass cls = classify(static_cast<char>(value));
        const std::size_t index = static_cast<std::size_t>(cls);
        if (index >= kClassCount) {
            every_byte_classified = false;
        } else {
            class_seen[index] = true;
        }
    }
    bool every_class_used = true;
    for (std::size_t c = 0; c < kClassCount; ++c) {
        if (!class_seen[c]) {
            every_class_used = false;
        }
    }
    checks.expect(every_byte_classified && every_class_used && classify('/') == CharClass::Slash &&
                      classify('\n') == CharClass::Newline,
                  u8"分类函数：0 到 255 全部能分类，七个输入类都用得上");

    /* 23 状态覆盖 */
    const ScanResult trace_run = scan(sample_trace(), MachineKind::Switch, 0);
    bool states_covered = true;
    for (std::size_t s = 0; s < kStateCount; ++s) {
        if (trace_run.state_entries[s] == 0) {
            states_covered = false;
        }
    }
    checks.expect(states_covered, u8"状态转移样例把九个状态都走到了");

    /* 24 格子覆盖 */
    std::vector<std::vector<std::size_t>> merged(kStateCount, std::vector<std::size_t>(kClassCount, 0));
    for (const std::string &sample : samples) {
        const ScanResult result = scan(sample, MachineKind::Switch, 0);
        for (std::size_t s = 0; s < kStateCount; ++s) {
            for (std::size_t c = 0; c < kClassCount; ++c) {
                merged[s][c] += result.cells[s][c];
            }
        }
    }
    std::size_t used = 0;
    for (std::size_t s = 0; s < kStateCount; ++s) {
        for (std::size_t c = 0; c < kClassCount; ++c) {
            if (merged[s][c] != 0) {
                ++used;
            }
        }
    }
    checks.expect_eq_size(used, 25, u8"五个样例合起来走到 63 格里的 25 格");

    return checks.finish();
}

}   /* namespace slab */
