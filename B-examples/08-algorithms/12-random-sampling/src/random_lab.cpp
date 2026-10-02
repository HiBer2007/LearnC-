/**
 * random_lab.cpp —— 随机与采样：取模偏差、Fisher-Yates、蓄水池与加权抽样
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

#include "random_lab.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

namespace rlab {

/* ================= 发动机 ================= */

CrandEngine::CrandEngine(std::uint32_t seed)
{
    std::srand(seed);
}

CrandEngine::result_type CrandEngine::operator()()
{
    ++calls_;
    return static_cast<result_type>(std::rand());
}

void CrandEngine::discard(unsigned long long z)
{
    for (unsigned long long i = 0; i < z; ++i) {
        (void)(*this)();
    }
}

std::uint64_t rand_range_size()
{
    return static_cast<std::uint64_t>(RAND_MAX) + 1;
}

/* ================= 一、两个发生器 ================= */

EngineSample sample_crand(std::uint32_t seed, std::size_t count)
{
    CrandEngine engine(seed);
    EngineSample sample;
    sample.seed = seed;
    sample.values.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        sample.values.push_back(engine());
    }
    sample.calls = engine.calls();
    return sample;
}

EngineSample sample_mt19937(std::uint32_t seed, std::size_t count)
{
    CountingEngine engine(seed);
    EngineSample sample;
    sample.seed = seed;
    sample.values.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        sample.values.push_back(engine());
    }
    sample.calls = engine.calls();
    return sample;
}

/* ================= 二、取模偏差 ================= */

long long modulo_quotient(int buckets)
{
    return static_cast<long long>(rand_range_size() / static_cast<std::uint64_t>(buckets));
}

long long modulo_remainder(int buckets)
{
    return static_cast<long long>(rand_range_size() % static_cast<std::uint64_t>(buckets));
}

std::uint64_t modulo_preimages(int buckets, int bucket)
{
    const std::uint64_t total = rand_range_size();
    const std::uint64_t n = static_cast<std::uint64_t>(buckets);
    const std::uint64_t quotient = total / n;
    const std::uint64_t remainder = total % n;
    return quotient + (static_cast<std::uint64_t>(bucket) < remainder ? 1u : 0u);
}

long long modulo_bias_ppm(int buckets, int bucket)
{
    const long long pre = static_cast<long long>(modulo_preimages(buckets, bucket));
    const long long n = buckets;
    const long long total = static_cast<long long>(rand_range_size());
    return (pre * n - total) * 1000000 / total;
}

long long deviation_ppm(std::uint64_t count, std::uint64_t trials, int buckets)
{
    return deviation_ratio_ppm(count, trials, 1, static_cast<std::uint64_t>(buckets));
}

long long deviation_ratio_ppm(std::uint64_t count, std::uint64_t trials,
                              std::uint64_t numerator, std::uint64_t denominator)
{
    const long long got = static_cast<long long>(count) * static_cast<long long>(denominator);
    const long long want = static_cast<long long>(trials) * static_cast<long long>(numerator);
    return (got - want) * 1000000 / want;
}

std::uint64_t preimages_min(const BucketStats &stats)
{
    if (stats.preimages.empty()) {
        return 0;
    }
    return *std::min_element(stats.preimages.begin(), stats.preimages.end());
}

std::uint64_t preimages_max(const BucketStats &stats)
{
    if (stats.preimages.empty()) {
        return 0;
    }
    return *std::max_element(stats.preimages.begin(), stats.preimages.end());
}

BucketStats modulo_buckets(int buckets, std::uint64_t trials, std::uint32_t seed)
{
    CrandEngine engine(seed);
    BucketStats stats;
    stats.buckets = buckets;
    stats.trials = trials;
    stats.observed.assign(static_cast<std::size_t>(buckets), 0);
    stats.preimages.resize(static_cast<std::size_t>(buckets));
    for (int b = 0; b < buckets; ++b) {
        stats.preimages[static_cast<std::size_t>(b)] = modulo_preimages(buckets, b);
    }
    for (std::uint64_t t = 0; t < trials; ++t) {
        const std::uint32_t value = engine();       /* [0, RAND_MAX] */
        ++stats.observed[static_cast<std::size_t>(value % static_cast<std::uint32_t>(buckets))];
    }
    stats.engine_calls = engine.calls();
    stats.rejects = 0;
    return stats;
}

BucketStats rejection_buckets(int buckets, std::uint64_t trials, std::uint32_t seed)
{
    CrandEngine engine(seed);
    BucketStats stats;
    stats.buckets = buckets;
    stats.trials = trials;
    stats.observed.assign(static_cast<std::size_t>(buckets), 0);

    /* 把取值域截到 n 的整数倍：可接受的取值个数是 n 的整数倍，
       因此每个桶分到的取值个数完全相同 */
    const std::uint64_t range = rand_range_size();
    const std::uint64_t limit = range - range % static_cast<std::uint64_t>(buckets);
    stats.preimages.assign(static_cast<std::size_t>(buckets),
                           limit / static_cast<std::uint64_t>(buckets));

    std::uint64_t rejects = 0;
    for (std::uint64_t t = 0; t < trials; ++t) {
        std::uint64_t value = 0;
        do {
            value = engine();
            if (value >= limit) {
                ++rejects;
            }
        } while (value >= limit);
        ++stats.observed[static_cast<std::size_t>(value % static_cast<std::uint64_t>(buckets))];
    }
    stats.engine_calls = engine.calls();
    stats.rejects = rejects;
    return stats;
}

BucketStats distribution_buckets(int buckets, std::uint64_t trials, std::uint32_t seed)
{
    CountingEngine engine(seed);
    std::uniform_int_distribution<int> dist(0, buckets - 1);
    BucketStats stats;
    stats.buckets = buckets;
    stats.trials = trials;
    stats.observed.assign(static_cast<std::size_t>(buckets), 0);
    for (std::uint64_t t = 0; t < trials; ++t) {
        ++stats.observed[static_cast<std::size_t>(dist(engine))];
    }
    stats.engine_calls = engine.calls();
    stats.rejects = 0;
    /* uniform_int_distribution 内部按自己的算法取值，取值域计数没有意义，留空 */
    return stats;
}

/* ================= 三、洗牌 ================= */

std::size_t factorial_size(std::size_t n)
{
    std::size_t result = 1;
    for (std::size_t i = 2; i <= n; ++i) {
        result *= i;
    }
    return result;
}

std::size_t permutation_rank(const std::vector<int> &perm)
{
    /* 逆序数法：第 i 位的系数是「它后面比它小的元素个数」 */
    const std::size_t n = perm.size();
    std::size_t rank = 0;
    for (std::size_t i = 0; i < n; ++i) {
        std::size_t smaller = 0;
        for (std::size_t k = i + 1; k < n; ++k) {
            if (perm[k] < perm[i]) {
                ++smaller;
            }
        }
        rank = rank * (n - i) + smaller;
    }
    return rank;
}

std::vector<int> permutation_at(std::size_t rank, int n)
{
    std::vector<int> available(static_cast<std::size_t>(n));
    for (int i = 0; i < n; ++i) {
        available[static_cast<std::size_t>(i)] = i;
    }
    std::vector<int> perm;
    perm.reserve(static_cast<std::size_t>(n));
    for (int i = n - 1; i >= 0; --i) {
        const std::size_t fact = factorial_size(static_cast<std::size_t>(i));
        const std::size_t index = rank / fact;
        rank %= fact;
        perm.push_back(available[index]);
        available.erase(available.begin() + static_cast<std::ptrdiff_t>(index));
    }
    return perm;
}

std::string permutation_text(const std::vector<int> &perm)
{
    std::ostringstream os;
    for (std::size_t i = 0; i < perm.size(); ++i) {
        if (i != 0) {
            os << ' ';
        }
        os << perm[i];
    }
    return os.str();
}

namespace {

/** 洗牌：correct 为真时走正确版，否则走「每张都与任意一张交换」的错误版。
    count 是「第几步」的意思：正确版从 n - 1 往 1 走，错误版从 0 往 n - 1 走 */
void walk_shuffle(std::vector<int> &a, int n, bool correct, CountingEngine &engine)
{
    if (correct) {
        for (int i = n - 1; i > 0; --i) {
            const std::uint32_t j = uniform_below(engine, static_cast<std::uint32_t>(i + 1));
            std::swap(a[static_cast<std::size_t>(i)], a[static_cast<std::size_t>(j)]);
        }
    } else {
        for (int i = 0; i < n; ++i) {
            const std::uint32_t j = uniform_below(engine, static_cast<std::uint32_t>(n));
            std::swap(a[static_cast<std::size_t>(i)], a[static_cast<std::size_t>(j)]);
        }
    }
}

std::vector<std::uint64_t> shuffle_counts(int n, std::uint64_t trials, std::uint32_t seed,
                                          bool correct, std::uint64_t *calls)
{
    CountingEngine engine(seed);
    std::vector<std::uint64_t> counts(factorial_size(static_cast<std::size_t>(n)), 0);
    std::vector<int> a(static_cast<std::size_t>(n));
    for (std::uint64_t t = 0; t < trials; ++t) {
        for (int i = 0; i < n; ++i) {
            a[static_cast<std::size_t>(i)] = i;
        }
        walk_shuffle(a, n, correct, engine);
        ++counts[permutation_rank(a)];
    }
    *calls = engine.calls();
    return counts;
}

/** 枚举全部随机路径：正确版每步的选择个数是 n, n-1, …, 2，
    错误版每步都是 n。一个排列分到几条路径，这里数得清清楚楚 */
void enumerate_paths(int n, bool correct, int step, std::vector<int> &a,
                     std::vector<std::uint64_t> &paths)
{
    if (correct) {
        const int i = n - 1 - step;
        if (i <= 0) {
            ++paths[permutation_rank(a)];
            return;
        }
        for (int j = 0; j <= i; ++j) {
            std::swap(a[static_cast<std::size_t>(i)], a[static_cast<std::size_t>(j)]);
            enumerate_paths(n, correct, step + 1, a, paths);
            std::swap(a[static_cast<std::size_t>(i)], a[static_cast<std::size_t>(j)]);
        }
    } else {
        const int i = step;
        if (i >= n) {
            ++paths[permutation_rank(a)];
            return;
        }
        for (int j = 0; j < n; ++j) {
            std::swap(a[static_cast<std::size_t>(i)], a[static_cast<std::size_t>(j)]);
            enumerate_paths(n, correct, step + 1, a, paths);
            std::swap(a[static_cast<std::size_t>(i)], a[static_cast<std::size_t>(j)]);
        }
    }
}

std::vector<std::uint64_t> shuffle_exact_paths(int n, bool correct)
{
    std::vector<std::uint64_t> paths(factorial_size(static_cast<std::size_t>(n)), 0);
    std::vector<int> a(static_cast<std::size_t>(n));
    for (int i = 0; i < n; ++i) {
        a[static_cast<std::size_t>(i)] = i;
    }
    enumerate_paths(n, correct, 0, a, paths);
    return paths;
}

/** 卡方统计量，单位 0.001：Σ (观察 - 期望)² / 期望，全程整数运算 */
long long chi2_milli_of(const std::vector<std::uint64_t> &counts, std::uint64_t trials)
{
    const long long f = static_cast<long long>(counts.size());
    const long long total = static_cast<long long>(trials);
    long long sum = 0;
    for (const std::uint64_t count : counts) {
        const long long diff = static_cast<long long>(count) * f - total;
        sum += diff * diff * 1000 / (f * total);
    }
    return sum;
}

/** 加权版本的卡方：每档的期望是 trials × w_i / 权重和，不是均分 */
long long chi2_weighted_milli(const std::vector<std::uint64_t> &counts,
                              const std::vector<long long> &weights, std::uint64_t trials)
{
    long long total = 0;
    for (const long long w : weights) {
        total += w;
    }
    long long sum = 0;
    for (std::size_t i = 0; i < counts.size(); ++i) {
        const long long diff = static_cast<long long>(counts[i]) * total -
                               static_cast<long long>(trials) * weights[i];
        const long long denominator =
            static_cast<long long>(trials) * weights[i] * total;
        sum += diff * diff * 1000 / denominator;
    }
    return sum;
}

ShuffleStats make_shuffle_stats(int n, std::uint64_t trials, std::uint32_t seed, bool correct)
{
    ShuffleStats stats;
    stats.n = n;
    stats.trials = trials;
    stats.observed = shuffle_counts(n, trials, seed, correct, &stats.engine_calls);
    stats.exact_paths = shuffle_exact_paths(n, correct);
    stats.path_total = 0;
    for (const std::uint64_t paths : stats.exact_paths) {
        stats.path_total += paths;
    }
    stats.chi2_milli = chi2_milli_of(stats.observed, trials);
    stats.min_count = stats.observed.front();
    stats.max_count = stats.observed.front();
    stats.min_rank = 0;
    stats.max_rank = 0;
    for (std::size_t i = 1; i < stats.observed.size(); ++i) {
        if (stats.observed[i] < stats.min_count) {
            stats.min_count = stats.observed[i];
            stats.min_rank = i;
        }
        if (stats.observed[i] > stats.max_count) {
            stats.max_count = stats.observed[i];
            stats.max_rank = i;
        }
    }
    return stats;
}

}   /* namespace */

ShuffleStats shuffle_fisher_yates(int n, std::uint64_t trials, std::uint32_t seed)
{
    return make_shuffle_stats(n, trials, seed, true);
}

ShuffleStats shuffle_naive(int n, std::uint64_t trials, std::uint32_t seed)
{
    return make_shuffle_stats(n, trials, seed, false);
}

/* ================= 四、蓄水池抽样 ================= */

ReservoirRun reservoir_once(int stream_size, int k, std::uint32_t seed)
{
    CountingEngine engine(seed);
    ReservoirRun run;
    run.stream_size = stream_size;
    run.k = k;
    std::vector<int> reservoir(static_cast<std::size_t>(k));
    for (int i = 0; i < k; ++i) {
        reservoir[static_cast<std::size_t>(i)] = i;
    }
    for (int i = k; i < stream_size; ++i) {
        const std::uint32_t j = uniform_below(engine, static_cast<std::uint32_t>(i + 1));
        if (static_cast<int>(j) < k) {
            run.replaced.push_back(reservoir[static_cast<std::size_t>(j)]);
            reservoir[static_cast<std::size_t>(j)] = i;
        }
    }
    run.picked = reservoir;
    run.replacements = run.replaced.size();
    run.engine_calls = engine.calls();
    return run;
}

ReservoirStats reservoir_repeat(int stream_size, int k, std::uint64_t rounds, std::uint32_t seed)
{
    CountingEngine engine(seed);
    ReservoirStats stats;
    stats.stream_size = stream_size;
    stats.k = k;
    stats.rounds = rounds;
    stats.hits.assign(static_cast<std::size_t>(stream_size), 0);
    std::vector<int> reservoir(static_cast<std::size_t>(k));
    for (std::uint64_t t = 0; t < rounds; ++t) {
        for (int i = 0; i < k; ++i) {
            reservoir[static_cast<std::size_t>(i)] = i;
        }
        for (int i = k; i < stream_size; ++i) {
            const std::uint32_t j = uniform_below(engine, static_cast<std::uint32_t>(i + 1));
            if (static_cast<int>(j) < k) {
                reservoir[static_cast<std::size_t>(j)] = i;
            }
        }
        for (int s = 0; s < k; ++s) {
            ++stats.hits[static_cast<std::size_t>(reservoir[static_cast<std::size_t>(s)])];
        }
    }
    stats.engine_calls = engine.calls();
    return stats;
}

namespace {

void reservoir_enumerate(int n, int k, int i, std::vector<int> &reservoir,
                         std::vector<std::uint64_t> &hits, std::uint64_t &paths)
{
    if (i >= n) {
        ++paths;
        for (int s = 0; s < k; ++s) {
            ++hits[static_cast<std::size_t>(reservoir[static_cast<std::size_t>(s)])];
        }
        return;
    }
    for (int j = 0; j <= i; ++j) {
        if (j < k) {
            const int old = reservoir[static_cast<std::size_t>(j)];
            reservoir[static_cast<std::size_t>(j)] = i;
            reservoir_enumerate(n, k, i + 1, reservoir, hits, paths);
            reservoir[static_cast<std::size_t>(j)] = old;
        } else {
            reservoir_enumerate(n, k, i + 1, reservoir, hits, paths);
        }
    }
}

}   /* namespace */

std::vector<std::uint64_t> reservoir_exact_hits(int n, int k, std::uint64_t *path_total)
{
    std::vector<std::uint64_t> hits(static_cast<std::size_t>(n), 0);
    std::vector<int> reservoir(static_cast<std::size_t>(k));
    for (int i = 0; i < k; ++i) {
        reservoir[static_cast<std::size_t>(i)] = i;
    }
    std::uint64_t paths = 0;
    reservoir_enumerate(n, k, k, reservoir, hits, paths);
    if (path_total != nullptr) {
        *path_total = paths;
    }
    return hits;
}

/* ================= 五、加权抽样 ================= */

AliasTable build_alias_table(const std::vector<long long> &weights)
{
    const std::size_t n = weights.size();
    AliasTable table;
    table.weights = weights;
    table.total = 0;
    for (const long long w : weights) {
        table.total += w;
    }
    table.prob_num.assign(n, 0);
    table.alias.assign(n, -1);

    /* 单位统一取「权重和」：第 i 列的总权重是 权重和，直接部分与别名部分都是整数 */
    std::vector<long long> scaled(n, 0);
    std::vector<std::size_t> small;
    std::vector<std::size_t> large;
    for (std::size_t i = 0; i < n; ++i) {
        scaled[i] = weights[i] * static_cast<long long>(n);
        if (scaled[i] < table.total) {
            small.push_back(i);
        } else {
            large.push_back(i);
        }
    }
    while (!small.empty() && !large.empty()) {
        const std::size_t s = small.back();
        small.pop_back();
        const std::size_t l = large.back();
        large.pop_back();
        table.prob_num[s] = scaled[s];
        table.alias[s] = static_cast<int>(l);
        scaled[l] -= (table.total - scaled[s]);
        if (scaled[l] < table.total) {
            small.push_back(l);
        } else {
            large.push_back(l);
        }
    }
    /* 循环结束时剩下的列整列都归自己：整数运算下它们的值恰好等于权重和 */
    for (const std::size_t i : small) {
        table.prob_num[i] = scaled[i];
    }
    for (const std::size_t i : large) {
        table.prob_num[i] = scaled[i];
    }
    return table;
}

std::size_t alias_identity_failures(const AliasTable &table)
{
    const std::size_t n = table.prob_num.size();
    std::size_t failures = 0;
    for (std::size_t i = 0; i < n; ++i) {
        long long total = table.prob_num[i];
        for (std::size_t j = 0; j < n; ++j) {
            if (table.alias[j] == static_cast<int>(i)) {
                total += table.total - table.prob_num[j];
            }
        }
        if (total != static_cast<long long>(n) * table.weights[i]) {
            ++failures;
        }
    }
    return failures;
}

int prefix_interval(const std::vector<long long> &prefix, long long value,
                    std::uint64_t *comparisons)
{
    /* prefix 的长度是 n + 1，prefix[0] = 0；
       要找出满足 prefix[i] <= value < prefix[i + 1] 的那个 i */
    std::size_t lo = 0;
    std::size_t hi = prefix.size() - 2;
    while (lo < hi) {
        if (comparisons != nullptr) {
            ++*comparisons;
        }
        const std::size_t mid = lo + (hi - lo + 1) / 2;
        if (prefix[mid] <= value) {
            lo = mid;
        } else {
            hi = mid - 1;
        }
    }
    return static_cast<int>(lo);
}

WeightedStats sample_prefix_binary(const std::vector<long long> &weights, std::uint64_t trials,
                                   std::uint32_t seed)
{
    CountingEngine engine(seed);
    WeightedStats stats;
    stats.weights = weights;
    stats.trials = trials;
    stats.observed.assign(weights.size(), 0);
    std::vector<long long> prefix(weights.size() + 1, 0);
    for (std::size_t i = 0; i < weights.size(); ++i) {
        prefix[i + 1] = prefix[i] + weights[i];
    }
    const std::uint32_t total = static_cast<std::uint32_t>(prefix.back());
    for (std::uint64_t t = 0; t < trials; ++t) {
        const std::uint32_t value = uniform_below(engine, total);
        const int index = prefix_interval(prefix, static_cast<long long>(value),
                                         &stats.comparisons);
        ++stats.observed[static_cast<std::size_t>(index)];
    }
    stats.engine_calls = engine.calls();
    return stats;
}

WeightedStats sample_alias(const std::vector<long long> &weights, std::uint64_t trials,
                           std::uint32_t seed)
{
    const AliasTable table = build_alias_table(weights);
    CountingEngine engine(seed);
    WeightedStats stats;
    stats.weights = weights;
    stats.trials = trials;
    stats.observed.assign(weights.size(), 0);
    for (std::uint64_t t = 0; t < trials; ++t) {
        const std::uint32_t column = uniform_below(engine, static_cast<std::uint32_t>(weights.size()));
        const std::uint32_t coin = uniform_below(engine, static_cast<std::uint32_t>(table.total));
        const std::size_t index =
            (static_cast<long long>(coin) < table.prob_num[column])
                ? static_cast<std::size_t>(column)
                : static_cast<std::size_t>(table.alias[column]);
        ++stats.observed[index];
    }
    stats.engine_calls = engine.calls();
    return stats;
}

/* ================= 报告的排版工具 ================= */

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

std::string pad_left(const std::string &text, std::size_t width)
{
    const std::size_t shown = display_width(text);
    if (shown >= width) {
        return text;
    }
    return std::string(width - shown, ' ') + text;
}

/** 定点打印：value 的单位是 10 的负 scale 次方 */
std::string format_scaled(long long value, int scale)
{
    long long unit = 1;
    for (int i = 0; i < scale; ++i) {
        unit *= 10;
    }
    const bool negative = value < 0;
    const unsigned long long magnitude =
        negative ? static_cast<unsigned long long>(-(value + 1)) + 1ULL
                 : static_cast<unsigned long long>(value);
    std::ostringstream os;
    if (negative) {
        os << '-';
    }
    os << magnitude / static_cast<unsigned long long>(unit) << '.'
       << std::setw(scale) << std::setfill('0')
       << magnitude % static_cast<unsigned long long>(unit);
    return os.str();
}

/** 四舍五入的整数除法，供折成百分数时用 */
long long divide_round(long long value, long long divisor)
{
    if (value >= 0) {
        return (value + divisor / 2) / divisor;
    }
    return -((-value + divisor / 2) / divisor);
}

std::string join_numbers(const std::vector<std::uint32_t> &values)
{
    std::ostringstream os;
    for (std::size_t i = 0; i < values.size(); ++i) {
        if (i != 0) {
            os << ' ';
        }
        os << values[i];
    }
    return os.str();
}

std::string join_ints(const std::vector<int> &values)
{
    std::ostringstream os;
    for (std::size_t i = 0; i < values.size(); ++i) {
        if (i != 0) {
            os << ' ';
        }
        os << values[i];
    }
    return os.str();
}

std::uint64_t sum_of(const std::vector<std::uint64_t> &values)
{
    std::uint64_t sum = 0;
    for (const std::uint64_t v : values) {
        sum += v;
    }
    return sum;
}

/** 一段桶频次统计里，某个桶范围上的平均频次，单位 0.001，截断 */
long long average_milli(const std::vector<std::uint64_t> &observed, int from, int count)
{
    std::uint64_t sum = 0;
    for (int b = from; b < from + count; ++b) {
        sum += observed[static_cast<std::size_t>(b)];
    }
    return static_cast<long long>(sum) * 1000 / count;
}

}   /* namespace */

std::string format_ppm(long long ppm)
{
    if (ppm > 0) {
        return "+" + std::to_string(ppm);
    }
    return std::to_string(ppm);
}

std::string format_ppm_percent(long long ppm)
{
    return format_scaled(divide_round(ppm, 10), 3) + "%";
}

std::string format_milli(long long milli)
{
    return format_scaled(milli, 3);
}

std::string format_permille_percent(long long permille)
{
    return format_scaled(permille, 1) + "%";
}

/* ================= 报告 ================= */

namespace {

const std::uint32_t kSeed = 20261002u;
const std::uint64_t kBucketTrials = 200000;
const std::uint64_t kShuffleFourTrials = 240000;
const std::uint64_t kShuffleFiveTrials = 120000;
const int kStreamSize = 20;
const int kReservoirK = 5;
const std::uint64_t kReservoirRounds = 20000;
const std::uint64_t kWeightedTrials = 200000;

void append_engines(std::ostringstream &os)
{
    const EngineSample crand = sample_crand(kSeed, 8);
    const EngineSample mt = sample_mt19937(kSeed, 8);

    os << u8"一、两个伪随机数发生器：取值域与序列\n";
    os << "  RAND_MAX = " << RAND_MAX << u8"：rand() 的取值域是 [0, " << RAND_MAX
       << u8"]，一共 " << rand_range_size() << u8" 个值\n";
    os << u8"  std::mt19937 的取值域是 [0, 4294967295]，一共 4294967296 个值，周期 2^19937 - 1\n";
    os << u8"  固定种子 " << kSeed << u8"，两个发生器各取 8 个值：\n";
    os << "    rand()   " << join_numbers(crand.values) << "\n";
    os << "    mt19937  " << join_numbers(mt.values) << "\n";
    os << u8"  两个序列都逐位可复现，但可复现的范围不同：\n";
    os << u8"    rand() 的序列标准没有规定，由标准库的实现决定（本机是 MinGW-w64 的 libstdc++，rand() 落在 Windows CRT 上）\n";
    os << u8"    std::mt19937 的序列由标准规定，换一个标准库也是这 8 个数（本机另用 MSVC 的 STL 对照过，逐位相同）\n";
    os << u8"  本段随机数调用：8 次 rand()，8 次 mt19937\n";
    os << "\n";
}

void append_bias_table(std::ostringstream &os)
{
    os << "  " << pad_right(u8"n", 9) << pad_left(u8"商 q", 8) << pad_left(u8"余 r", 8)
       << pad_left(u8"加成桶理论偏差", 16) << pad_left(u8"其余桶理论偏差", 16) << "\n";
    const int sizes[] = {6, 10, 100, 1000, 20000};
    for (const int n : sizes) {
        const long long remainder = modulo_remainder(n);
        os << "  " << pad_right(std::to_string(n), 9) << pad_left(std::to_string(modulo_quotient(n)), 8)
           << pad_left(std::to_string(remainder), 8)
           << pad_left(format_ppm(modulo_bias_ppm(n, 0)), 16)
           << pad_left(format_ppm(modulo_bias_ppm(n, n - 1)), 16) << "\n";
    }
}

void append_bucket_small(std::ostringstream &os)
{
    const int n = 10;
    const BucketStats mod = modulo_buckets(n, kBucketTrials, kSeed);
    const BucketStats rej = rejection_buckets(n, kBucketTrials, kSeed);
    const BucketStats dis = distribution_buckets(n, kBucketTrials, kSeed);

    os << u8"  n = 10，固定种子 " << kSeed << u8"，三种做法各 " << kBucketTrials
       << u8" 次抽样（期望每个桶 " << (kBucketTrials / n) << u8" 次）\n";
    os << "  " << pad_right(u8"桶", 6) << pad_left(u8"取值域计数", 12) << pad_left(u8"取模频次", 12)
       << pad_left(u8"取模偏差", 12) << pad_left(u8"拒绝频次", 12) << pad_left(u8"拒绝偏差", 12)
       << pad_left(u8"分布频次", 12) << pad_left(u8"分布偏差", 12) << "\n";
    for (int b = 0; b < n; ++b) {
        const std::size_t i = static_cast<std::size_t>(b);
        os << "  " << pad_right(std::to_string(b), 6)
           << pad_left(std::to_string(mod.preimages[i]), 12)
           << pad_left(std::to_string(mod.observed[i]), 12)
           << pad_left(format_ppm(deviation_ppm(mod.observed[i], kBucketTrials, n)), 12)
           << pad_left(std::to_string(rej.observed[i]), 12)
           << pad_left(format_ppm(deviation_ppm(rej.observed[i], kBucketTrials, n)), 12)
           << pad_left(std::to_string(dis.observed[i]), 12)
           << pad_left(format_ppm(deviation_ppm(dis.observed[i], kBucketTrials, n)), 12) << "\n";
    }
    os << "  " << pad_right(u8"合计", 6) << pad_left(std::to_string(rand_range_size()), 12)
       << pad_left(std::to_string(sum_of(mod.observed)), 12) << pad_left("0", 12)
       << pad_left(std::to_string(sum_of(rej.observed)), 12) << pad_left("0", 12)
       << pad_left(std::to_string(sum_of(dis.observed)), 12) << pad_left("0", 12) << "\n";
    os << u8"    取值域计数：rand() 的 " << rand_range_size()
       << u8" 个取值里有几个会落到这个桶上，是精确值，不是抽出来的\n";
    os << u8"    取模法：桶 0 到 " << (modulo_remainder(n) - 1) << u8" 是 "
       << format_ppm(modulo_bias_ppm(n, 0)) << u8" ppm，其余桶是 " << format_ppm(modulo_bias_ppm(n, n - 1))
       << u8" ppm，结构性的，再多跑也消不掉\n";
    os << u8"    取模法与拒绝采样用的是同一条 rand() 序列，只有落在尾巴上的 "
       << (rand_range_size() % static_cast<std::uint64_t>(n))
       << u8" 个取值处理不同，所以两列几乎一样：\n";
    os << u8"      取模法把这 " << (rand_range_size() % static_cast<std::uint64_t>(n))
       << u8" 个取值也算进前 " << (rand_range_size() % static_cast<std::uint64_t>(n))
       << u8" 个桶，拒绝采样把它们整个丢掉\n";
    os << u8"    拒绝采样：可接受的取值个数 " << (rand_range_size() - rand_range_size() % n)
       << u8" 是 10 的整数倍，每个桶的取值域计数完全相同\n";
    os << u8"    取值域计数的极差：取模法 " << preimages_min(mod) << u8" 到 " << preimages_max(mod)
       << u8"，拒绝采样 " << preimages_min(rej) << u8" 到 " << preimages_max(rej)
       << u8"（每个桶完全相同）\n";
    os << u8"    分布一列是 std::uniform_int_distribution 配 mt19937，走的是另一条序列\n";
    os << u8"    " << kBucketTrials << u8" 次试验的抽样噪声：每个桶的标准差约 "
       << static_cast<long long>(std::sqrt(kBucketTrials * 0.1 * 0.9) + 0.5)
       << u8" 次，折合约 6700 ppm，比取模偏差大两个数量级\n";
    os << u8"    小 n 上看不出取模偏差，正是它危险的地方：分布错了，单次结果却看不出异常\n";
    os << "\n";
}

void append_bucket_grid(std::ostringstream &os)
{
    const int n = 100;
    const BucketStats mod = modulo_buckets(n, kBucketTrials, kSeed);
    const BucketStats rej = rejection_buckets(n, kBucketTrials, kSeed);
    const BucketStats dis = distribution_buckets(n, kBucketTrials, kSeed);

    os << u8"  n = 100，同一试验次数，rand() % 100 每个桶的频次（10 × 10，行是十位、列是个位）\n";
    for (int row = 0; row < 10; ++row) {
        os << "    ";
        for (int col = 0; col < 10; ++col) {
            os << pad_left(std::to_string(mod.observed[static_cast<std::size_t>(row * 10 + col)]), 8);
        }
        os << "\n";
    }
    os << u8"    观察到的极差 " << *std::min_element(mod.observed.begin(), mod.observed.end())
       << u8" 到 " << *std::max_element(mod.observed.begin(), mod.observed.end())
       << u8"，全落在抽样噪声里（标准差约 " << static_cast<long long>(std::sqrt(kBucketTrials * 0.01 * 0.99) + 0.5)
       << u8" 次）\n";
    os << "\n";
}

/** 分组对照表：把 n 个桶按「是否分到多出来的取值」分成两组。
    n = 100 时两组各占 0.1%，n = 20000 时一组是另一组的两倍 */
void append_bucket_groups(std::ostringstream &os, int n, const BucketStats &mod,
                          const BucketStats &rej, const BucketStats &dis)
{
    const int first = static_cast<int>(modulo_remainder(n));
    const int edge[] = {0, first, n};
    os << "  " << pad_right(u8"桶范围", 15) << pad_left(u8"桶数", 7) << pad_left(u8"取值域计数", 12)
       << pad_left(u8"理论偏差", 12) << pad_left(u8"取模平均", 11) << pad_left(u8"拒绝平均", 11)
       << pad_left(u8"分布平均", 11) << pad_left(u8"每桶期望", 11) << "\n";
    for (int g = 0; g < 2; ++g) {
        const int from = edge[g];
        const int count = edge[g + 1] - edge[g];
        std::ostringstream range;
        range << from << ".." << (from + count - 1);
        os << "  " << pad_right(range.str(), 15) << pad_left(std::to_string(count), 7)
           << pad_left(std::to_string(mod.preimages[static_cast<std::size_t>(from)]), 12)
           << pad_left(format_ppm(modulo_bias_ppm(n, from)), 12)
           << pad_left(format_milli(average_milli(mod.observed, from, count)), 11)
           << pad_left(format_milli(average_milli(rej.observed, from, count)), 11)
           << pad_left(format_milli(average_milli(dis.observed, from, count)), 11)
           << pad_left(format_milli(static_cast<long long>(kBucketTrials) * 1000 / n), 11) << "\n";
    }
}

void append_bias(std::ostringstream &os)
{
    os << u8"二、rand() % n 的取模偏差\n";
    os << u8"  N = RAND_MAX + 1 = " << rand_range_size()
       << u8"，把 N 分成 n 份：商 q = N / n，余 r = N % n\n";
    os << u8"  v % n 落在桶 b 上的取值个数是：b < r 时 q + 1 个，否则 q 个——\n";
    os << u8"  多出来的 r 个取值全部落进前 r 个桶，这就是偏差的全部来源\n";
    append_bias_table(os);
    os << u8"  偏差一列是精确值，与试验次数无关：n = 10 时是 " << format_ppm(modulo_bias_ppm(10, 0))
       << u8" ppm，即 " << format_ppm_percent(modulo_bias_ppm(10, 0)) << "\n";
    os << "\n";
    append_bucket_small(os);

    const int n = 100;
    const BucketStats mod = modulo_buckets(n, kBucketTrials, kSeed);
    const BucketStats rej = rejection_buckets(n, kBucketTrials, kSeed);
    const BucketStats dis = distribution_buckets(n, kBucketTrials, kSeed);
    append_bucket_grid(os);
    os << u8"  n = 100，分组对照（桶 0 到 " << (modulo_remainder(n) - 1) << u8" 分到 "
       << modulo_preimages(n, 0) << u8" 个取值，其余分到 " << modulo_preimages(n, n - 1)
       << u8" 个）\n";
    append_bucket_groups(os, n, mod, rej, dis);
    {
        const int first = static_cast<int>(modulo_remainder(n));
        const long long low = average_milli(mod.observed, 0, first);
        const long long high = average_milli(mod.observed, first, n - first);
        const double sigma = std::sqrt(static_cast<double>(kBucketTrials) * 0.01 * 0.99);
        const double average_sigma = std::sqrt(sigma * sigma / first + sigma * sigma / (n - first));
        const long long want_gap = divide_round(
            static_cast<long long>(modulo_preimages(n, 0)) * static_cast<long long>(kBucketTrials) * 1000,
            static_cast<long long>(rand_range_size())) -
            divide_round(static_cast<long long>(modulo_preimages(n, n - 1)) *
                             static_cast<long long>(kBucketTrials) * 1000,
                         static_cast<long long>(rand_range_size()));
        os << u8"    两组观察值之差 " << format_milli(low - high) << u8" 次，方向与理论相反：每桶的噪声标准差约 "
           << static_cast<long long>(sigma + 0.5) << u8" 次，两组平均之后仍有约 "
           << static_cast<long long>(average_sigma + 0.5) << u8" 次，与 " << format_milli(want_gap)
           << u8" 次的理论差同量级\n";
        os << u8"    所以 n = 100 时观察值看不出这个偏差，要看的是「理论偏差」那一列\n";
    }
    os << "\n";

    const int big = 20000;
    const BucketStats mod_big = modulo_buckets(big, kBucketTrials, kSeed);
    const BucketStats rej_big = rejection_buckets(big, kBucketTrials, kSeed);
    const BucketStats dis_big = distribution_buckets(big, kBucketTrials, kSeed);
    os << u8"  n = 20000，同一试验次数：这时偏差大到用眼睛就能看出来\n";
    append_bucket_groups(os, big, mod_big, rej_big, dis_big);
    const long long low_average = average_milli(mod_big.observed, 0, static_cast<int>(modulo_remainder(big)));
    const long long high_average = average_milli(mod_big.observed, static_cast<int>(modulo_remainder(big)),
                                                 big - static_cast<int>(modulo_remainder(big)));
    os << u8"    取模法两组之比 = " << format_milli(low_average) << " : " << format_milli(high_average)
       << u8"，正是「2 个取值」与「1 个取值」之比\n";
    os << u8"    同一次数下拒绝采样与 uniform_int_distribution 两组都在 1 : 1 附近\n";
    os << "\n";
    os << u8"  随机数调用次数（三段试验共 " << (kBucketTrials * 3) << u8" 次抽样）\n";
    os << "  " << pad_right(u8"做法", 26) << pad_left(u8"n = 10", 12) << pad_left(u8"n = 100", 12)
       << pad_left(u8"n = 20000", 12) << pad_left(u8"合计", 12) << "\n";
    const BucketStats list[] = {modulo_buckets(10, kBucketTrials, kSeed),
                                modulo_buckets(100, kBucketTrials, kSeed), mod_big};
    const BucketStats rej_list[] = {rejection_buckets(10, kBucketTrials, kSeed),
                                    rejection_buckets(100, kBucketTrials, kSeed), rej_big};
    const BucketStats dis_list[] = {distribution_buckets(10, kBucketTrials, kSeed),
                                    distribution_buckets(100, kBucketTrials, kSeed), dis_big};
    const char *labels[] = {u8"rand() % n（有偏）", u8"拒绝采样（无偏）",
                            u8"uniform_int_distribution"};
    const BucketStats *rows[] = {list, rej_list, dis_list};
    for (int r = 0; r < 3; ++r) {
        std::uint64_t total = 0;
        os << "  " << pad_right(labels[r], 26);
        for (int c = 0; c < 3; ++c) {
            os << pad_left(std::to_string(rows[r][c].engine_calls), 12);
            total += rows[r][c].engine_calls;
        }
        os << pad_left(std::to_string(total), 12) << "\n";
    }
    os << "  " << pad_right(u8"其中被拒绝丢掉的", 26);
    std::uint64_t reject_total = 0;
    for (int c = 0; c < 3; ++c) {
        os << pad_left(std::to_string(rej_list[c].rejects), 12);
        reject_total += rej_list[c].rejects;
    }
    os << pad_left(std::to_string(reject_total), 12) << "\n";
    os << u8"    取模法一次抽样取一个随机数；拒绝采样平均每次多取 "
       << format_milli(divide_round(static_cast<long long>(reject_total) * 1000,
                                    static_cast<long long>(kBucketTrials * 3)))
       << u8" 个，多出来的就是被丢掉的尾巴\n";
    os << "\n";
}

void append_shuffle_four(std::ostringstream &os)
{
    const int n = 4;
    const ShuffleStats fy = shuffle_fisher_yates(n, kShuffleFourTrials, kSeed);
    const ShuffleStats nv = shuffle_naive(n, kShuffleFourTrials, kSeed);

    os << u8"  n = 4，固定种子 " << kSeed << u8"，两个版本各 " << kShuffleFourTrials
       << u8" 次（期望每个排列 " << (kShuffleFourTrials / factorial_size(n)) << u8" 次）\n";
    const std::uint64_t fact = factorial_size(static_cast<std::size_t>(n));
    os << "  " << pad_right(u8"排列", 10) << pad_left(u8"正确版频次", 12) << pad_left(u8"正确版偏差", 12)
       << pad_left(u8"错误版频次", 12) << pad_left(u8"错误版偏差", 12)
       << pad_left(u8"错误版路径数", 14) << pad_left(u8"错误版精确概率", 16) << "\n";
    for (std::size_t r = 0; r < fy.observed.size(); ++r) {
        const std::vector<int> perm = permutation_at(r, n);
        os << "  " << pad_right(permutation_text(perm), 10)
           << pad_left(std::to_string(fy.observed[r]), 12)
           << pad_left(format_ppm(deviation_ratio_ppm(fy.observed[r], kShuffleFourTrials, 1, fact)), 12)
           << pad_left(std::to_string(nv.observed[r]), 12)
           << pad_left(format_ppm(deviation_ratio_ppm(nv.observed[r], kShuffleFourTrials, 1, fact)), 12)
           << pad_left(std::to_string(nv.exact_paths[r]), 14)
           << pad_left(std::to_string(nv.exact_paths[r]) + "/" + std::to_string(nv.path_total), 16)
           << "\n";
    }
    os << "  " << pad_right(u8"合计", 10) << pad_left(std::to_string(sum_of(fy.observed)), 12)
       << pad_left("0", 12) << pad_left(std::to_string(sum_of(nv.observed)), 12)
       << pad_left("0", 12) << pad_left(std::to_string(nv.path_total), 14) << "\n";
    os << u8"    路径数一列是把 4^4 = " << nv.path_total
       << u8" 条随机路径全部走一遍数出来的：正确版每一行都是 1（24 条路径铺满 24 个排列）\n";
    os << u8"    错误版从 " << *std::min_element(nv.exact_paths.begin(), nv.exact_paths.end())
       << u8" 条到 " << *std::max_element(nv.exact_paths.begin(), nv.exact_paths.end())
       << u8" 条不等，排列之间相差近一倍\n";
    os << u8"    错误版最多的排列 [" << permutation_text(permutation_at(nv.max_rank, n)) << u8"]："
       << nv.exact_paths[nv.max_rank] << u8" 条路径，" << format_milli(static_cast<long long>(nv.exact_paths[nv.max_rank]) * 100000 / static_cast<long long>(nv.path_total))
       << u8"% 的精确概率，观察 " << nv.max_count << u8" 次\n";
    os << u8"    错误版最少的排列 [" << permutation_text(permutation_at(nv.min_rank, n)) << u8"]："
       << nv.exact_paths[nv.min_rank] << u8" 条路径，" << format_milli(static_cast<long long>(nv.exact_paths[nv.min_rank]) * 100000 / static_cast<long long>(nv.path_total))
       << u8"% 的精确概率，观察 " << nv.min_count << u8" 次\n";
    os << u8"    卡方（自由度 23，均匀时约等于 23）：正确版 " << format_milli(fy.chi2_milli)
       << u8"，错误版 " << format_milli(nv.chi2_milli) << "\n";
    os << u8"    随机数调用次数：正确版每次洗牌 n - 1 = " << (n - 1) << u8" 个（共 "
       << fy.engine_calls << u8"），错误版每次 n = " << n << u8" 个（共 " << nv.engine_calls << u8"）\n";
    os << "\n";
}

void append_shuffle_five(std::ostringstream &os)
{
    const int n = 5;
    const ShuffleStats fy = shuffle_fisher_yates(n, kShuffleFiveTrials, kSeed);
    const ShuffleStats nv = shuffle_naive(n, kShuffleFiveTrials, kSeed);

    os << u8"  n = 5，两个版本各 " << kShuffleFiveTrials << u8" 次（120 个排列，期望每个 "
       << (kShuffleFiveTrials / factorial_size(n)) << u8" 次）\n";
    os << u8"    卡方（自由度 119，均匀时约等于 119）：正确版 " << format_milli(fy.chi2_milli)
       << u8"，错误版 " << format_milli(nv.chi2_milli) << "\n";

    /* 错误版路径数最少与最多的各三个排列 */
    std::vector<std::size_t> order(nv.exact_paths.size());
    for (std::size_t i = 0; i < order.size(); ++i) {
        order[i] = i;
    }
    std::sort(order.begin(), order.end(), [&nv](std::size_t a, std::size_t b) {
        if (nv.exact_paths[a] != nv.exact_paths[b]) {
            return nv.exact_paths[a] < nv.exact_paths[b];
        }
        return a < b;
    });
    os << u8"    错误版偏少的三个排列（按精确路径数从小到大）\n";
    os << "    " << pad_right(u8"排列", 16) << pad_left(u8"路径数", 8)
       << pad_left(u8"精确概率", 12) << pad_left(u8"观察频次", 10) << "\n";
    for (int k = 0; k < 3; ++k) {
        const std::size_t r = order[static_cast<std::size_t>(k)];
        os << "    " << pad_right("[" + permutation_text(permutation_at(r, n)) + "]", 16)
           << pad_left(std::to_string(nv.exact_paths[r]), 8)
           << pad_left(format_milli(static_cast<long long>(nv.exact_paths[r]) * 100000 /
                                    static_cast<long long>(nv.path_total)) + "%", 12)
           << pad_left(std::to_string(nv.observed[r]), 10) << "\n";
    }
    os << u8"    错误版偏多的三个排列（按精确路径数从大到小）\n";
    os << "    " << pad_right(u8"排列", 16) << pad_left(u8"路径数", 8)
       << pad_left(u8"精确概率", 12) << pad_left(u8"观察频次", 10) << "\n";
    for (int k = 0; k < 3; ++k) {
        const std::size_t r = order[order.size() - 1 - static_cast<std::size_t>(k)];
        os << "    " << pad_right("[" + permutation_text(permutation_at(r, n)) + "]", 16)
           << pad_left(std::to_string(nv.exact_paths[r]), 8)
           << pad_left(format_milli(static_cast<long long>(nv.exact_paths[r]) * 100000 /
                                    static_cast<long long>(nv.path_total)) + "%", 12)
           << pad_left(std::to_string(nv.observed[r]), 10) << "\n";
    }
    os << u8"    路径总数：正确版 " << fy.path_total << u8" = n!，错误版 " << nv.path_total
       << u8" = n^n；排列只有 120 个，路径多出来那么多条，摊到每个排列上的条数就不一样了\n";
    os << u8"    随机数调用次数：正确版 " << fy.engine_calls << u8" 个，错误版 " << nv.engine_calls
       << u8" 个\n";
    os << "\n";
}

void append_shuffle(std::ostringstream &os)
{
    os << u8"三、Fisher-Yates 洗牌：正确版与错误版\n";
    os << u8"  正确版：i 从 n - 1 递减到 1，把 a[i] 与 a[0..i] 里的随机一个交换\n";
    os << u8"    第 i 步有 i + 1 种选择，路径总数 n × (n - 1) × … × 2 = n!，一个排列恰好一条路径\n";
    os << u8"  错误版：i 从 0 递增到 n - 1，把 a[i] 与 a[0..n-1] 里的随机一个交换\n";
    os << u8"    每一步都是 n 种选择，路径总数 n^n，路径比排列多得多，一个排列摊到好几条\n";
    append_shuffle_four(os);
    append_shuffle_five(os);
}

void append_reservoir(std::ostringstream &os)
{
    os << u8"四、蓄水池抽样：只知道一遍的数据流里抽 k 个\n";
    os << u8"  数据流：" << kStreamSize << u8" 个元素，下标 0 到 " << (kStreamSize - 1)
       << u8"，值 = 下标 × 3 + 7（在源码里就地构造，不读外部文件）\n";
    const ReservoirRun once = reservoir_once(kStreamSize, kReservoirK, kSeed);
    os << u8"  k = " << kReservoirK << u8"，种子 " << kSeed << u8"，跑一次：\n";
    os << u8"    前 " << kReservoirK << u8" 个先填进蓄水池：槽位 0 到 " << (kReservoirK - 1)
       << u8" 收下 0 1 2 3 4\n";
    os << u8"    第 " << kReservoirK << u8" 到第 " << (kStreamSize - 1)
       << u8" 个元素，每读一个取一次 [0, i] 的随机数，落在 [0, " << kReservoirK << u8") 里就把那一格换掉\n";
    os << u8"    被换出去的下标（按时间顺序）：" << join_ints(once.replaced) << "\n";
    os << u8"    最终蓄水池（按槽位）：" << join_ints(once.picked) << "\n";
    os << u8"    随机数调用 " << once.engine_calls << u8" 次（n - k = " << (kStreamSize - kReservoirK)
       << u8"，每读一个元素一次）\n";
    os << "\n";

    const ReservoirStats rep = reservoir_repeat(kStreamSize, kReservoirK, kReservoirRounds, kSeed);
    const std::uint64_t expect = kReservoirRounds * static_cast<std::uint64_t>(kReservoirK) /
                                 static_cast<std::uint64_t>(kStreamSize);
    os << u8"  试验 " << kReservoirRounds << u8" 次，统计每个下标被选中的次数，期望 "
       << kReservoirRounds << u8" × " << kReservoirK << u8" / " << kStreamSize << u8" = " << expect << "\n";
    long long worst = 0;
    for (int i = 0; i < kStreamSize; ++i) {
        const long long ppm = deviation_ratio_ppm(rep.hits[static_cast<std::size_t>(i)],
                                                  kReservoirRounds,
                                                  static_cast<std::uint64_t>(kReservoirK),
                                                  static_cast<std::uint64_t>(kStreamSize));
        if (std::llabs(ppm) > std::llabs(worst)) {
            worst = ppm;
        }
    }
    for (int from = 0; from < kStreamSize; from += 10) {
        const int count = (kStreamSize - from < 10) ? (kStreamSize - from) : 10;
        os << "    " << pad_right(u8"下标", 6);
        for (int i = from; i < from + count; ++i) {
            os << pad_left(std::to_string(i), 9);
        }
        os << "\n    " << pad_right(u8"次数", 6);
        for (int i = from; i < from + count; ++i) {
            os << pad_left(std::to_string(rep.hits[static_cast<std::size_t>(i)]), 9);
        }
        os << "\n    " << pad_right(u8"偏差", 6);
        for (int i = from; i < from + count; ++i) {
            os << pad_left(format_ppm(deviation_ratio_ppm(rep.hits[static_cast<std::size_t>(i)],
                                                          kReservoirRounds,
                                                          static_cast<std::uint64_t>(kReservoirK),
                                                          static_cast<std::uint64_t>(kStreamSize))),
                           9);
        }
        os << "\n";
    }
    os << u8"    最大偏差 " << format_ppm(worst) << u8" ppm；抽样噪声标准差约 "
       << static_cast<long long>(std::sqrt(static_cast<double>(kReservoirRounds) * 0.25 * 0.75) + 0.5)
       << u8" 次，折合约 12200 ppm，观察到的偏差都在这个量级\n";
    os << u8"  为什么每个元素被选中的概率相同：\n";
    os << u8"    第 i 个元素先以 k / (i + 1) 的概率进入蓄水池，之后第 j 步（j > i）把它换掉的概率是 1 / (j + 1)，\n";
    os << u8"    一路乘下来：k/(i+1) × (i+1)/(i+2) × … × (n-1)/n = k/n，与 i 无关；\n";
    os << u8"    每一步只看当前元素与蓄水池，因此数据流读一遍就够\n";
    std::uint64_t paths = 0;
    const std::vector<std::uint64_t> hits = reservoir_exact_hits(6, 2, &paths);
    os << u8"  精确核对：n = 6、k = 2 时把全部 " << paths << u8" 条随机路径枚举一遍，每个位置被选中 "
       << hits.front() << u8" 次，恰好是 " << paths << u8" × 2 / 6（自测里核对两种规模）\n";
    os << u8"  随机数调用次数：一次抽样 " << once.engine_calls << u8" 个，" << kReservoirRounds
       << u8" 次共 " << rep.engine_calls << u8" 个\n";
    os << "\n";
}

void append_weighted(std::ostringstream &os)
{
    const std::vector<long long> weights = {500, 250, 125, 75, 50};
    const char *names[] = {"A", "B", "C", "D", "E"};
    const std::size_t n = weights.size();
    long long total = 0;
    for (const long long w : weights) {
        total += w;
    }

    os << u8"五、加权抽样：前缀和加二分，与别名法\n";
    os << u8"  权重（和为 " << total << u8"）：A 500、B 250、C 125、D 75、E 50\n";
    os << "  " << pad_right(u8"元素", 7) << pad_left(u8"权重", 8) << pad_left(u8"理论占比", 11)
       << "  " << pad_left(u8"前缀和区间", 18) << "\n";
    long long running = 0;
    for (std::size_t i = 0; i < n; ++i) {
        std::ostringstream range;
        range << "[" << running << ", " << (running + weights[i]) << ")";
        os << "  " << pad_right(names[i], 7) << pad_left(std::to_string(weights[i]), 8)
           << pad_left(format_permille_percent(weights[i] * 1000 / total), 11) << "  "
           << pad_left(range.str(), 18) << "\n";
        running += weights[i];
    }

    const WeightedStats by_binary = sample_prefix_binary(weights, kWeightedTrials, kSeed);
    const WeightedStats by_alias = sample_alias(weights, kWeightedTrials, kSeed);
    const AliasTable table = build_alias_table(weights);

    os << u8"  做法一 前缀和加二分：每次取一个 [0, " << total << u8") 的整数，在 "
       << n << u8" 个前缀和里二分找区间\n";
    os << u8"    " << kWeightedTrials << u8" 次抽样：随机数 " << by_binary.engine_calls
       << u8" 个，比较 " << by_binary.comparisons << u8" 次（每次 "
       << (by_binary.comparisons / kWeightedTrials) << u8" 到 "
       << (by_binary.comparisons / kWeightedTrials + 1) << u8" 次）\n";
    os << u8"  做法二 别名法（Vose），建表全程整数运算：\n";
    os << "  " << pad_right(u8"元素", 7) << pad_left(u8"直接部分 / " + std::to_string(total), 20)
       << "  " << pad_left(u8"别名指向", 10) << "\n";
    for (std::size_t i = 0; i < n; ++i) {
        os << "  " << pad_right(names[i], 7) << pad_left(std::to_string(table.prob_num[i]), 20) << "  "
           << pad_left(table.alias[i] < 0
                           ? std::string(u8"无")
                           : std::string(names[static_cast<std::size_t>(table.alias[i])]),
                       10)
           << "\n";
    }
    os << u8"    每一列的总概率都是 " << total << " / " << total
       << u8"：直接部分归自己，剩下的归别名指向的元素\n";
    os << u8"    恒等式：n × w_i = 直接部分 + 所有指向 i 的别名部分之和，"
       << n - alias_identity_failures(table) << u8" 项全部精确成立\n";
    os << u8"    " << kWeightedTrials << u8" 次抽样：随机数 " << by_alias.engine_calls
       << u8" 个（选列一个、抛硬币一个），一次比较都不用\n";
    os << "\n";
    os << u8"  频次对照（各 " << kWeightedTrials << u8" 次，期望 = 试验次数 × 理论占比）\n";
    os << "  " << pad_right(u8"元素", 7) << pad_left(u8"理论占比", 11) << pad_left(u8"期望", 10)
       << pad_left(u8"前缀和加二分", 14) << pad_left(u8"偏差", 11) << pad_left(u8"别名法", 12)
       << pad_left(u8"偏差", 11) << "\n";
    for (std::size_t i = 0; i < n; ++i) {
        const std::uint64_t want = kWeightedTrials * static_cast<std::uint64_t>(weights[i]) /
                                   static_cast<std::uint64_t>(total);
        os << "  " << pad_right(names[i], 7)
           << pad_left(format_permille_percent(weights[i] * 1000 / total), 11)
           << pad_left(std::to_string(want), 10)
           << pad_left(std::to_string(by_binary.observed[i]), 14)
           << pad_left(format_ppm(deviation_ratio_ppm(by_binary.observed[i], kWeightedTrials,
                                                      static_cast<std::uint64_t>(weights[i]),
                                                      static_cast<std::uint64_t>(total))),
                       11)
           << pad_left(std::to_string(by_alias.observed[i]), 12)
           << pad_left(format_ppm(deviation_ratio_ppm(by_alias.observed[i], kWeightedTrials,
                                                      static_cast<std::uint64_t>(weights[i]),
                                                      static_cast<std::uint64_t>(total))),
                       11)
           << "\n";
    }
    os << "  " << pad_right(u8"合计", 7) << pad_left("100.0%", 11) << pad_left(std::to_string(kWeightedTrials), 10)
       << pad_left(std::to_string(sum_of(by_binary.observed)), 14) << pad_left("0", 11)
       << pad_left(std::to_string(sum_of(by_alias.observed)), 12) << pad_left("0", 11) << "\n";
    os << u8"    两种做法的观察偏差都在抽样噪声之内：50% 那一档的标准差约 "
       << static_cast<long long>(std::sqrt(kWeightedTrials * 0.5 * 0.5) + 0.5)
       << u8" 次，折合约 3162 ppm\n";
    os << u8"    随机数调用次数：前缀和版 " << by_binary.engine_calls << u8" 个加 "
       << by_binary.comparisons << u8" 次比较；别名法 " << by_alias.engine_calls
       << u8" 个，一次比较都不用\n";
    os << u8"    " << n << u8" 个元素时省下的比较只有个位数；元素到几万时，别名法把每次抽样的 log2(n) 次比较换成恒定两个随机数\n";
}

}   /* namespace */

std::string build_report()
{
    std::ostringstream os;
    append_engines(os);
    append_bias(os);
    append_shuffle(os);
    append_reservoir(os);
    append_weighted(os);
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

    void expect_eq_size(std::size_t got, std::size_t want, const std::string &what)
    {
        const bool ok = (got == want);
        if (ok) {
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

    /* 1 取值域 */
    checks.expect(RAND_MAX >= 32767 &&
                      rand_range_size() == static_cast<std::uint64_t>(RAND_MAX) + 1,
                  u8"RAND_MAX 至少是 32767（标准给的下限），rand_range_size() 等于 RAND_MAX + 1");

    /* 2 同一种子两次取数逐位相同 */
    const EngineSample crand_once = sample_crand(kSeed, 8);
    const EngineSample crand_twice = sample_crand(kSeed, 8);
    const EngineSample mt_once = sample_mt19937(kSeed, 8);
    const EngineSample mt_twice = sample_mt19937(kSeed, 8);
    checks.expect(crand_once.values == crand_twice.values && crand_once.calls == 8 &&
                      mt_once.values == mt_twice.values && mt_once.calls == 8,
                  u8"同一种子两次取数逐位相同：rand() 与 mt19937 各 8 个值，调用次数都是 8");

    /* 3 mt19937 的序列由标准规定 */
    const std::vector<std::uint32_t> mt_reference = {
        3205851784u, 2467874039u, 1342359732u, 4155908089u,
        3882826974u, 4219272983u, 2866877852u, 638747925u};
    checks.expect(mt_once.values == mt_reference,
                  u8"mt19937(20261002) 的前 8 个值与标准规定的序列一致（换一个标准库也是这 8 个数）");

    /* 4 取值域计数：和等于 N，且第 b 个桶是「商 +（b 小于余数 ? 1 : 0）」 */
    bool preimage_formula = true;
    const int probe_sizes[] = {6, 10, 100, 1000, 20000};
    for (const int n : probe_sizes) {
        std::uint64_t sum = 0;
        const long long quotient = modulo_quotient(n);
        const long long remainder = modulo_remainder(n);
        for (int b = 0; b < n; ++b) {
            const std::uint64_t pre = modulo_preimages(n, b);
            sum += pre;
            const std::uint64_t want =
                static_cast<std::uint64_t>(quotient) + (b < remainder ? 1u : 0u);
            if (pre != want) {
                preimage_formula = false;
            }
        }
        if (sum != rand_range_size()) {
            preimage_formula = false;
        }
    }
    checks.expect(preimage_formula,
                  u8"取值域计数之和等于 N，且第 b 个桶的计数是「商 +（b 小于余数 ? 1 : 0）」");

    /* 5 理论偏差与取值域计数一致 */
    const long long preimages_first = static_cast<long long>(modulo_preimages(10, 0));
    const long long total_range = static_cast<long long>(rand_range_size());
    const long long bias_from_preimages =
        (preimages_first * 10 - total_range) * 1000000 / total_range;
    checks.expect(modulo_bias_ppm(10, 0) == bias_from_preimages && modulo_bias_ppm(10, 0) > 0 &&
                      modulo_bias_ppm(10, 9) < 0,
                  u8"取模法的理论偏差与取值域计数一致：n = 10 时加成桶为正、其余桶为负");

    /* 6 拒绝采样把取值域截到 n 的整数倍 */
    bool reject_slices_evenly = true;
    const int reject_sizes[] = {10, 100, 20000};
    for (const int n : reject_sizes) {
        const BucketStats stats = rejection_buckets(n, 1000, kSeed);
        for (const std::uint64_t pre : stats.preimages) {
            if (pre != stats.preimages.front()) {
                reject_slices_evenly = false;
            }
        }
        const std::uint64_t range = rand_range_size();
        if ((range - range % static_cast<std::uint64_t>(n)) % static_cast<std::uint64_t>(n) != 0) {
            reject_slices_evenly = false;
        }
    }
    checks.expect(reject_slices_evenly,
                  u8"拒绝采样把取值域截到 n 的整数倍：每个桶分到的取值个数完全相同");

    /* 7 桶频次的账目配平 */
    const BucketStats mod_small = modulo_buckets(10, kBucketTrials, kSeed);
    const BucketStats rej_small = rejection_buckets(10, kBucketTrials, kSeed);
    const BucketStats dis_small = distribution_buckets(10, kBucketTrials, kSeed);
    checks.expect(sum_of(mod_small.observed) == kBucketTrials &&
                      sum_of(rej_small.observed) == kBucketTrials &&
                      sum_of(dis_small.observed) == kBucketTrials,
                  u8"三段桶频次的账目配平：三个做法的频次之和都等于试验次数 200000");

    /* 8 n 越接近 N，取模偏差越大 */
    checks.expect(modulo_bias_ppm(20000, 0) > 200000 && modulo_bias_ppm(10, 0) < 100,
                  u8"n 越接近 N 取模偏差越大：n = 20000 的加成桶超过 200000 ppm，n = 10 的不到 100 ppm");

    /* 9—15 洗牌 */
    const ShuffleStats fy4 = shuffle_fisher_yates(4, kShuffleFourTrials, kSeed);
    const ShuffleStats nv4 = shuffle_naive(4, kShuffleFourTrials, kSeed);
    const ShuffleStats fy5 = shuffle_fisher_yates(5, kShuffleFiveTrials, kSeed);
    const ShuffleStats nv5 = shuffle_naive(5, kShuffleFiveTrials, kSeed);
    checks.expect(sum_of(fy4.observed) == kShuffleFourTrials &&
                      sum_of(nv4.observed) == kShuffleFourTrials &&
                      sum_of(fy5.observed) == kShuffleFiveTrials &&
                      sum_of(nv5.observed) == kShuffleFiveTrials,
                  u8"洗牌的账目配平：两个版本、两个规模的频次之和都等于试验次数");

    bool all_permutations_seen = true;
    for (const std::uint64_t count : fy4.observed) {
        if (count == 0) {
            all_permutations_seen = false;
        }
    }
    checks.expect(all_permutations_seen, u8"正确版 240000 次洗牌把 24 个排列全部走到了");

    checks.expect(fy4.path_total == 24 && fy5.path_total == 120 && nv4.path_total == 256 &&
                      nv5.path_total == 3125,
                  u8"路径总数：正确版是 n!（24 与 120），错误版是 n^n（256 与 3125）");

    bool one_path_per_permutation = true;
    for (const std::uint64_t paths : fy4.exact_paths) {
        if (paths != 1) {
            one_path_per_permutation = false;
        }
    }
    for (const std::uint64_t paths : fy5.exact_paths) {
        if (paths != 1) {
            one_path_per_permutation = false;
        }
    }
    checks.expect(one_path_per_permutation,
                  u8"正确版每条随机路径恰好对应一个排列：144 个排列的路径数全是 1");

    const std::uint64_t nv4_min = *std::min_element(nv4.exact_paths.begin(), nv4.exact_paths.end());
    const std::uint64_t nv4_max = *std::max_element(nv4.exact_paths.begin(), nv4.exact_paths.end());
    const std::uint64_t nv5_min = *std::min_element(nv5.exact_paths.begin(), nv5.exact_paths.end());
    const std::uint64_t nv5_max = *std::max_element(nv5.exact_paths.begin(), nv5.exact_paths.end());
    checks.expect(nv4_max * 2 > nv4_min * 3 && nv5_max * 2 > nv5_min * 3,
                  u8"错误版的路径数不均匀：最多的排列比最少的多出五成以上（n = 4 与 n = 5 都成立）");

    checks.expect(fy4.chi2_milli < 3 * 23 * 1000 && fy5.chi2_milli < 3 * 119 * 1000,
                  u8"正确版卡方落在噪声范围内：n = 4 小于 3 × 23，n = 5 小于 3 × 119");
    checks.expect(nv4.chi2_milli > 10 * 23 * 1000 && nv5.chi2_milli > 10 * 119 * 1000,
                  u8"错误版卡方远超自由度：两个规模都超过自由度的 10 倍");

    /* 16 排列序号与排列互为逆 */
    bool rank_round_trip = (permutation_rank({0, 1, 2, 3}) == 0 &&
                            permutation_rank({3, 2, 1, 0}) == 23);
    std::vector<std::size_t> ranks;
    for (std::size_t r = 0; r < 24; ++r) {
        const std::vector<int> perm = permutation_at(r, 4);
        if (permutation_rank(perm) != r) {
            rank_round_trip = false;
        }
        ranks.push_back(permutation_rank(perm));
    }
    std::sort(ranks.begin(), ranks.end());
    for (std::size_t i = 0; i < ranks.size(); ++i) {
        if (ranks[i] != i) {
            rank_round_trip = false;
        }
    }
    checks.expect(rank_round_trip,
                  u8"排列序号与排列互为逆：0 1 2 3 是第 0 个、3 2 1 0 是第 23 个，24 个序号互不相同");

    /* 17—18 蓄水池一次抽样 */
    const ReservoirRun once = reservoir_once(kStreamSize, kReservoirK, kSeed);
    bool once_ok = (once.picked.size() == static_cast<std::size_t>(kReservoirK) &&
                    once.engine_calls == static_cast<std::uint64_t>(kStreamSize - kReservoirK));
    std::vector<int> sorted_picked = once.picked;
    std::sort(sorted_picked.begin(), sorted_picked.end());
    for (std::size_t i = 0; i < sorted_picked.size(); ++i) {
        if (sorted_picked[i] < 0 || sorted_picked[i] >= kStreamSize) {
            once_ok = false;
        }
        if (i > 0 && sorted_picked[i] == sorted_picked[i - 1]) {
            once_ok = false;
        }
    }
    checks.expect(once_ok,
                  u8"蓄水池一次抽样：抽出的 5 个下标互不相同、都在 [0, 20) 内，随机数恰好调用 15 次");
    checks.expect(once.replacements == once.replaced.size() && once.replacements <= 15,
                  u8"被替换出去的下标个数等于替换次数，且不超过元素个数减 k");

    /* 19—20 蓄水池的精确枚举 */
    std::uint64_t paths_six_two = 0;
    const std::vector<std::uint64_t> hits_six_two = reservoir_exact_hits(6, 2, &paths_six_two);
    bool exact_six_two = (paths_six_two == 360);
    for (const std::uint64_t hits : hits_six_two) {
        if (hits != 120) {
            exact_six_two = false;
        }
    }
    checks.expect(exact_six_two,
                  u8"蓄水池精确枚举（n = 6、k = 2）：360 条路径，每个位置被选中 120 次，恰好是 360 × 2 / 6");

    std::uint64_t paths_six_three = 0;
    const std::vector<std::uint64_t> hits_six_three = reservoir_exact_hits(6, 3, &paths_six_three);
    bool exact_six_three = (paths_six_three == 120 && paths_six_three == factorial_size(6) / factorial_size(3));
    for (const std::uint64_t hits : hits_six_three) {
        if (hits != 60) {
            exact_six_three = false;
        }
    }
    checks.expect(exact_six_three,
                  u8"蓄水池精确枚举（n = 6、k = 3）：120 条路径，每个位置被选中 60 次，路径总数等于 n! / k!");

    /* 21 蓄水池的观察频次 */
    const ReservoirStats repeated =
        reservoir_repeat(kStreamSize, kReservoirK, kReservoirRounds, kSeed);
    long long worst_ppm = 0;
    const int hit_scale = kStreamSize / kReservoirK;
    for (const std::uint64_t hits : repeated.hits) {
        const long long ppm = deviation_ppm(hits, kReservoirRounds, hit_scale);
        if (std::llabs(ppm) > std::llabs(worst_ppm)) {
            worst_ppm = ppm;
        }
    }
    checks.expect(sum_of(repeated.hits) == kReservoirRounds * kReservoirK &&
                      std::llabs(worst_ppm) < 40000,
                  u8"蓄水池 20000 次试验：命中次数之和等于 rounds × k，最大偏差在 4 倍标准差之内");

    /* 22—23 别名表 */
    const std::vector<long long> weights = {500, 250, 125, 75, 50};
    const AliasTable table = build_alias_table(weights);
    checks.expect(alias_identity_failures(table) == 0,
                  u8"别名法的恒等式逐项成立：n × w_i 等于直接部分加上所有指向 i 的别名部分之和");
    bool alias_shape = true;
    for (std::size_t i = 0; i < weights.size(); ++i) {
        if (table.prob_num[i] < 0 || table.prob_num[i] > table.total) {
            alias_shape = false;
        }
        if (table.prob_num[i] < table.total && table.alias[i] < 0) {
            alias_shape = false;
        }
        if (table.prob_num[i] == table.total && table.alias[i] != -1) {
            alias_shape = false;
        }
    }
    checks.expect(alias_shape,
                  u8"别名表结构完整：直接部分不超过权重和，留了余量的列都指向了某个别名");

    /* 24 前缀和加二分与线性扫描一致 */
    std::vector<long long> prefix(weights.size() + 1, 0);
    for (std::size_t i = 0; i < weights.size(); ++i) {
        prefix[i + 1] = prefix[i] + weights[i];
    }
    bool search_matches = true;
    for (long long value = 0; value < table.total; ++value) {
        std::uint64_t comparisons = 0;
        const int index = prefix_interval(prefix, value, &comparisons);
        std::size_t linear = 0;
        while (linear + 1 < prefix.size() && prefix[linear + 1] <= value) {
            ++linear;
        }
        if (static_cast<std::size_t>(index) != linear || comparisons < 2 || comparisons > 3) {
            search_matches = false;
        }
    }
    checks.expect(search_matches,
                  u8"前缀和加二分与线性扫描结果一致：1000 个取值逐个核对，每次比较 2 到 3 次");

    /* 25 加权抽样的账目与卡方 */
    const WeightedStats by_binary = sample_prefix_binary(weights, kWeightedTrials, kSeed);
    const WeightedStats by_alias = sample_alias(weights, kWeightedTrials, kSeed);
    checks.expect(sum_of(by_binary.observed) == kWeightedTrials &&
                      sum_of(by_alias.observed) == kWeightedTrials &&
                      chi2_weighted_milli(by_binary.observed, weights, kWeightedTrials) < 3 * 4 * 1000 &&
                      chi2_weighted_milli(by_alias.observed, weights, kWeightedTrials) < 3 * 4 * 1000,
                  u8"加权抽样的账目配平，两个做法的卡方都在噪声范围内（自由度 4，上限 12）");

    /* 26 同参数两次调用逐位相同 */
    const BucketStats again_one = modulo_buckets(10, 5000, kSeed);
    const BucketStats again_two = modulo_buckets(10, 5000, kSeed);
    checks.expect(again_one.observed == again_two.observed &&
                      again_one.engine_calls == again_two.engine_calls,
                  u8"同参数两次调用逐位相同：取模统计的频次与随机数调用次数都一样");

    return checks.finish();
}

}   /* namespace rlab */
