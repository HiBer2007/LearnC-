/* benchmark.cpp —— 小基准工具的实现。不含任何界面代码，也不打印 */
#include "benchmark.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <numeric>
#include <sstream>

namespace bench {
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
double median_of(const std::vector<double> &sorted_ms)
{
    const std::size_t size = sorted_ms.size();
    if (size == 0) {
        return 0.0;
    }
    return (size % 2 == 1) ? sorted_ms[size / 2]
                           : (sorted_ms[size / 2 - 1] + sorted_ms[size / 2]) / 2.0;
}
/* 把一条耗时写成 6 位宽、3 位小数 */
std::string ms_text(double value)
{
    std::ostringstream os;
    os << std::fixed << std::setprecision(3) << std::setw(9) << value;
    return os.str();
}
}   /* namespace */
std::string CheckResult::summary() const
{
    std::ostringstream os;
    os << lines.size() << " 项中 " << passed << " 项通过";
    os << (failed == 0 ? "，全部通过" : "，" + std::to_string(failed) + " 项失败");
    return os.str();
}
/* ── 被测的三段纯计算 ─────────────────────────────────── */
long long sum_of_squares(std::atomic<long long> &counter, std::size_t scale)
{
    /* 内层按 256 一段，段尾一次性把这一段的次数记进 atomic：
       原子加本身也要几十个时钟周期，逐次加会让计时测的是原子操作而不是计算。 */
    constexpr std::size_t kChunk = 256;
    long long total = 0;
    for (std::size_t base = 0; base < scale; base += kChunk) {
        const std::size_t end = std::min(base + kChunk, scale);
        for (std::size_t i = base; i < end; ++i) {
            const long long value = static_cast<long long>(i % 97);
            total += value * value;
        }
        counter.fetch_add(static_cast<long long>(end - base), std::memory_order_relaxed);
    }
    return total;
}
long long prime_count(std::atomic<long long> &counter, std::size_t scale)
{
    long long primes = 0;
    for (std::size_t candidate = 2; candidate < scale; ++candidate) {
        bool is_prime = true;
        for (std::size_t divisor = 2; divisor * divisor <= candidate; ++divisor) {
            if (candidate % divisor == 0) {
                is_prime = false;
                break;
            }
        }
        if (is_prime) {
            ++primes;
        }
        counter.fetch_add(1, std::memory_order_relaxed);   /* 每考察一个候选数记一笔 */
    }
    return primes;
}
long long matrix_multiply(std::atomic<long long> &counter, std::size_t scale)
{
    const std::size_t n = scale;
    std::vector<int> left(n * n);
    std::vector<int> right(n * n);
    std::vector<int> product(n * n, 0);
    for (std::size_t i = 0; i < n * n; ++i) {
        left[i] = static_cast<int>(i % 7) - 3;
        right[i] = static_cast<int>(i % 5) - 2;
    }
    for (std::size_t row = 0; row < n; ++row) {
        for (std::size_t col = 0; col < n; ++col) {
            int sum = 0;
            for (std::size_t k = 0; k < n; ++k) {
                sum += left[row * n + k] * right[k * n + col];
            }
            product[row * n + col] = sum;
        }
        counter.fetch_add(static_cast<long long>(n) * static_cast<long long>(n),
                          std::memory_order_relaxed);
    }
    long long checksum = 0;
    for (int value : product) {
        checksum += value;
    }
    return checksum;
}
const std::vector<Case> &all_cases()
{
    static const std::vector<Case> cases{
        {"sum-of-squares", "把 i*i%97 累加 scale 次", 100000, &sum_of_squares},
        {"prime-count", "试除法数出 scale 以内的素数个数", 30000, &prime_count},
        {"matrix-multiply", "两个 scale 阶方阵相乘", 64, &matrix_multiply},
    };
    return cases;
}
bool find_case(const std::string &name, const Case *&found)
{
    for (const Case &item : all_cases()) {
        if (item.name == name) {
            found = &item;
            return true;
        }
    }
    return false;
}
/* ── 统计量 ───────────────────────────────────────────── */
double percentile(const std::vector<double> &sorted_ms, double percent)
{
    if (sorted_ms.empty()) {
        return 0.0;
    }
    /* 排序后向上取整定位：n 个样本的 P90 取第 ceil(0.9n) 小的那个 */
    const double rank = percent / 100.0 * static_cast<double>(sorted_ms.size());
    std::size_t index = static_cast<std::size_t>(std::ceil(rank));
    index = (index == 0) ? 0 : index - 1;
    if (index >= sorted_ms.size()) {
        index = sorted_ms.size() - 1;
    }
    return sorted_ms[index];
}
Stats summarize(std::vector<double> samples_ms)
{
    Stats stats;
    stats.samples = samples_ms.size();
    if (samples_ms.empty()) {
        return stats;
    }
    std::sort(samples_ms.begin(), samples_ms.end());
    stats.minimum_ms = samples_ms.front();
    stats.maximum_ms = samples_ms.back();
    stats.median_ms = median_of(samples_ms);
    stats.p90_ms = percentile(samples_ms, 90.0);
    stats.p99_ms = percentile(samples_ms, 99.0);
    const double total = std::accumulate(samples_ms.begin(), samples_ms.end(), 0.0);
    stats.mean_ms = total / static_cast<double>(samples_ms.size());
    double spread = 0.0;
    for (double value : samples_ms) {
        spread += (value - stats.mean_ms) * (value - stats.mean_ms);
    }
    stats.stddev_ms = std::sqrt(spread / static_cast<double>(samples_ms.size()));
    return stats;
}
/* ── 两个时钟的分辨率对照 ─────────────────────────────── */
ClockProbe probe_clocks(std::size_t reads)
{
    ClockProbe probe;
    probe.reads = reads;
    if (reads == 0) {
        return probe;
    }
    std::vector<long long> steady_ticks;
    std::vector<long long> system_ticks;
    steady_ticks.reserve(reads);
    system_ticks.reserve(reads);
    const std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();
    for (std::size_t i = 0; i < reads; ++i) {
        steady_ticks.push_back(
            std::chrono::steady_clock::now().time_since_epoch().count());
        system_ticks.push_back(
            std::chrono::system_clock::now().time_since_epoch().count());
    }
    const std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
    probe.steady_span_ms =
        std::chrono::duration<double, std::milli>(end - begin).count();
    /* 数一数各自出现了多少个不同的值：越多说明能分辨的时间间隔越细 */
    const auto distinct = [](std::vector<long long> ticks) {
        std::sort(ticks.begin(), ticks.end());
        ticks.erase(std::unique(ticks.begin(), ticks.end()), ticks.end());
        return ticks.size();
    };
    probe.steady_distinct = distinct(steady_ticks);
    probe.system_distinct = distinct(system_ticks);
    return probe;
}
/* ── 测量 ─────────────────────────────────────────────── */
double measure_once(const Case &item, std::atomic<long long> &counter, long long &checksum)
{
    const std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();
    checksum = item.body(counter, item.scale);
    const std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
    return std::chrono::duration<double, std::milli>(end - begin).count();
}
RunResult measure_case(const Case &item, std::size_t repeats, std::size_t warmups)
{
    RunResult result;
    result.name = item.name;
    result.title = item.title;
    result.repeats = (repeats == 0) ? 1 : repeats;
    result.warmups = warmups;
    std::atomic<long long> counter{0};
    long long checksum = 0;
    /* 只测一次：这一次数值就是「单次测量」，报表里拿它和分位数对照 */
    result.single_ms = measure_once(item, counter, checksum);
    result.iterations = counter.load();
    counter.store(0);
    /* 预热：结果丢掉。第一轮要把代码与数据带进缓存、把页换进来 */
    for (std::size_t i = 0; i < warmups; ++i) {
        (void)measure_once(item, counter, checksum);
        counter.store(0);
    }
    std::vector<double> samples(result.repeats);
    for (std::size_t i = 0; i < result.repeats; ++i) {
        samples[i] = measure_once(item, counter, checksum);
        counter.store(0);
    }
    result.checksum = checksum;
    result.first_ms = samples.front();
    result.stats = summarize(samples);
    return result;
}
/* ── 报表 ─────────────────────────────────────────────── */
Report make_report(const Options &options)
{
    Report report;
    report.options = options;
    report.clocks = probe_clocks(1000);
    for (const Case &item : all_cases()) {
        if (!options.cases.empty()
            && std::find(options.cases.begin(), options.cases.end(), item.name)
                   == options.cases.end()) {
            continue;
        }
        report.runs.push_back(measure_case(item, options.repeats, options.warmups));
    }
    const ClockProbe &clocks = report.clocks;
    std::ostringstream method;
    method << "计时口径\n"
           << "  时钟：用 std::chrono::steady_clock，它单调，两次读数相减不会为负；\n"
           << "        system_clock 是对过时的墙上时间，对时会让它往回跳，因此不用它计时。\n"
           << "  分辨率：连续读取 " << clocks.reads << " 次，steady_clock 出现 "
           << clocks.steady_distinct << " 个不同的值，\n"
           << "        system_clock 出现 " << clocks.system_distinct << " 个，这一串读取跨了 "
           << ms_text(clocks.steady_span_ms) << " 毫秒，\n"
           << "        两者测不出差别：选 steady_clock 的理由是单调性，不是分辨率。\n"
           << "  预热：先跑 " << options.warmups << " 轮，结果丢掉。第一轮要把代码与数据带进缓存，\n"
           << "        比后面每一轮都慢，混进统计量会把整体抬高。\n"
           << "  重复：正式测 " << options.repeats << " 轮，报最小值、中位数与 P90、P99。\n"
           << "  一次测量为什么不可信：它只是「这一次」的快慢。同一段代码在不同时刻测，\n"
           << "        结果能差出几倍，下表每一行的「单次」与「最大」就是例子。\n"
           << "        分位数把「最好能到多少」（最小值）与「通常是多少」（中位数）分开，\n"
           << "        P90、P99 说明最坏的一成、百分之一有多坏。\n";
    report.method_text = method.str();
    std::ostringstream table;
    table << "单次测量与多次测量的对照（单位：毫秒）\n"
          << "  " << std::left << std::setw(18) << "用例" << std::right
          << std::setw(9) << "单次" << std::setw(9) << "第1次" << std::setw(9) << "最小"
          << std::setw(9) << "中位" << std::setw(9) << "P90" << std::setw(9) << "P99"
          << std::setw(9) << "最大" << std::setw(9) << "均值" << std::setw(9) << "标准差"
          << "\n";
    for (const RunResult &run : report.runs) {
        const Stats &stats = run.stats;
        table << "  " << std::left << std::setw(18) << run.name << std::right
              << ms_text(run.single_ms) << ms_text(run.first_ms)
              << ms_text(stats.minimum_ms) << ms_text(stats.median_ms)
              << ms_text(stats.p90_ms) << ms_text(stats.p99_ms)
              << ms_text(stats.maximum_ms) << ms_text(stats.mean_ms)
              << ms_text(stats.stddev_ms) << "\n";
    }
    report.table_text = table.str();
    std::ostringstream detail;
    detail << "每个用例的口径（repeats=" << options.repeats
           << "，warmups=" << options.warmups << "）\n";
    for (const RunResult &run : report.runs) {
        const Stats &stats = run.stats;
        const double ratio =
            (stats.median_ms > 0.0) ? (run.single_ms / stats.median_ms) : 0.0;
        detail << "  " << run.name << "：" << run.title << "\n"
               << "    一轮迭代 " << run.iterations << " 次（std::atomic 计数器给出），校验和 "
               << run.checksum << "\n"
               << "    多次测量 " << stats.samples << " 个样本：最小 " << ms_text(stats.minimum_ms)
               << " / 中位 " << ms_text(stats.median_ms) << " / P90 " << ms_text(stats.p90_ms)
               << " / P99 " << ms_text(stats.p99_ms) << " / 最大 " << ms_text(stats.maximum_ms)
               << "\n"
               << "    单次 " << ms_text(run.single_ms) << " 与中位数之比 "
               << std::fixed << std::setprecision(2) << ratio << "\n";
    }
    report.detail_text = detail.str();
    return report;
}

/* ── 自测 ─────────────────────────────────────────────── */
CheckResult run_self_tests()
{
    Checker checker;
    /* 1 至 7：统计量。全部用写死的一小组数，不碰真实耗时 */
    const std::vector<double> odd{1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0};
    const Stats odd_stats = summarize(odd);
    checker.check(odd_stats.samples == 9, "summarize 的样本数等于输入的个数",
                  std::to_string(odd_stats.samples) + " / 9");
    checker.check(odd_stats.minimum_ms == 1.0 && odd_stats.maximum_ms == 9.0,
                  "最小值与最大值等于排序后的首尾");
    checker.check(odd_stats.median_ms == 5.0
                      && summarize(std::vector<double>{1.0, 2.0, 3.0, 4.0}).median_ms == 2.5,
                  "中位数：奇数个样本取中间那个，偶数个取中间两个的平均");
    const Stats single_stats = summarize(std::vector<double>{7.5});
    checker.check(single_stats.minimum_ms == 7.5 && single_stats.maximum_ms == 7.5
                      && single_stats.median_ms == 7.5 && single_stats.mean_ms == 7.5
                      && single_stats.p90_ms == 7.5 && single_stats.p99_ms == 7.5
                      && single_stats.stddev_ms == 0.0,
                  "只有一个样本时除了标准差为 0，其余统计量都等于它自己");
    checker.check(odd_stats.p90_ms >= odd_stats.minimum_ms
                      && odd_stats.p90_ms <= odd_stats.maximum_ms
                      && odd_stats.p99_ms >= odd_stats.p90_ms
                      && odd_stats.p99_ms <= odd_stats.maximum_ms,
                  "P90 与 P99 落在 [最小值, 最大值] 内，且 P99 不小于 P90");
    checker.check(std::fabs(odd_stats.mean_ms - 5.0) < 1e-12, "均值等于算术平均");
    const Stats pair_stats = summarize(std::vector<double>{2.0, 4.0});
    checker.check(std::fabs(pair_stats.stddev_ms - 1.0) < 1e-12,
                  "标准差是总体标准差：{2, 4} 的是 1",
                  std::to_string(pair_stats.stddev_ms) + " / 1");
    /* 8、9：用例表 */
    const std::vector<Case> &cases = all_cases();
    bool names_unique = true;
    for (std::size_t i = 0; i < cases.size(); ++i) {
        for (std::size_t j = i + 1; j < cases.size(); ++j) {
            names_unique = names_unique && cases[i].name != cases[j].name;
        }
    }
    checker.check(cases.size() == 3 && names_unique, "用例表里有三个用例，名字互不相同",
                  std::to_string(cases.size()) + " 个");
    const Case *found = nullptr;
    checker.check(find_case("sum-of-squares", found) && find_case("prime-count", found)
                      && find_case("matrix-multiply", found)
                      && !find_case("no-such-case", found),
                  "按名字能找到三个用例，未知名字返回 false");
    /* 10 至 12：纯计算与 atomic 计数 */
    std::atomic<long long> counter{0};
    const long long sum_a = sum_of_squares(counter, 1000);
    const long long sum_b = sum_of_squares(counter, 1000);
    checker.check(sum_a == sum_b && counter.load() == 2000,
                  "sum_of_squares 可复现，且把每轮 1000 次迭代记进了计数器",
                  std::to_string(counter.load()) + " / 2000");
    counter.store(0);
    const long long primes = prime_count(counter, 1000);
    checker.check(counter.load() == 998 && primes == 168,
                  "prime_count 考察 998 个候选数，数出 1000 以内的 168 个素数",
                  std::to_string(counter.load()) + " / 998");
    counter.store(0);
    (void)matrix_multiply(counter, 8);
    checker.check(counter.load() == 512,
                  "matrix_multiply 的迭代次数等于阶数的三次方（8^3 = 512）",
                  std::to_string(counter.load()) + " / 512");
    /* 13、14：测量框架的结构与计数，不断言任何耗时数值 */
    counter.store(0);
    long long checksum = 0;
    const Case &first_case = cases.front();
    const double once_ms = measure_once(first_case, counter, checksum);
    checker.check(once_ms >= 0.0 && counter.load() == static_cast<long long>(first_case.scale),
                  "measure_once 返回非负的毫秒数，计数器恰好增加一轮的迭代次数",
                  std::to_string(counter.load()));
    const RunResult run = measure_case(first_case, 5, 2);
    checker.check(run.stats.samples == 5 && run.repeats == 5 && run.warmups == 2
                      && run.iterations == static_cast<long long>(first_case.scale)
                      && run.first_ms >= 0.0 && run.single_ms >= 0.0,
                  "measure_case 按 repeats 出样本，迭代次数与单轮一致",
                  std::to_string(run.stats.samples));
    const ClockProbe probe = probe_clocks(64);
    checker.check(probe.reads == 64 && probe.steady_distinct >= 1
                      && probe.steady_distinct <= 64 && probe.system_distinct >= 1
                      && probe.system_distinct <= 64,
                  "probe_clocks 读回 64 对时间戳，两组不同值个数都在 1 到 64 之间",
                  std::to_string(probe.steady_distinct) + " / "
                      + std::to_string(probe.system_distinct));
    return checker.take();
}
}   /* namespace bench */
