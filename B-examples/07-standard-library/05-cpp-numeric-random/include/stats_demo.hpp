/**
 * stats_demo.hpp —— 随机数造数据与统计，不依赖任何界面
 * 命令行版（src/main_cli.cpp）与 Win32 版（src/main_gui_win32.cpp）都链接它：
 * 这里的函数不打印、不建窗口，只负责算并返回结果与文本。
 */
#ifndef STATS_DEMO_HPP
#define STATS_DEMO_HPP

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace demo {

/** 一次抽样用到的全部参数。默认种子写死，输出因此可复现 */
struct Options {
    std::uint64_t seed = 20240601;  /**< 引擎种子 */
    std::size_t count = 4000;       /**< 每个分布取多少个样本 */
    int low = 0;                    /**< 均匀分布的下界（含） */
    int high = 99;                  /**< 均匀分布的上界（含） */
    int bin_width = 10;             /**< 直方图的箱宽 */
};
/* 三个分布各出一组样本；iota_sequence 另外给出 1、2、3……n */
std::vector<int> uniform_samples(std::uint64_t seed, std::size_t count,
                                 int low, int high);
std::vector<int> normal_samples(std::uint64_t seed, std::size_t count,
                                double mean, double stddev);
std::vector<int> discrete_samples(std::uint64_t seed, std::size_t count);
std::vector<int> iota_sequence(std::size_t n);
/** 一组样本的统计量，全部由 <numeric> 的三个算法算出 */
struct Stats {
    std::size_t count = 0;
    long long sum = 0;              /**< accumulate */
    double mean = 0.0;
    long long square_sum = 0;       /**< inner_product，自己与自己内积 */
    double variance = 0.0;          /**< 平方和 / count − 均值² */
    double stddev = 0.0;
    int minimum = 0;
    int maximum = 0;
    std::vector<long long> prefix;  /**< partial_sum，第 i 项是前 i+1 项之和 */
};
Stats compute_stats(const std::vector<int> &values);
/** 一个箱。区间是闭区间 [low, high]，最后一个箱可能超出样本最大值 */
struct Bin { int low = 0; int high = 0; std::size_t count = 0; };
struct Histogram {
    std::vector<Bin> bins;
    std::size_t total = 0;
    std::size_t peak = 0;       /**< 各箱计数的最大值，画条形图时当标尺 */
    int bin_width = 1;
};
Histogram make_histogram(const std::vector<int> &values, int bin_width);
std::string render_histogram(const Histogram &histogram, int bar_width = 40);
/** 同规模取数之后，rand() 与 mt19937_64 各自可观察到的差异 */
struct RandCompare {
    std::size_t low_bit_limit = 0;      /**< 低位周期检查到多大的步长为止 */
    std::size_t rand_low_bit_period = 0;/**< rand() 低 1 位的重复步长，0 表示没测到 */
    std::size_t mt_low_bit_period = 0;  /**< mt19937_64 同样检查的结果 */
    int fold_modulus = 0;               /**< 把取值折叠到多少个值上 */
    std::size_t fold_samples = 0;
    std::size_t rand_first_half = 0;
    std::size_t mt_first_half = 0;
    int rand_first_percent = 0;         /**< 落在前半区间的百分比 */
    int mt_first_percent = 0;
};
RandCompare compare_with_rand(std::uint64_t seed);
/** 一次完整运行的结果：数据、统计量、直方图，以及排好版的四段文本 */
struct Report {
    Options options;
    std::vector<int> uniform;   /**< 均匀样本；GUI 按它画柱子 */
    std::vector<int> normal;
    std::vector<int> discrete;
    Stats stats;
    Histogram histogram;
    RandCompare rand_compare;
    std::string data_text;      /**< 三个分布各自的样本与说明 */
    std::string stats_text;     /**< <numeric> 算出来的统计量 */
    std::string histogram_text; /**< 分箱表与文本条形图 */
    std::string rand_text;      /**< 与 rand() 的对照与结论 */
};
Report make_report(const Options &options);
/** 把 "20240601" 这样的文本解析成种子。失败时返回 false 并写明原因 */
bool parse_seed(const std::string &text, std::uint64_t &seed, std::string &error);
/** 自测结果。不打印，交给界面决定怎么显示 */
struct CheckResult {
    std::vector<std::string> lines;     /**< 每项一行，例如 "[通过] 3. ……" */
    std::size_t passed = 0;
    std::size_t failed = 0;
    bool all_passed() const { return failed == 0; }
    std::string summary() const;        /**< "16 项中 16 项通过，全部通过" */
};
CheckResult run_self_tests();

}   /* namespace demo */

#endif /* STATS_DEMO_HPP */
