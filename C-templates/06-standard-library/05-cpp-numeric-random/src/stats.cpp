/**
 * stats.cpp —— 练习模板 05 的核心逻辑（<random> 与 <numeric>）
 *
 * 3 个阶段的实现都写在这个文件里。骨架给的是占位实现：
 * 能编译、能运行、结果明显不对（返回空容器、0、-1）。
 * 各阶段的任务、验收标准与自查方法见同目录《配置步骤.md》。
 *
 * 构建与运行（在模板目录下）：
 *     cmake --preset mingw-gdb
 *     cmake --build --preset mingw-gdb
 *     build\mingw\bin\app_cli.exe
 *     build\mingw\bin\app_gui_win32.exe
 */
#include "stats.hpp"

#include <algorithm>
#include <cstdlib>
#include <numeric>
#include <random>
#include <sstream>

namespace st {

/* ==================================================================
 * 阶段 1：<random> 的引擎与分布
 * ================================================================== */

/* TODO（阶段 1-1）：用 mt19937 固定种子取 n 个原始值。
 *
 * 提示：
 *   1. std::mt19937 engine(seed);  然后循环 engine()；
 *   2. 返回类型是 unsigned long，直接 push_back(engine()) 即可；
 *   3. 引擎的输出序列是标准规定的：同一个种子在任何实现上都得到同一串数字，
 *      因此这几行可以逐字写进验收标准。
 *
 * 验收：种子 2026 的前 5 个值与《配置步骤.md》阶段 1 一致。 */
std::vector<unsigned long> engine_values(unsigned seed, std::size_t n)
{
    (void)seed;
    (void)n;
    return {};
}

/* TODO（阶段 1-2）：均匀分布掷骰子。
 *
 * 提示：
 *   1. std::uniform_int_distribution<int> dist(low, high)；两端都取得到；
 *   2. dist(engine) 每次取一个；
 *   3. 分布的具体实现由标准库决定，因此不同标准库的桶频次会有差异，
 *      验收只看「每桶都在合理区间内」。
 *
 * 验收：6000 次投掷里每面都在 800 到 1200 之间。 */
std::vector<int> roll_uniform(unsigned seed, int rolls, int low, int high)
{
    (void)seed;
    (void)rolls;
    (void)low;
    (void)high;
    return {};
}

/* TODO（阶段 1-3）：用 rand() 做同一件事，作为对照。
 *
 * 提示：先 std::srand(seed)，再循环 std::rand() % (high - low + 1) + low。
 *       这里的取模会让各面不再等概率（RAND_MAX 不是 6 的倍数），
 *       而且 rand() 的周期与质量都远不如 mt19937；
 *       这一点见《06-标准库/A-03-数值、数学与随机.md》第 4 节。
 *
 * 验收：每面同样在 800 到 1200 之间，但与 uniform 的桶频次不同。 */
std::vector<int> roll_rand(unsigned seed, int rolls, int low, int high)
{
    (void)seed;
    (void)rolls;
    (void)low;
    (void)high;
    return {};
}

/* TODO（阶段 1-4）：正态分布取样。
 *
 * 提示：std::normal_distribution<double> dist(mean, sigma)；
 *       注意第二个参数是标准差，不是方差。
 *
 * 验收：1000 个样本里落在 [mean - 3*sigma, mean + 3*sigma] 的比例很高。 */
std::vector<double> normal_sample(unsigned seed, std::size_t n, double mean, double sigma)
{
    (void)seed;
    (void)n;
    (void)mean;
    (void)sigma;
    return {};
}

/* TODO（阶段 1-5）：数每个取值出现多少次。
 *
 * 提示：返回长度为 high - low + 1 的数组，遍历时用 v - low 当下标；
 *       落在区间外的样本直接跳过，不要越界。 */
std::vector<long> histogram(const std::vector<int> &values, int low, int high)
{
    (void)values;
    (void)low;
    (void)high;
    return {};
}

/* TODO（阶段 1-6）：把 [lo, hi) 等分成 buckets 个桶。
 *
 * 提示：
 *   1. 桶宽 width = (hi - lo) / buckets；
 *   2. 下标是 static_cast<int>((v - lo) / width)，等于 buckets 的算进最后一桶；
 *   3. 小于 lo 或大于 hi 的样本不计。
 *
 * 验收：12 个桶的计数之和等于样本数。 */
std::vector<long> bucket_counts(const std::vector<double> &values,
                                double lo, double hi, int buckets)
{
    (void)values;
    (void)lo;
    (void)hi;
    (void)buckets;
    return {};
}

/* ==================================================================
 * 阶段 2：<numeric> 与统计量
 * ================================================================== */

/* TODO（阶段 2-1）：求和。
 * 提示：std::accumulate(v.begin(), v.end(), 0L)；初值写成 0（int）在元素多时会溢出。 */
long sum(const std::vector<int> &v)
{
    (void)v;
    return -1;
}

/* TODO（阶段 2-2）：平方和，用 accumulate 的第四个参数传一个 lambda。
 * 提示：[](long acc, int x) { return acc + static_cast<long>(x) * x; }。 */
long sum_of_squares(const std::vector<int> &v)
{
    (void)v;
    return -1;
}

/* TODO（阶段 2-3）：生成 1..n。
 * 提示：先 std::vector<int> v(n)，再 std::iota(v.begin(), v.end(), 1)。 */
std::vector<int> iota_sequence(int n)
{
    (void)n;
    return {};
}

/* TODO（阶段 2-4）：前缀和。
 * 提示：std::partial_sum(v.begin(), v.end(), out.begin())，输出容器先按同样大小准备好。 */
std::vector<int> partial_sums(const std::vector<int> &v)
{
    (void)v;
    return {};
}

/* TODO（阶段 2-5）：内积。
 * 提示：std::inner_product(a.begin(), a.end(), b.begin(), 0L)；
 *       两个序列不等长时行为未定义，函数开头先判断一下再决定返回什么。 */
long dot_product(const std::vector<int> &a, const std::vector<int> &b)
{
    (void)a;
    (void)b;
    return -1;
}

/* TODO（阶段 2-6）：C++17 的 std::gcd 与 std::lcm。
 * 提示：两者都在 <numeric> 里；lcm 可以用 a / gcd(a, b) * b 得到。 */
GcdLcm gcd_lcm(long a, long b)
{
    (void)a;
    (void)b;
    return GcdLcm{};
}

/* TODO（阶段 2-7）：均值、总体方差、中位数。
 *
 * 提示：
 *   1. 均值用 accumulate 求和再除以个数，注意除以 double；
 *   2. 方差 = 每个样本与均值之差的平方和 / n；
 *   3. 中位数要先 std::sort；个数是偶数时取中间两个的平均；
 *   4. 空序列返回全 0，别除以 0。
 *
 * 验收：{1,2,3,4} 得到 mean = 2.5、variance = 1.25、median = 2.5。 */
Summary summarize(std::vector<double> v)
{
    (void)v;
    return Summary{};
}

/* ==================================================================
 * 阶段 3：直方图文本
 * ================================================================== */

/* TODO（阶段 3-1）：把桶计数画成文本直方图。
 *
 * 提示：
 *   1. std::ostringstream os；
 *   2. 每行：桶号右对齐宽度 6，再跟 " | "，再跟若干 '#'，再跟一个空格与计数；
 *   3. '#' 的个数是 计数 / scale（整数除法），计数不为 0 时至少一个；
 *   4. 行尾要有换行，最后一行也要。
 *
 * 验收：与《配置步骤.md》阶段 3 的输出一致；界面版把这个字符串画成柱子。 */
std::string text_histogram(const std::vector<long> &counts, int scale)
{
    (void)counts;
    (void)scale;
    return "(TODO stage 3-1: text_histogram not implemented yet)\n";
}

} /* namespace st */
