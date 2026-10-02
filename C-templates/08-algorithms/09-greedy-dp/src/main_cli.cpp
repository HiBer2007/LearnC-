/* main_cli.cpp —— 练习模板 09 的命令行验收程序（C++）
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
 * 这个文件**不需要改**：它按 4 个阶段调用 gdp 里的函数，把结果打印成
 * 《配置步骤.md》里的验收输出。你的实现写在 src/greedydp.cpp 里。
 *
 * 构建与运行（在模板目录下）：
 *     cmake --preset mingw-gdb
 *     cmake --build --preset mingw-gdb
 *     build\mingw\bin\app_cli.exe
 *
 * 输出全部写成 ASCII：换一台代码页不是 65001 的机器也照样能读。
 * 数据写死在下面，没有随机数、没有时间种子、不读文件，重跑逐位相同。
 */
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

#include "greedydp.hpp"

namespace {

void line(const char *label, long long value)
{
    std::cout << std::left << std::setw(32) << label << ": " << value << "\n";
}

void line(const char *label, const std::string &text)
{
    std::cout << std::left << std::setw(32) << label << ": " << text << "\n";
}

std::string yes_no(bool v)
{
    return v ? "yes" : "no";
}

/* 阶段 1 用的区间表：[lo, hi)，第三个分量是它在输入里的原始下标。
 * 三条错开的链，外加一条又早又长的区间，专门用来把错误策略挡住。 */
const gdp::Interval kIntervals[7] = {
    {  1, 100, 0 },
    {  2,  12, 1 },
    { 12,  22, 2 },
    { 11,  13, 3 },
    { 22,  27, 4 },
    { 27,  32, 5 },
    { 32,  37, 6 },
};

/* 阶段 2 到阶段 4 共用的物品表：分量是重量、价值、原始下标。
 * 前两件单位价值最高的物品，正好每件都塞不进「收了第一件之后剩下的容量」，
 * 于是贪心在这里必定偏离最优解。 */
const gdp::Item kItems[9] = {
    {  4,  9, 0 },
    {  9, 19, 1 },
    {  9, 19, 2 },
    { 11, 22, 3 },
    {  5,  6, 4 },
    {  7,  8, 5 },
    {  3,  3, 6 },
    {  2,  2, 7 },
    {  6,  6, 8 },
};

const int kCapacity = 20;

} /* namespace */

int main()
{
    const std::vector<gdp::Interval> intervals(kIntervals, kIntervals + 7);
    const std::vector<gdp::Item> items(kItems, kItems + 9);

    /* ---------------------------------------------------------- 阶段 1 */
    std::cout << "=== Stage 1: which order, and what to keep ===\n";
    {
        std::cout << "intervals (lo, hi):\n";
        gdp::print_intervals(intervals, std::cout);

        const std::vector<int> best = gdp::interval_schedule(intervals);
        line("schedule count", static_cast<long long>(best.size()));
        line("schedule indices", gdp::indices_to_string(best));

        const std::vector<int> by_left = gdp::schedule_by_left(intervals);
        line("wrong: by left end, count", static_cast<long long>(by_left.size()));
        line("wrong: by left end, indices", gdp::indices_to_string(by_left));

        const std::vector<int> by_len = gdp::schedule_by_length(intervals);
        line("wrong: by length, count", static_cast<long long>(by_len.size()));
        line("wrong: by length, indices", gdp::indices_to_string(by_len));
    }

    /* ---------------------------------------------------------- 阶段 2 */
    std::cout << "\n=== Stage 2: greedy by unit value, and where it fails ===\n";
    {
        std::cout << "items (weight, value):\n";
        gdp::print_items(items, std::cout);
        line("capacity", kCapacity);

        const gdp::KnapsackResult g = gdp::greedy_knapsack(items, kCapacity);
        line("greedy value", g.value);
        line("greedy weight", g.weight);
        line("greedy taken", gdp::indices_to_string(g.taken));

        const int best = gdp::knapsack_brute_force(items, kCapacity);
        line("best by brute force", best);
        line("greedy short by", best - g.value);
    }

    /* ---------------------------------------------------------- 阶段 3 */
    std::cout << "\n=== Stage 3: the two-dim table and its transition ===\n";
    {
        line("items", static_cast<long long>(items.size()));
        line("capacity", kCapacity);

        const gdp::DpResult d = gdp::knapsack_dp(items, kCapacity);
        line("dp best", d.best);
        line("dp cells filled", d.cells);
        line("dp chosen", gdp::indices_to_string(d.chosen));

        const int best = gdp::knapsack_brute_force(items, kCapacity);
        line("dp best equals brute force", yes_no(d.best == best));
    }

    /* ---------------------------------------------------------- 阶段 4 */
    std::cout << "\n=== Stage 4: rolling array, which row and which way ===\n";
    {
        const gdp::DpResult d = gdp::knapsack_dp(items, kCapacity);
        const gdp::RollingResult roll = gdp::knapsack_rolling(items, kCapacity);
        const gdp::RollingResult unb = gdp::knapsack_unbounded(items, kCapacity);

        line("two-dim best", d.best);
        line("two-dim cells", d.cells);
        line("rolling best", roll.best);
        line("rolling array cells", roll.cells);
        line("rolling same as two-dim", yes_no(roll.best == d.best));
        line("unbounded (forward) best", unb.best);
        line("unbounded differs from 0/1", yes_no(unb.best != d.best));
    }

    return 0;
}
