/**
 * list_mini.cpp —— 链表的实现、项目输出与自测
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
 * 这里没有任何界面代码：不含 <windows.h>，也不打印任何东西。
 * 报告里的数字全是计数器的增量：节点分配与归还、元素构造与析构、
 * 指针赋值次数、为了数元素个数走过的步数。
 * 节点地址一个都不打印——地址每次运行都不同，那属于不可复现的量。
 */
#include "list_mini.hpp"

#include <iomanip>
#include <sstream>
#include <string>
#include <utility>

namespace lmini {

void LinkStats::reset()
{
    *this = LinkStats();
}

LinkStats &stats()
{
    static LinkStats instance;
    return instance;
}

std::string CheckResult::summary() const
{
    std::ostringstream os;
    os << total << u8" 项中 " << passed << u8" 项通过";
    if (failed == 0) {
        os << u8"，全部通过";
    } else {
        os << u8"，" << failed << u8" 项失败";
    }
    return os.str();
}

/* ================= 报告用的小工具 ================= */

namespace {

bool is_wide_codepoint(unsigned int cp)
{
    return (cp >= 0x1100u && cp <= 0x115Fu) || (cp >= 0x2E80u && cp <= 0x303Eu)
           || (cp >= 0x3041u && cp <= 0x33FFu) || (cp >= 0x3400u && cp <= 0x4DBFu)
           || (cp >= 0x4E00u && cp <= 0x9FFFu) || (cp >= 0xA000u && cp <= 0xA4CFu)
           || (cp >= 0xAC00u && cp <= 0xD7A3u) || (cp >= 0xF900u && cp <= 0xFAFFu)
           || (cp >= 0xFE30u && cp <= 0xFE6Fu) || (cp >= 0xFF00u && cp <= 0xFF60u)
           || (cp >= 0xFFE0u && cp <= 0xFFE6u);
}

/** 文本占多少列：CJK 与全角标点算 2 列，其余算 1 列 */
std::size_t display_width(const std::string &text)
{
    std::size_t width = 0;
    for (std::size_t i = 0; i < text.size();) {
        const unsigned char lead = static_cast<unsigned char>(text[i]);
        unsigned int cp = lead;
        std::size_t length = 1;
        if (lead >= 0xF0u) {
            cp = lead & 0x07u;
            length = 4;
        } else if (lead >= 0xE0u) {
            cp = lead & 0x0Fu;
            length = 3;
        } else if (lead >= 0xC0u) {
            cp = lead & 0x1Fu;
            length = 2;
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
    const std::size_t used = display_width(text);
    return used >= width ? text : text + std::string(width - used, ' ');
}

std::string pad_left(std::size_t value, std::size_t width)
{
    const std::string text = std::to_string(value);
    return text.size() >= width ? text : std::string(width - text.size(), ' ') + text;
}

std::string pad_left_text(const std::string &text, std::size_t width)
{
    const std::size_t used = display_width(text);
    return used >= width ? text : std::string(width - used, ' ') + text;
}

/** 把一条链的元素按顺序拼出来，元素之间留一个空格 */
std::string join_values(const List<int> &list)
{
    std::ostringstream os;
    bool first = true;
    for (int value : list) {
        if (!first) {
            os << ' ';
        }
        os << value;
        first = false;
    }
    return os.str();
}

/** 走一遍数节点个数，用来核对 size() 没有算错 */
std::size_t count_nodes(const List<int> &list)
{
    std::size_t count = 0;
    for (auto it = list.begin(); it != list.end(); ++it) {
        ++count;
    }
    return count;
}

/* 自测的小工具：把每一项的结果记下来，不打印 */
class Checker {
public:
    void check(bool ok, const std::string &what, const std::string &detail = std::string())
    {
        ++result_.total;
        std::ostringstream line;
        if (ok) {
            ++result_.passed;
            line << u8"[通过] " << result_.total << ". " << what;
        } else {
            ++result_.failed;
            line << u8"[失败] " << result_.total << ". " << what;
            if (!detail.empty()) {
                line << u8"（" << detail << u8"）";
            }
        }
        result_.lines.push_back(line.str());
    }

    CheckResult take() { return result_; }

private:
    CheckResult result_;
};

List<int> make_sequence(int from, int count)
{
    List<int> list;
    for (int i = 0; i < count; ++i) {
        list.push_back(from + i);
    }
    return list;
}

/* 一次 splice 的账 */
struct SpliceProbe {
    std::size_t moved = 0;
    std::size_t link_writes = 0;
    std::size_t count_steps = 0;
    std::size_t value_creations = 0;
    std::size_t value_destructions = 0;
    std::size_t node_creations = 0;
    std::size_t node_destructions = 0;
    std::size_t from_size = 0;
    std::size_t to_size = 0;
    bool address_kept = false;
};

/** 一段：从 a 里搬走全部 len 个元素 */
SpliceProbe probe_range(std::size_t len)
{
    List<int> a = make_sequence(1000, static_cast<int>(len));
    List<int> b = make_sequence(0, 3);
    List<int>::iterator first = a.begin();
    const NodeBase *address = first.node_address();
    const int head = *first;

    stats().reset();
    b.splice(b.end(), a, first, a.end());

    SpliceProbe probe;
    probe.moved = len;
    probe.link_writes = stats().link_writes;
    probe.count_steps = stats().count_steps;
    probe.value_creations = stats().value_constructions;
    probe.value_destructions = stats().value_destructions;
    probe.node_creations = stats().node_creations;
    probe.node_destructions = stats().node_destructions;
    probe.from_size = a.size();
    probe.to_size = b.size();
    probe.address_kept = first.node_address() == address && *first == head;
    return probe;
}

/** 一个元素：把 a 的第一个搬到 b 的开头 */
SpliceProbe probe_one()
{
    List<int> a = make_sequence(1000, 1);
    List<int> b = make_sequence(0, 3);
    List<int>::iterator it = a.begin();
    const NodeBase *address = it.node_address();
    const int head = *it;

    stats().reset();
    b.splice(b.begin(), a, it);

    SpliceProbe probe;
    probe.moved = 1;
    probe.link_writes = stats().link_writes;
    probe.count_steps = stats().count_steps;
    probe.value_creations = stats().value_constructions;
    probe.value_destructions = stats().value_destructions;
    probe.node_creations = stats().node_creations;
    probe.node_destructions = stats().node_destructions;
    probe.from_size = a.size();
    probe.to_size = b.size();
    probe.address_kept = it.node_address() == address && *it == head;
    return probe;
}

/** 整条链：把 a 全部接到 b 的末尾 */
SpliceProbe probe_whole(std::size_t len)
{
    List<int> a = make_sequence(1000, static_cast<int>(len));
    List<int> b = make_sequence(0, 3);
    List<int>::iterator first = a.begin();
    const NodeBase *address = first.node_address();
    const int head = *first;

    stats().reset();
    b.splice(b.end(), a);

    SpliceProbe probe;
    probe.moved = len;
    probe.link_writes = stats().link_writes;
    probe.count_steps = stats().count_steps;
    probe.value_creations = stats().value_constructions;
    probe.value_destructions = stats().value_destructions;
    probe.node_creations = stats().node_creations;
    probe.node_destructions = stats().node_destructions;
    probe.from_size = a.size();
    probe.to_size = b.size();
    probe.address_kept = first.node_address() == address && *first == head;
    return probe;
}

void append_splice_row(std::ostringstream &os, const std::string &what, std::size_t length,
                       const SpliceProbe &probe)
{
    os << "  " << pad_right(what, 12) << pad_left(length, 6) << "  "
       << pad_left(probe.link_writes, 10) << "  " << pad_left(probe.count_steps, 10) << "  "
       << pad_left(probe.value_creations, 8) << "  " << pad_left(probe.value_destructions, 8)
       << "  " << pad_right(probe.address_kept ? u8"是" : u8"否", 8)
       << pad_left(probe.from_size, 6) << "  " << pad_left(probe.to_size, 6) << "\n";
}

void append_sentinel_section(std::ostringstream &os)
{
    os << u8"哨兵不构造元素：节点与元素的计数（连续 push_back 1 到 10 个）\n";
    os << u8"  元素个数  节点分配次数  元素构造次数\n";

    stats().reset();
    List<int> list;
    os << "  " << pad_left(0, 8) << "  " << pad_left(stats().node_creations, 12) << "  "
       << pad_left(stats().value_constructions, 12) << "\n";
    for (int i = 1; i <= 10; ++i) {
        list.push_back(i);
        os << "  " << pad_left(list.size(), 8) << "  " << pad_left(stats().node_creations, 12)
           << "  " << pad_left(stats().value_constructions, 12) << "\n";
    }
    os << u8"  两条计数始终相等：每插一个元素恰好分配一个节点、构造一个元素\n";
    os << u8"  空链那一行是 0 与 0——哨兵是内嵌成员，既不分配节点也不构造元素\n";
}

void append_iterator_section(std::ostringstream &os)
{
    const List<int> list = make_sequence(1, 8);
    int sum = 0;
    for (int value : list) {
        sum += value;
    }

    std::ostringstream reversed;
    auto it = list.end();
    while (it != list.begin()) {
        --it;
        if (!reversed.str().empty()) {
            reversed << ' ';
        }
        reversed << *it;
    }

    auto tail = list.end();
    --tail;

    os << u8"\n能进范围 for：迭代器与 --end()\n";
    os << u8"  正向（范围 for）：" << join_values(list) << u8"，求和 " << sum << "\n";
    os << u8"  反向（从 end() 起步反复 --）：" << reversed.str() << "\n";
    os << u8"  --end() 指向最后一个元素：值是 " << *tail << "\n";
    os << u8"  距离类型是 ptrdiff_t，因此 std::distance 这类算法也能用："
       << std::distance(list.begin(), list.end()) << u8" 步走完\n";
}

void append_splice_section(std::ostringstream &os)
{
    os << u8"\nsplice 只改指针（元素构造与析构的增量都应当是 0）\n";
    os << "  " << pad_right(u8"搬什么", 12) << pad_left_text(u8"段长", 6) << "  "
       << pad_left_text(u8"改指针次数", 10) << "  " << pad_left_text(u8"数元素步数", 10) << "  "
       << pad_left_text(u8"构造增量", 8) << "  " << pad_left_text(u8"析构增量", 8) << "  "
       << pad_left_text(u8"地址不变", 8) << pad_left_text(u8"源链", 6) << "  "
       << pad_left_text(u8"目标链", 6) << "\n";
    append_splice_row(os, u8"一个元素", 1, probe_one());
    append_splice_row(os, u8"一段", 10, probe_range(10));
    append_splice_row(os, u8"一段", 100, probe_range(100));
    append_splice_row(os, u8"一段", 1000, probe_range(1000));
    append_splice_row(os, u8"整条链", 1000, probe_whole(1000));
    os << u8"  指针赋值次数恒为 6：摘下来 2 处、接上去 4 处，与段长无关——这是 O(1) 的来路\n";
    os << u8"  按段的 splice 还要走一遍数元素个数，那是为了维护 size()，不是搬元素\n";
    os << u8"  整条链的 splice 不必数：两边的 size() 都是现成的\n";
}

/** 原链上每个元素的值与它的节点地址 */
using AddressLog = std::vector<std::pair<int, const NodeBase *>>;

AddressLog log_addresses(const List<int> &list)
{
    AddressLog log;
    for (auto it = list.begin(); it != list.end(); ++it) {
        log.emplace_back(*it, it.node_address());
    }
    return log;
}

/** 现在这条链上，值在日志里出现过、但节点地址变了的元素有几个 */
std::size_t count_moved(const List<int> &list, const AddressLog &log)
{
    std::size_t moved = 0;
    for (auto it = list.begin(); it != list.end(); ++it) {
        for (const auto &entry : log) {
            if (entry.first == *it) {
                if (entry.second != it.node_address()) {
                    ++moved;
                }
                break;
            }
        }
    }
    return moved;
}

void append_stability_section(std::ostringstream &os)
{
    List<int> list = make_sequence(0, 10);
    const AddressLog log = log_addresses(list);

    os << u8"\n迭代器稳定性：插入与删除只改指针，别的节点一个都不动\n";
    os << u8"  起始：" << join_values(list) << "\n";

    list.push_front(-1);
    os << u8"  头插一个元素之后，地址变了的元素个数：" << count_moved(list, log) << "\n";

    List<int>::iterator middle = list.begin();
    for (int i = 0; i < 5; ++i) {
        ++middle;
    }
    list.insert(middle, 100);
    os << u8"  中间插一个元素之后，地址变了的元素个数：" << count_moved(list, log) << "\n";

    for (auto it = list.begin(); it != list.end(); ++it) {
        if (*it == 2) {
            list.erase(it);
            break;
        }
    }
    os << u8"  删掉值为 2 的元素之后，地址变了的元素个数：" << count_moved(list, log) << "\n";

    list.pop_back();
    os << u8"  尾部再删一个之后，地址变了的元素个数：" << count_moved(list, log) << "\n";
    os << u8"  现在：" << join_values(list) << "\n";
    os << u8"  只有被删掉的那个节点被归还，别的节点连地址都没动\n";
}

void append_boundary_section(std::ostringstream &os)
{
    stats().reset();
    List<int> list;

    os << u8"\n边界情形：空链、头部、尾部走的是同一条代码路径\n";
    list.push_back(1);
    os << u8"  空链插入第一个元素：size " << list.size() << u8"，front " << list.front()
       << u8"，back " << list.back() << "\n";
    list.push_front(0);
    os << u8"  头部插入：size " << list.size() << u8"，front " << list.front()
       << u8"，back " << list.back() << "\n";
    list.push_back(2);
    os << u8"  尾部插入：size " << list.size() << u8"，front " << list.front()
       << u8"，back " << list.back() << "\n";
    list.pop_front();
    list.pop_front();
    list.pop_back();
    os << u8"  删到空：size " << list.size() << u8"，begin == end："
       << (list.begin() == list.end() ? u8"是" : u8"否") << "\n";
    os << u8"  三次插入共走 link_in " << stats().link_in_calls << u8" 次，三次删除共走 unlink "
       << stats().unlink_calls << u8" 次\n";
    os << u8"  插入路径只有一条、删除路径只有一条，哨兵替掉的是四段边界特例\n";
}

}   /* namespace */

std::string build_report()
{
    std::ostringstream os;
    append_sentinel_section(os);
    append_iterator_section(os);
    append_splice_section(os);
    append_stability_section(os);
    append_boundary_section(os);
    return os.str();
}

/* ================= 自测 ================= */

CheckResult run_self_tests()
{
    Checker c;

    /* 1. 空链：begin 与 end 都在哨兵上 */
    {
        List<int> list;
        auto last = list.end();
        --last;
        c.check(list.begin() == list.end() && list.size() == 0 && list.empty() && last == list.end(),
                u8"空链：begin == end、size 0，--end() 仍是哨兵自己");
    }

    /* 2. push_back 之后 size 与两端都对 */
    {
        List<int> list;
        for (int i = 1; i <= 3; ++i) {
            list.push_back(i * 10);
        }
        c.check(list.size() == 3 && list.front() == 10 && list.back() == 30
                    && join_values(list) == "10 20 30",
                u8"push_back 之后 size、front、back 与序列都对", join_values(list));
    }

    /* 3. 范围 for 与手写遍历给出同一串值 */
    {
        const List<int> list = make_sequence(1, 5);
        int sum = 0;
        for (int value : list) {
            sum += value;
        }
        std::size_t walked = 0;
        for (auto it = list.cbegin(); it != list.cend(); ++it) {
            ++walked;
        }
        c.check(sum == 15 && walked == 5, u8"范围 for 与手写遍历给出同一串值：求和 15、5 个元素",
                std::to_string(sum) + u8" / " + std::to_string(walked));
    }

    /* 4. 反向遍历与正向相反 */
    {
        const List<int> list = make_sequence(1, 4);
        std::ostringstream reversed;
        auto it = list.end();
        while (it != list.begin()) {
            --it;
            if (!reversed.str().empty()) {
                reversed << ' ';
            }
            reversed << *it;
        }
        c.check(reversed.str() == "4 3 2 1", u8"从 end() 起步反复 -- 得到倒序序列", reversed.str());
    }

    /* 5. --end() 是最后一个元素 */
    {
        const List<int> list = make_sequence(7, 3);
        auto tail = list.end();
        --tail;
        c.check(*tail == 9 && tail != list.end(), u8"--end() 指向最后一个元素");
    }

    /* 6. insert 返回的迭代器指向新元素 */
    {
        List<int> list = make_sequence(1, 3);
        auto pos = list.begin();
        ++pos;
        auto inserted = list.insert(pos, 99);
        c.check(*inserted == 99 && list.size() == 4 && join_values(list) == "1 99 2 3",
                u8"insert 返回的迭代器指向新元素", join_values(list));
    }

    /* 7. erase 返回被删元素的下一个位置 */
    {
        List<int> list = make_sequence(1, 4);
        auto pos = list.begin();
        ++pos;
        auto next = list.erase(pos);
        c.check(*next == 3 && list.size() == 3 && join_values(list) == "1 3 4",
                u8"erase 返回被删元素的下一个位置", join_values(list));
    }

    /* 8. size 与实际节点数一致 */
    {
        List<int> list = make_sequence(0, 17);
        list.push_front(-1);
        list.pop_back();
        c.check(list.size() == count_nodes(list) && list.size() == 17,
                u8"size() 与走一遍数出来的节点个数一致",
                std::to_string(list.size()) + u8" / " + std::to_string(count_nodes(list)));
    }

    /* 9. clear 把节点全部归还 */
    {
        stats().reset();
        List<int> list = make_sequence(0, 5);
        const std::size_t created = stats().node_creations;
        list.clear();
        c.check(list.begin() == list.end() && list.size() == 0 && stats().live_nodes() == 0
                    && stats().node_destructions == created,
                u8"clear 之后链空了、节点全部归还",
                std::to_string(created) + u8" 个节点建了又还");
    }

    /* 10. 析构之后一个节点都不剩 */
    {
        stats().reset();
        {
            List<int> a = make_sequence(0, 4);
            List<int> b = make_sequence(10, 6);
            b = a;
        }
        c.check(stats().live_nodes() == 0 && stats().node_creations == stats().node_destructions,
                u8"两条链析构之后一个节点都不剩",
                std::to_string(stats().node_creations) + u8" 建 / "
                    + std::to_string(stats().node_destructions) + u8" 删");
    }

    /* 11. 拷贝构造得到的链独立 */
    {
        List<int> a = make_sequence(1, 3);
        List<int> b(a);
        b.push_back(4);
        b.front() = 100;
        c.check(a.size() == 3 && a.front() == 1 && b.size() == 4 && b.front() == 100,
                u8"拷贝构造得到独立的一条链：改一个不影响另一个");
    }

    /* 12. 移动构造整链接走，元素一个都不构造 */
    {
        List<int> a = make_sequence(1, 3);
        stats().reset();
        List<int> b(std::move(a));
        c.check(b.size() == 3 && a.size() == 0 && a.begin() == a.end()
                    && stats().value_constructions == 0 && stats().node_creations == 0,
                u8"移动构造把整链接走：元素构造 0 次、节点分配 0 次",
                std::to_string(stats().value_constructions) + u8" 次元素构造");
    }

    /* 13. splice 整条链 */
    {
        List<int> a = make_sequence(1, 3);
        List<int> b = make_sequence(10, 2);
        b.splice(b.end(), a);
        c.check(a.size() == 0 && b.size() == 5 && join_values(b) == "10 11 1 2 3",
                u8"splice 整条链：两边 size 与序列都对", join_values(b));
    }

    /* 14. splice 一段 */
    {
        List<int> a = make_sequence(1, 5);
        List<int> b = make_sequence(0, 2);
        auto first = a.begin();
        ++first;
        auto last = first;
        ++last;
        ++last;
        b.splice(b.end(), a, first, last);
        c.check(a.size() == 3 && b.size() == 4 && join_values(a) == "1 4 5"
                    && join_values(b) == "0 1 2 3",
                u8"splice 一段：两边的序列都对",
                join_values(a) + u8" | " + join_values(b));
    }

    /* 15. splice 不构造也不析构元素 */
    {
        List<int> a = make_sequence(1, 100);
        List<int> b;
        stats().reset();
        b.splice(b.end(), a);
        c.check(stats().value_constructions == 0 && stats().value_destructions == 0
                    && stats().node_creations == 0 && stats().node_destructions == 0,
                u8"splice 不构造也不析构元素、不分配也不归还节点",
                std::to_string(stats().value_constructions) + u8" 次构造");
    }

    /* 16. splice 的指针赋值次数与段长无关 */
    {
        const SpliceProbe one = probe_one();
        const SpliceProbe many = probe_range(1000);
        c.check(one.link_writes == 6 && many.link_writes == 6 && many.count_steps == 1000,
                u8"splice 的指针赋值次数恒为 6，与段长无关（段长只影响数个数走的步数）",
                std::to_string(one.link_writes) + u8" / " + std::to_string(many.link_writes));
    }

    /* 17. splice 之后指向被搬元素的迭代器仍然有效 */
    {
        List<int> a = make_sequence(1, 4);
        List<int> b;
        auto it = a.begin();
        ++it;
        const NodeBase *address = it.node_address();
        b.splice(b.begin(), a, it);
        c.check(it.node_address() == address && *it == 2 && b.size() == 1 && a.size() == 3,
                u8"splice 之后迭代器仍然有效：节点地址没变、值也没变");
    }

    /* 18. 哨兵不构造元素 */
    {
        stats().reset();
        List<int> list;
        const bool empty_cost_none = stats().node_creations == 0 && stats().value_constructions == 0;
        list.push_back(1);
        c.check(empty_cost_none && stats().node_creations == 1 && stats().value_constructions == 1,
                u8"哨兵不构造元素：空链的节点分配与元素构造都是 0",
                std::to_string(stats().node_creations) + u8" 个节点");
    }

    /* 19. 自己 splice 自己 */
    {
        List<int> list = make_sequence(1, 6);
        auto first = list.begin();
        ++first;
        auto last = first;
        ++last;
        ++last;
        list.splice(list.end(), list, first, last);
        c.check(list.size() == 6 && join_values(list) == "1 4 5 6 2 3",
                u8"自己 splice 自己：元素个数不变、顺序正确", join_values(list));
    }

    /* 20. swap 只换节点，不重新构造元素 */
    {
        List<int> a = make_sequence(1, 3);
        List<int> b = make_sequence(7, 2);
        stats().reset();
        a.swap(b);
        c.check(join_values(a) == "7 8" && join_values(b) == "1 2 3" && a.size() == 2
                    && b.size() == 3 && stats().value_constructions == 0,
                u8"swap 只换节点：元素构造 0 次，两条链的内容互换",
                join_values(a) + u8" | " + join_values(b));
    }

    return c.take();
}

}   /* namespace lmini */
