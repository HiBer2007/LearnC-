/**
 * vector_mini.cpp —— 动态数组的实现、项目输出与自测
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
 * 报告文本在库里拼好，由界面层决定怎么显示、按哪种编码显示。
 * 报告里出现的每个数字都是算出来的：容量、搬移次数、构造与析构次数全是计数器的值。
 * 一个地址都不打印——地址每次运行都不同，那属于不可复现的量。
 */
#include "vector_mini.hpp"

#include <iomanip>
#include <sstream>
#include <string>
#include <type_traits>

namespace vmini {

/* ================= 全局分配计数 ================= */

void AllocStats::reset()
{
    *this = AllocStats();
}

AllocStats &stats()
{
    static AllocStats instance;
    return instance;
}

namespace {

/* throw_after 大于 0 时，第 throw_after 次拷贝（或可能抛的移动）抛出 test_failure */
void arm_check()
{
    if (RelocationCounters::throw_after > 0) {
        --RelocationCounters::throw_after;
        if (RelocationCounters::throw_after == 0) {
            ++RelocationCounters::throws;
            throw test_failure();
        }
    }
}

}   /* namespace */

/* ================= 搬迁实验用的类型 ================= */

std::size_t RelocationCounters::copies = 0;
std::size_t RelocationCounters::moves = 0;
std::size_t RelocationCounters::throws = 0;
int RelocationCounters::throw_after = 0;

void RelocationCounters::reset()
{
    copies = 0;
    moves = 0;
    throws = 0;
    throw_after = 0;
}

void RelocationCounters::note_copy()
{
    ++copies;
    arm_check();
}

void RelocationCounters::note_move()
{
    ++moves;
    arm_check();
}

void RelocationCounters::count_move_noexcept() noexcept
{
    ++moves;
}

/* ================= 增长策略 ================= */

const char *growth_name(Growth policy)
{
    switch (policy) {
    case Growth::Double:
        return u8"2 倍";
    case Growth::OneAndHalf:
        return u8"1.5 倍";
    case Growth::PlusOne:
        return u8"每次加一";
    case Growth::PlusTenPercent:
        return u8"每次加 10%";
    }
    return u8"未知";
}

namespace {

/* 从 cur 再长一步。小容量下整数除法会让 1.5 倍与 10% 退化成原地踏步，届时一律加一 */
std::size_t grow_once(Growth policy, std::size_t cur)
{
    std::size_t next = cur;
    switch (policy) {
    case Growth::Double:
        next = cur * 2;
        break;
    case Growth::OneAndHalf:
        next = cur + cur / 2;
        break;
    case Growth::PlusOne:
        next = cur + 1;
        break;
    case Growth::PlusTenPercent:
        next = cur + cur / 10;
        break;
    }
    return next > cur ? next : cur + 1;
}

}   /* namespace */

std::size_t next_capacity(Growth policy, std::size_t current, std::size_t needed)
{
    std::size_t next = current == 0 ? 1 : current;
    while (next < needed) {
        next = grow_once(policy, next);
    }
    return next;
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

std::string percent(std::size_t part, std::size_t whole)
{
    if (whole == 0) {
        return "0.00%";
    }
    std::ostringstream os;
    os << std::fixed << std::setprecision(2)
       << (100.0 * static_cast<double>(part) / static_cast<double>(whole)) << "%";
    return os.str();
}

std::string ratio(std::size_t part, std::size_t whole)
{
    if (whole == 0) {
        return "0.00";
    }
    std::ostringstream os;
    os << std::fixed << std::setprecision(2)
       << (static_cast<double>(part) / static_cast<double>(whole));
    return os.str();
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

/* 自测里用来数元素生命周期的类型：构造与析构必须配平 */
struct Tracked {
    static int live;
    static int created;
    static int destroyed;

    int value = 0;

    Tracked() { ++live; ++created; }
    explicit Tracked(int v) : value(v) { ++live; ++created; }
    Tracked(const Tracked &other) : value(other.value) { ++live; ++created; }
    Tracked(Tracked &&other) noexcept : value(other.value) { ++live; ++created; }
    Tracked &operator=(const Tracked &) = delete;
    Tracked &operator=(Tracked &&) = delete;
    ~Tracked()
    {
        --live;
        ++destroyed;
    }

    static void reset()
    {
        live = 0;
        created = 0;
        destroyed = 0;
    }
};

int Tracked::live = 0;
int Tracked::created = 0;
int Tracked::destroyed = 0;

/* 装 8 个元素、容量恰好是 8 的一个向量，用来做搬迁实验 */
template <class T>
Vector<T> make_full(std::size_t count)
{
    Vector<T> v;
    for (std::size_t i = 0; i < count; ++i) {
        v.push_back(T(static_cast<int>(i)));
    }
    return v;
}

std::string elements_of(const Vector<CopyOnly> &v)
{
    std::ostringstream os;
    for (std::size_t i = 0; i < v.size(); ++i) {
        if (i != 0) {
            os << ' ';
        }
        os << v[i].value();
    }
    return os.str();
}

/* 一次增长跑完之后的账 */
struct GrowthRun {
    std::size_t reallocations = 0;
    std::size_t relocated = 0;
    std::size_t final_capacity = 0;
    std::size_t final_size = 0;
    std::size_t allocations = 0;
    std::size_t deallocations = 0;
    std::size_t peak_bytes = 0;
    std::size_t live_bytes = 0;
};

GrowthRun run_growth(Growth policy, std::size_t count)
{
    stats().reset();
    Vector<int> v(policy);
    for (std::size_t i = 0; i < count; ++i) {
        v.push_back(static_cast<int>(i));
    }
    GrowthRun run;
    run.reallocations = v.reallocation_count();
    run.relocated = v.relocated_count();
    run.final_capacity = v.capacity();
    run.final_size = v.size();
    run.allocations = stats().allocations;
    run.deallocations = stats().deallocations;
    run.peak_bytes = stats().peak_bytes;
    run.live_bytes = stats().live_bytes;
    return run;
}

/* 搬迁方式：只做一次 reserve，看那一次搬迁用的是拷贝还是移动 */
struct MoveProbe {
    bool nothrow_move = false;
    bool copyable = false;
    std::size_t copies = 0;
    std::size_t moves = 0;
};

template <class T>
MoveProbe probe_move(std::size_t count)
{
    Vector<T> v;
    for (std::size_t i = 0; i < count; ++i) {
        v.push_back(T(static_cast<int>(i)));
    }
    RelocationCounters::reset();
    v.reserve(count * 2);       /* 这一步只搬元素，不构造新元素 */

    MoveProbe probe;
    probe.nothrow_move = std::is_nothrow_move_constructible<T>::value;
    probe.copyable = std::is_copy_constructible<T>::value;
    probe.copies = RelocationCounters::copies;
    probe.moves = RelocationCounters::moves;
    return probe;
}

}   /* namespace */

/* ================= 项目输出 ================= */

namespace {

void append_growth_section(std::ostringstream &os)
{
    const std::size_t count = 5000;
    const Growth policies[4] = {
        Growth::Double, Growth::OneAndHalf, Growth::PlusOne, Growth::PlusTenPercent,
    };

    os << u8"增长策略对照（各 push_back " << count << u8" 个 int，都从容量 0 起步）\n";
    os << u8"  " << pad_right(u8"策略", 12) << pad_right(u8"扩容次数", 10)
       << pad_right(u8"搬移元素总数", 14) << pad_right(u8"搬移/元素", 11)
       << pad_right(u8"末尾容量", 10) << u8"末尾余量\n";

    for (Growth policy : policies) {
        const GrowthRun run = run_growth(policy, count);
        std::ostringstream row;
        row << "  " << pad_right(growth_name(policy), 12)
            << pad_right(std::to_string(run.reallocations), 10)
            << pad_right(std::to_string(run.relocated), 14)
            << pad_right(ratio(run.relocated, count), 11)
            << pad_right(std::to_string(run.final_capacity), 10)
            << percent(run.final_capacity - run.final_size, run.final_capacity);
        os << row.str() << "\n";
    }
    os << u8"  「每次加一」的搬移总量约等于 n²/2，这一列就是它不能用的理由\n";
    os << u8"  「末尾余量」= (容量 - 元素个数) / 容量，2 倍策略最多会空一半\n";

    const GrowthRun twice = run_growth(Growth::Double, count);
    os << u8"\n分配与释放（策略 2 倍，全程累计）\n";
    os << u8"  分配 " << twice.allocations << u8" 次，释放 " << twice.deallocations
       << u8" 次（最后一次分配的缓冲区还活着）\n";
    os << u8"  扩容 " << twice.reallocations << u8" 次，累计搬走 " << twice.relocated
       << u8" 个元素\n";
    os << u8"  当前占用 " << twice.live_bytes << u8" 字节，峰值占用 " << twice.peak_bytes
       << u8" 字节（扩容时新旧两块同时活着）\n";
}

void append_exception_section(std::ostringstream &os)
{
    stats().reset();
    Vector<CopyOnly> v = make_full<CopyOnly>(8);
    const std::size_t size_before = v.size();
    const std::size_t cap_before = v.capacity();
    const std::string before = elements_of(v);
    const std::size_t live_before = stats().live_bytes;
    const std::size_t allocations_before = stats().allocations;
    const std::size_t deallocations_before = stats().deallocations;

    RelocationCounters::reset();
    RelocationCounters::throw_after = 5;    /* 第 5 次拷贝时抛 */

    bool caught = false;
    std::string what;
    std::size_t copies_at_throw = 0;
    try {
        v.push_back(CopyOnly(99));
    } catch (const test_failure &error) {
        caught = true;
        what = error.what();
        copies_at_throw = RelocationCounters::copies;
    }
    RelocationCounters::throw_after = 0;

    os << u8"\n异常安全：搬到一半抛异常\n";
    os << u8"  搬迁前：size " << size_before << u8"，capacity " << cap_before
       << u8"，元素 " << before << "\n";
    os << u8"  容量满了，插入第 " << (size_before + 1) << u8" 个元素要先扩容到 "
       << next_capacity(Growth::Double, cap_before, size_before + 1) << "\n";
    os << u8"  第 5 次拷贝时抛出，捕获到的异常类型 " << (caught ? what : u8"（没有抛）")
       << u8"，那时已经拷贝了 " << copies_at_throw << u8" 个\n";
    os << u8"  捕获之后：size " << v.size() << u8"，capacity " << v.capacity()
       << u8"，元素 " << elements_of(v) << "\n";
    os << u8"  元素与操作前逐位一致：" << (elements_of(v) == before ? u8"是" : u8"否") << "\n";
    os << u8"  当前占用 " << stats().live_bytes << u8" 字节，操作前 "
       << live_before << u8" 字节，相同："
       << (stats().live_bytes == live_before ? u8"是" : u8"否") << "\n";
    os << u8"  这次失败的扩容：分配 "
       << (stats().allocations - allocations_before) << u8" 次、释放 "
       << (stats().deallocations - deallocations_before) << u8" 次，峰值占用 "
       << stats().peak_bytes << u8" 字节（旧 " << live_before << u8" + 新 "
       << (stats().peak_bytes - live_before) << u8"）\n";
    os << u8"  三条不变式仍然成立：" << (v.invariants_ok() ? u8"是" : u8"否") << "\n";
}

template <class T>
void append_move_row(std::ostringstream &os, const std::string &name)
{
    const std::size_t count = 8;
    const MoveProbe probe = probe_move<T>(count);
    os << "  " << pad_right(name, 20)
       << pad_right(probe.nothrow_move ? u8"是" : u8"否", 20)
       << pad_right(probe.moves > 0 ? u8"移动" : u8"拷贝", 12)
       << pad_right(std::to_string(probe.copies), 10)
       << std::to_string(probe.moves) << "\n";
}

void append_move_section(std::ostringstream &os)
{
    os << u8"\n搬迁方式由移动构造是否 noexcept 决定（各搬 8 个元素，只做一次 reserve）\n";
    os << u8"  " << pad_right(u8"元素类型", 20) << pad_right(u8"移动构造 noexcept", 20)
       << pad_right(u8"实际用的", 12) << pad_right(u8"拷贝次数", 10) << u8"移动次数\n";
    append_move_row<CopyOnly>(os, u8"CopyOnly（只有拷贝）");
    append_move_row<ThrowingMover>(os, u8"ThrowingMover（没标）");
    append_move_row<NoThrowMover>(os, u8"NoThrowMover（标了）");
    os << u8"  move_if_noexcept 的判据：移动构造 noexcept 才敢移动，否则退回复制\n";
}

void append_iterator_section(std::ostringstream &os)
{
    Vector<int> v;
    v.push_back(1);
    v.push_back(2);
    const int *before = v.data();
    v.reserve(64);
    const bool moved = v.data() != before;

    os << u8"\n迭代器失效：扩容之后旧地址作废\n";
    os << u8"  两个元素时 reserve(64)：缓冲区地址变了："
       << (moved ? u8"是" : u8"否") << u8"（地址本身每次运行都不同，故不打印）\n";
    os << u8"  旧迭代器指向的内存已经被还给分配器，再用就是悬垂访问\n";

    Vector<int> q;
    q.reserve(64);
    const std::size_t reallocations_after_reserve = q.reallocation_count();
    std::size_t changes = 0;
    for (int i = 0; i < 64; ++i) {
        const int *current = q.data();
        q.push_back(i);
        if (q.data() != current) {
            ++changes;
        }
    }
    os << u8"  先 reserve(64) 再插满 64 个元素：缓冲区地址变化 " << changes
       << u8" 次，插满期间新增加的扩容次数 "
       << (q.reallocation_count() - reallocations_after_reserve) << u8" 次\n";
    os << u8"  reserve 只保证不扩容；在中间插入仍会让插入点之后的迭代器失效\n";

    const std::size_t first = next_capacity(Growth::Double, 8, 9);
    os << u8"  容量 8 的向量插第 9 个：新容量 " << first << u8"，是「至少装得下」的最小一步\n";
}

}   /* namespace */

std::string build_report()
{
    std::ostringstream os;
    append_growth_section(os);
    append_exception_section(os);
    append_move_section(os);
    append_iterator_section(os);
    return os.str();
}

/* ================= 自测 ================= */

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

CheckResult run_self_tests()
{
    Checker c;

    /* 1. 默认构造是一个空壳：没有缓冲区，begin 与 end 都在空处 */
    {
        const Vector<int> v;
        c.check(v.size() == 0 && v.capacity() == 0 && v.data() == nullptr
                    && v.begin() == v.end() && v.empty() && v.invariants_ok(),
                u8"默认构造：size 0、capacity 0、data 为空、begin == end");
    }

    /* 2. push_back 之后元素与 size 都对 */
    {
        Vector<int> v;
        for (int i = 0; i < 5; ++i) {
            v.push_back(i * 10);
        }
        bool ok = v.size() == 5;
        for (std::size_t i = 0; i < v.size(); ++i) {
            ok = ok && v[i] == static_cast<int>(i) * 10;
        }
        c.check(ok && v.invariants_ok(), u8"push_back 之后元素与 size 都对",
                std::to_string(v.size()) + u8" 个元素，容量 " + std::to_string(v.capacity()));
    }

    /* 3. 2 倍策略的容量序列 */
    {
        Vector<int> v;
        std::ostringstream seq;
        for (int i = 1; i <= 5; ++i) {
            v.push_back(i);
            seq << v.capacity() << (i == 5 ? "" : " ");
        }
        c.check(seq.str() == "1 2 4 4 8",
                u8"2 倍策略下插 1 到 5 个元素，容量序列是 1 2 4 4 8（装得下就不长）", seq.str());
    }

    /* 4. reserve 只增不减 */
    {
        Vector<int> v;
        v.reserve(3);
        const std::size_t first = v.capacity();
        const std::size_t allocations_after_first = stats().allocations;
        v.reserve(2);
        c.check(first == 3 && v.capacity() == 3
                    && stats().allocations == allocations_after_first,
                u8"reserve 只增不减：reserve(3) 之后再 reserve(2) 容量与分配次数都不动",
                std::to_string(v.capacity()));
    }

    /* 5. 预留之后插满不扩容 */
    {
        stats().reset();
        Vector<int> v;
        v.reserve(32);
        const std::size_t after_reserve = v.reallocation_count();
        for (int i = 0; i < 32; ++i) {
            v.push_back(i);
        }
        c.check(v.reallocation_count() == after_reserve && stats().allocations == 1
                    && v.capacity() == 32,
                u8"预留 32 之后插满 32 个：插满期间一次都没再扩容、只分配过 1 次",
                std::to_string(stats().allocations) + u8" 次分配，插满期间扩容 "
                    + std::to_string(v.reallocation_count() - after_reserve) + u8" 次");
    }

    /* 6. 扩容之后旧地址作废 */
    {
        Vector<int> v;
        v.push_back(1);
        v.push_back(2);
        const int *old = v.data();
        v.reserve(64);
        c.check(v.data() != old && v.capacity() == 64,
                u8"扩容之后缓冲区地址变了：旧迭代器一律失效");
    }

    /* 7. clear 不释放内存 */
    {
        Vector<int> v;
        v.reserve(16);
        v.push_back(7);
        v.clear();
        c.check(v.size() == 0 && v.capacity() == 16 && v.begin() == v.end(),
                u8"clear 把 size 归零但容量不变", std::to_string(v.capacity()) + u8" 字节容量仍在");
    }

    /* 8. shrink_to_fit 把容量降到 size */
    {
        Vector<int> v;
        v.reserve(64);
        for (int i = 0; i < 4; ++i) {
            v.push_back(i);
        }
        const std::size_t before = v.capacity();
        v.shrink_to_fit();
        c.check(before == 64 && v.capacity() == 4 && v.size() == 4 && v[3] == 3,
                u8"shrink_to_fit 把容量从 64 降到 4",
                std::to_string(before) + u8" → " + std::to_string(v.capacity()));
    }

    /* 9. 拷贝构造得到独立的一份 */
    {
        Vector<int> a;
        a.push_back(1);
        a.push_back(2);
        Vector<int> b(a);
        b[0] = 99;
        c.check(a[0] == 1 && b[0] == 99 && a.data() != b.data(),
                u8"拷贝构造得到独立的一份：改一个不影响另一个");
    }

    /* 10. 移动构造把内容接走，源容器变空 */
    {
        Vector<int> a;
        a.push_back(5);
        a.push_back(6);
        const int *buffer = a.data();
        Vector<int> b(std::move(a));
        c.check(b.size() == 2 && b[1] == 6 && a.size() == 0 && a.data() == nullptr
                    && b.data() == buffer,
                u8"移动构造把内容接走（缓冲区地址不变），源容器变空");
    }

    /* 11. 元素的构造与析构配平 */
    {
        Tracked::reset();
        {
            Vector<Tracked> v;
            for (int i = 0; i < 20; ++i) {
                v.push_back(Tracked(i));
            }
            v.pop_back();
            v.resize(25);
            v.resize(3);
        }
        c.check(Tracked::live == 0 && Tracked::created == Tracked::destroyed,
                u8"元素的构造与析构配平：扩容、pop_back、resize 都没有漏析构或多析构",
                std::to_string(Tracked::created) + u8" 建 / "
                    + std::to_string(Tracked::destroyed) + u8" 删");
    }

    /* 12. 拷贝搬迁抛异常后，容器与内存都回到原样 */
    {
        stats().reset();
        Vector<CopyOnly> v = make_full<CopyOnly>(8);
        const std::string before = elements_of(v);
        const std::size_t live_before = stats().live_bytes;
        const std::size_t allocations_before = stats().allocations;
        const std::size_t deallocations_before = stats().deallocations;
        RelocationCounters::reset();
        RelocationCounters::throw_after = 5;
        bool caught = false;
        try {
            v.push_back(CopyOnly(99));
        } catch (const test_failure &) {
            caught = true;
        }
        RelocationCounters::throw_after = 0;
        const std::size_t fresh_allocs = stats().allocations - allocations_before;
        const std::size_t fresh_frees = stats().deallocations - deallocations_before;
        c.check(caught && v.size() == 8 && v.capacity() == 8 && elements_of(v) == before
                    && stats().live_bytes == live_before && fresh_allocs == 1
                    && fresh_frees == 1 && v.invariants_ok(),
                u8"拷贝搬到第 5 个时抛异常：内容不变、新缓冲区已归还、不变式仍成立",
                u8"分配 " + std::to_string(fresh_allocs) + u8" 次、释放 "
                    + std::to_string(fresh_frees) + u8" 次");
    }

    /* 13. 抛过异常的容器还能继续用 */
    {
        Vector<CopyOnly> v = make_full<CopyOnly>(8);
        RelocationCounters::reset();
        RelocationCounters::throw_after = 3;
        try {
            v.push_back(CopyOnly(99));
        } catch (const test_failure &) {
        }
        RelocationCounters::throw_after = 0;
        v.push_back(CopyOnly(42));
        c.check(v.size() == 9 && v[8].value() == 42 && v[0].value() == 0,
                u8"抛过异常的容器仍然可用：紧接着再插一个就成功",
                std::to_string(v.size()) + u8" 个元素");
    }

    /* 14. 没标 noexcept 的移动构造让容器退回拷贝 */
    {
        const MoveProbe probe = probe_move<ThrowingMover>(8);
        c.check(!probe.nothrow_move && probe.copies == 8 && probe.moves == 0,
                u8"移动构造没标 noexcept：搬迁退回拷贝，拷贝 8 次、移动 0 次",
                std::to_string(probe.copies) + u8" / " + std::to_string(probe.moves));
    }

    /* 15. noexcept 移动构造让容器用移动 */
    {
        const MoveProbe probe = probe_move<NoThrowMover>(8);
        c.check(probe.nothrow_move && probe.copies == 0 && probe.moves == 8,
                u8"移动构造标了 noexcept：搬迁走移动，拷贝 0 次、移动 8 次",
                std::to_string(probe.copies) + u8" / " + std::to_string(probe.moves));
    }

    /* 16. 每次加一的搬移总量是平方级 */
    {
        const std::size_t count = 2000;
        const GrowthRun run = run_growth(Growth::PlusOne, count);
        const std::size_t expected = count * (count - 1) / 2;
        c.check(run.relocated == expected && run.reallocations == count,
                u8"每次加一：2000 个元素搬 1999000 次，是 n(n-1)/2",
                std::to_string(run.relocated) + u8" 次");
    }

    /* 17. pop_back 析构最后一个元素、resize 缩小同理 */
    {
        Tracked::reset();
        {
            Vector<Tracked> v;
            v.push_back(Tracked(1));
            v.push_back(Tracked(2));
            v.pop_back();
            const int after_pop = Tracked::live;
            v.resize(1);
            c.check(after_pop == 1 && Tracked::live == 1 && v.size() == 1,
                    u8"pop_back 与 resize 缩小都只析构尾部元素",
                    std::to_string(Tracked::live) + u8" 个还活着");
        }
        c.check(Tracked::live == 0, u8"容器析构之后元素一个不剩");
    }

    /* 18. 自赋值与移动赋值 */
    {
        Vector<int> a;
        a.push_back(3);
        Vector<int> b;
        b.push_back(9);
        b = a;
        const bool copy_ok = b.size() == 1 && b[0] == 3 && a[0] == 3;
        a = std::move(b);
        const bool move_ok = a.size() == 1 && a[0] == 3 && b.size() == 0;
        Vector<int> &self = a;
        a = self;
        c.check(copy_ok && move_ok && a.size() == 1 && a[0] == 3,
                u8"拷贝赋值、移动赋值与自赋值都正确");
    }

    return c.take();
}

}   /* namespace vmini */
