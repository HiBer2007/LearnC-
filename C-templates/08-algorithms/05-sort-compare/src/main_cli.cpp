/* main_cli.cpp —— 练习模板 05 的命令行验收程序（C++）
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
 * 这个文件**不需要改**：它按 4 个阶段调用 sc 里的函数，把结果打印成
 * 《配置步骤.md》里的验收输出。你的实现写在 src/sortcmp.cpp 里。
 *
 * 构建与运行（在模板目录下）：
 *     cmake --preset mingw-gdb
 *     cmake --build --preset mingw-gdb
 *     build\mingw\bin\app_cli.exe
 *
 * 输出全部写成 ASCII：换一台代码页不是 65001 的机器也照样能读。
 */
#include <iomanip>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

#include "sortcmp.hpp"

namespace {

/* 同一个模板里列宽一致：标签一律 32 列，左对齐 */
const int kLabelWidth = 32;

void line(const char *label, long long value)
{
    std::cout << std::left << std::setw(kLabelWidth) << label << ": " << value << "\n";
}

void line(const char *label, const std::string &text)
{
    std::cout << std::left << std::setw(kLabelWidth) << label << ": " << text << "\n";
}

void keys_line(const char *label, const sc::Item *a, int n)
{
    std::cout << std::left << std::setw(kLabelWidth) << label << ": ";
    sc::print_keys(a, n, std::cout);
}

std::string yes_no(bool v)
{
    return v ? "yes" : "no";
}

/* 一次排序跑完之后，把四个结构量与两条判据一起带出来 */
struct Run {
    std::vector<sc::Item> out;      /* 排完之后的那一份，用来与标尺逐项比 */
    long long compares = 0;
    long long moves = 0;
    long long depth_max = 0;
    bool sorted = false;
    bool stable = false;
};

using SortFn = void (*)(sc::Item *a, int n, sc::Stats &s);

/* 每一种排序都从同一份输入开始：data 按值传进来，每次调用一份新的 */
Run run_sort(SortFn fn, std::vector<sc::Item> data)
{
    const int n = static_cast<int>(data.size());

    sc::Stats s;
    fn(data.data(), n, s);

    Run r;
    r.compares = s.compares;
    r.moves = s.moves;
    r.depth_max = s.depth_max;
    r.sorted = sc::is_sorted(data.data(), n);
    r.stable = sc::stable_ok(data.data(), n);
    r.out = std::move(data);
    return r;
}

void report(const char *prefix, const Run &r, bool with_depth)
{
    const std::string p(prefix);
    line((p + ": sorted").c_str(), yes_no(r.sorted));
    line((p + ": stable").c_str(), yes_no(r.stable));
    line((p + ": compares").c_str(), r.compares);
    line((p + ": moves").c_str(), r.moves);
    if (with_depth) {
        line((p + ": depth max").c_str(), r.depth_max);
    }
}

} /* namespace */

int main()
{
    const int blocks_n = 32;
    const int blocks_keys = 4;
    const int random_n = 1024;
    const unsigned random_seed = 12345u;
    const int random_key_max = 4096;
    const int ascending_n = 128;

    /* ---------------------------------------------------------- 阶段 1 */
    std::cout << "=== Stage 1: the merge step, top-down and bottom-up ===\n";
    {
        const std::vector<sc::Item> blocks = sc::make_blocks(blocks_n, blocks_keys);
        const std::vector<sc::Item> want = sc::reference_order(blocks);
        const int nb = static_cast<int>(blocks.size());

        std::cout << std::left << std::setw(kLabelWidth) << "data"
                  << ": 32 items, only 4 distinct keys\n";
        keys_line("input keys", blocks.data(), nb);

        const Run top = run_sort(sc::merge_sort, blocks);
        report("merge", top, true);
        line("merge: same as reference", yes_no(sc::same_seq(top.out.data(), want.data(), nb)));
        keys_line("merge: sorted keys", top.out.data(), nb);

        const Run bot = run_sort(sc::merge_sort_bottomup, blocks);
        report("bottom-up", bot, false);
        line("bottom-up: same as reference", yes_no(sc::same_seq(bot.out.data(), want.data(), nb)));

        const std::vector<sc::Item> rnd =
            sc::make_random(random_n, random_seed, random_key_max);
        std::cout << "1024 items, keys 0..4095, seed 12345:\n";
        const Run big = run_sort(sc::merge_sort, rnd);
        line("merge: compares", big.compares);
        line("merge: moves", big.moves);
        line("merge: depth max", big.depth_max);
        line("std::sort: compares", sc::std_sort_compares(rnd));
    }

    /* ---------------------------------------------------------- 阶段 2 */
    std::cout << "\n=== Stage 2: Lomuto partition, pivot is the last element ===\n";
    {
        const std::vector<sc::Item> blocks = sc::make_blocks(blocks_n, blocks_keys);
        std::cout << "32 items, only 4 distinct keys:\n";
        report("last-element pivot", run_sort(sc::quick_sort_last, blocks), true);

        const std::vector<sc::Item> rnd =
            sc::make_random(random_n, random_seed, random_key_max);
        std::cout << "1024 items, keys 0..4095, seed 12345:\n";
        const Run big = run_sort(sc::quick_sort_last, rnd);
        line("last-element pivot: compares", big.compares);
        line("last-element pivot: moves", big.moves);
        line("last-element pivot: depth max", big.depth_max);
        line("std::sort: compares", sc::std_sort_compares(rnd));
    }

    /* ---------------------------------------------------------- 阶段 3 */
    std::cout << "\n=== Stage 3: last-element pivot vs median-of-three ===\n";
    {
        const std::vector<sc::Item> asc = sc::make_ascending(ascending_n);
        std::cout << "128 items, already ascending:\n";
        const Run a1 = run_sort(sc::quick_sort_last, asc);
        line("last-element pivot: compares", a1.compares);
        line("last-element pivot: depth max", a1.depth_max);
        line("last-element pivot: sorted", yes_no(a1.sorted));

        const Run a2 = run_sort(sc::quick_sort_med, asc);
        line("median-of-three: compares", a2.compares);
        line("median-of-three: depth max", a2.depth_max);
        line("median-of-three: sorted", yes_no(a2.sorted));

        const std::vector<sc::Item> rnd =
            sc::make_random(random_n, random_seed, random_key_max);
        std::cout << "1024 items, keys 0..4095, seed 12345:\n";
        const Run b1 = run_sort(sc::quick_sort_last, rnd);
        line("last-element pivot: compares", b1.compares);
        line("last-element pivot: depth max", b1.depth_max);

        const Run b2 = run_sort(sc::quick_sort_med, rnd);
        line("median-of-three: compares", b2.compares);
        line("median-of-three: depth max", b2.depth_max);
        line("median-of-three: sorted", yes_no(b2.sorted));
    }

    /* ---------------------------------------------------------- 阶段 4 */
    std::cout << "\n=== Stage 4: heap sort, sift down ===\n";
    {
        const std::vector<sc::Item> blocks = sc::make_blocks(blocks_n, blocks_keys);
        std::cout << "32 items, only 4 distinct keys:\n";
        report("heap", run_sort(sc::heap_sort, blocks), false);

        const std::vector<sc::Item> rnd =
            sc::make_random(random_n, random_seed, random_key_max);
        std::cout << "1024 items, keys 0..4095, seed 12345:\n";
        const Run big = run_sort(sc::heap_sort, rnd);
        line("heap: compares", big.compares);
        line("heap: moves", big.moves);
        line("heap: depth max", big.depth_max);
        line("std::sort: compares", sc::std_sort_compares(rnd));
    }

    /* ---------------------------------------------------------- 汇总 */
    std::cout << "\n=== Summary: 1024 items, keys 0..4095, seed 12345, compares ===\n";
    {
        const std::vector<sc::Item> rnd =
            sc::make_random(random_n, random_seed, random_key_max);
        line("merge (top-down)", run_sort(sc::merge_sort, rnd).compares);
        line("merge (bottom-up)", run_sort(sc::merge_sort_bottomup, rnd).compares);
        line("quick (last pivot)", run_sort(sc::quick_sort_last, rnd).compares);
        line("quick (median-of-three)", run_sort(sc::quick_sort_med, rnd).compares);
        line("heap", run_sort(sc::heap_sort, rnd).compares);
        line("std::sort", sc::std_sort_compares(rnd));
    }

    return 0;
}
