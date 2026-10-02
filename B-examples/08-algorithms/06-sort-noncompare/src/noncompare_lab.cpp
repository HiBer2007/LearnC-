/**
 * noncompare_lab.cpp —— 计数排序、基数排序（LSD）与桶排序
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

#include "noncompare_lab.hpp"

#include <algorithm>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

namespace nclab {

namespace {

/* ================= 记账 ================= */

/** 记一个辅助数组：槽数为 0 时 std::vector 不会分配，这里也不记 */
void book_aux(std::size_t slots, std::size_t element_size, SortStats *stats)
{
    if (slots == 0) {
        return;
    }
    ++stats->aux_arrays;
    ++stats->allocations;
    stats->aux_slots += slots;
    stats->aux_bytes += slots * element_size;
}

/* ================= 计数排序的骨架 ================= */

/** 计数排序的通用实现。元素类型与取键方式由调用方给。
 *
 *  两版只在「落点怎么算」上不同：
 *    稳定版    前缀和给每段的起点，从前往后取值、落点也往后走
 *    不稳定版  前缀和给每段的终点，从前往后取值、落点却往前走
 *  不稳定版排出来的数组一样有序，同键元素的原有次序却保不住。 */
template <typename T, typename GetKey>
std::vector<T> counting_sort_impl(const std::vector<T> &input, GetKey get_key, Key key_min,
                                  Key key_max, Stability stability, SortStats *stats)
{
    SortStats scratch;
    SortStats *const st = (stats != nullptr) ? stats : &scratch;
    st->passes = 1;

    const std::size_t n = input.size();
    if (key_max < key_min) {
        return std::vector<T>();
    }
    const std::size_t range = static_cast<std::size_t>(key_max - key_min) + 1;

    std::vector<std::size_t> count(range, 0);
    book_aux(count.size(), sizeof(std::size_t), st);
    std::vector<T> out(n);
    book_aux(out.size(), sizeof(T), st);

    /* 第一遍：数每个键出现几次 */
    for (const T &item : input) {
        ++st->reads;
        ++count[static_cast<std::size_t>(get_key(item) - key_min)];
    }

    if (stability == Stability::Stable) {
        std::size_t sum = 0;
        for (std::size_t i = 0; i < range; ++i) {
            const std::size_t here = count[i];
            count[i] = sum;
            sum += here;
        }
        /* 第二遍：从前往后取值，落点也在各段内从前往后 */
        for (const T &item : input) {
            ++st->reads;
            const std::size_t at = count[static_cast<std::size_t>(get_key(item) - key_min)]++;
            out[at] = item;
            ++st->writes;
        }
    } else {
        std::size_t sum = 0;
        for (std::size_t i = 0; i < range; ++i) {
            sum += count[i];
            count[i] = sum;
        }
        /* 第二遍：从前往后取值，落点却在各段内从后往前：稳定性就是这么丢的 */
        for (const T &item : input) {
            ++st->reads;
            const std::size_t at = --count[static_cast<std::size_t>(get_key(item) - key_min)];
            out[at] = item;
            ++st->writes;
        }
    }
    return out;
}

/* ================= 桶内排序 ================= */

/** 对 a[lo, hi) 做插入排序。这是桶排序里唯一做元素比较的地方 */
void insertion_sort_range(std::vector<Key> &a, std::size_t lo, std::size_t hi, SortStats *st)
{
    for (std::size_t i = lo + 1; i < hi; ++i) {
        const Key key = a[i];
        ++st->reads;
        std::size_t j = i;
        while (j > lo) {
            const Key prev = a[j - 1];
            ++st->reads;
            ++st->bucket_compares;
            if (prev <= key) {
                break;                    /* 已经不比 key 大，位置就是这里 */
            }
            a[j] = prev;
            ++st->writes;
            --j;
        }
        a[j] = key;
        ++st->writes;
    }
}

/* ================= 演示数据 ================= */

/** 固定种子的线性同余发生器：数值每次都一样，不用 rand() */
std::vector<Key> make_keys(std::size_t n, Key max_key, unsigned int seed)
{
    std::vector<Key> keys;
    keys.reserve(n);
    unsigned int state = seed;
    for (std::size_t i = 0; i < n; ++i) {
        state = state * 1664525u + 1013904223u;
        keys.push_back(static_cast<Key>((state >> 16) % (max_key + 1u)));
    }
    return keys;
}

/** 主键 0 到 2、次键 0 到 2 的 12 条记录，主键与次键都有重复 */
std::vector<Record> make_record_demo()
{
    const int primary[12] = {2, 0, 1, 2, 0, 1, 2, 0, 1, 2, 0, 1};
    const int secondary[12] = {0, 1, 0, 1, 2, 2, 2, 0, 1, 0, 1, 0};
    std::vector<Record> records;
    records.reserve(12);
    for (int i = 0; i < 12; ++i) {
        records.push_back(Record{static_cast<Key>(primary[i]),
                                 static_cast<Key>(secondary[i]), i + 1});
    }
    return records;
}

/* ================= 排版 ================= */

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

/** 倍率写成两位小数 */
std::string times(std::size_t part, std::size_t whole)
{
    std::ostringstream os;
    os << std::fixed << std::setprecision(2);
    if (whole == 0) {
        os << 0.0;
    } else {
        os << static_cast<double>(part) / static_cast<double>(whole);
    }
    return os.str();
}

std::string yes_no(bool value)
{
    return value ? u8"是" : u8"否";
}

/* ================= 报告：第一段 ================= */

/** 三种排序在同一批键上的结构量 */
struct SortRun {
    std::string name;
    std::vector<Key> result;
    SortStats stats;
};

void append_overview(std::ostringstream &os, const std::vector<Key> &keys, Key key_min,
                     Key key_max, std::size_t bucket_count)
{
    SortRun runs[3];
    runs[0].name = u8"计数排序（稳定）";
    runs[0].result = counting_sort(keys, key_min, key_max, Stability::Stable, &runs[0].stats);
    runs[1].name = u8"基数排序（LSD）";
    runs[1].result = radix_sort_lsd(keys, key_max, &runs[1].stats);
    runs[2].name = u8"桶排序";
    runs[2].result = bucket_sort(keys, bucket_count, key_min, key_max, &runs[2].stats);

    const std::size_t data_bytes = keys.size() * sizeof(Key);

    os << u8"一、三种非比较排序的结构量（同一批 " << keys.size() << u8" 个键，键域 ["
       << key_min << ", " << key_max << u8"]）\n";
    os << u8"  三种排序的主循环里都没有元素间的比较，次序来自「把键换算成一个下标」\n";
    os << "  " << pad_right(u8"排序", 20) << pad_right(u8"读次数", 8) << pad_right(u8"写次数", 8)
       << pad_right(u8"桶内比较", 10) << pad_right(u8"辅助数组", 10) << pad_right(u8"辅助槽位", 10)
       << pad_right(u8"辅助字节", 10) << pad_right(u8"分配次数", 10) << pad_right(u8"趟数", 6)
       << pad_right(u8"桶数", 6) << u8"辅助/数据\n";
    for (const SortRun &run : runs) {
        os << "  " << pad_right(run.name, 20) << pad_right(std::to_string(run.stats.reads), 8)
           << pad_right(std::to_string(run.stats.writes), 8)
           << pad_right(std::to_string(run.stats.bucket_compares), 10)
           << pad_right(std::to_string(run.stats.aux_arrays), 10)
           << pad_right(std::to_string(run.stats.aux_slots), 10)
           << pad_right(std::to_string(run.stats.aux_bytes), 10)
           << pad_right(std::to_string(run.stats.allocations), 10)
           << pad_right(std::to_string(run.stats.passes), 6)
           << pad_right(std::to_string(run.stats.buckets), 6)
           << times(run.stats.aux_bytes, data_bytes) << "\n";
    }
    os << u8"  数据本身占 " << data_bytes << u8" 字节；「辅助/数据」是辅助空间与它的比值\n";
    os << u8"  计数排序的读 2n 次、写 n 次，与键域宽度无关，但空间随键域线性增长\n";
    os << u8"  基数排序的读写乘上趟数，空间只跟元素个数与数位表（256 槽）有关\n";
    os << u8"  桶排序空间最小，代价是把比较挪进了桶里：唯一一列桶内比较不是 0\n";

    /* 先算出比值再输出，避免把 0 当成结论 */
    const std::size_t aux_of_counting = runs[0].stats.aux_bytes;
    const std::size_t aux_of_radix = runs[1].stats.aux_bytes;
    os << u8"  计数排序的辅助空间是基数排序的 " << times(aux_of_counting, aux_of_radix)
       << u8" 倍，两者排的是同一批键\n";
    os << "\n";
}

/* ================= 报告：第二段 ================= */

void append_preconditions(std::ostringstream &os, const std::vector<Key> &keys, Key key_max,
                          std::size_t bucket_count)
{
    os << u8"二、三种排序各自的前提\n";
    os << u8"  计数排序\n";
    os << u8"    键：必须是整数，或者能一一映射到连续整数下标的键，浮点要先做变换\n";
    os << u8"    范围：上下界必须事先知道，而且宽度不能太大——计数数组的长度就是宽度\n";
    os << u8"    分布：不限，元素怎么分布都不影响读写次数\n";
    os << u8"    代价：读 2n 次、写 n 次，辅助空间只由键域宽度决定，与元素个数无关\n";
    os << u8"  基数排序（LSD，低位优先）\n";
    os << u8"    键：必须能拆成定长的数位；整数天然满足，位数由键的表示宽度定\n";
    os << u8"    范围：不限，这是它比计数排序强的地方——键域再宽，一趟只看一个数位\n";
    os << u8"    分布：不限\n";
    os << u8"    代价：读 2n 次、写 n 次，乘上趟数；辅助空间 = 元素个数 × 2 + 数位表\n";
    os << u8"    前提：每一趟都必须稳定，否则高位那一趟会把低位排好的次序打乱\n";
    os << u8"  桶排序\n";
    os << u8"    键：能映射成有序下标的键，整数与浮点都行\n";
    os << u8"    范围：不限\n";
    os << u8"    分布：必须接近均匀——桶内还要靠比较来排，偏斜会让某一个桶退化成 O(n^2)\n";
    os << u8"    代价：读写各 n 次起步，再加桶内排序的读写与比较\n";
    os << "\n";

    /* 退化一：计数排序的键域远大于元素个数 */
    const std::vector<Key> small = make_keys(16, 31u, 7u);
    std::vector<Key> shifted;
    shifted.reserve(small.size());
    for (const Key k : small) {
        shifted.push_back(k + 1000000u);
    }
    SortStats narrow;
    SortStats wide;
    const std::vector<Key> r_narrow = counting_sort(small, 0u, 31u, Stability::Stable, &narrow);
    const std::vector<Key> r_wide =
        counting_sort(shifted, 1000000u, 2048575u, Stability::Stable, &wide);

    bool shift_kept = (r_narrow.size() == r_wide.size());
    for (std::size_t i = 0; shift_kept && i < r_narrow.size(); ++i) {
        shift_kept = (r_wide[i] == r_narrow[i] + 1000000u);
    }
    const std::size_t small_data_bytes = small.size() * sizeof(Key);

    os << u8"  退化一 计数排序：键域远大于元素个数（两批都是 " << small.size() << u8" 个键）\n";
    os << u8"    键落在 [0, 31]             计数数组 " << narrow.aux_slots - small.size()
       << u8" 槽，辅助合计 " << narrow.aux_bytes << u8" 字节，是数据（" << small_data_bytes
       << u8" 字节）的 " << times(narrow.aux_bytes, small_data_bytes) << u8" 倍\n";
    os << u8"    同一批键整体加 1000000     计数数组 " << wide.aux_slots - small.size()
       << u8" 槽，辅助合计 " << wide.aux_bytes << u8" 字节，是数据的 "
       << times(wide.aux_bytes, small_data_bytes) << u8" 倍\n";
    os << u8"    两批键的相对间隔一模一样，排序结果也只差这个平移量：" << yes_no(shift_kept)
       << "\n";
    os << u8"    计数数组里 " << wide.aux_slots - small.size() << u8" 个槽，落进去的键至多 "
       << small.size() << u8" 个：辅助空间由键域决定，不由个数决定\n";
    os << u8"    键域宽度是元素个数的 " << times(wide.aux_slots - small.size(), small.size())
       << u8" 倍，而排序要走的元素一个没多\n";
    os << u8"    把读 2n 写 n 的代价与这一段并排看：计数排序省下的是比较，付出的是键域\n";
    os << "\n";

    /* 退化二：桶排序在极端偏斜分布下退化 */
    const std::vector<Key> uniform = make_keys(64, 999u, 11u);
    std::vector<Key> skew;
    skew.reserve(64);
    for (std::size_t i = 0; i < 64; ++i) {
        skew.push_back(static_cast<Key>(124 - i));   /* 124 递减到 61，全部落进第 0 个桶 */
    }
    SortStats even;
    SortStats tilted;
    const std::vector<Key> r_even = bucket_sort(uniform, 8, 0u, 999u, &even);
    const std::vector<Key> r_tilt = bucket_sort(skew, 8, 0u, 999u, &tilted);

    os << u8"  退化二 桶排序：分布偏斜（两批都是 64 个键，8 个桶，键域 [0, 999]）\n";
    os << u8"    均匀分布                 桶内比较 " << even.bucket_compares << u8" 次，最满的桶 "
       << even.max_bucket_size << u8" 个元素，读 " << even.reads << u8" 次、写 " << even.writes
       << u8" 次\n";
    os << u8"    全部挤进第 0 个桶且逆序  桶内比较 " << tilted.bucket_compares << u8" 次，最满的桶 "
       << tilted.max_bucket_size << u8" 个元素，读 " << tilted.reads << u8" 次、写 "
       << tilted.writes << u8" 次\n";
    os << u8"    " << tilted.bucket_compares << u8" 次 = 64 × 63 / 2：插入排序在逆序输入上的比较次数就是 n(n − 1) / 2\n";
    os << u8"    两次调用的读写与比较只差在分布上，排序结果都正确："
       << yes_no(std::is_sorted(r_even.begin(), r_even.end()) &&
                 std::is_sorted(r_tilt.begin(), r_tilt.end()))
       << "\n";
    os << u8"    比较次数之比 " << times(tilted.bucket_compares, even.bucket_compares)
       << u8"：桶排序的分布前提不是建议，是代价公式的一部分\n";
    os << "\n";

    /* 把第一段的数据也标一句，说明桶数与键域是配套的 */
    const Key actual_max = keys.empty() ? 0u : *std::max_element(keys.begin(), keys.end());
    os << u8"  第一段的 " << keys.size() << u8" 个键里最大的是 " << actual_max
       << u8"；键域宽度 " << (key_max + 1u) << u8" 取 " << bucket_count << u8" 个桶，"
       << u8"平均每个桶分到 " << times(static_cast<std::size_t>(key_max) + 1u, bucket_count)
       << u8" 个键值\n";
    os << u8"  桶数取多少，等于把键域切成多细的段：分得越细，桶内要排的元素越少\n";
    os << "\n";
}

/* ================= 报告：第三段 ================= */

std::string record_text(const Record &r)
{
    return u8"主 " + std::to_string(r.primary) + u8" 次 " + std::to_string(r.secondary) +
           u8" #" + std::to_string(r.id);
}

void append_stability(std::ostringstream &os)
{
    const std::vector<Record> input = make_record_demo();
    const Key primary_max = 2;
    const Key secondary_max = 2;

    SortStats stable_stats;
    SortStats unstable_stats;
    SortStats packed_stats;
    const std::vector<Record> by_stable =
        sort_two_keys_lsd(input, primary_max, secondary_max, Stability::Stable, &stable_stats);
    const std::vector<Record> by_unstable =
        sort_two_keys_lsd(input, primary_max, secondary_max, Stability::Unstable, &unstable_stats);
    const std::vector<Record> by_packed =
        sort_two_keys_packed(input, primary_max, secondary_max, &packed_stats);

    const bool stable_ok = ordered_primary_then_secondary(by_stable);
    const bool unstable_ok = ordered_primary_then_secondary(by_unstable);
    const bool packed_same = (by_packed == by_stable);
    const bool stable_state_ok = std::is_sorted(by_stable.begin(), by_stable.end(),
                                                [](const Record &a, const Record &b) {
                                                    return a.primary < b.primary;
                                                });
    const bool unstable_state_ok = std::is_sorted(by_unstable.begin(), by_unstable.end(),
                                                  [](const Record &a, const Record &b) {
                                                      return a.primary < b.primary;
                                                  });

    os << u8"三、稳定性与多关键字排序：先按次键排，再按主键排\n";
    os << u8"  " << input.size() << u8" 条记录，主键 0 到 " << primary_max << u8"，次键 0 到 "
       << secondary_max << u8"；同一趟里的排序算法完全相同，只有落点方式不同\n";
    os << "  " << pad_right(u8"序号", 6) << pad_right(u8"做法 A：两趟都用稳定版", 28)
       << u8"做法 B：两趟都用不稳定版\n";
    for (std::size_t i = 0; i < by_stable.size(); ++i) {
        os << "  " << pad_right(std::to_string(i + 1) + ".", 6)
           << pad_right(record_text(by_stable[i]), 28) << record_text(by_unstable[i]) << "\n";
    }
    os << u8"  做法 A 两趟都稳定：同一主键的记录保持「次键递增」：" << yes_no(stable_ok) << "\n";
    os << u8"  做法 B 两趟都不稳定：同一主键的记录次序被打乱：" << yes_no(unstable_ok) << "\n";
    os << u8"  两边按主键看都还有序（主键已经排好）：做法 A " << yes_no(stable_state_ok)
       << u8"、做法 B " << yes_no(unstable_state_ok) << "\n";
    os << u8"  做法 B 的每一条记录都还在，只是同主键的那几段被翻了个个儿\n";
    os << u8"  第二趟排的是主键，主键相同的记录谁在前由第一趟的次序决定；\n";
    os << u8"  落点从后往前，第一趟排出来的次键次序就被倒过来了\n";
    os << u8"  对照：把（主键，次键）打包成一个键（主键 × " << (secondary_max + 1)
       << u8" + 次键），用一趟稳定计数排序排完\n";
    os << u8"    与做法 A 逐位相同：" << yes_no(packed_same) << u8"；那一趟的辅助槽位 "
       << packed_stats.aux_slots << u8"、读 " << packed_stats.reads << u8" 次、写 "
       << packed_stats.writes << u8" 次\n";
    os << u8"    打包要求次键的取值范围已知：主键先乘上（次键上界 + 1）再加次键，两列才不重叠；\n";
    os << u8"    两趟稳定排序没有这个限制，键域再宽也不怕，这就是 LSD 基数排序的来路\n";
    os << u8"  LSD 的成立条件只有一条：每一趟都稳定。上面的对照把这条条件变成了可数的结果\n";
    os << "\n";
}

/* ================= 报告：第四段 ================= */

void append_cross_checks(std::ostringstream &os, const std::vector<Key> &keys, Key key_min,
                         Key key_max, std::size_t bucket_count)
{
    const std::vector<Key> by_counting =
        counting_sort(keys, key_min, key_max, Stability::Stable, nullptr);
    const std::vector<Key> by_radix = radix_sort_lsd(keys, key_max, nullptr);
    const std::vector<Key> by_bucket = bucket_sort(keys, bucket_count, key_min, key_max, nullptr);
    std::vector<Key> by_std = keys;
    std::stable_sort(by_std.begin(), by_std.end());

    const std::size_t wide_buckets = static_cast<std::size_t>(key_max - key_min) + 1;
    SortStats narrow_bucket_stats;
    SortStats wide_bucket_stats;
    const std::vector<Key> by_narrow_bucket =
        bucket_sort(keys, bucket_count, key_min, key_max, &narrow_bucket_stats);
    const std::vector<Key> by_wide_bucket =
        bucket_sort(keys, wide_buckets, key_min, key_max, &wide_bucket_stats);

    os << u8"四、三种排序之间的对照\n";
    os << u8"  同一批 " << keys.size() << u8" 个键，三种排序的结果逐位相同："
       << yes_no(by_counting == by_radix && by_radix == by_bucket) << "\n";
    os << u8"  三种排序的结果都与 std::stable_sort 相同："
       << yes_no(by_counting == by_std && by_radix == by_std && by_bucket == by_std) << "\n";
    os << u8"  桶数取到键域宽度（" << wide_buckets << u8" 个桶）时每个桶最多一个元素：\n";
    os << u8"    桶内比较 " << wide_bucket_stats.bucket_compares << u8" 次，最满的桶 "
       << wide_bucket_stats.max_bucket_size << u8" 个元素，辅助槽位 " << wide_bucket_stats.aux_slots
       << u8"（" << bucket_count << u8" 个桶时是 " << narrow_bucket_stats.aux_slots << u8"）\n";
    os << u8"    结果与计数排序相同：" << yes_no(by_wide_bucket == by_counting)
       << u8"；桶数一多，桶排序就退化成计数排序，比较次数归零、辅助数组变长\n";
    os << u8"  基数排序的趟数由最大键定：最大键 " << key_max << u8" 在 256 进制下需要 "
       << radix_passes(key_max) << u8" 趟（256^" << radix_passes(key_max) << u8" > " << key_max
       << u8" ≥ 256^" << (radix_passes(key_max) - 1) << u8"）\n";
    os << u8"  桶数对桶排序的影响（同一批 " << keys.size() << u8" 个键，键域 [" << key_min
       << ", " << key_max << u8"]）\n";
    os << "    " << pad_right(u8"桶数", 8) << pad_right(u8"最满的桶", 12)
       << pad_right(u8"桶内比较", 12) << pad_right(u8"辅助槽位", 12) << u8"辅助字节\n";
    const std::size_t bucket_choices[4] = {4, 8, 16, 32};
    bool sweep_sorted = true;
    for (const std::size_t choice : bucket_choices) {
        SortStats st;
        const std::vector<Key> r = bucket_sort(keys, choice, key_min, key_max, &st);
        if (!std::is_sorted(r.begin(), r.end()) || r.size() != keys.size()) {
            sweep_sorted = false;
        }
        os << "    " << pad_right(std::to_string(choice), 8)
           << pad_right(std::to_string(st.max_bucket_size), 12)
           << pad_right(std::to_string(st.bucket_compares), 12)
           << pad_right(std::to_string(st.aux_slots), 12) << st.aux_bytes << "\n";
    }
    os << u8"  四档桶数的结果都正确：" << yes_no(sweep_sorted) << "\n";
    os << u8"  每一档的辅助槽位都是（桶数 + 1）+ 桶数 + n，桶数只影响最前面两项\n";
    os << u8"  桶数越多，桶内比较越少，边界与游标两个数组越长：取多少要看键的分布\n";
}

}   /* namespace */

/* ================= 公开接口 ================= */

std::vector<Key> counting_sort(const std::vector<Key> &input, Key key_min, Key key_max,
                               Stability stability, SortStats *stats)
{
    return counting_sort_impl(
        input, [](const Key &k) { return k; }, key_min, key_max, stability, stats);
}

std::vector<Record> counting_sort(const std::vector<Record> &input, KeyField field, Key key_min,
                                  Key key_max, Stability stability, SortStats *stats)
{
    const auto get_key = [field](const Record &r) {
        return (field == KeyField::Primary) ? r.primary : r.secondary;
    };
    return counting_sort_impl(input, get_key, key_min, key_max, stability, stats);
}

std::vector<Record> sort_two_keys_lsd(const std::vector<Record> &input, Key primary_max,
                                      Key secondary_max, Stability stability, SortStats *stats)
{
    /* 先次键、后主键：低位先排，高位后排在稳定排序下不会打乱低位的结果 */
    const std::vector<Record> first =
        counting_sort(input, KeyField::Secondary, 0u, secondary_max, stability, stats);
    return counting_sort(first, KeyField::Primary, 0u, primary_max, stability, stats);
}

std::vector<Record> sort_two_keys_packed(const std::vector<Record> &input, Key primary_max,
                                         Key secondary_max, SortStats *stats)
{
    const std::size_t base = static_cast<std::size_t>(secondary_max) + 1;
    const auto get_key = [base](const Record &r) {
        return static_cast<Key>(static_cast<std::size_t>(r.primary) * base + r.secondary);
    };
    const Key packed_max =
        static_cast<Key>(static_cast<std::size_t>(primary_max) * base + secondary_max);
    return counting_sort_impl(input, get_key, 0u, packed_max, Stability::Stable, stats);
}

bool ordered_primary_then_secondary(const std::vector<Record> &records)
{
    for (std::size_t i = 1; i < records.size(); ++i) {
        const Record &prev = records[i - 1];
        const Record &here = records[i];
        if (prev.primary > here.primary) {
            return false;
        }
        if (prev.primary == here.primary && prev.secondary > here.secondary) {
            return false;
        }
    }
    return true;
}

std::size_t radix_passes(Key key_max)
{
    std::size_t passes = 1;
    while ((static_cast<unsigned long long>(key_max) >> (8 * passes)) != 0) {
        ++passes;
    }
    return passes;
}

std::vector<Key> radix_sort_lsd(const std::vector<Key> &input, Key key_max, SortStats *stats)
{
    SortStats scratch;
    SortStats *const st = (stats != nullptr) ? stats : &scratch;

    const std::size_t n = input.size();
    const std::size_t base = 256;
    const std::size_t passes = radix_passes(key_max);
    st->passes = passes;

    std::vector<std::size_t> count(base, 0);
    book_aux(count.size(), sizeof(std::size_t), st);
    std::vector<Key> buf(input);
    book_aux(buf.size(), sizeof(Key), st);
    std::vector<Key> out(n);
    book_aux(out.size(), sizeof(Key), st);

    for (std::size_t pass = 0; pass < passes; ++pass) {
        const unsigned shift = static_cast<unsigned>(8 * pass);
        std::fill(count.begin(), count.end(), std::size_t(0));
        for (const Key k : buf) {
            ++st->reads;
            ++count[(k >> shift) & 0xFFu];
        }
        std::size_t sum = 0;
        for (std::size_t i = 0; i < base; ++i) {
            const std::size_t here = count[i];
            count[i] = sum;
            sum += here;
        }
        /* 从前往后取值，落点也在各段内从前往后：同数位的元素保持原有次序。
           这两处方向必须一致，一个从前往后、一个从后往前，稳定性就没了 */
        for (std::size_t i = 0; i < n; ++i) {
            ++st->reads;
            const Key k = buf[i];
            out[count[(k >> shift) & 0xFFu]++] = k;
            ++st->writes;
        }
        std::swap(buf, out);   /* 只换两块缓冲区的归属，元素一个没动 */
    }
    return buf;
}

std::vector<Key> bucket_sort(const std::vector<Key> &input, std::size_t bucket_count,
                             Key domain_min, Key domain_max, SortStats *stats)
{
    SortStats scratch;
    SortStats *const st = (stats != nullptr) ? stats : &scratch;
    st->passes = 1;
    st->buckets = bucket_count;
    st->max_bucket_size = 0;

    if (bucket_count == 0 || domain_max < domain_min) {
        return std::vector<Key>();
    }
    const std::size_t n = input.size();
    const std::size_t domain = static_cast<std::size_t>(domain_max - domain_min) + 1;

    std::vector<std::size_t> bound(bucket_count + 1, 0);
    book_aux(bound.size(), sizeof(std::size_t), st);
    std::vector<std::size_t> cursor(bucket_count, 0);
    book_aux(cursor.size(), sizeof(std::size_t), st);
    std::vector<Key> flat(n);
    book_aux(flat.size(), sizeof(Key), st);

    const auto bucket_of = [bucket_count, domain, domain_min](Key k) {
        return static_cast<std::size_t>(k - domain_min) * bucket_count / domain;
    };

    /* 第一遍：数每个桶里有多少个元素。bound[i] 起初是第 i − 1 个桶的个数 */
    for (const Key k : input) {
        ++st->reads;
        ++bound[bucket_of(k) + 1];
    }
    /* 前缀和：bound[i] 变成第 i 个桶的起始槽位，同时记下最满的桶 */
    for (std::size_t i = 1; i <= bucket_count; ++i) {
        bound[i] += bound[i - 1];
        if (bound[i] - bound[i - 1] > st->max_bucket_size) {
            st->max_bucket_size = bound[i] - bound[i - 1];
        }
    }
    for (std::size_t i = 0; i < bucket_count; ++i) {
        cursor[i] = bound[i];
    }
    /* 第二遍：把元素放进各自的桶，同一个桶在 flat 上是一段连续的槽 */
    for (const Key k : input) {
        ++st->reads;
        flat[cursor[bucket_of(k)]++] = k;
        ++st->writes;
    }
    /* 每个桶里用插入排序收尾：桶排序里唯一的元素比较就在这里 */
    for (std::size_t i = 0; i < bucket_count; ++i) {
        insertion_sort_range(flat, bound[i], bound[i + 1], st);
    }
    return flat;
}

/* ================= 报告 ================= */

std::string build_report()
{
    /* 第一段与第四段共用这一批键：32 个，键域 [0, 1000] */
    const std::vector<Key> keys = make_keys(32, 1000u, 20261002u);
    const Key key_min = 0;
    const Key key_max = 1000;
    const std::size_t bucket_count = 8;

    std::ostringstream os;
    append_overview(os, keys, key_min, key_max, bucket_count);
    append_preconditions(os, keys, key_max, bucket_count);
    append_stability(os);
    append_cross_checks(os, keys, key_min, key_max, bucket_count);
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

    void expect_eq(std::size_t got, std::size_t want, const std::string &what)
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

    const std::vector<Key> keys = make_keys(32, 1000u, 20261002u);
    const std::size_t n = keys.size();

    /* 1—3 三种排序的结果与 std::stable_sort 一致 */
    const std::vector<Key> by_counting = counting_sort(keys, 0u, 1000u, Stability::Stable, nullptr);
    const std::vector<Key> by_radix = radix_sort_lsd(keys, 1000u, nullptr);
    const std::vector<Key> by_bucket = bucket_sort(keys, 8, 0u, 1000u, nullptr);
    std::vector<Key> by_std = keys;
    std::stable_sort(by_std.begin(), by_std.end());

    checks.expect(by_counting == by_std, u8"计数排序对 32 个键的结果与 std::stable_sort 逐位相同");
    checks.expect(by_radix == by_std && by_bucket == by_std,
                  u8"LSD 基数排序与桶排序对同一批键的结果也都与 std::stable_sort 相同");
    checks.expect(by_counting == by_radix && by_radix == by_bucket,
                  u8"三种排序的结果两两逐位相同");

    /* 4—6 计数排序的结构量公式 */
    SortStats counting_stats;
    const std::vector<Key> counted = counting_sort(keys, 0u, 1000u, Stability::Stable, &counting_stats);
    checks.expect(counting_stats.reads == 2 * n && counting_stats.writes == n,
                  u8"计数排序读 2n 次、写 n 次，与键域宽度无关");
    checks.expect_eq(counting_stats.aux_slots, 1001 + n, u8"计数排序的辅助槽位 = 键域宽度 + n");
    checks.expect(counting_stats.allocations == 2 && counting_stats.bucket_compares == 0,
                  u8"计数排序分配 2 次（计数数组与输出数组），一次元素比较都不做");
    checks.expect(counted == by_std, u8"带上计数的计数排序结果不变");

    /* 7—8 计数排序的稳定性：稳定版保序、不稳定版仍有序但不保序 */
    const std::vector<Record> records = make_record_demo();
    const std::vector<Record> stable_once =
        counting_sort(records, KeyField::Secondary, 0u, 2u, Stability::Stable, nullptr);
    const std::vector<Record> unstable_once =
        counting_sort(records, KeyField::Secondary, 0u, 2u, Stability::Unstable, nullptr);

    bool stable_kept_order = true;
    {
        /* 稳定版：次键相同的记录，编号必须还是从小到大 */
        std::size_t last_id = 0;
        Key last_key = 0;
        bool first = true;
        for (const Record &r : stable_once) {
            if (first || r.secondary != last_key) {
                last_key = r.secondary;
                last_id = 0;
            }
            if (r.id < static_cast<int>(last_id)) {
                stable_kept_order = false;
            }
            last_id = static_cast<std::size_t>(r.id);
            first = false;
        }
    }
    bool unstable_kept_order = true;
    {
        std::size_t last_id = 0;
        Key last_key = 0;
        bool first = true;
        for (const Record &r : unstable_once) {
            if (first || r.secondary != last_key) {
                last_key = r.secondary;
                last_id = 0;
            }
            if (r.id < static_cast<int>(last_id)) {
                unstable_kept_order = false;
            }
            last_id = static_cast<std::size_t>(r.id);
            first = false;
        }
    }
    const bool stable_sorted = std::is_sorted(stable_once.begin(), stable_once.end(),
                                              [](const Record &a, const Record &b) {
                                                  return a.secondary < b.secondary;
                                              });
    const bool unstable_sorted = std::is_sorted(unstable_once.begin(), unstable_once.end(),
                                                [](const Record &a, const Record &b) {
                                                    return a.secondary < b.secondary;
                                                });
    checks.expect(stable_sorted && stable_kept_order,
                  u8"计数排序稳定版按次键排完后，次键相同的记录仍是原来的先后");
    checks.expect(unstable_sorted && !unstable_kept_order,
                  u8"计数排序不稳定版同样排出有序结果，但次键相同的记录次序被翻了过来");

    /* 9—10 键域宽度决定空间：平移不改变相对结果 */
    const std::vector<Key> small = make_keys(16, 31u, 7u);
    std::vector<Key> shifted;
    shifted.reserve(small.size());
    for (const Key k : small) {
        shifted.push_back(k + 1000000u);
    }
    SortStats narrow;
    SortStats wide;
    const std::vector<Key> r_narrow = counting_sort(small, 0u, 31u, Stability::Stable, &narrow);
    const std::vector<Key> r_wide =
        counting_sort(shifted, 1000000u, 2048575u, Stability::Stable, &wide);

    bool shift_kept = (r_narrow.size() == r_wide.size());
    for (std::size_t i = 0; shift_kept && i < r_narrow.size(); ++i) {
        shift_kept = (r_wide[i] == r_narrow[i] + 1000000u);
    }
    checks.expect(shift_kept, u8"同一批键整体平移 1000000 之后，排序结果也只差这个平移量");
    checks.expect_eq(wide.aux_slots, 1048576 + small.size(),
                     u8"键域宽度 1048576 时计数数组就是 1048576 槽，与元素个数无关");
    const std::size_t narrow_slots = narrow.aux_slots - small.size();   /* 计数数组 32 槽 */
    const std::size_t wide_slots = wide.aux_slots - small.size();       /* 计数数组 1048576 槽 */
    checks.expect(narrow.aux_bytes == 32 * sizeof(std::size_t) + 16 * sizeof(Key) &&
                      wide.aux_bytes == 1048576 * sizeof(std::size_t) + 16 * sizeof(Key) &&
                      wide_slots / narrow_slots == 32768,
                  u8"键域 [0, 31] 时辅助 320 字节，宽度换成 1048576 时是 8388672 字节，"
                     u8"其中计数数组那一段放大 32768 倍");

    /* 11—13 基数排序的趟数与结构量公式 */
    checks.expect(radix_passes(0u) == 1 && radix_passes(255u) == 1 && radix_passes(256u) == 2 &&
                      radix_passes(1000u) == 2 && radix_passes(65536u) == 3,
                  u8"256 进制下的趟数：0 与 255 是 1 趟，256 与 1000 是 2 趟，65536 是 3 趟");
    SortStats radix_stats;
    const std::vector<Key> radix_again = radix_sort_lsd(keys, 1000u, &radix_stats);
    checks.expect(radix_stats.reads == 2 * n * radix_stats.passes &&
                      radix_stats.writes == n * radix_stats.passes,
                  u8"基数排序读 2n 次、写 n 次，都乘以趟数");
    checks.expect_eq(radix_stats.aux_slots, 256 + 2 * n,
                     u8"基数排序的辅助槽位 = 数位表 256 槽 + 两块 n 槽的缓冲区");
    checks.expect(radix_again == by_std, u8"基数排序重跑结果不变");

    /* 14—15 基数排序每一趟都稳定 */
    std::vector<Key> dup_keys;
    for (std::size_t i = 0; i < 16; ++i) {
        dup_keys.push_back(static_cast<Key>((i % 4) * 256u + (i / 4)));
    }
    std::vector<Key> dup_sorted = dup_keys;
    std::stable_sort(dup_sorted.begin(), dup_sorted.end());
    SortStats dup_stats;
    const std::vector<Key> dup_radix = radix_sort_lsd(dup_keys, 1000u, &dup_stats);
    checks.expect(dup_radix == dup_sorted && dup_stats.passes == 2,
                  u8"两个数位各有重复时走 2 趟仍等于 std::stable_sort：高位那一趟没有打乱低位排好的次序");

    /* 16—18 桶排序的结构量与退化 */
    SortStats bucket_stats;
    const std::vector<Key> bucketed = bucket_sort(keys, 8, 0u, 1000u, &bucket_stats);
    checks.expect(bucket_stats.aux_slots == 9 + 8 + n && bucket_stats.allocations == 3,
                  u8"桶排序的辅助槽位 =（桶数 + 1）+ 桶数 + n，分配 3 次");
    checks.expect(bucketed == by_std && bucket_stats.buckets == 8,
                  u8"桶排序结果正确，桶数记在结构量里");

    const std::size_t wide_buckets = 1001;
    SortStats wide_bucket_stats;
    const std::vector<Key> wide_bucketed = bucket_sort(keys, wide_buckets, 0u, 1000u, &wide_bucket_stats);
    checks.expect(wide_bucket_stats.bucket_compares == 0 && wide_bucketed == by_counting,
                  u8"桶数取到键域宽度时每个桶最多一个元素：桶内比较 0 次，结果与计数排序相同");

    const std::vector<Key> uniform64 = make_keys(64, 999u, 11u);
    std::vector<Key> skew64;
    skew64.reserve(64);
    for (std::size_t i = 0; i < 64; ++i) {
        skew64.push_back(static_cast<Key>(124 - i));
    }
    SortStats even;
    SortStats tilted;
    const std::vector<Key> r_even = bucket_sort(uniform64, 8, 0u, 999u, &even);
    const std::vector<Key> r_tilt = bucket_sort(skew64, 8, 0u, 999u, &tilted);
    checks.expect(even.max_bucket_size < tilted.max_bucket_size && !r_even.empty() &&
                      !r_tilt.empty(),
                  u8"64 个键全挤进一个桶时，最满的桶比均匀分布时大");
    checks.expect_eq(tilted.bucket_compares, 2016,
                     u8"全部挤进一个桶且逆序时，桶内比较 2016 次 = 64 × 63 / 2");
    checks.expect(tilted.bucket_compares > 10 * even.bucket_compares,
                  u8"偏斜分布的桶内比较次数是均匀分布的十倍以上");
    std::vector<Key> skew_sorted = skew64;
    std::sort(skew_sorted.begin(), skew_sorted.end());
    checks.expect(std::is_sorted(r_even.begin(), r_even.end()) && r_tilt == skew_sorted,
                  u8"两种分布下的桶排序结果都正确：偏斜那批排完就是 61 到 124 的升序");

    /* 19—21 多关键字与稳定性 */
    const std::vector<Record> input_records = make_record_demo();
    const std::vector<Record> still =
        sort_two_keys_lsd(input_records, 2u, 2u, Stability::Stable, nullptr);
    const std::vector<Record> scrambled =
        sort_two_keys_lsd(input_records, 2u, 2u, Stability::Unstable, nullptr);
    const std::vector<Record> packed =
        sort_two_keys_packed(input_records, 2u, 2u, nullptr);

    checks.expect(ordered_primary_then_secondary(still),
                  u8"先次键、后主键，两趟都用稳定版：结果满足主键优先、次键次之");
    checks.expect(!ordered_primary_then_secondary(scrambled),
                  u8"同样两趟改用不稳定版：结果按主键有序，次键却乱了");
    checks.expect(packed == still,
                  u8"把两个键打包成一个键排一趟，结果与两趟稳定排序逐位相同");

    bool same_members = (scrambled.size() == input_records.size());
    {
        /* 不稳定版只是换了次序，记录一条都没丢：按编号排序后两批应当逐位相同 */
        std::vector<Record> a = scrambled;
        std::vector<Record> b = input_records;
        const auto by_id = [](const Record &x, const Record &y) { return x.id < y.id; };
        std::sort(a.begin(), a.end(), by_id);
        std::sort(b.begin(), b.end(), by_id);
        same_members = same_members && (a == b);
    }
    checks.expect(same_members, u8"不稳定版排完之后 12 条记录一条不少，只是次序不同");

    /* 22—23 边界输入 */
    const std::vector<Key> empty;
    const std::vector<Key> one(1, 7u);
    checks.expect(counting_sort(empty, 0u, 0u, Stability::Stable, nullptr).empty() &&
                      radix_sort_lsd(empty, 0u, nullptr).empty() &&
                      bucket_sort(empty, 4, 0u, 9u, nullptr).empty(),
                  u8"空数组对三种排序都返回空数组");
    checks.expect(counting_sort(one, 7u, 7u, Stability::Stable, nullptr) == one &&
                      radix_sort_lsd(one, 7u, nullptr) == one &&
                      bucket_sort(one, 4, 0u, 9u, nullptr) == one,
                  u8"只有一个元素的数组，三种排序都原样返回");

    SortStats empty_bucket;
    const std::vector<Key> bad_bucket = bucket_sort(one, 0, 0u, 9u, &empty_bucket);
    SortStats bad_counting;
    const std::vector<Key> bad_counted = counting_sort(one, 5u, 4u, Stability::Stable, &bad_counting);
    checks.expect(bad_bucket.empty() && bad_counted.empty() && empty_bucket.aux_arrays == 0 &&
                      bad_counting.aux_arrays == 0,
                  u8"桶数为 0、键域上下界反了这两种退化输入都返回空数组，且一次都不分配");

    return checks.finish();
}

}   /* namespace nclab */
