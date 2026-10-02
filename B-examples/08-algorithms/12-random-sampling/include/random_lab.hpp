/**
 * random_lab.hpp —— 随机与采样：取模偏差、Fisher-Yates、蓄水池与加权抽样
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

/**
 * 报告里的每个数字都由这里的计数函数产出，重跑逐位相同：
 *
 *   engine_calls   取了多少个随机数。「抽一次样」与「取一个随机数」不是一回事：
 *                  拒绝采样会把多出来的尾巴丢掉，uniform_int_distribution 内部
 *                  也按自己的算法取值与拒绝，两者都记在这里
 *   rejects        拒绝采样丢掉了多少个取值
 *   preimages      取值域计数：引擎的取值域里有几个值会落到某个桶上（精确值，不是抽出来的）
 *   observed       固定种子下每个桶、每个排列、每个位置的观察频次
 *   exact_paths    把全部随机路径枚举一遍得到的精确路径数（洗牌与蓄水池各有一套枚举）
 *   chi2_milli     卡方统计量，单位 0.001
 *
 * 两条随机数流：rand()（取值域 [0, RAND_MAX]，序列由标准库的实现决定）与
 * std::mt19937（取值域 2^32，序列由标准规定）。两条流都包了调用计数，
 * 而且都能喂给 uniform_int_distribution——它们满足 UniformRandomBitGenerator 的要求。
 */
#ifndef RANDOM_LAB_HPP
#define RANDOM_LAB_HPP

#include <cstddef>
#include <cstdint>
#include <random>
#include <string>
#include <vector>

namespace rlab {

/* ================= 发动机：两条随机数流，都带调用计数 ================= */

/** std::mt19937 加一层调用计数 */
class CountingEngine {
public:
    using result_type = std::uint32_t;

    explicit CountingEngine(std::uint32_t seed) : engine_(seed) {}

    static constexpr result_type min() { return std::mt19937::min(); }
    static constexpr result_type max() { return std::mt19937::max(); }

    result_type operator()()
    {
        ++calls_;
        return engine_();
    }

    void discard(unsigned long long z)
    {
        calls_ += z;
        engine_.discard(z);
    }

    std::uint64_t calls() const { return calls_; }

private:
    std::mt19937 engine_;
    std::uint64_t calls_ = 0;
};

/** rand() 加一层调用计数：取值域是 [0, RAND_MAX]，种子是全局状态，
    要可复现就必须在每次实验开始之前 srand 一次 */
class CrandEngine {
public:
    using result_type = std::uint32_t;

    explicit CrandEngine(std::uint32_t seed);

    static constexpr result_type min() { return 0; }
    static constexpr result_type max() { return static_cast<result_type>(RAND_MAX); }

    result_type operator()();
    void discard(unsigned long long z);

    std::uint64_t calls() const { return calls_; }

private:
    std::uint64_t calls_ = 0;
};

/** [0, n) 上的均匀整数：拒绝采样。
    把引擎的取值域截到 n 的整数倍，多出来的尾巴整个丢掉，因此每个值被取到的
    取值个数完全相同——这是「无偏」的精确含义，见自测里的取值域计数核对。 */
template <class Engine>
std::uint32_t uniform_below(Engine &engine, std::uint32_t n)
{
    const std::uint64_t low = static_cast<std::uint64_t>(Engine::min());
    const std::uint64_t range = static_cast<std::uint64_t>(Engine::max()) - low + 1;
    const std::uint64_t limit = range - range % n;
    std::uint64_t value = 0;
    do {
        value = static_cast<std::uint64_t>(engine()) - low;
    } while (value >= limit);
    return static_cast<std::uint32_t>(value % n);
}

/** rand() 取值域的大小：RAND_MAX + 1 */
std::uint64_t rand_range_size();

/* ================= 一、两个发生器 ================= */

struct EngineSample {
    std::uint32_t seed = 0;
    std::vector<std::uint32_t> values;
    std::uint64_t calls = 0;
};

/** srand(seed) 之后取 count 个 rand() */
EngineSample sample_crand(std::uint32_t seed, std::size_t count);

/** std::mt19937(seed) 之后取 count 个数 */
EngineSample sample_mt19937(std::uint32_t seed, std::size_t count);

/* ================= 二、取模偏差 ================= */

struct BucketStats {
    int buckets = 0;
    std::uint64_t trials = 0;
    std::vector<std::uint64_t> observed;    /**< 每个桶的观察频次 */
    std::vector<std::uint64_t> preimages;   /**< 每个桶对应的取值个数（精确值） */
    std::uint64_t engine_calls = 0;
    std::uint64_t rejects = 0;
};

/** 有偏做法：rand() % buckets */
BucketStats modulo_buckets(int buckets, std::uint64_t trials, std::uint32_t seed);

/** 无偏做法一：rand() 加拒绝采样 */
BucketStats rejection_buckets(int buckets, std::uint64_t trials, std::uint32_t seed);

/** 无偏做法二：std::mt19937 配 std::uniform_int_distribution */
BucketStats distribution_buckets(int buckets, std::uint64_t trials, std::uint32_t seed);

/** 观察频次相对期望频次（trials / buckets）的偏差，单位 ppm（百万分之一，即 0.0001%），
    向零取整。+1000 ppm 就是 +0.1% */
long long deviation_ppm(std::uint64_t count, std::uint64_t trials, int buckets);

/** 观察频次相对「trials × numerator / denominator」的偏差，单位 ppm。
    洗牌的期望是 trials / n!、蓄水池是 trials × k / n、加权是 trials × w / 权重和，
    都不是「均分」，因此需要一个按比例算期望的版本 */
long long deviation_ratio_ppm(std::uint64_t count, std::uint64_t trials,
                              std::uint64_t numerator, std::uint64_t denominator);

/** N = RAND_MAX + 1 除以桶数的商与余 */
long long modulo_quotient(int buckets);
long long modulo_remainder(int buckets);

/** 取模法下第 bucket 个桶对应的取值个数：桶号小于余数时是商加一，否则是商 */
std::uint64_t modulo_preimages(int buckets, int bucket);

/** 取模法的理论偏差 ppm：preimages 与「每个桶均分」之间的差距。
    这一列不随试验次数变化，也不会被抽样噪声掩盖 */
long long modulo_bias_ppm(int buckets, int bucket);

std::uint64_t preimages_min(const BucketStats &stats);
std::uint64_t preimages_max(const BucketStats &stats);

/* ================= 三、洗牌 ================= */

struct ShuffleStats {
    int n = 0;
    std::uint64_t trials = 0;
    std::vector<std::uint64_t> observed;      /**< 按排列序号（字典序）计频次 */
    std::vector<std::uint64_t> exact_paths;   /**< 枚举全部随机路径得到的精确路径数 */
    std::uint64_t path_total = 0;             /**< n!（正确版）或 n^n（错误版） */
    std::uint64_t engine_calls = 0;
    long long chi2_milli = 0;                 /**< 卡方，单位 0.001 */
    std::uint64_t min_count = 0;
    std::uint64_t max_count = 0;
    std::size_t min_rank = 0;
    std::size_t max_rank = 0;
};

/** 正确版：i 从 n - 1 递减到 1，把 a[i] 与 a[0..i] 里的随机一个交换 */
ShuffleStats shuffle_fisher_yates(int n, std::uint64_t trials, std::uint32_t seed);

/** 错误版：i 从 0 递增到 n - 1，把 a[i] 与 a[0..n-1] 里的随机一个交换 */
ShuffleStats shuffle_naive(int n, std::uint64_t trials, std::uint32_t seed);

std::size_t factorial_size(std::size_t n);

/** 排列的字典序序号（Lehmer 码，阶乘进制） */
std::size_t permutation_rank(const std::vector<int> &perm);

/** 排列的文本形式，例如 "0 1 2 3" */
std::string permutation_text(const std::vector<int> &perm);

/** 序号对应的排列（permutation_rank 的逆） */
std::vector<int> permutation_at(std::size_t rank, int n);

/* ================= 四、蓄水池抽样 ================= */

struct ReservoirRun {
    int stream_size = 0;
    int k = 0;
    std::vector<int> picked;      /**< 最终蓄水池里的下标，按槽位 */
    std::vector<int> replaced;    /**< 被替换出去的下标，按时间顺序 */
    std::uint64_t replacements = 0;
    std::uint64_t engine_calls = 0;
};

/** 算法 R：前 k 个先填进蓄水池，之后每读一个元素 i 取一次 [0, i] 的随机数，
    落在 [0, k) 里就把那一格换掉 */
ReservoirRun reservoir_once(int stream_size, int k, std::uint32_t seed);

struct ReservoirStats {
    int stream_size = 0;
    int k = 0;
    std::uint64_t rounds = 0;
    std::vector<std::uint64_t> hits;   /**< 每个下标被选中（留在最终蓄水池里）的次数 */
    std::uint64_t engine_calls = 0;
};

ReservoirStats reservoir_repeat(int stream_size, int k, std::uint64_t rounds,
                                std::uint32_t seed);

/** 把「每读一个元素取一次随机数」的全部路径枚举一遍（路径总数 = n! / k!），
    返回每个位置被选中的次数；这是「每个元素概率相同」的精确证据 */
std::vector<std::uint64_t> reservoir_exact_hits(int n, int k, std::uint64_t *path_total);

/* ================= 五、加权抽样 ================= */

struct AliasTable {
    std::vector<long long> weights;    /**< 建表时用的权重，恒等式核对要用 */
    std::vector<long long> prob_num;   /**< 直接部分，单位是权重和（整列都归自己时等于权重和） */
    std::vector<int> alias;            /**< 别名指向，-1 表示这一列没有别名 */
    long long total = 0;               /**< 权重和 */
};

/** 别名法（Vose）的建表，全程整数运算：把第 i 列的权重放大 n 倍再互相填补，
    单位统一取权重和，于是「概率」就是整数之比 */
AliasTable build_alias_table(const std::vector<long long> &weights);

/** 逐项核对 n × w_i = 直接部分 + 所有指向 i 的别名部分之和，返回不成立的项数 */
std::size_t alias_identity_failures(const AliasTable &table);

/** 在前缀和里二分，返回区间下标；比较次数记在 comparisons 上 */
int prefix_interval(const std::vector<long long> &prefix, long long value,
                    std::uint64_t *comparisons);

struct WeightedStats {
    std::vector<long long> weights;
    std::uint64_t trials = 0;
    std::vector<std::uint64_t> observed;
    std::uint64_t engine_calls = 0;
    std::uint64_t comparisons = 0;   /**< 只有前缀和加二分这一版用得上 */
};

/** 做法一：前缀和加二分，每次抽样一个 [0, 权重和) 的整数 */
WeightedStats sample_prefix_binary(const std::vector<long long> &weights,
                                   std::uint64_t trials, std::uint32_t seed);

/** 做法二：别名法，每次抽样固定两个整数（先选列，再抛硬币） */
WeightedStats sample_alias(const std::vector<long long> &weights, std::uint64_t trials,
                           std::uint32_t seed);

/* ================= 报告与自测 ================= */

/** 自测结果。不打印，交给界面决定怎么显示 */
struct CheckResult {
    std::size_t total = 0;
    std::size_t passed = 0;
    std::size_t failed = 0;
    std::vector<std::string> lines;   /**< 每项一行，例如 "[通过] 3. ……" */

    bool all_passed() const { return failed == 0; }
    std::string summary() const;      /**< "21 项中 21 项通过，全部通过" */
};

/** 项目输出：发生器、取模偏差、洗牌、蓄水池、加权抽样五段。
    返回多行 UTF-8 文本，末尾带一个换行；由界面层决定怎么显示 */
std::string build_report();

/** 逐项核对取值域计数、路径枚举、恒等式与统计量 */
CheckResult run_self_tests();

/** ppm 的文本形式："+61" / "-244" */
std::string format_ppm(long long ppm);

/** ppm 折成百分数的文本形式，三位小数："+0.006%" / "-0.024%" */
std::string format_ppm_percent(long long ppm);

/** 千分单位的文本形式："23.150" */
std::string format_milli(long long milli);

/** 权重占比的文本形式，一位小数："50.0%" */
std::string format_permille_percent(long long permille);

}   /* namespace rlab */

#endif /* RANDOM_LAB_HPP */
