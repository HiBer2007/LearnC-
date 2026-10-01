/**
 * stats.hpp —— 练习模板 05 的核心接口（C++）
 *
 * 接口已经定好，src/main_cli.cpp 与 src/main_gui_win32.cpp 都调用它们。
 * 你要做的是在 src/stats.cpp 里把标了 TODO 的函数实现出来。
 *
 * 三个阶段的对应关系：
 *     阶段 1   engine_values、roll_uniform、roll_rand、
 *              normal_sample、histogram、bucket_counts   <random>
 *     阶段 2   sum、sum_of_squares、iota_sequence、
 *              partial_sums、dot_product、gcd_lcm、
 *              summarize                                 <numeric> 与统计量
 *     阶段 3   text_histogram                           直方图（界面版也用它）
 */
#ifndef STATS_HPP
#define STATS_HPP

#include <cstddef>
#include <string>
#include <vector>

namespace st {

/* ==================================================================
 * 阶段 1：<random> 的引擎与分布
 * ================================================================== */

/* 用 std::mt19937 固定种子取 n 个原始值。
 * 引擎的输出序列由标准规定，因此在任何实现上都是同一串数字。 */
std::vector<unsigned long> engine_values(unsigned seed, std::size_t n);

/* 掷骰子：把 [low, high] 上的均匀分布取 rolls 次。
 * 提示：std::uniform_int_distribution<int> dist(low, high)（注意两端都取得到）。 */
std::vector<int> roll_uniform(unsigned seed, int rolls, int low, int high);

/* 同样掷骰子，但用 C 的 rand()：std::rand() % (high - low + 1) + low。
 * 这是拿来对照的写法，用来观察它的问题。 */
std::vector<int> roll_rand(unsigned seed, int rolls, int low, int high);

/* 正态分布取样：均值 mean、标准差 sigma。 */
std::vector<double> normal_sample(unsigned seed, std::size_t n, double mean, double sigma);

/* 数每个取值出现了多少次，返回长度为 high - low + 1 的数组，下标 0 对应 low。 */
std::vector<long> histogram(const std::vector<int> &values, int low, int high);

/* 把 [lo, hi) 等分成 buckets 个桶，数每个区间里有多少个样本。
 * 恰好等于 hi 的样本算进最后一个桶，桶外的不计。 */
std::vector<long> bucket_counts(const std::vector<double> &values,
                                double lo, double hi, int buckets);

/* ==================================================================
 * 阶段 2：<numeric> 与统计量
 * ================================================================== */

/* 求和：std::accumulate 的初值要给 0L，否则 int 会溢出 */
long sum(const std::vector<int> &v);

/* 平方和：用 accumulate 的自定义二元操作 */
long sum_of_squares(const std::vector<int> &v);

/* 生成 1..n：std::iota */
std::vector<int> iota_sequence(int n);

/* 前缀和：std::partial_sum */
std::vector<int> partial_sums(const std::vector<int> &v);

/* 内积：std::inner_product */
long dot_product(const std::vector<int> &a, const std::vector<int> &b);

struct GcdLcm {
    long gcd = 0;
    long lcm = 0;
};

/* C++17 的 std::gcd 与 std::lcm（在 <numeric> 里） */
GcdLcm gcd_lcm(long a, long b);

struct Summary {
    double mean = 0.0;
    double variance = 0.0;   /* 总体方差，除以 n */
    double median = 0.0;
};

/* 均值、方差、中位数。形参按值传入，函数内部可以排序求中位数。
 * 空序列返回全 0。 */
Summary summarize(std::vector<double> v);

/* ==================================================================
 * 阶段 3：直方图文本
 * ================================================================== */

/* 把每个桶的计数画成一行：每 scale 个计数画一个 '#'，至少一个。
 * 格式（宽度与分隔符必须一致）：
 *       1 | #################### 1013
 * 提示：用 std::ostringstream，桶号右对齐宽度 6，再跟 " | "。 */
std::string text_histogram(const std::vector<long> &counts, int scale);

} /* namespace st */

#endif /* STATS_HPP */
