/**
 * benchmark.hpp —— 用 <chrono> 做小基准，不依赖任何界面
 *
 * 命令行版（src/main_cli.cpp）链接它。这里的函数不打印，
 * 只负责计时、统计并排好文本；该显示成什么样子由调用方决定。
 */
#ifndef BENCHMARK_HPP
#define BENCHMARK_HPP

#include <atomic>
#include <cstddef>
#include <string>
#include <vector>

namespace bench {

/* ── 被测的三段纯计算 ─────────────────────────────────── */

/** 被测函数的签名：算一遍，把迭代次数记进计数器，返回一个校验和 */
using Body = long long (*)(std::atomic<long long> &counter, std::size_t scale);
struct Case {
    std::string name;       /**< 命令行里用的名字，纯 ASCII */
    std::string title;      /**< 报表里的中文说明 */
    std::size_t scale = 0;  /**< 每轮的规模 */
    Body body = nullptr;
};
const std::vector<Case> &all_cases();
/** 按名字找用例，找到时把指针写进 found 并返回 true */
bool find_case(const std::string &name, const Case *&found);
long long sum_of_squares(std::atomic<long long> &counter, std::size_t scale);
long long prime_count(std::atomic<long long> &counter, std::size_t scale);
long long matrix_multiply(std::atomic<long long> &counter, std::size_t scale);
/* ── 统计量 ───────────────────────────────────────────── */

/** 一组耗时样本的统计量，单位是毫秒。P90 与 P99 用「排序后向上取整」定位 */
struct Stats {
    std::size_t samples = 0;
    double minimum_ms = 0.0;
    double maximum_ms = 0.0;
    double median_ms = 0.0;
    double p90_ms = 0.0;
    double p99_ms = 0.0;
    double mean_ms = 0.0;
    double stddev_ms = 0.0;
};
Stats summarize(std::vector<double> samples_ms);
double percentile(const std::vector<double> &sorted_ms, double percent);
/* ── 两个时钟的分辨率对照 ─────────────────────────────── */

/** 连续读取两个时钟若干次，数一数各自出现了多少个不同的值 */
struct ClockProbe {
    std::size_t reads = 0;
    std::size_t steady_distinct = 0;
    std::size_t system_distinct = 0;
    double steady_span_ms = 0.0;    /**< 这一串读取本身跨了多少毫秒 */
};
ClockProbe probe_clocks(std::size_t reads = 1000);
/* ── 测量 ─────────────────────────────────────────────── */

/** 一个用例测完之后的全部结果 */
struct RunResult {
    std::string name;
    std::string title;
    std::size_t repeats = 0;
    std::size_t warmups = 0;
    long long iterations = 0;   /**< 一轮的迭代次数，由 atomic 计数器给出 */
    long long checksum = 0;     /**< 最后一轮的返回值，用来确认计算真的发生了 */
    double single_ms = 0.0;     /**< 只测一次得到的值 */
    double first_ms = 0.0;      /**< 多次测量里的第 1 次 */
    Stats stats;
};
double measure_once(const Case &item, std::atomic<long long> &counter,
                    long long &checksum);
RunResult measure_case(const Case &item, std::size_t repeats, std::size_t warmups);
/* ── 报表 ─────────────────────────────────────────────── */

struct Options {
    std::size_t repeats = 11;           /**< 正式测量多少轮 */
    std::size_t warmups = 2;            /**< 预热多少轮，结果丢掉 */
    std::vector<std::string> cases;     /**< 要跑哪些用例，空表示全部 */
};
struct Report {
    Options options;
    ClockProbe clocks;
    std::vector<RunResult> runs;
    std::string method_text;    /**< 为什么用 steady_clock、为什么要预热与重复 */
    std::string table_text;     /**< 单次测量与多次测量的对照表 */
    std::string detail_text;    /**< 每个用例的迭代次数与完整分位数 */
};
Report make_report(const Options &options);
/* ── 自测 ─────────────────────────────────────────────── */

struct CheckResult {
    std::vector<std::string> lines;     /**< 每项一行，例如 "[通过] 3. ……" */
    std::size_t passed = 0;
    std::size_t failed = 0;
    bool all_passed() const { return failed == 0; }
    std::string summary() const;        /**< "15 项中 15 项通过，全部通过" */
};
CheckResult run_self_tests();
}   /* namespace bench */
#endif /* BENCHMARK_HPP */
