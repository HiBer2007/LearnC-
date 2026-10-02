/* main_cli.cpp —— 练习模板 06 的命令行验收程序（C++）
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
 * 这个文件**不需要改**：它按 4 个阶段调用 snc 里的函数，把结果打印成
 * 《配置步骤.md》里的验收输出。你的实现写在 src/sortnc.cpp 里。
 *
 * 构建与运行（在模板目录下）：
 *     cmake --preset mingw-gdb
 *     cmake --build --preset mingw-gdb
 *     build\mingw\bin\app_cli.exe
 *
 * 输出全部写成 ASCII：换一台代码页不是 65001 的机器也照样能读。
 */
#include <cstddef>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

#include "sortnc.hpp"

namespace {

/* 同一个模板内列宽一致 */
const int kWidth = 36;

void line(const std::string &label, long long value)
{
    std::cout << std::left << std::setw(kWidth) << label << ": " << value << "\n";
}

void line(const std::string &label, const std::string &text)
{
    std::cout << std::left << std::setw(kWidth) << label << ": " << text << "\n";
}

std::string yes_no(bool v)
{
    return v ? "yes" : "no";
}

std::string join_ints(const int *v, int n)
{
    std::string s;
    for (int i = 0; i < n; ++i) {
        if (i != 0) {
            s += ' ';
        }
        s += std::to_string(v[i]);
    }
    return s;
}

bool all_zero(const int *v, int n)
{
    for (int i = 0; i < n; ++i) {
        if (v[i] != 0) {
            return false;
        }
    }
    return true;
}

int sum_ints(const int *v, int n)
{
    int s = 0;
    for (int i = 0; i < n; ++i) {
        s += v[i];
    }
    return s;
}

/* 数据写死在源码里：40 个元素，只有 5 个不同的键（0、4、9、13、15），
 * 每个键各出现 8 次，而且同样的键常常连着一片——不成片的话，
 * 「相等键的先后有没有被打乱」这一列就形同虚设。
 * 下标由 make_items 按位置填进去，它一路跟着元素走，排完之后用来判稳定性。 */
const int kKeys40[40] = {
    13, 13, 13,  0,  0,  9,  9, 13,
     4,  4, 15,  0,  9, 15, 15,  4,
    13,  0,  9,  4, 15, 15,  0,  0,
    13,  9,  4, 15,  9,  0,  4, 13,
     9,  4, 15, 13,  0,  4,  9, 15
};

/* 第二组：24 个元素，最大值 900，让基数排序走满三轮；
 * 里面既有 0 与 7 这样一位、两位的数，也有 900 这种低位全零的数。 */
const int kKeys24[24] = {
    314,   7,   7, 900,   0, 314, 900, 900,
     42,   7,   0, 314,  42,  42, 900,   7,
    314,   0,  42, 900,   7, 314,   0,  42
};

/* 两组越界数据：越界的那个键不在头一个，因此「边验边记」的写法会露馅 */
const int kBadHigh[6] = {13, 0, 4, 16, 9, 4};
const int kBadLow[6] = {13, 0, 4, -1, 9, 4};

std::vector<snc::Item> make_items(const int *keys, int n)
{
    std::vector<snc::Item> v(static_cast<std::size_t>(n));
    for (int i = 0; i < n; ++i) {
        v[static_cast<std::size_t>(i)].key = keys[i];
        v[static_cast<std::size_t>(i)].index = i;
    }
    return v;
}

/* 数据里每个键出现过几次。这一份是从写死的数据直接数出来的，
 * 与学生写的那一段无关，用来决定「打印哪几个键的下标」。 */
std::vector<int> frequencies(const int *keys, int n, int slots)
{
    std::vector<int> f(static_cast<std::size_t>(slots), 0);
    for (int i = 0; i < n; ++i) {
        f[static_cast<std::size_t>(keys[i])] += 1;
    }
    return f;
}

int count_positive(const std::vector<int> &v)
{
    int c = 0;
    for (std::size_t i = 0; i < v.size(); ++i) {
        if (v[i] > 0) {
            ++c;
        }
    }
    return c;
}

} /* namespace */

int main()
{
    const std::vector<snc::Item> data40 = make_items(kKeys40, 40);
    const std::vector<snc::Item> data24 = make_items(kKeys24, 24);
    const std::vector<int> freq40 = frequencies(kKeys40, 40, snc::kKeySlots);

    std::vector<snc::Item> ref40(40);
    snc::stable_sort_reference(data40.data(), 40, ref40.data());

    /* ---------------------------------------------------------- 阶段 1 */
    std::cout << "=== Stage 1: counting the keys ===\n";
    {
        std::cout << std::left << std::setw(kWidth) << "data (40 items)" << ": ";
        snc::print_keys(data40.data(), 40, std::cout);

        line("distinct keys", static_cast<long long>(count_positive(freq40)));
        line("key range", std::string("0..15"));

        int counts[snc::kKeySlots] = {0};
        snc::Counters c;
        const bool ok = snc::count_keys(data40.data(), 40, counts, snc::kKeySlots, c);

        line("counts (16 cells, one per key)", join_ints(counts, snc::kKeySlots));
        line("counts total", static_cast<long long>(sum_ints(counts, snc::kKeySlots)));
        line("count_keys, returned", std::string(ok ? "accepted" : "rejected"));
        line("count_keys, writes", c.writes);
        line("count_keys, bucket ops", c.bucket_ops);

        const std::vector<snc::Item> bad_high = make_items(kBadHigh, 6);
        const std::vector<snc::Item> bad_low = make_items(kBadLow, 6);

        int counts_high[snc::kKeySlots] = {0};
        snc::Counters c_high;
        const bool ok_high =
            snc::count_keys(bad_high.data(), 6, counts_high, snc::kKeySlots, c_high);
        line("bad key 16, returned", std::string(ok_high ? "accepted" : "rejected"));
        line("bad key 16, counts untouched",
             yes_no(all_zero(counts_high, snc::kKeySlots)));

        int counts_low[snc::kKeySlots] = {0};
        snc::Counters c_low;
        const bool ok_low =
            snc::count_keys(bad_low.data(), 6, counts_low, snc::kKeySlots, c_low);
        line("bad key -1, returned", std::string(ok_low ? "accepted" : "rejected"));
        line("bad key -1, counts untouched", yes_no(all_zero(counts_low, snc::kKeySlots)));
    }

    /* ---------------------------------------------------------- 阶段 2 */
    std::cout << "\n=== Stage 2: from the prefix sums to the output ===\n";
    {
        /* 把流水线的三站分别执行一次，让中间结果也能看见 */
        int counts[snc::kKeySlots] = {0};
        snc::Counters c1;
        const bool ok1 = snc::count_keys(data40.data(), 40, counts, snc::kKeySlots, c1);
        line("counts after count_keys", join_ints(counts, snc::kKeySlots));
        line("count_keys, returned", std::string(ok1 ? "accepted" : "rejected"));

        snc::prefix_sum(counts, snc::kKeySlots);
        line("counts after prefix sum", join_ints(counts, snc::kKeySlots));

        int starts[snc::kKeySlots] = {0};
        snc::block_starts(counts, snc::kKeySlots, starts);
        line("starts of each key's block", join_ints(starts, snc::kKeySlots));

        /* 真正跑一遍计数排序 */
        std::vector<snc::Item> out(40);
        int counts2[snc::kKeySlots] = {0};
        snc::Counters c2;
        const bool ok2 =
            snc::count_sort(data40.data(), 40, out.data(), counts2, snc::kKeySlots, c2);

        std::cout << std::left << std::setw(kWidth) << "count_sort, sorted keys" << ": ";
        snc::print_keys(out.data(), 40, std::cout);

        line("count_sort, returned", std::string(ok2 ? "accepted" : "rejected"));
        line("count_sort, sorted by key", yes_no(snc::is_sorted_by_key(out.data(), 40)));
        line("count_sort, same as stable sort",
             yes_no(snc::same_items(out.data(), ref40.data(), 40)));
        line("count_sort, stable (equal keys)",
             yes_no(snc::all_indices_ascending(out.data(), 40)));

        for (int k = 0; k < snc::kKeySlots; ++k) {
            if (freq40[static_cast<std::size_t>(k)] == 0) {
                continue;
            }
            const std::string label = "key " + std::to_string(k) + " indices (in out)";
            std::cout << std::left << std::setw(kWidth) << label << ": ";
            snc::print_indices_of_key(out.data(), 40, k, std::cout);
        }

        line("count_sort, writes", c2.writes);
        line("count_sort, bucket ops", c2.bucket_ops);
    }

    /* ---------------------------------------------------------- 阶段 3 */
    std::cout << "\n=== Stage 3: distributing by digit ===\n";
    {
        const int rounds40 = snc::radix_rounds(data40.data(), 40);
        line("40 items, max key", static_cast<long long>(snc::max_key(data40.data(), 40)));
        line("40 items, rounds", static_cast<long long>(rounds40));

        std::vector<int> round_counts(static_cast<std::size_t>(rounds40 * snc::kRadixSlots), 0);
        std::vector<snc::Item> out(40);
        snc::Counters c;
        const bool ok = snc::radix_sort(data40.data(), 40, out.data(),
                                        round_counts.data(), c);

        for (int r = 0; r < rounds40; ++r) {
            const std::string label =
                "round " + std::to_string(r + 1) + ", bucket sizes (digit 0..9)";
            line(label, join_ints(&round_counts[static_cast<std::size_t>(r * snc::kRadixSlots)],
                                  snc::kRadixSlots));
        }

        std::cout << std::left << std::setw(kWidth) << "40 items, sorted keys" << ": ";
        snc::print_keys(out.data(), 40, std::cout);

        line("40 items, returned", std::string(ok ? "accepted" : "rejected"));
        line("40 items, sorted by key", yes_no(snc::is_sorted_by_key(out.data(), 40)));
        line("40 items, same as stable sort",
             yes_no(snc::same_items(out.data(), ref40.data(), 40)));
        line("40 items, stable (equal keys)",
             yes_no(snc::all_indices_ascending(out.data(), 40)));
        std::cout << std::left << std::setw(kWidth) << "40 items, key 0 indices" << ": ";
        snc::print_indices_of_key(out.data(), 40, 0, std::cout);

        line("40 items, writes", c.writes);
        line("40 items, bucket ops", c.bucket_ops);

        std::cout << "--- second data set ---\n";
        const int rounds24 = snc::radix_rounds(data24.data(), 24);
        line("24 items, max key", static_cast<long long>(snc::max_key(data24.data(), 24)));
        line("24 items, rounds", static_cast<long long>(rounds24));

        std::cout << std::left << std::setw(kWidth) << "24 items, keys" << ": ";
        snc::print_keys(data24.data(), 24, std::cout);

        std::vector<int> round_counts24(static_cast<std::size_t>(rounds24 * snc::kRadixSlots), 0);
        std::vector<snc::Item> out24(24);
        std::vector<snc::Item> ref24(24);
        snc::stable_sort_reference(data24.data(), 24, ref24.data());

        snc::Counters c24;
        const bool ok24 = snc::radix_sort(data24.data(), 24, out24.data(),
                                          round_counts24.data(), c24);

        for (int r = 0; r < rounds24; ++r) {
            const std::string label =
                "round " + std::to_string(r + 1) + ", bucket sizes (digit 0..9)";
            line(label,
                 join_ints(&round_counts24[static_cast<std::size_t>(r * snc::kRadixSlots)],
                           snc::kRadixSlots));
        }

        std::cout << std::left << std::setw(kWidth) << "24 items, sorted keys" << ": ";
        snc::print_keys(out24.data(), 24, std::cout);

        line("24 items, returned", std::string(ok24 ? "accepted" : "rejected"));
        line("24 items, sorted by key", yes_no(snc::is_sorted_by_key(out24.data(), 24)));
        line("24 items, same as stable sort",
             yes_no(snc::same_items(out24.data(), ref24.data(), 24)));
        line("24 items, stable (equal keys)",
             yes_no(snc::all_indices_ascending(out24.data(), 24)));
        line("24 items, writes", c24.writes);
        line("24 items, bucket ops", c24.bucket_ops);
    }

    /* ---------------------------------------------------------- 阶段 4 */
    std::cout << "\n=== Stage 4: the scan direction decides stability ===\n";
    {
        for (int dir = 0; dir < 2; ++dir) {
            const bool forward = (dir == 0);
            const std::string tag = forward ? "forward" : "backward";

            std::vector<snc::Item> out(40);
            snc::Counters c;
            snc::distribute_stable(data40.data(), 40, out.data(), forward, c);

            std::cout << "--- scan " << tag << " ---\n";

            std::cout << std::left << std::setw(kWidth) << "sorted keys" << ": ";
            snc::print_keys(out.data(), 40, std::cout);

            line("sorted by key", yes_no(snc::is_sorted_by_key(out.data(), 40)));
            line("same as stable sort",
                 yes_no(snc::same_items(out.data(), ref40.data(), 40)));
            line("stable (equal keys keep order)",
                 yes_no(snc::all_indices_ascending(out.data(), 40)));

            std::cout << std::left << std::setw(kWidth) << "key 0 indices" << ": ";
            snc::print_indices_of_key(out.data(), 40, 0, std::cout);
            std::cout << std::left << std::setw(kWidth) << "key 15 indices" << ": ";
            snc::print_indices_of_key(out.data(), 40, 15, std::cout);

            line("writes", c.writes);
            line("bucket ops", c.bucket_ops);
        }
    }

    return 0;
}
