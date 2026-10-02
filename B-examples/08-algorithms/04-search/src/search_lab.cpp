/**
 * search_lab.cpp —— 查找：从线性到索引
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

#include "search_lab.hpp"

#include <algorithm>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

namespace slab {

/* ================= 数据构造 ================= */

Table make_arithmetic_table(std::size_t n, long long step, long long first)
{
    Table table(n);
    for (std::size_t i = 0; i < n; ++i) {
        table[i] = first + step * static_cast<long long>(i);
    }
    return table;
}

Table make_blocked_table(std::size_t n, std::size_t block, long long step)
{
    Table table(n);
    for (std::size_t i = 0; i < n; ++i) {
        table[i] = step * static_cast<long long>(i / block);
    }
    return table;
}

Table make_uniform_table(std::size_t n, long long step)
{
    Table table(n);
    for (std::size_t i = 0; i < n; ++i) {
        table[i] = step * static_cast<long long>(i);
    }
    return table;
}

Table make_power_table(std::size_t n)
{
    Table table(n);
    for (std::size_t i = 0; i < n; ++i) {
        table[i] = (i < 63) ? (1LL << i) : 0;   /* i 到 63 时左移会溢出，这里截住 */
    }
    return table;
}

Table make_lehmer_keys(std::size_t n, unsigned int seed)
{
    const unsigned long long modulus = 2147483647ull;   /* 2^31 − 1，素数 */
    const unsigned long long multiplier = 48271ull;     /* 模 2^31 − 1 的原根 */
    unsigned long long state = static_cast<unsigned long long>(seed) % modulus;
    if (state == 0) {
        state = 1;
    }
    Table keys(n);
    for (std::size_t i = 0; i < n; ++i) {
        state = (state * multiplier) % modulus;
        keys[i] = static_cast<long long>(state);        /* 落在 1 到 2^31 − 2 之间 */
    }
    return keys;
}

/* ================= 线性查找 ================= */

SearchOutcome linear_plain(const Table &table, long long target)
{
    SearchOutcome out;
    const std::size_t n = table.size();
    for (std::size_t i = 0; i < n; ++i) {
        ++out.stats.bounds;        /* 下标有没有越界，这也要算一次判断 */
        ++out.stats.iterations;
        ++out.stats.comparisons;
        if (table[i] == target) {
            out.index = i;
            out.found = true;
            return out;
        }
    }
    ++out.stats.bounds;            /* 最后一次判断失败，循环才结束 */
    out.index = kNotFound;
    return out;
}

SearchOutcome linear_sentinel(const Table &table, long long target)
{
    Table buffer = table;
    buffer.push_back(target);      /* 哨兵：循环一定会在这里停下 */

    SearchOutcome out;
    std::size_t i = 0;
    for (;;) {
        ++out.stats.iterations;
        ++out.stats.comparisons;
        if (buffer[i] == target) {
            break;
        }
        ++i;
    }
    out.index = i;                 /* 未命中时这就是哨兵槽的下标，等于原表长 */
    out.found = (i < table.size());
    return out;
}

/* ================= 二分查找的三种边界写法 ================= */

SearchOutcome binary_closed(const Table &table, long long target)
{
    SearchOutcome out;
    if (table.empty()) {
        return out;
    }
    /* 闭区间 [lo, hi]：hi 指着候选范围里的最后一个元素。
       mid 取 0 而目标值比 table[0] 还小时，hi = mid − 1 会取到 −1，
       因此这两个下标必须带符号 */
    long long lo = 0;
    long long hi = static_cast<long long>(table.size()) - 1;
    while (lo <= hi) {
        const std::size_t mid = static_cast<std::size_t>(lo + (hi - lo) / 2);
        ++out.stats.iterations;
        ++out.stats.comparisons;
        if (table[mid] < target) {
            lo = static_cast<long long>(mid) + 1;
        } else {
            ++out.stats.comparisons;
            if (table[mid] > target) {
                hi = static_cast<long long>(mid) - 1;
            } else {
                out.index = mid;
                out.found = true;
                return out;
            }
        }
    }
    out.index = kNotFound;
    return out;
}

SearchOutcome binary_half_open(const Table &table, long long target)
{
    SearchOutcome out;
    /* 半开区间 [lo, hi)：hi 指着候选范围之后的第一格，lo == hi 时范围为空 */
    std::size_t lo = 0;
    std::size_t hi = table.size();
    while (lo < hi) {
        const std::size_t mid = lo + (hi - lo) / 2;
        ++out.stats.iterations;
        ++out.stats.comparisons;
        if (table[mid] < target) {
            lo = mid + 1;
        } else {
            ++out.stats.comparisons;
            if (table[mid] > target) {
                hi = mid;
            } else {
                out.index = mid;
                out.found = true;
                return out;
            }
        }
    }
    out.index = kNotFound;
    return out;
}

SearchOutcome lower_bound_probe(const Table &table, long long target)
{
    SearchOutcome out;
    /* [0, lo) 里的值都小于目标值，[hi, n) 里的值都不小于目标值。
       每轮只比一次：比出来的是「mid 该不该被排除」这一个信息 */
    std::size_t lo = 0;
    std::size_t hi = table.size();
    while (lo < hi) {
        const std::size_t mid = lo + (hi - lo) / 2;
        ++out.stats.iterations;
        ++out.stats.comparisons;
        if (table[mid] < target) {
            lo = mid + 1;
        } else {
            hi = mid;
        }
    }
    out.index = lo;
    if (lo < table.size()) {
        ++out.stats.comparisons;   /* 收尾这一次：判断找到的位置上的值是不是目标值 */
        out.found = (table[lo] == target);
    }
    return out;
}

/* ================= 插值查找 ================= */

SearchOutcome interpolation_search(const Table &table, long long target)
{
    SearchOutcome out;
    if (table.empty()) {
        return out;
    }
    std::size_t lo = 0;
    std::size_t hi = table.size() - 1;
    std::size_t insert_at = 0;

    for (;;) {
        /* 两次区间守卫：目标值跑到区间两端之外，就说明表里没有它。
           这两次判断与「估计位置」无关，单独记在 bounds 里 */
        ++out.stats.bounds;
        if (target < table[lo]) {
            insert_at = lo;        /* 区间里的值都比目标值大，插入点就是 lo */
            break;
        }
        ++out.stats.bounds;
        if (target > table[hi]) {
            insert_at = hi + 1;    /* 区间里的值都比目标值小，插入点在 hi 之后 */
            break;
        }
        if (lo == hi) {
            /* 两端重合：守卫已经保证目标值就在这个值上，直接收工，
               同时避免下面除以 0 */
            ++out.stats.iterations;
            ++out.stats.comparisons;
            out.index = lo;
            out.found = true;
            return out;
        }

        std::size_t pos = lo;
        const double span = static_cast<double>(table[hi]) - static_cast<double>(table[lo]);
        if (span > 0.0) {
            const double offset = static_cast<double>(target) - static_cast<double>(table[lo]);
            const double ratio = offset / span;
            const double guess = ratio * static_cast<double>(hi - lo);
            if (guess > 0.0) {
                pos = lo + static_cast<std::size_t>(guess);
            }
        }
        if (pos < lo) {
            pos = lo;
        } else if (pos > hi) {
            pos = hi;
        }

        ++out.stats.iterations;
        ++out.stats.comparisons;
        if (table[pos] == target) {
            out.index = pos;
            out.found = true;
            return out;
        }
        if (table[pos] < target) {
            lo = pos + 1;
        } else {
            hi = pos - 1;
        }
    }

    out.index = insert_at;        /* 未命中时给出插入点 */
    return out;
}

/* ================= 手写的哈希索引 ================= */

namespace {

/** 表长向上取到 2 的幂：这样取模可以用一次按位与代替 */
std::size_t next_power_of_two(std::size_t value)
{
    std::size_t power = 1;
    while (power < value) {
        power <<= 1;
    }
    return power;
}

/** splitmix64 的收尾混合：整数乘法与移位，结果与平台无关 */
std::uint64_t mix64(std::uint64_t value)
{
    value += 0x9E3779B97F4A7C15ull;
    value = (value ^ (value >> 30)) * 0xBF58476D1CE4E5B9ull;
    value = (value ^ (value >> 27)) * 0x94D049BB133111EBull;
    return value ^ (value >> 31);
}

}   /* namespace */

HashIndex::HashIndex(std::size_t expected_keys, Hash kind)
    : kind_(kind)
{
    const std::size_t wanted = std::max<std::size_t>(expected_keys * 2, 2);
    const std::size_t size = next_power_of_two(wanted);
    slots_.assign(size, 0);
    used_.assign(size, 0);
}

double HashIndex::load_factor() const
{
    if (slots_.empty()) {
        return 0.0;
    }
    return static_cast<double>(size_) / static_cast<double>(slots_.size());
}

std::size_t HashIndex::start_slot(long long key) const
{
    const std::uint64_t bits = static_cast<std::uint64_t>(key);
    const std::uint64_t mixed = (kind_ == Hash::Mixed) ? mix64(bits) : bits;
    return static_cast<std::size_t>(mixed) & (slots_.size() - 1);
}

void HashIndex::insert(long long key)
{
    const std::size_t mask = slots_.size() - 1;
    std::size_t slot = start_slot(key);
    std::size_t probes = 0;
    while (used_[slot] != 0) {
        ++probes;
        slot = (slot + 1) & mask;
    }
    ++probes;                      /* 最后探到的这个空槽也算一次探查 */
    slots_[slot] = key;
    used_[slot] = 1;
    ++size_;
    insert_probes_ += probes;
    if (probes > max_insert_probe_) {
        max_insert_probe_ = probes;
    }
}

SearchOutcome HashIndex::find(long long key) const
{
    SearchOutcome out;
    const std::size_t mask = slots_.size() - 1;
    std::size_t slot = start_slot(key);
    for (std::size_t step = 0; step < slots_.size(); ++step) {
        ++out.stats.iterations;
        if (used_[slot] == 0) {
            out.index = kNotFound;      /* 探到空槽：这个键不在表里 */
            return out;
        }
        ++out.stats.comparisons;        /* 槽里有键，才发生一次键比较 */
        if (slots_[slot] == key) {
            out.index = slot;
            out.found = true;
            return out;
        }
        slot = (slot + 1) & mask;
    }
    out.index = kNotFound;              /* 表全满：找不到空槽来收尾 */
    return out;
}

/* ================= 报告用的排版工具 ================= */

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

std::string fixed_string(double value, int digits)
{
    std::ostringstream os;
    os << std::fixed << std::setprecision(digits) << value;
    return os.str();
}

std::string count_text(std::size_t value)
{
    return std::to_string(value);
}

/** 不小于 value 的 2 的幂的指数：ceil(log2(value))，value 至少为 1 */
std::size_t ceil_log2(std::size_t value)
{
    std::size_t bits = 0;
    std::size_t power = 1;
    while (power < value) {
        power <<= 1;
        ++bits;
    }
    return bits;
}

/** 一次查找的「返回」一列怎么写。miss_text 由调用方给出：
    普通线性与二分写「未找到」，下界与插值写插入点 */
std::string index_cell(const SearchOutcome &out, const std::string &miss_text)
{
    if (out.found) {
        return std::to_string(out.index);
    }
    return miss_text;
}

/** 结构量直方图：三行，第一行是均值与极值，后两行是分布。
    取值不超过 12 时每个步数占一列，超过之后合并成十档 */
std::string histogram_block(const std::string &label, const std::vector<std::size_t> &steps)
{
    std::ostringstream os;
    std::size_t min_step = steps.empty() ? 0 : steps[0];
    std::size_t max_step = 0;
    double sum = 0.0;
    for (const std::size_t value : steps) {
        min_step = std::min(min_step, value);
        max_step = std::max(max_step, value);
        sum += static_cast<double>(value);
    }
    const double average = steps.empty() ? 0.0 : sum / static_cast<double>(steps.size());

    os << "  " << label << u8"（" << steps.size() << u8" 次查询）"
       << u8"  平均 " << fixed_string(average, 2) << u8" 步"
       << u8"  最少 " << min_step << u8" 步"
       << u8"  最多 " << max_step << u8" 步\n";

    if (max_step <= 12) {
        const int column = 8;
        os << u8"    步数  ";
        for (std::size_t step = min_step; step <= max_step; ++step) {
            os << std::setw(column) << step;
        }
        os << "\n";
        os << u8"    查询数";
        for (std::size_t step = min_step; step <= max_step; ++step) {
            std::size_t count = 0;
            for (const std::size_t value : steps) {
                if (value == step) {
                    ++count;
                }
            }
            os << std::setw(column) << count;
        }
        os << "\n";
        return os.str();
    }

    const std::size_t span = max_step - min_step + 1;
    const std::size_t buckets = std::min<std::size_t>(10, span);
    const std::size_t width = (span + buckets - 1) / buckets;
    const int column = 11;
    os << u8"    步数区间";
    for (std::size_t b = 0; b < buckets; ++b) {
        const std::size_t lo = min_step + b * width;
        if (lo > max_step) {
            break;
        }
        const std::size_t hi = std::min(lo + width - 1, max_step);
        os << std::setw(column)
           << (lo == hi ? std::to_string(lo) : std::to_string(lo) + "-" + std::to_string(hi));
    }
    os << "\n";
    os << u8"    查询数  ";
    for (std::size_t b = 0; b < buckets; ++b) {
        const std::size_t lo = min_step + b * width;
        if (lo > max_step) {
            break;
        }
        const std::size_t hi = std::min(lo + width - 1, max_step);
        std::size_t count = 0;
        for (const std::size_t value : steps) {
            if (value >= lo && value <= hi) {
                ++count;
            }
        }
        os << std::setw(column) << count;
    }
    os << "\n";
    return os.str();
}

struct Average {
    double average = 0.0;
    std::size_t worst = 0;
};

Average summarize(const std::vector<std::size_t> &values)
{
    Average result;
    if (values.empty()) {
        return result;
    }
    double sum = 0.0;
    for (const std::size_t value : values) {
        sum += static_cast<double>(value);
        result.worst = std::max(result.worst, value);
    }
    result.average = sum / static_cast<double>(values.size());
    return result;
}

/* ================= 报告：第一段 线性查找 ================= */

void append_linear(std::ostringstream &os)
{
    const Table data = make_arithmetic_table(1024, 3, 1);

    struct Query {
        std::string label;
        long long value;
    };
    const Query queries[] = {
        {u8"命中首位 x = 1", 1},
        {u8"命中中间 x = 1537", 1 + 3 * 512},
        {u8"命中末尾 x = 3070", 1 + 3 * 1023},
        {u8"未命中 x = 1000000", 1000000},
    };

    os << u8"一、线性查找：普通写法与哨兵写法\n";
    os << u8"  数据：1024 个元素，a[i] = 3i + 1（1, 4, 7, …, 3070），值互不相同\n";
    os << u8"  普通写法每轮判断两次：下标有没有越界、这个元素是不是目标值\n";
    os << u8"  哨兵写法把目标值写进末尾多出来的一格，循环里只判断元素，代价是每一次「没比中」\n";
    os << u8"  都要继续往下走，而收尾那一次比的是哨兵本身\n";
    os << "  " << pad_right(u8"查询", 22) << pad_right(u8"写法", 10) << pad_right(u8"步数", 6)
       << pad_right(u8"元素比较", 10) << pad_right(u8"上界判断", 10) << pad_right(u8"合计", 8)
       << u8"返回\n";

    for (const Query &query : queries) {
        const SearchOutcome plain = linear_plain(data, query.value);
        const SearchOutcome sentinel = linear_sentinel(data, query.value);
        for (int variant = 0; variant < 2; ++variant) {
            const bool is_plain = (variant == 0);
            const SearchOutcome &out = is_plain ? plain : sentinel;
            const std::string returned =
                is_plain ? index_cell(out, u8"未找到")
                         : index_cell(out, count_text(out.index) + u8"（哨兵槽）");
            os << "  " << pad_right(is_plain ? query.label : std::string(), 22)
               << pad_right(is_plain ? u8"普通" : u8"哨兵", 10) << std::setw(6)
               << out.stats.iterations << std::setw(10) << out.stats.comparisons << std::setw(10)
               << out.stats.bounds << std::setw(8)
               << (out.stats.comparisons + out.stats.bounds) << "  " << returned << "\n";
        }
    }

    const SearchOutcome plain_miss = linear_plain(data, 1000000);
    const SearchOutcome sentinel_miss = linear_sentinel(data, 1000000);
    const SearchOutcome plain_last = linear_plain(data, 1 + 3 * 1023);
    const SearchOutcome sentinel_last = linear_sentinel(data, 1 + 3 * 1023);

    os << u8"  未命中时普通写法做了 " << (plain_miss.stats.comparisons + plain_miss.stats.bounds)
       << u8" 次判断（" << plain_miss.stats.comparisons << u8" 次元素比较加 "
       << plain_miss.stats.bounds << u8" 次下标判断），哨兵写法 "
       << (sentinel_miss.stats.comparisons + sentinel_miss.stats.bounds) << u8" 次，全是元素比较\n";
    os << u8"  命中末尾时两者都是 " << plain_last.stats.comparisons << u8" 次元素比较（哨兵写法同样 "
       << sentinel_last.stats.comparisons << u8" 次），普通写法另外还做了 " << plain_last.stats.bounds
       << u8" 次下标判断\n";
    os << u8"  两种写法的差距全在「上界判断」这一列：哨兵写法把它压到了 0\n";
    os << u8"  哨兵写法的前提是表可写、末尾多留一格；本实现复制一份再追加哨兵，复制的代价不计入比较次数\n";
    os << u8"  未命中时哨兵写法返回的下标是原表长，found 为假：这个下标不是命中位置，是哨兵槽\n";
    os << "\n";
}

/* ================= 报告：第二段 二分三种边界 ================= */

void append_binary(std::ostringstream &os)
{
    const Table table = make_arithmetic_table(1000, 2, 0);   /* 0, 2, 4, …, 1998 */

    struct Query {
        std::string label;
        long long value;
    };
    const Query queries[] = {
        {u8"命中首位 x = 0", 0},
        {u8"命中中间 x = 1000", 1000},
        {u8"命中末尾 x = 1998", 1998},
        {u8"未命中（中间）x = 999", 999},
        {u8"未命中（小于全部）x = -1", -1},
        {u8"未命中（大于全部）x = 3000", 3000},
    };

    os << u8"二、二分查找的三种边界写法\n";
    os << u8"  数据：1000 个互不相同的升序元素，a[i] = 2i（0, 2, 4, …, 1998）\n";
    os << u8"  写法一 闭区间 [lo, hi]：候选范围含两端，不变式是「目标值若在表里，下标必落在 [lo, hi] 内」，\n";
    os << u8"          lo > hi 时范围为空；小于走 lo = mid + 1，大于走 hi = mid − 1\n";
    os << u8"  写法二 半开区间 [lo, hi)：hi 指着候选范围之后的第一格，不变式是「候选范围是 [lo, hi)」，\n";
    os << u8"          lo == hi 时范围为空；小于走 lo = mid + 1，否则走 hi = mid\n";
    os << u8"  写法三 下界：返回第一个满足 a[pos] >= x 的位置，不变式是「[0, lo) 都小于 x，[hi, n) 都不小于 x」，\n";
    os << u8"          lo == hi 时答案就是 lo；每轮只比一次\n";
    os << "  " << pad_right(u8"查询", 28) << pad_right(u8"写法", 12) << pad_right(u8"步数", 6)
       << pad_right(u8"元素比较", 10) << u8"返回\n";

    std::size_t worst_steps = 0;
    for (const Query &query : queries) {
        const SearchOutcome closed = binary_closed(table, query.value);
        const SearchOutcome half = binary_half_open(table, query.value);
        const SearchOutcome lower = lower_bound_probe(table, query.value);
        const SearchOutcome *const outcomes[] = {&closed, &half, &lower};
        const char *const names[] = {u8"闭区间", u8"半开区间", u8"下界"};
        for (int variant = 0; variant < 3; ++variant) {
            const SearchOutcome &out = *outcomes[variant];
            worst_steps = std::max(worst_steps, out.stats.iterations);
            os << "  " << pad_right(variant == 0 ? query.label : std::string(), 28)
               << pad_right(names[variant], 12) << std::setw(6) << out.stats.iterations
               << std::setw(10) << out.stats.comparisons << "  "
               << (out.found ? std::to_string(out.index)
                             : (variant == 2 ? count_text(out.index) + u8"（插入点）" : u8"未找到"))
               << "\n";
        }
    }

    const SearchOutcome closed_first = binary_closed(table, 0);
    const SearchOutcome half_first = binary_half_open(table, 0);
    const SearchOutcome lower_first = lower_bound_probe(table, 0);
    os << u8"  命中首位这一次，三种写法分别比了 " << closed_first.stats.comparisons << u8"、"
       << half_first.stats.comparisons << u8"、" << lower_first.stats.comparisons << u8" 次\n";
    os << u8"    闭区间与半开区间每轮最多比两次（先比小于，再比大于）\n";
    os << u8"    下界每轮只比一次，收尾再比一次判断找到的位置上是不是目标值，比较次数因此少得多\n";
    os << u8"    命中中间那一次半开区间只用 " << binary_half_open(table, 1000).stats.comparisons
       << u8" 次：第一刀的中点正好就是目标值，撞上了就提前返回\n";
    os << u8"  未命中时半开区间与下界走的路径完全相同：步数一样，下界返回的位置就是插入点\n";
    os << u8"  三条终止条件说的是同一件事：闭区间的 lo > hi 与半开区间的 lo == hi 都是「候选范围空了」\n";
    os << u8"  六行查询里步数最多的是 " << worst_steps << u8" 步；" << table.size()
       << u8" 个元素的步数上界是 ceil(log2(n + 1)) = " << ceil_log2(table.size() + 1) << u8"\n";
    os << u8"  下界写法在目标值小于全部时返回 0，大于全部时返回 1000（等于表长）\n";
    os << "\n";
}

/* ================= 报告：第三段 重复元素 ================= */

void append_duplicates(std::ostringstream &os)
{
    const Table table = make_blocked_table(1000, 50, 100);   /* 每 50 个位置同一个值 */

    struct Query {
        std::string label;
        long long value;
    };
    const Query queries[] = {
        {u8"值 0", 0},
        {u8"值 500", 500},
        {u8"值 900", 900},
        {u8"值 150（不在表里）", 150},
        {u8"值 1000（不在表里）", 1000},
    };

    os << u8"三、二分查找在有重复元素的表上\n";
    os << u8"  数据：1000 个元素，b[i] = (i / 50) × 100，每 50 个位置是同一个值，共 20 档\n";
    os << u8"  表里同一个值出现多次时，「找到」这件事要问清楚：找的是哪一个\n";
    os << "  " << pad_right(u8"查询", 22) << pad_right(u8"出现次数", 10) << pad_right(u8"首次出现", 10)
       << pad_right(u8"闭区间 下标/比较", 20) << pad_right(u8"半开区间 下标/比较", 22)
       << u8"下界 下标/比较\n";

    for (const Query &query : queries) {
        std::size_t occurrences = 0;
        std::size_t first = kNotFound;
        for (std::size_t i = 0; i < table.size(); ++i) {
            if (table[i] == query.value) {
                ++occurrences;
                if (first == kNotFound) {
                    first = i;
                }
            }
        }
        const SearchOutcome closed = binary_closed(table, query.value);
        const SearchOutcome half = binary_half_open(table, query.value);
        const SearchOutcome lower = lower_bound_probe(table, query.value);

        auto cell = [](const SearchOutcome &out, bool lower_form) {
            if (out.found) {
                return std::to_string(out.index) + " / " + std::to_string(out.stats.comparisons);
            }
            if (lower_form) {
                return std::string(u8"插入点 ") + std::to_string(out.index) + " / " +
                       std::to_string(out.stats.comparisons);
            }
            return std::string(u8"未找到 / ") + std::to_string(out.stats.comparisons);
        };

        os << "  " << pad_right(query.label, 22) << std::setw(8) << occurrences << "  "
           << pad_right(first == kNotFound ? std::string(u8"—") : std::to_string(first), 10)
           << pad_right(cell(closed, false), 20) << pad_right(cell(half, false), 22)
           << cell(lower, true) << "\n";
    }

    const SearchOutcome closed = binary_closed(table, 500);
    const SearchOutcome half = binary_half_open(table, 500);
    const SearchOutcome lower = lower_bound_probe(table, 500);
    os << u8"  值 500 出现 50 次：闭区间返回 " << closed.index << u8"，半开区间返回 " << half.index
       << u8"，下界返回 " << lower.index << u8"\n";
    os << u8"  三个下标上的值都等于 500，但只有下界保证是第一次出现的位置\n";
    os << u8"  命中之后闭区间与半开区间立刻返回，落在哪一个重复值上取决于中点先撞上哪一个：\n";
    os << u8"    值 500 这一行，闭区间返回 " << closed.index << u8"（比 " << closed.stats.comparisons
       << u8" 次），半开区间返回 " << half.index << u8"（比 " << half.stats.comparisons << u8" 次）\n";
    os << u8"    下界不提前返回，一路把右端压到左端，比 " << lower.stats.comparisons
       << u8" 次，返回第一次出现的 " << lower.index << u8"\n";
    os << u8"  未命中时下界给出的位置就是插入点：值 150 的插入点是 100，值 1000 的插入点是 1000\n";
    os << "\n";
}

/* ================= 报告：第四段 插值查找 ================= */

void append_interpolation(std::ostringstream &os)
{
    const Table uniform = make_uniform_table(1000, 10);   /* 0, 10, 20, …, 9990 */
    const Table skewed = make_power_table(63);            /* 1, 2, 4, …, 2^62 */

    struct Query {
        std::string label;
        long long value;
    };

    os << u8"四、插值查找：均匀分布与极端偏斜分布\n";
    os << u8"  插值查找不取中点，而是按值的比例估计位置：pos = lo + (x − a[lo]) / (a[hi] − a[lo]) × (hi − lo)\n";
    os << u8"  每轮另外做两次区间守卫判断（x 落在两端之外就收工），记在「守卫判断」一列，不混进探查比较\n";

    const Query uniform_queries[] = {
        {u8"命中首位 x = 0", 0},
        {u8"命中中间 x = 5000", 5000},
        {u8"命中末尾 x = 9990", 9990},
        {u8"未命中 x = 4995", 4995},
        {u8"未命中（表外）x = 99999", 99999},
    };
    os << "  " << u8"均匀分布 u[i] = 10i（n = 1000）\n";
    os << "    " << pad_right(u8"查询", 26) << pad_right(u8"写法", 12) << pad_right(u8"步数", 6)
       << pad_right(u8"探查比较", 10) << pad_right(u8"守卫判断", 10) << u8"返回\n";
    for (const Query &query : uniform_queries) {
        const SearchOutcome interp = interpolation_search(uniform, query.value);
        const SearchOutcome binary = lower_bound_probe(uniform, query.value);
        const SearchOutcome *const outcomes[] = {&interp, &binary};
        for (int variant = 0; variant < 2; ++variant) {
            const SearchOutcome &out = *outcomes[variant];
            os << "    " << pad_right(variant == 0 ? query.label : std::string(), 26)
               << pad_right(variant == 0 ? u8"插值" : u8"二分下界", 12) << std::setw(6)
               << out.stats.iterations << std::setw(10) << out.stats.comparisons << std::setw(10)
               << out.stats.bounds << "  "
               << (out.found ? std::to_string(out.index)
                             : count_text(out.index) + u8"（插入点）")
               << "\n";
        }
    }

    const Query skewed_queries[] = {
        {u8"命中 x = 1（第 0 个）", 1},
        {u8"命中 x = 1024（第 10 个）", 1024},
        {u8"命中 x = 2^40（第 40 个）", 1LL << 40},
        {u8"命中 x = 2^62（第 62 个）", 1LL << 62},
        {u8"未命中 x = 2^40 + 1", (1LL << 40) + 1},
    };
    os << "  " << u8"偏斜分布 s[i] = 2^i（n = 63，值域 1 到 2^62）\n";
    os << "    " << pad_right(u8"查询", 26) << pad_right(u8"写法", 12) << pad_right(u8"步数", 6)
       << pad_right(u8"探查比较", 10) << pad_right(u8"守卫判断", 10) << u8"返回\n";
    std::size_t skewed_worst = 0;
    for (const Query &query : skewed_queries) {
        const SearchOutcome interp = interpolation_search(skewed, query.value);
        const SearchOutcome binary = lower_bound_probe(skewed, query.value);
        skewed_worst = std::max(skewed_worst, interp.stats.comparisons);
        const SearchOutcome *const outcomes[] = {&interp, &binary};
        for (int variant = 0; variant < 2; ++variant) {
            const SearchOutcome &out = *outcomes[variant];
            os << "    " << pad_right(variant == 0 ? query.label : std::string(), 26)
               << pad_right(variant == 0 ? u8"插值" : u8"二分下界", 12) << std::setw(6)
               << out.stats.iterations << std::setw(10) << out.stats.comparisons << std::setw(10)
               << out.stats.bounds << "  "
               << (out.found ? std::to_string(out.index)
                             : count_text(out.index) + u8"（插入点）")
               << "\n";
        }
    }

    const SearchOutcome skewed_mid = interpolation_search(skewed, 1LL << 40);
    const SearchOutcome skewed_binary = lower_bound_probe(skewed, 1LL << 40);
    os << u8"  均匀分布上插值一两次就命中，二分要 9 到 10 次；偏斜分布上反了过来：\n";
    os << u8"    查 2^40 时插值比了 " << skewed_mid.stats.comparisons << u8" 次，二分只比了 "
       << skewed_binary.stats.comparisons << u8" 次\n";
    os << u8"  2 的幂分布的值域是 1 到 2^62：值域上半段里只落着 2^62 这一个键，\n";
    os << u8"  另外 62 个键全挤在下半段，插值的估计位置几乎不动，每轮只把区间缩短一格左右\n";
    os << u8"  插值查找退化的条件就是「值的分布与位置不成比例」；最坏情况下它每次只前进一格，是 O(n)\n";
    os << u8"  最大那个查询反而是一步命中：估计位置正好落在最右端\n";
    os << u8"  两端的值相等时插值公式会除以 0，实现里用区间守卫与「两端重合就收工」两条挡在前面\n";
    os << "\n";
}

/* ================= 报告：第五段 哈希与二分 ================= */

void append_hash_vs_binary(std::ostringstream &os)
{
    const std::size_t key_count = 4096;
    const std::size_t miss_count = 2048;
    const Table keys = make_lehmer_keys(key_count + miss_count, 20261002u);

    HashIndex index(key_count);
    for (std::size_t i = 0; i < key_count; ++i) {
        index.insert(keys[i]);
    }
    Table sorted(keys.begin(), keys.begin() + static_cast<std::ptrdiff_t>(key_count));
    std::size_t sort_comparisons = 0;
    std::sort(sorted.begin(), sorted.end(),
              [&sort_comparisons](long long left, long long right) {
                  ++sort_comparisons;
                  return left < right;
              });

    std::vector<SearchOutcome> hash_hits;
    std::vector<SearchOutcome> hash_misses;
    std::vector<SearchOutcome> binary_hits;
    std::vector<SearchOutcome> binary_misses;
    for (std::size_t i = 0; i < miss_count; ++i) {
        const long long present = keys[i * 2];
        const long long absent = keys[key_count + i];
        hash_hits.push_back(index.find(present));
        binary_hits.push_back(lower_bound_probe(sorted, present));
        hash_misses.push_back(index.find(absent));
        binary_misses.push_back(lower_bound_probe(sorted, absent));
    }

    auto steps_of = [](const std::vector<SearchOutcome> &outcomes) {
        std::vector<std::size_t> values;
        values.reserve(outcomes.size());
        for (const SearchOutcome &out : outcomes) {
            values.push_back(out.stats.iterations);
        }
        return summarize(values);
    };
    auto comparisons_of = [](const std::vector<SearchOutcome> &outcomes) {
        std::vector<std::size_t> values;
        values.reserve(outcomes.size());
        for (const SearchOutcome &out : outcomes) {
            values.push_back(out.stats.comparisons);
        }
        return summarize(values);
    };

    const Average hash_hit_steps = steps_of(hash_hits);
    const Average hash_hit_cmp = comparisons_of(hash_hits);
    const Average hash_miss_steps = steps_of(hash_misses);
    const Average hash_miss_cmp = comparisons_of(hash_misses);
    const Average bin_hit_steps = steps_of(binary_hits);
    const Average bin_hit_cmp = comparisons_of(binary_hits);
    const Average bin_miss_steps = steps_of(binary_misses);
    const Average bin_miss_cmp = comparisons_of(binary_misses);

    os << u8"五、同一批键：哈希索引与「排序后二分」的对照\n";
    os << u8"  键集：" << key_count << u8" 个互不相同的键，由固定种子的 Lehmer 生成器产出\n";
    os << u8"        （递推式 x ← 48271 × x mod 2147483647；前 " << key_count
       << u8" 个建索引，后 " << miss_count << u8" 个当作不在表里的键）\n";
    os << u8"  建索引的代价：\n";
    os << u8"    哈希索引   表长 " << index.table_size() << u8"（2 的幂），装载因子 "
       << fixed_string(index.load_factor(), 3) << u8"，插入 " << key_count << u8" 个键共探查 "
       << index.insert_probes() << u8" 次，平均 "
       << fixed_string(static_cast<double>(index.insert_probes()) / static_cast<double>(key_count), 2)
       << u8" 次，最坏 " << index.max_insert_probe() << u8" 次\n";
    os << u8"    排序后二分 先排一次序：比较 " << sort_comparisons
       << u8" 次（std::sort，比较次数随标准库实现而变），此后不再有额外结构\n";
    os << u8"  查找 " << (miss_count * 2) << u8" 次，每次一个键：\n";
    os << "    " << pad_right(u8"查询类别", 18) << pad_right(u8"写法", 14) << pad_right(u8"查询数", 10)
       << pad_right(u8"平均步数", 12) << pad_right(u8"最坏步数", 12) << pad_right(u8"平均比较", 12)
       << u8"最坏比较\n";

    struct Row {
        const char *label;
        const char *writer;
        const Average *steps;
        const Average *comparisons;
    };
    const Row rows[] = {
        {u8"在表里的键", u8"哈希索引", &hash_hit_steps, &hash_hit_cmp},
        {u8"在表里的键", u8"排序后二分", &bin_hit_steps, &bin_hit_cmp},
        {u8"不在表里的键", u8"哈希索引", &hash_miss_steps, &hash_miss_cmp},
        {u8"不在表里的键", u8"排序后二分", &bin_miss_steps, &bin_miss_cmp},
    };
    for (int i = 0; i < 4; ++i) {
        const Row &row = rows[i];
        os << "    " << pad_right((i % 2 == 0) ? row.label : "", 18) << pad_right(row.writer, 14)
           << pad_right(count_text(miss_count), 10) << pad_right(fixed_string(row.steps->average, 2), 12)
           << pad_right(count_text(row.steps->worst), 12)
           << pad_right(fixed_string(row.comparisons->average, 2), 12) << row.comparisons->worst
           << "\n";
    }

    const double alpha = index.load_factor();
    const double expected_hit = 0.5 * (1.0 + 1.0 / (1.0 - alpha));
    const double expected_miss = 0.5 * (1.0 + 1.0 / ((1.0 - alpha) * (1.0 - alpha)));
    os << u8"  哈希的「步数」是探查过的槽数，「比较」只发生在槽里有键的时候：\n";
    os << u8"    未命中时最后探到的是一个空槽，那一次不算键比较，因此未命中的平均比较比平均步数少 1\n";
    os << u8"    装载因子 " << fixed_string(alpha, 3) << u8" 的线性探查期望值（近似公式）：命中 "
       << fixed_string(expected_hit, 3) << u8" 次探查，未命中 " << fixed_string(expected_miss, 3)
       << u8" 次探查\n";
    os << u8"    实测命中平均 " << fixed_string(hash_hit_steps.average, 2) << u8" 次、未命中平均 "
       << fixed_string(hash_miss_steps.average, 2) << u8" 次，与期望值吻合\n";
    os << u8"  哈希用一次乘法把查找压到常数级，代价是表长翻倍、建表要 O(n) 次探查；\n";
    os << u8"  二分不需要额外内存，代价是每次查找约 " << fixed_string(bin_hit_cmp.average, 1) << u8" 次比较\n";

    /* 坏散列函数：键全部落在同一个起始槽上 */
    Table clustered;
    for (long long i = 1; i <= 64; ++i) {
        clustered.push_back(8192 * i);
    }
    HashIndex bad(64, HashIndex::Hash::IdentityMask);
    HashIndex good(64, HashIndex::Hash::Mixed);
    for (const long long key : clustered) {
        bad.insert(key);
        good.insert(key);
    }
    const SearchOutcome bad_last = bad.find(clustered.back());
    const SearchOutcome good_last = good.find(clustered.back());

    os << u8"  坏散列函数把开放寻址退化成顺序扫描：\n";
    os << u8"    64 个键都取 8192 的倍数，散列写成「键 & (表长 − 1)」，表长 128：全部落在 0 号槽\n";
    os << u8"    插入第 k 个键要探查 k 个槽，64 个键合计 " << bad.insert_probes()
       << u8" 次（等于 64 × 65 / 2），查最后一个键要 " << bad_last.stats.iterations << u8" 次\n";
    os << u8"    同一批键换成 64 位混合散列：插入合计 " << good.insert_probes() << u8" 次，平均 "
       << fixed_string(static_cast<double>(good.insert_probes()) / 64.0, 2) << u8" 次，查最后一个键 "
       << good_last.stats.iterations << u8" 次\n";
    os << u8"    开放寻址的查找代价由探查长度决定，散列函数的质量直接写在这个长度上\n";
    os << "\n";
}

/* ================= 报告：第六段 直方图 ================= */

void append_histograms(std::ostringstream &os, const Table &even)
{
    os << u8"六、结构量：查找步数直方图\n";
    os << u8"  「步数」= 循环体执行次数，各种写法用同一个口径；查询按固定次序发出，命中查询按目标值升序，\n";
    os << u8"  未命中查询按插入点升序\n";
    os << u8"  插值查找的区间守卫在目标值落到区间之外时立刻收工，表外目标因此会出现 0 步\n";

    /* 线性：1000 个命中 + 1000 个缝隙 */
    std::vector<std::size_t> linear_steps;
    for (std::size_t i = 0; i < even.size(); ++i) {
        linear_steps.push_back(linear_sentinel(even, even[i]).stats.iterations);
    }
    for (std::size_t i = 0; i < even.size(); ++i) {
        linear_steps.push_back(linear_sentinel(even, 2 * static_cast<long long>(i) + 1).stats.iterations);
    }
    os << histogram_block(u8"线性查找（哨兵写法），1000 个命中查询加 1000 个未命中查询", linear_steps);

    /* 二分下界：命中与未命中各一批 */
    std::vector<std::size_t> lower_hit_steps;
    std::vector<std::size_t> lower_miss_steps;
    for (std::size_t i = 0; i < even.size(); ++i) {
        lower_hit_steps.push_back(lower_bound_probe(even, even[i]).stats.iterations);
        lower_miss_steps.push_back(
            lower_bound_probe(even, 2 * static_cast<long long>(i) + 1).stats.iterations);
    }
    os << histogram_block(u8"二分下界写法，1000 个命中查询", lower_hit_steps);
    os << histogram_block(u8"二分下界写法，1000 个未命中查询", lower_miss_steps);

    /* 插值：均匀表上的命中与未命中各一批 */
    const Table uniform = make_uniform_table(1000, 10);
    std::vector<std::size_t> interp_uniform_steps;
    for (std::size_t i = 0; i < uniform.size(); ++i) {
        interp_uniform_steps.push_back(interpolation_search(uniform, uniform[i]).stats.iterations);
        interp_uniform_steps.push_back(
            interpolation_search(uniform, 10 * static_cast<long long>(i) + 5).stats.iterations);
    }
    os << histogram_block(u8"插值查找，均匀分布表上 1000 个命中加 1000 个未命中查询",
                          interp_uniform_steps);

    const Table skewed = make_power_table(63);
    std::vector<std::size_t> interp_skewed_steps;
    for (std::size_t i = 0; i < skewed.size(); ++i) {
        interp_skewed_steps.push_back(interpolation_search(skewed, skewed[i]).stats.iterations);
        interp_skewed_steps.push_back(interpolation_search(skewed, skewed[i] + 1).stats.iterations);
    }
    os << histogram_block(u8"插值查找，偏斜分布表上的 63 个命中加 63 个未命中查询",
                          interp_skewed_steps);

    /* 哈希：命中与未命中各一批 */
    const std::size_t key_count = 4096;
    const std::size_t miss_count = 2048;
    const Table keys = make_lehmer_keys(key_count + miss_count, 20261002u);
    HashIndex index(key_count);
    for (std::size_t i = 0; i < key_count; ++i) {
        index.insert(keys[i]);
    }
    std::vector<std::size_t> hash_hit_steps;
    std::vector<std::size_t> hash_miss_steps;
    for (std::size_t i = 0; i < miss_count; ++i) {
        hash_hit_steps.push_back(index.find(keys[i * 2]).stats.iterations);
        hash_miss_steps.push_back(index.find(keys[key_count + i]).stats.iterations);
    }
    os << histogram_block(u8"哈希索引，2048 个在表里的键", hash_hit_steps);
    os << histogram_block(u8"哈希索引，2048 个不在表里的键", hash_miss_steps);

    os << u8"  表长与装载因子：\n";
    os << u8"    线性与二分用的是同一张 1000 个元素的有序表，没有额外的表长与装载因子\n";
    os << u8"    哈希索引表长 " << index.table_size() << u8"，键 " << index.size() << u8" 个，装载因子 "
       << fixed_string(index.load_factor(), 4) << u8"\n";
    os << u8"    插值查找的两张表分别是 1000 个元素（均匀）与 63 个元素（偏斜）\n";
}

}   /* namespace */

std::string build_report()
{
    const Table even = make_arithmetic_table(1000, 2, 0);

    std::ostringstream os;
    append_linear(os);
    append_binary(os);
    append_duplicates(os);
    append_interpolation(os);
    append_hash_vs_binary(os);
    append_histograms(os, even);
    return os.str();
}

/* ================= 计时 ================= */

std::string run_timing()
{
    using clock = std::chrono::steady_clock;
    const std::size_t key_count = 1u << 18;    /* 262144 个键 */
    const std::size_t miss_count = 1u << 15;   /* 32768 个不在表里的键 */
    const std::size_t rounds = 6;

    const Table keys = make_lehmer_keys(key_count + miss_count, 20261002u);
    std::vector<long long> queries;
    queries.reserve(2 * key_count);
    for (std::size_t i = 0; i < key_count; ++i) {
        queries.push_back(keys[i]);                                /* 命中 */
        queries.push_back(keys[key_count + (i % miss_count)]);     /* 未命中 */
    }

    const clock::time_point build_start = clock::now();
    HashIndex index(key_count);
    for (std::size_t i = 0; i < key_count; ++i) {
        index.insert(keys[i]);
    }
    const clock::time_point build_end = clock::now();

    Table sorted(keys.begin(), keys.begin() + static_cast<std::ptrdiff_t>(key_count));
    const clock::time_point sort_start = clock::now();
    std::sort(sorted.begin(), sorted.end());
    const clock::time_point sort_end = clock::now();

    long long checksum = 0;
    double best_hash = 0.0;
    double best_binary = 0.0;
    for (std::size_t round = 0; round < rounds; ++round) {
        const clock::time_point hash_start = clock::now();
        for (const long long key : queries) {
            const SearchOutcome out = index.find(key);
            checksum += static_cast<long long>(out.index & 0xFFu);
        }
        const clock::time_point hash_end = clock::now();

        const clock::time_point binary_start = clock::now();
        for (const long long key : queries) {
            const SearchOutcome out = lower_bound_probe(sorted, key);
            checksum += static_cast<long long>(out.index & 0xFFu);
        }
        const clock::time_point binary_end = clock::now();

        const double hash_ms = std::chrono::duration<double, std::milli>(hash_end - hash_start).count();
        const double binary_ms =
            std::chrono::duration<double, std::milli>(binary_end - binary_start).count();
        if (round == 0 || hash_ms < best_hash) {
            best_hash = hash_ms;
        }
        if (round == 0 || binary_ms < best_binary) {
            best_binary = binary_ms;
        }
    }

    const double per_query = static_cast<double>(queries.size());
    const double hash_ns = best_hash * 1e6 / per_query;
    const double binary_ns = best_binary * 1e6 / per_query;
    const double build_hash_ms =
        std::chrono::duration<double, std::milli>(build_end - build_start).count();
    const double sort_ms = std::chrono::duration<double, std::milli>(sort_end - sort_start).count();

    std::ostringstream os;
    os << u8"  数据：" << key_count << u8" 个键，哈希表长 " << index.table_size() << u8"，装载因子 "
       << fixed_string(index.load_factor(), 3) << u8"；每轮 " << queries.size() << u8" 次查询，共 "
       << rounds << u8" 轮\n";
    os << u8"  建索引：哈希表插入 " << key_count << u8" 个键 " << fixed_string(build_hash_ms, 2)
       << u8" ms；std::sort 排 " << key_count << u8" 个键 " << fixed_string(sort_ms, 2) << u8" ms\n";
    os << u8"  哈希查找：" << fixed_string(hash_ns, 1) << u8" ns/次（" << rounds
       << u8" 轮里最快的一轮）\n";
    os << u8"  二分查找：" << fixed_string(binary_ns, 1) << u8" ns/次\n";
    os << u8"  比值：二分 / 哈希 = " << fixed_string(binary_ns / hash_ns, 2) << u8" 倍\n";
    os << u8"  校验和 " << checksum << u8"（把每次查找的结果加起来，防止整段被优化掉）\n";
    os << u8"  数字随机器、编译器与优化等级变化，程序不对它做任何断言\n";
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

    void expect_count(std::size_t got, std::size_t want, const std::string &what)
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

/** 暴力查找：找不到返回 kNotFound。所有二分写法的结果都拿它当尺子 */
std::size_t brute_find(const Table &table, long long target)
{
    for (std::size_t i = 0; i < table.size(); ++i) {
        if (table[i] == target) {
            return i;
        }
    }
    return kNotFound;
}

/** 暴力下界：第一个不小于 target 的位置，可能在表尾之后 */
std::size_t brute_lower(const Table &table, long long target)
{
    std::size_t pos = 0;
    while (pos < table.size() && table[pos] < target) {
        ++pos;
    }
    return pos;
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

    /* 1—5 线性查找的两种写法 */
    const Table data = make_arithmetic_table(1024, 3, 1);
    const SearchOutcome plain_first = linear_plain(data, 1);
    const SearchOutcome plain_mid = linear_plain(data, 1 + 3 * 512);
    const SearchOutcome plain_last = linear_plain(data, 1 + 3 * 1023);
    const SearchOutcome plain_miss = linear_plain(data, 1000000);
    checks.expect(plain_first.index == 0 && plain_first.stats.comparisons == 1 &&
                      plain_mid.index == 512 && plain_mid.stats.comparisons == 513 &&
                      plain_last.index == 1023 && plain_last.stats.comparisons == 1024 &&
                      !plain_miss.found && plain_miss.stats.comparisons == 1024,
                  u8"普通线性查找：命中首位比 1 次、命中中间比 513 次、命中末尾比 1024 次、未命中比 1024 次");
    checks.expect(plain_first.stats.bounds == 1 && plain_mid.stats.bounds == 513 &&
                      plain_last.stats.bounds == 1024 && plain_miss.stats.bounds == 1025,
                  u8"普通线性查找：下标上界判断次数是 1、513、1024、1025");

    const SearchOutcome sent_first = linear_sentinel(data, 1);
    const SearchOutcome sent_mid = linear_sentinel(data, 1 + 3 * 512);
    const SearchOutcome sent_last = linear_sentinel(data, 1 + 3 * 1023);
    const SearchOutcome sent_miss = linear_sentinel(data, 1000000);
    checks.expect(sent_first.stats.comparisons == 1 && sent_mid.stats.comparisons == 513 &&
                      sent_last.stats.comparisons == 1024 && sent_miss.stats.comparisons == 1025,
                  u8"哨兵线性查找：四种查询的元素比较次数是 1、513、1024、1025");
    checks.expect(sent_first.stats.bounds == 0 && sent_mid.stats.bounds == 0 &&
                      sent_last.stats.bounds == 0 && sent_miss.stats.bounds == 0,
                  u8"哨兵线性查找：下标上界判断恒为 0");
    checks.expect(!sent_miss.found && sent_miss.index == data.size() &&
                      sent_first.found && sent_last.index == 1023,
                  u8"哨兵线性查找：未命中时返回哨兵槽的下标 1024，命中时返回真实下标");

    /* 5 两种线性写法在全部位置与表外目标上一致 */
    bool linear_same = true;
    for (std::size_t i = 0; i < data.size(); ++i) {
        const SearchOutcome a = linear_plain(data, data[i]);
        const SearchOutcome b = linear_sentinel(data, data[i]);
        if (!a.found || !b.found || a.index != i || b.index != i) {
            linear_same = false;
        }
    }
    for (long long x = -50; x <= 3200; x += 7) {
        const bool in_table = (x >= 1 && x <= 3070 && (x - 1) % 3 == 0);
        if (in_table) {
            continue;
        }
        if (linear_plain(data, x).found || linear_sentinel(data, x).found) {
            linear_same = false;
        }
    }
    checks.expect(linear_same, u8"两种线性写法在全部 1024 个位置与表外目标上返回一致");

    /* 6—9 三种二分写法与暴力对照 */
    const Table even = make_arithmetic_table(1000, 2, 0);
    Table probes;
    for (std::size_t i = 0; i < even.size(); ++i) {
        probes.push_back(even[i]);
        probes.push_back(even[i] - 1);
        probes.push_back(even[i] + 1);
    }
    probes.push_back(-1);
    probes.push_back(100000);
    const Table extra = make_lehmer_keys(512, 99u);
    probes.insert(probes.end(), extra.begin(), extra.end());

    bool closed_ok = true;
    bool half_ok = true;
    bool lower_ok = true;
    bool found_index_ok = true;
    bool miss_same = true;
    bool steps_ok = true;
    for (const long long x : probes) {
        const std::size_t want = brute_find(even, x);
        const SearchOutcome closed = binary_closed(even, x);
        const SearchOutcome half = binary_half_open(even, x);
        const SearchOutcome lower = lower_bound_probe(even, x);

        if (closed.index != want || closed.found != (want != kNotFound)) {
            closed_ok = false;
        }
        if (half.index != want || half.found != (want != kNotFound)) {
            half_ok = false;
        }
        if (lower.index != brute_lower(even, x) ||
            lower.found != (lower.index < even.size() && even[lower.index] == x)) {
            lower_ok = false;
        }
        if (want != kNotFound &&
            (even[closed.index] != x || even[half.index] != x || even[lower.index] != x)) {
            found_index_ok = false;
        }
        if (want == kNotFound &&
            (half.index != kNotFound || lower.index != brute_lower(even, x) ||
             half.stats.iterations != lower.stats.iterations)) {
            miss_same = false;
        }
        if (closed.stats.iterations > 10 || half.stats.iterations > 10 ||
            lower.stats.iterations > 10) {
            steps_ok = false;
        }
    }
    checks.expect(closed_ok, u8"闭区间二分：在 3512 个目标上与暴力查找给出同一个下标");
    checks.expect(half_ok, u8"半开区间二分：在 3512 个目标上与暴力查找给出同一个下标");
    checks.expect(lower_ok, u8"下界写法：返回的位置与暴力求出的「第一个不小于 x」逐位相同");
    checks.expect(found_index_ok, u8"命中时三种写法返回的位置上的值都等于目标值");
    checks.expect(miss_same, u8"未命中时半开区间返回未找到，下界给出的位置等于插入点，两者步数相同");
    checks.expect(steps_ok, u8"三种二分写法在 1000 个元素上步数都不超过 10");

    /* 12—14 重复元素 */
    const Table dup = make_blocked_table(1000, 50, 100);
    bool dup_lower = true;
    bool dup_value = true;
    bool dup_order = true;
    for (std::size_t i = 0; i < dup.size(); ++i) {
        if (i > 0 && dup[i] == dup[i - 1]) {
            continue;                     /* 每个值看一次 */
        }
        const SearchOutcome lower = lower_bound_probe(dup, dup[i]);
        const SearchOutcome closed = binary_closed(dup, dup[i]);
        const SearchOutcome half = binary_half_open(dup, dup[i]);
        if (!lower.found || lower.index != i) {
            dup_lower = false;
        }
        if (!closed.found || dup[closed.index] != dup[i] || !half.found || dup[half.index] != dup[i]) {
            dup_value = false;
        }
        if (!closed.found || closed.index < lower.index) {
            dup_order = false;
        }
    }
    checks.expect(dup_lower, u8"重复表上下界返回每个值第一次出现的位置");
    checks.expect(dup_value, u8"重复表上闭区间与半开区间返回的位置上的值等于目标值");
    checks.expect(dup_order, u8"重复表上闭区间返回的下标不小于下界返回的下标");

    /* 15—19 插值查找 */
    const Table uniform = make_uniform_table(1000, 10);
    bool interp_uniform = true;
    for (std::size_t i = 0; i < uniform.size(); ++i) {
        const SearchOutcome out = interpolation_search(uniform, uniform[i]);
        if (!out.found || out.index != i || out.stats.comparisons > 3) {
            interp_uniform = false;
        }
    }
    checks.expect(interp_uniform,
                  u8"均匀表上插值查找对全部 1000 个值一次命中，比较次数不超过 3");

    const Table skewed = make_power_table(63);
    bool interp_same = true;
    std::size_t skewed_worst = 0;
    for (std::size_t i = 0; i < skewed.size(); ++i) {
        const SearchOutcome interp = interpolation_search(skewed, skewed[i]);
        const SearchOutcome lower = lower_bound_probe(skewed, skewed[i]);
        skewed_worst = std::max(skewed_worst, interp.stats.comparisons);
        if (!interp.found || interp.index != lower.index) {
            interp_same = false;
        }
    }
    checks.expect(interp_same, u8"偏斜表上插值查找与二分返回同一个位置");
    checks.expect(skewed_worst > 20,
                  u8"偏斜表上插值查找的比较次数超过 20 次（均匀表上只要 1 次）");

    const Table flat(16, 5);
    const SearchOutcome flat_hit = interpolation_search(flat, 5);
    checks.expect(flat_hit.found && flat[flat_hit.index] == 5,
                  u8"两端的值相等的表上插值查找仍然能找到（不会除以 0）");
    checks.expect(!interpolation_search(uniform, -1).found &&
                      !interpolation_search(uniform, 999999).found,
                  u8"表外目标上插值查找返回未找到（区间守卫挡住）");

    /* 21 插值查找给出的位置与暴力对照一致：命中给下标，未命中给插入点 */
    auto positions_match = [](const Table &table, const Table &targets) {
        for (const long long x : targets) {
            const SearchOutcome out = interpolation_search(table, x);
            if (out.found) {
                if (out.index != brute_find(table, x)) {
                    return false;
                }
            } else if (out.index != brute_lower(table, x)) {
                return false;
            }
        }
        return true;
    };
    Table uniform_probes;
    for (std::size_t i = 0; i < uniform.size(); i += 7) {
        uniform_probes.push_back(uniform[i]);
        uniform_probes.push_back(uniform[i] + 1);
        uniform_probes.push_back(uniform[i] - 1);
    }
    uniform_probes.push_back(-5);
    uniform_probes.push_back(100000);
    Table skewed_probes;
    for (std::size_t i = 0; i < skewed.size(); ++i) {
        skewed_probes.push_back(skewed[i]);
        skewed_probes.push_back(skewed[i] + 1);
    }
    checks.expect(positions_match(uniform, uniform_probes) &&
                      positions_match(skewed, skewed_probes),
                  u8"插值查找给出的位置与暴力对照一致：命中给下标，未命中给插入点");

    /* 20—26 哈希索引 */
    const std::size_t key_count = 4096;
    const std::size_t miss_count = 2048;
    const Table keys = make_lehmer_keys(key_count + miss_count, 20261002u);
    HashIndex index(key_count);
    for (std::size_t i = 0; i < key_count; ++i) {
        index.insert(keys[i]);
    }
    bool table_shape = (index.table_size() & (index.table_size() - 1)) == 0 &&
                       index.table_size() >= 2 * key_count && index.load_factor() <= 0.5;
    checks.expect(table_shape, u8"哈希索引：表长是 2 的幂、不小于 2n，装载因子不超过 0.5");

    bool all_hit = true;
    for (std::size_t i = 0; i < key_count; ++i) {
        const SearchOutcome out = index.find(keys[i]);
        if (!out.found || out.index >= index.table_size()) {
            all_hit = false;
        }
    }
    checks.expect(all_hit, u8"哈希索引：4096 个插入过的键全部查到");

    bool all_miss = true;
    for (std::size_t i = 0; i < miss_count; ++i) {
        if (index.find(keys[key_count + i]).found) {
            all_miss = false;
        }
    }
    checks.expect(all_miss, u8"哈希索引：2048 个没有插入过的键全部未命中");
    checks.expect(index.insert_probes() >= key_count && index.insert_probes() <= 4 * key_count,
                  u8"哈希索引：插入探查总次数落在 n 与 4n 之间（装载因子 0.5 的平均值是 1.5n）");

    Table clustered;
    for (long long i = 1; i <= 64; ++i) {
        clustered.push_back(8192 * i);
    }
    HashIndex bad(64, HashIndex::Hash::IdentityMask);
    HashIndex good(64, HashIndex::Hash::Mixed);
    for (const long long key : clustered) {
        bad.insert(key);
        good.insert(key);
    }
    checks.expect(bad.insert_probes() == 2080 && bad.max_insert_probe() == 64 &&
                      bad.find(clustered.back()).stats.iterations == 64,
                  u8"坏散列：64 个同余键插入共探查 2080 次，查最后一个键要 64 次");
    checks.expect(good.insert_probes() < 64 * 3,
                  u8"同一批键换成 64 位混合散列：平均插入探查少于 3 次");

    /* 26 哈希与二分的平均代价对比 */
    Table sorted(keys.begin(), keys.begin() + static_cast<std::ptrdiff_t>(key_count));
    std::sort(sorted.begin(), sorted.end());
    double hash_sum = 0.0;
    double binary_sum = 0.0;
    for (std::size_t i = 0; i < miss_count; ++i) {
        hash_sum += static_cast<double>(index.find(keys[i * 2]).stats.iterations);
        binary_sum += static_cast<double>(lower_bound_probe(sorted, keys[i * 2]).stats.iterations);
    }
    checks.expect(hash_sum / static_cast<double>(miss_count) <
                      binary_sum / static_cast<double>(miss_count),
                  u8"同一批命中查询：哈希的平均探查次数小于二分的平均步数");

    /* 27 直方图自洽 */
    std::size_t miss_steps = 0;
    for (std::size_t i = 0; i < even.size(); ++i) {
        miss_steps += linear_sentinel(even, 2 * static_cast<long long>(i) + 1).stats.iterations;
    }
    checks.expect_count(miss_steps, even.size() * (even.size() + 1),
                        u8"哨兵写法在 1000 个未命中目标上的步数之和等于 1000 × 1001");

    return checks.finish();
}

}   /* namespace slab */
