/* stats_demo.cpp —— 随机数造数据与统计的实现。不含 <windows.h>，也不打印 */
#include "stats_demo.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <numeric>
#include <random>
#include <sstream>
#include <stdexcept>

namespace demo {
namespace {

/* 自测的小工具：把每一项的结果记下来，不打印 */
class Checker {
public:
    void check(bool ok, const std::string &what, const std::string &detail = std::string())
    {
        const std::size_t index = result_.lines.size() + 1;
        std::ostringstream line;
        line << (ok ? "[通过] " : "[失败] ") << index << ". " << what;
        if (!ok && !detail.empty()) {
            line << "（" << detail << "）";
        }
        (ok ? result_.passed : result_.failed) += 1;
        result_.lines.push_back(line.str());
    }
    CheckResult take() const { return result_; }

private:
    CheckResult result_;
};

/* 把前面几项接成一行，报表与自测都用得上 */
std::string head_of(const std::vector<int> &values, std::size_t n)
{
    std::ostringstream os;
    for (std::size_t i = 0; i < n && i < values.size(); ++i) {
        os << (i > 0 ? " " : "") << values[i];
    }
    return os.str();
}

/* 从 2 开始按 2 的幂找低 1 位的最小重复步长，找不到返回 0 */
std::size_t smallest_low_bit_period(const std::vector<int> &values, std::size_t limit)
{
    for (std::size_t step = 2; step <= limit; step *= 2) {
        bool repeats = true;
        for (std::size_t i = 0; repeats && i + step < values.size(); ++i) {
            repeats = (values[i] & 1) == (values[i + step] & 1);
        }
        if (repeats) {
            return step;
        }
    }
    return 0;
}

/* 三个整数分布共用的取数循环：引擎固定 mt19937_64，分布由调用方给 */
template <typename Distribution>
std::vector<int> draw(std::uint64_t seed, std::size_t count, Distribution dist)
{
    std::mt19937_64 engine(seed);
    std::vector<int> values(count);
    for (std::size_t i = 0; i < count; ++i) {
        values[i] = dist(engine);
    }
    return values;
}
}   /* namespace */

std::string CheckResult::summary() const
{
    std::ostringstream os;
    os << lines.size() << " 项中 " << passed << " 项通过";
    os << (failed == 0 ? "，全部通过" : "，" + std::to_string(failed) + " 项失败");
    return os.str();
}

std::vector<int> uniform_samples(std::uint64_t seed, std::size_t count,
                                 int low, int high)
{
    return draw(seed, count, std::uniform_int_distribution<int>(low, high));
}
std::vector<int> discrete_samples(std::uint64_t seed, std::size_t count)
{
    /* 权重 5:3:2，discrete_distribution 自己把它归一化成 50% / 30% / 20% */
    return draw(seed, count, std::discrete_distribution<int>{5.0, 3.0, 2.0});
}
std::vector<int> normal_samples(std::uint64_t seed, std::size_t count,
                                double mean, double stddev)
{
    std::mt19937_64 engine(seed);
    std::normal_distribution<double> dist(mean, stddev);
    std::vector<int> values(count);
    for (std::size_t i = 0; i < count; ++i) {
        values[i] = static_cast<int>(std::lround(dist(engine)));  /* 取整再统计 */
    }
    return values;
}
std::vector<int> iota_sequence(std::size_t n)
{
    std::vector<int> values(n);
    std::iota(values.begin(), values.end(), 1);     /* 填成 1、2、3……n */
    return values;
}
Stats compute_stats(const std::vector<int> &values)
{
    Stats stats;
    stats.count = values.size();
    if (values.empty()) {
        return stats;
    }
    /* accumulate 求和，初值写 0LL，避免按 int 累加溢出 */
    stats.sum = std::accumulate(values.begin(), values.end(), 0LL);
    /* inner_product 拿自己与自己内积，就是平方和 */
    stats.square_sum = std::inner_product(values.begin(), values.end(),
                                          values.begin(), 0LL);
    stats.mean = static_cast<double>(stats.sum) / static_cast<double>(stats.count);
    const double mean_square =
        static_cast<double>(stats.square_sum) / static_cast<double>(stats.count);
    stats.variance = mean_square - stats.mean * stats.mean;
    if (stats.variance < 0.0) {         /* 浮点舍入可能把它压成负数 */
        stats.variance = 0.0;
    }
    stats.stddev = std::sqrt(stats.variance);
    stats.minimum = *std::min_element(values.begin(), values.end());
    stats.maximum = *std::max_element(values.begin(), values.end());
    /* partial_sum：第 i 项是原序列前 i+1 项之和 */
    stats.prefix.resize(values.size());
    std::partial_sum(values.begin(), values.end(), stats.prefix.begin());
    return stats;
}
Histogram make_histogram(const std::vector<int> &values, int bin_width)
{
    Histogram histogram;
    if (values.empty() || bin_width <= 0) {
        return histogram;
    }
    histogram.bin_width = bin_width;
    const int lowest = *std::min_element(values.begin(), values.end());
    const int highest = *std::max_element(values.begin(), values.end());

    /* 从最小值起按箱宽往上铺，铺到盖住最大值为止 */
    const int bin_count = (highest - lowest) / bin_width + 1;
    histogram.bins.resize(static_cast<std::size_t>(bin_count));
    for (int i = 0; i < bin_count; ++i) {
        const int low = lowest + i * bin_width;
        histogram.bins[static_cast<std::size_t>(i)] = Bin{low, low + bin_width - 1, 0};
    }
    for (int value : values) {
        ++histogram.bins[static_cast<std::size_t>((value - lowest) / bin_width)].count;
    }
    histogram.total = values.size();
    for (const Bin &bin : histogram.bins) {
        histogram.peak = std::max(histogram.peak, bin.count);
    }
    return histogram;
}
std::string render_histogram(const Histogram &histogram, int bar_width)
{
    if (histogram.bins.empty()) {
        return "（没有样本，画不出直方图）\n";
    }
    std::ostringstream os;
    for (const Bin &bin : histogram.bins) {
        std::size_t bars = histogram.peak > 0
            ? bin.count * static_cast<std::size_t>(bar_width) / histogram.peak : 0;
        if (bin.count > 0 && bars == 0) {
            bars = 1;                   /* 有样本就至少画一格，否则看不见 */
        }
        os << "   [" << std::setw(4) << bin.low << ", " << std::setw(4) << bin.high
           << "] " << std::setw(5) << bin.count << " | " << std::string(bars, '#') << "\n";
    }
    return os.str();
}
RandCompare compare_with_rand(std::uint64_t seed)
{
    /* 2^17 步：本机 rand() 的低 1 位正好以它为周期，取两倍长度做前后比对。
       折叠用的模数刻意取 20000：32768 不是它的整数倍，偏差才看得出来。 */
    const std::size_t period = 131072;
    const std::size_t probe = period * 2;
    const int modulus = 20000;
    const std::size_t folds = 20000;

    RandCompare result;
    result.low_bit_limit = period;
    result.fold_modulus = modulus;
    result.fold_samples = folds;

    /* rand()：种子由 srand 定，序列可复现，但实现由运行库决定 */
    std::vector<int> rand_values(probe);
    std::srand(static_cast<unsigned int>(seed));
    for (std::size_t i = 0; i < probe; ++i) {
        rand_values[i] = std::rand();
    }
    result.rand_low_bit_period = smallest_low_bit_period(rand_values, period);
    /* mt19937_64：同样的取值区间，同样的规模 */
    result.mt_low_bit_period =
        smallest_low_bit_period(uniform_samples(seed, probe, 0, RAND_MAX), period);

    /* 折叠：把取值压到 [0, modulus)，数一数落在前半区间的有多少个 */
    std::srand(static_cast<unsigned int>(seed));
    for (std::size_t i = 0; i < folds; ++i) {
        if (std::rand() % modulus < modulus / 2) {
            ++result.rand_first_half;
        }
    }
    for (int value : uniform_samples(seed, folds, 0, modulus - 1)) {
        if (value < modulus / 2) {
            ++result.mt_first_half;
        }
    }
    result.rand_first_percent = static_cast<int>(
        std::lround(100.0 * static_cast<double>(result.rand_first_half) / folds));
    result.mt_first_percent = static_cast<int>(
        std::lround(100.0 * static_cast<double>(result.mt_first_half) / folds));
    return result;
}
Report make_report(const Options &options)
{
    Report report;
    report.options = options;
    report.uniform = uniform_samples(options.seed, options.count,
                                     options.low, options.high);
    const double center = (options.low + options.high) / 2.0;
    const double spread = (options.high - options.low) / 6.0;
    report.normal = normal_samples(options.seed + 1, options.count, center, spread);
    report.discrete = discrete_samples(options.seed + 2, options.count);
    report.stats = compute_stats(report.uniform);
    report.histogram = make_histogram(report.uniform, options.bin_width);
    report.rand_compare = compare_with_rand(options.seed);
    const RandCompare &cmp = report.rand_compare;
    std::vector<std::size_t> kinds(3, 0);           /* 三个类别各出现几次 */
    for (int kind : report.discrete) {
        ++kinds[static_cast<std::size_t>(kind)];
    }

    std::ostringstream data;
    data << "抽样参数：固定种子 seed=" << options.seed << "，每个分布取 " << options.count
         << " 个样本\n\n"
         << "均匀分布 uniform_int_distribution<int>(" << options.low << ", " << options.high
         << ") 前 12 个：\n  " << head_of(report.uniform, 12) << "\n\n"
         << "正态分布 normal_distribution<double>(" << center << ", " << spread
         << ") 前 12 个（四舍五入）：\n  " << head_of(report.normal, 12) << "\n\n"
         << "离散分布 discrete_distribution 权重 5:3:2 前 12 个类别：\n  "
         << head_of(report.discrete, 12) << "\n  类别计数：" << kinds[0] << " / "
         << kinds[1] << " / " << kinds[2] << "（理论比例 50% / 30% / 20%）\n";
    report.data_text = data.str();

    std::ostringstream stats;
    stats << "<numeric> 统计（对均匀分布的 " << report.stats.count << " 个样本）\n"
          << "  accumulate    总和      = " << report.stats.sum << "\n"
          << std::fixed << std::setprecision(3)
          << "  总和 / 项数   均值      = " << report.stats.mean << "\n"
          << "  inner_product 平方和    = " << report.stats.square_sum << "\n"
          << "  平方和/n - 均值^2  方差  = " << report.stats.variance << "\n"
          << "  方差的平方根  标准差    = " << report.stats.stddev << "\n"
          << "  最小值 / 最大值         = " << report.stats.minimum << " / "
          << report.stats.maximum << "\n  partial_sum   前 10 项  = ";
    for (std::size_t i = 0; i < 10 && i < report.stats.prefix.size(); ++i) {
        stats << (i > 0 ? " " : "") << report.stats.prefix[i];
    }
    stats << "\n  partial_sum   末项      = " << report.stats.prefix.back()
          << "（与 accumulate 的总和相同）\n";
    report.stats_text = stats.str();

    std::ostringstream chart;
    chart << "直方图：箱宽 " << report.histogram.bin_width << "，共 "
          << report.histogram.bins.size() << " 个箱，合计 " << report.histogram.total
          << " 个样本，最高的箱 " << report.histogram.peak << " 个\n"
          << render_histogram(report.histogram);
    report.histogram_text = chart.str();

    std::ostringstream compare;
    compare << "与 rand() 对照（同一台机器，RAND_MAX = " << RAND_MAX << "）\n"
            << "  低位周期：rand() 的低 1 位在 " << cmp.rand_low_bit_period
            << " 步之后逐位重复；mt19937_64 在 " << cmp.low_bit_limit
            << " 步的检查范围内测不到周期\n"
            << "  区间分布：把取值折叠到 " << cmp.fold_modulus
            << " 个值上，rand() 落在前半区间的占 " << cmp.rand_first_percent
            << "%，uniform_int_distribution 占 " << cmp.mt_first_percent
            << "%（各 " << cmp.fold_samples << " 个样本）\n"
            << "  结论：rand() 只有 " << (RAND_MAX + 1)
            << " 个取值、低位周期短、折叠到非整数倍的值域时分布偏移明显；\n"
            << "        <random> 把引擎与分布分开，分布用拒绝采样，因此没有折叠偏差。\n"
            << "        两条都是弱证据：样本少于 " << cmp.low_bit_limit << " 个时低位周期看不出来，\n"
            << "        模数远小于 RAND_MAX 时折叠偏差也测不出来。\n";
    report.rand_text = compare.str();
    return report;
}
bool parse_seed(const std::string &text, std::uint64_t &seed, std::string &error)
{
    /* 去掉两端的空白：界面上从输入框读回来的文本常带换行 */
    const std::size_t begin = text.find_first_not_of(" \t\r\n");
    if (begin == std::string::npos) {
        error = "种子不能为空";
        return false;
    }
    const std::string trimmed =
        text.substr(begin, text.find_last_not_of(" \t\r\n") - begin + 1);
    if (!std::all_of(trimmed.begin(), trimmed.end(),
                     [](char ch) { return ch >= '0' && ch <= '9'; })) {
        error = "种子只能由数字组成，收到：" + trimmed;
        return false;
    }
    try {
        seed = static_cast<std::uint64_t>(std::stoull(trimmed));
        return true;
    } catch (const std::exception &) {
        error = "种子太大，最多 18446744073709551615";
        return false;
    }
}
CheckResult run_self_tests()
{
    Checker checker;
    const std::uint64_t seed = 20240601;
    const std::size_t count = 4000;
    const std::vector<int> values = uniform_samples(seed, count, 0, 99);
    const Stats stats = compute_stats(values);
    const std::vector<int> other = uniform_samples(seed + 7, count, 0, 99);

    /* 1、2：固定种子可复现，换种子换序列；3、4：取值区间与 iota */
    checker.check(values == uniform_samples(seed, count, 0, 99),
                  "固定种子下两次调用逐项相同", head_of(values, 3));
    checker.check(values != other, "换一个种子得到不同的序列", head_of(other, 3));
    checker.check(stats.minimum >= 0 && stats.maximum <= 99,
                  "均匀样本全部落在 [0, 99] 内");
    checker.check(iota_sequence(5) == std::vector<int>({1, 2, 3, 4, 5})
                      && compute_stats(iota_sequence(100)).prefix.back() == 5050LL,
                  "iota 造出 1..n，其 partial_sum 末项等于 n(n+1)/2");

    /* 5 至 8：accumulate、inner_product、均值、方差各自与手算一致 */
    long long manual_sum = 0;
    long long manual_square = 0;
    for (int value : values) {
        manual_sum += value;
        manual_square += static_cast<long long>(value) * value;
    }
    const double mean = static_cast<double>(manual_sum) / static_cast<double>(count);
    double deviation = 0.0;
    for (int value : values) {
        deviation += (value - mean) * (value - mean);
    }
    checker.check(stats.sum == manual_sum, "accumulate 的总和等于逐项相加",
                  std::to_string(stats.sum) + " / " + std::to_string(manual_sum));
    checker.check(stats.square_sum == manual_square,
                  "inner_product 的平方和等于逐项平方相加");
    checker.check(std::fabs(stats.mean - mean) < 1e-9, "均值等于总和除以项数");
    checker.check(std::fabs(stats.variance - deviation / count) < 1e-6,
                  "方差（平方和/n - 均值^2）与离差平方的平均一致");

    /* 9：partial_sum 逐项与手算一致，末项等于总和 */
    long long running = 0;
    bool prefix_ok = stats.prefix.size() == values.size();
    for (std::size_t i = 0; prefix_ok && i < values.size(); ++i) {
        running += values[i];
        prefix_ok = stats.prefix[i] == running;
    }
    checker.check(prefix_ok && stats.prefix.back() == stats.sum,
                  "partial_sum 第 k 项等于前 k+1 项之和，末项等于 accumulate 的总和");

    /* 10 至 13：分箱的边界、合计、覆盖范围与峰值 */
    const Histogram bins = make_histogram(values, 10);
    std::size_t bin_total = 0;
    std::size_t bin_peak = 0;
    bool contiguous = !bins.bins.empty();
    for (std::size_t i = 0; i < bins.bins.size(); ++i) {
        bin_total += bins.bins[i].count;
        bin_peak = std::max(bin_peak, bins.bins[i].count);
        contiguous = contiguous
            && (i + 1 == bins.bins.size()
                || bins.bins[i].high + 1 == bins.bins[i + 1].low);
    }
    checker.check(contiguous, "相邻箱的边界首尾相接，既不重叠也不留缝");
    checker.check(bin_total == values.size(), "各箱计数合计等于样本总数",
                  std::to_string(bin_total) + " / " + std::to_string(values.size()));
    checker.check(bins.bins.front().low <= stats.minimum
                      && bins.bins.back().high >= stats.maximum,
                  "最小值与最大值都落在首箱与末箱覆盖的范围里");
    checker.check(bin_peak == bins.peak, "峰值等于各箱计数的最大值");

    /* 14、15：与 rand() 对照的两条结论；16：三个分布的样本数 */
    const RandCompare cmp = compare_with_rand(seed);
    checker.check(cmp.rand_low_bit_period > 0 && cmp.mt_low_bit_period == 0,
                  "rand() 的低 1 位有短周期，mt19937_64 在同一步数内测不到",
                  std::to_string(cmp.rand_low_bit_period) + " / "
                      + std::to_string(cmp.mt_low_bit_period));
    checker.check(cmp.rand_first_percent > 55 && std::abs(cmp.mt_first_percent - 50) <= 4,
                  "折叠到非整数倍值域时 rand() 偏移明显，uniform_int_distribution 不明显",
                  std::to_string(cmp.rand_first_percent) + "% / "
                      + std::to_string(cmp.mt_first_percent) + "%");
    checker.check(uniform_samples(seed, 7, 0, 99).size() == 7
                      && normal_samples(seed, 7, 0.0, 1.0).size() == 7
                      && discrete_samples(seed, 7).size() == 7,
                  "三个分布各取到请求的样本数");
    return checker.take();
}

}   /* namespace demo */
