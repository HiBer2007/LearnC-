/**
 * main_cli.cpp —— 练习模板 05 的命令行验收程序（C++）
 *
 * 这个文件**不需要改**：它按 3 个阶段调用 stats.cpp 里的函数，
 * 把结果打印成《配置步骤.md》里的验收输出。你的实现写在 src/stats.cpp 里。
 *
 * 构建与运行（在模板目录下）：
 *     cmake --preset mingw-gdb
 *     cmake --build --preset mingw-gdb
 *     build\mingw\bin\app_cli.exe
 *
 * 阶段的划分：
 *     阶段 1  <random>：引擎、均匀分布、与 rand() 对照、正态分布
 *     阶段 2  <numeric>：求和、前缀和、内积、gcd/lcm、统计量
 *     阶段 3  直方图：文本柱状图（界面版画成柱子）
 */
#include <cstddef>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

#include "stats.hpp"

namespace {

void print_counts(const char *label, const std::vector<long> &counts)
{
    std::cout << label << "\n";
    for (std::size_t i = 0; i < counts.size(); ++i) {
        std::cout << "  " << std::setw(2) << (i + 1) << " : "
                  << std::setw(6) << counts[i] << "\n";
    }
}

bool all_between(const std::vector<long> &counts, long lo, long hi)
{
    if (counts.empty()) {
        return false;
    }
    for (long c : counts) {
        if (c < lo || c > hi) {
            return false;
        }
    }
    return true;
}

long total_of(const std::vector<long> &counts)
{
    long total = 0;
    for (long c : counts) {
        total += c;
    }
    return total;
}

} /* namespace */

int main()
{
    /* ---------------------------------------------------------- 阶段 1 */
    std::cout << "=== Stage 1: engines and distributions ===\n";

    const std::vector<unsigned long> raw = st::engine_values(2026u, 5);
    std::cout << "mt19937(2026), first 5:";
    for (unsigned long v : raw) {
        std::cout << " " << v;
    }
    std::cout << "\n";

    const std::vector<int>  dice      = st::roll_uniform(2026u, 6000, 1, 6);
    const std::vector<int>  rand_dice = st::roll_rand(2026u, 6000, 1, 6);
    const std::vector<long> h_uniform = st::histogram(dice, 1, 6);
    const std::vector<long> h_rand    = st::histogram(rand_dice, 1, 6);

    print_counts("uniform_int_distribution(1,6), 6000 rolls:", h_uniform);
    print_counts("rand() % 6 + 1, 6000 rolls:", h_rand);

    std::cout << "uniform all in 800..1200 : "
              << (all_between(h_uniform, 800, 1200) ? "yes" : "no") << "\n";
    std::cout << "rand    all in 800..1200 : "
              << (all_between(h_rand, 800, 1200) ? "yes" : "no") << "\n";

    const std::vector<double> normal = st::normal_sample(2026u, 1000, 50.0, 10.0);
    const std::vector<long>   buckets = st::bucket_counts(normal, 20.0, 80.0, 12);
    std::cout << "normal(50,10), 1000 samples in [20,80): " << total_of(buckets) << "\n";

    {
        const st::Summary s = st::summarize(normal);
        std::cout << std::fixed << std::setprecision(2)
                  << "normal mean = " << s.mean << "   (expected about 50.00)\n"
                  << std::defaultfloat << std::setprecision(6);
    }

    /* ---------------------------------------------------------- 阶段 2 */
    std::cout << "\n=== Stage 2: numeric algorithms ===\n";

    const std::vector<int> v10 = st::iota_sequence(10);

    std::cout << "accumulate sum      : " << st::sum(v10) << "\n";
    std::cout << "accumulate squares  : " << st::sum_of_squares(v10) << "\n";

    std::cout << "iota 1..10          :";
    for (int x : v10) {
        std::cout << " " << x;
    }
    std::cout << "\n";

    std::cout << "partial_sum         :";
    for (int x : st::partial_sums(v10)) {
        std::cout << " " << x;
    }
    std::cout << "\n";

    std::cout << "inner_product(v,v)  : " << st::dot_product(v10, v10) << "\n";

    {
        const st::GcdLcm gl = st::gcd_lcm(24, 36);
        std::cout << "gcd(24,36)          : " << gl.gcd << "\n";
        std::cout << "lcm(24,36)          : " << gl.lcm << "\n";
    }

    {
        const st::Summary f = st::summarize({1.0, 2.0, 3.0, 4.0});
        std::cout << "summary {1,2,3,4}   : mean = " << f.mean
                  << "   variance = " << f.variance
                  << "   median = " << f.median << "\n";
    }

    /* ---------------------------------------------------------- 阶段 3 */
    std::cout << "\n=== Stage 3: histogram ===\n";
    std::cout << "uniform dice, 6000 rolls, one '#' per 20 counts:\n";
    std::cout << st::text_histogram(h_uniform, 20);
    std::cout << "normal sample, 12 buckets in [20,80), one '#' per 2 counts:\n";
    std::cout << st::text_histogram(buckets, 2);

    return 0;
}
