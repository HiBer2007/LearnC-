/**
 * bench.hpp —— 练习模板 06 的核心接口（C++）
 *
 * 接口已经定好，src/main_cli.cpp 按 4 个阶段调用它们。
 * 你要做的是在 src/bench.cpp 里把标了 TODO 的函数实现出来。
 *
 * 四个阶段的对应关系：
 *     阶段 1   ms_of_seconds、seconds_of_ms、
 *              us_of_ms、add_ms            duration 与单位换算
 *     阶段 2   workload、measure           steady_clock 多次测量
 *     阶段 3   format_now、epoch_seconds   system_clock 与日历
 *     阶段 4   Counter、count_with_atomic、
 *              sleep_ms_measured            atomic 计数与 sleep_for
 */
#ifndef BENCH_HPP
#define BENCH_HPP

#include <atomic>
#include <chrono>
#include <string>

namespace bench {

/* ==================================================================
 * 阶段 1：duration 与单位换算
 * ================================================================== */

/* 秒换算成毫秒：std::chrono::seconds -> milliseconds，用 duration_cast */
long long ms_of_seconds(long long s);

/* 毫秒换算成秒，带小数：用 std::chrono::duration<double>（不丢精度） */
double seconds_of_ms(long long ms);

/* 毫秒换算成微秒 */
long long us_of_ms(long long ms);

/* 两个毫秒数相加，返回毫秒：用来演示 1s + 500ms = 1500ms */
long long add_ms(long long a, long long b);

/* ==================================================================
 * 阶段 2：steady_clock 多次测量
 * ================================================================== */

/* 一段固定工作量：把 1 到 n 的平方按 1e9 取模后累加，返回累加值。
 * 它存在的意义是「有活干」：不许被编译器优化掉（返回值要参与输出）。 */
long long workload(long n);

struct Timing {
    int    samples = 0;
    double min_ms = 0.0;
    double median_ms = 0.0;
    double max_ms = 0.0;
};

/* 把 workload(n) 跑 samples 次，每次用 steady_clock 量一遍，返回统计量。
 * 为什么用 steady_clock 而不是 system_clock，见《06-标准库/B-06-时间：chrono.md》第 2 节。 */
Timing measure(int samples, long n);

/* ==================================================================
 * 阶段 3：system_clock 与日历
 * ================================================================== */

/* 当前本地时间，格式固定为 "YYYY-MM-DD hh:mm:ss"。
 * 提示：system_clock::now() -> to_time_t -> localtime -> std::put_time。 */
std::string format_now();

/* 当前时间的 Unix 纪元秒数（1970-01-01 00:00:00 UTC 起算） */
long long epoch_seconds();

/* ==================================================================
 * 阶段 4：atomic 计数与 sleep_for
 * ================================================================== */

/* 一个用 std::atomic 保护的计数器。多线程版本要用它，见《06-标准库/B-10-内存与并发的基础设施.md》第 5 节。 */
class Counter {
public:
    void add(long long n) { value_.fetch_add(n, std::memory_order_relaxed); }
    long long get() const { return value_.load(std::memory_order_relaxed); }

private:
    std::atomic<long long> value_{0};
};

/* 跑一遍 workload(n)，每完成一次就给计数器加上返回值。
 * 返回计数器的最终值：它同时起到「别让编译器把计算优化掉」的作用。 */
long long count_with_atomic(long n);

/* 先 sleep_for 指定的毫秒数，再用 steady_clock 量实际过去了多久，返回毫秒。
 * Windows 的定时器分辨率通常在 15 毫秒上下，因此实测值往往大于请求值。 */
double sleep_ms_measured(long long requested_ms);

} /* namespace bench */

#endif /* BENCH_HPP */
