/**
 * main_cli.cpp —— 练习模板 06 的命令行验收程序（C++）
 *
 * 这个文件**不需要改**：它按 4 个阶段调用 bench.cpp 里的函数，
 * 把结果打印成《配置步骤.md》里的验收输出。你的实现写在 src/bench.cpp 里。
 *
 * 构建与运行（在模板目录下）：
 *     cmake --preset mingw-gdb
 *     cmake --build --preset mingw-gdb
 *     build\mingw\bin\app_cli.exe
 *
 * 阶段的划分：
 *     阶段 1  duration 与单位换算
 *     阶段 2  steady_clock 多次测量与分位数
 *     阶段 3  system_clock 与本地时间格式
 *     阶段 4  atomic 计数与 sleep_for 校准
 */
#include <chrono>
#include <iomanip>
#include <iostream>
#include <string>

#include "bench.hpp"

namespace {

/* 阶段 2 的重复次数与工作量：数字调大能看得更清楚，也更容易看出测量噪声 */
constexpr int  kSamples = 9;
constexpr long kWork    = 200000L;

bool looks_like_timestamp(const std::string &s)
{
    if (s.size() != 19) {
        return false;
    }
    const char *pattern = "0000-00-00 00:00:00";
    for (std::size_t i = 0; i < s.size(); ++i) {
        const char c = s[i];
        if (pattern[i] == '-') {
            if (c != '-') { return false; }
        } else if (pattern[i] == ' ') {
            if (c != ' ') { return false; }
        } else if (pattern[i] == ':') {
            if (c != ':') { return false; }
        } else if (c < '0' || c > '9') {
            return false;
        }
    }
    return true;
}

} /* namespace */

int main()
{
    /* ---------------------------------------------------------- 阶段 1 */
    std::cout << "=== Stage 1: durations and conversions ===\n";
    std::cout << "1500 ms as seconds   : " << std::fixed << std::setprecision(3)
              << bench::seconds_of_ms(1500) << "\n";
    std::cout << "2 s as milliseconds  : " << bench::ms_of_seconds(2) << "\n";
    std::cout << "1 s + 500 ms         : " << bench::add_ms(1000, 500) << " ms\n";
    std::cout << "1500 ms as us        : " << bench::us_of_ms(1500) << "\n";
    std::cout << std::defaultfloat << std::setprecision(6);

    /* ---------------------------------------------------------- 阶段 2 */
    std::cout << "\n=== Stage 2: steady_clock, repeated measurement ===\n";
    std::cout << "workload        : n = " << kWork << "\n";
    const bench::Timing t = bench::measure(kSamples, kWork);
    std::cout << "samples         : " << t.samples << "\n";
    std::cout << std::fixed << std::setprecision(3);
    std::cout << "min / median / max : " << t.min_ms << " / " << t.median_ms
              << " / " << t.max_ms << " ms\n";
    std::cout << std::defaultfloat << std::setprecision(6);
    std::cout << "monotonic_ok    : "
              << ((t.samples > 0 && t.min_ms <= t.median_ms && t.median_ms <= t.max_ms)
                      ? "yes" : "no")
              << "\n";
    std::cout << "note            : the milliseconds change on every run\n";

    /* ---------------------------------------------------------- 阶段 3 */
    std::cout << "\n=== Stage 3: system_clock and calendar ===\n";
    const std::string now_text = bench::format_now();
    std::cout << "now (local)     : " << now_text << "\n";
    std::cout << "format_ok       : " << (looks_like_timestamp(now_text) ? "yes" : "no") << "\n";
    std::cout << "epoch seconds   : " << bench::epoch_seconds() << "\n";
    std::cout << "note            : system_clock can be adjusted, use steady_clock to time code\n";

    /* ---------------------------------------------------------- 阶段 4 */
    std::cout << "\n=== Stage 4: atomic counter and sleep ===\n";
    const long long counted = bench::count_with_atomic(200000L);
    std::cout << "counter         : " << counted << "\n";
    std::cout << "sleep asked     : 5 ms\n";
    const double slept = bench::sleep_ms_measured(5);
    std::cout << std::fixed << std::setprecision(3);
    std::cout << "sleep measured  : " << slept << " ms\n";
    std::cout << "at least asked  : " << ((slept >= 5.0) ? "yes" : "no") << "\n";
    std::cout << std::defaultfloat << std::setprecision(6);

    return 0;
}
