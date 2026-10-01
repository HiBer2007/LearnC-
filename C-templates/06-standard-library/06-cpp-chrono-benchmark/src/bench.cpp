/**
 * bench.cpp —— 练习模板 06 的核心逻辑（<chrono> 基准测试工具）
 *
 * 4 个阶段的实现都写在这个文件里。骨架给的是占位实现：
 * 能编译、能运行、结果明显不对（返回 0 或 -1）。
 * 各阶段的任务、验收标准与自查方法见同目录《配置步骤.md》。
 *
 * 构建与运行（在模板目录下）：
 *     cmake --preset mingw-gdb
 *     cmake --build --preset mingw-gdb
 *     build\mingw\bin\app_cli.exe
 */
#include "bench.hpp"

#include <algorithm>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <thread>
#include <vector>

namespace bench {

/* ==================================================================
 * 阶段 1：duration 与单位换算
 * ================================================================== */

/* TODO（阶段 1-1）：秒换算成毫秒。
 *
 * 提示：
 *   std::chrono::milliseconds ms = std::chrono::duration_cast<std::chrono::milliseconds>(
 *       std::chrono::seconds(s));
 *   return ms.count();
 * duration_cast 是「截断」而不是四舍五入，这一点在 1.5 秒这类值上看得出来。 */
long long ms_of_seconds(long long s)
{
    (void)s;
    return -1;
}

/* TODO（阶段 1-2）：毫秒换算成秒，保留小数。
 *
 * 提示：不要用 duration_cast<seconds>（那会截断成整数）；
 *       用 std::chrono::duration<double>(std::chrono::milliseconds(ms)).count()。 */
double seconds_of_ms(long long ms)
{
    (void)ms;
    return -1.0;
}

/* TODO（阶段 1-3）：毫秒换算成微秒。
 * 提示：duration_cast<microseconds>，这是「往小单位换」，不会丢精度。 */
long long us_of_ms(long long ms)
{
    (void)ms;
    return -1;
}

/* TODO（阶段 1-4）：两个毫秒数相加。
 * 提示：可以直接用毫秒的 operator+，也可以各自转成 duration 再相加。 */
long long add_ms(long long a, long long b)
{
    (void)a;
    (void)b;
    return -1;
}

/* ==================================================================
 * 阶段 2：steady_clock 多次测量
 * ================================================================== */

long long workload(long n)
{
    /* 已给出：一段固定工作量，返回值必须被用到，否则会被优化掉。
     * 平台不同，long long 与 int 的宽度也不同，这里固定用 long long。 */
    long long acc = 0;
    for (long i = 1; i <= n; ++i) {
        acc = (acc + static_cast<long long>(i) * i) % 1000000007LL;
    }
    return acc;
}

/* TODO（阶段 2-1）：把 workload(n) 跑 samples 次，每次量一遍。
 *
 * 提示：
 *   1. 每次这样量：
 *        const auto t0 = std::chrono::steady_clock::now();
 *        const long long value = workload(n);
 *        const auto t1 = std::chrono::steady_clock::now();
 *        const double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
 *   2. 把 value 累加到一个变量里并在返回前「用掉」（例如累加到 Timing 之外的
 *      一个 static 变量），否则编译器可能把整段计算优化掉；
 *   3. 把所有耗时存进 std::vector<double>，排序后取最小、中位、最大；
 *      中位数按「排序后取中间那个」算即可（samples 是奇数时最省事）；
 *   4. samples 小于 1 时原样返回一个全 0 的 Timing。
 *
 * 验收：samples = 9、min <= median <= max，具体毫秒数每次运行都不同。 */
Timing measure(int samples, long n)
{
    (void)samples;
    (void)n;
    return Timing{};
}

/* ==================================================================
 * 阶段 3：system_clock 与日历
 * ================================================================== */

/* TODO（阶段 3-1）：把当前本地时间格式化成 "YYYY-MM-DD hh:mm:ss"。
 *
 * 提示：
 *   1. const auto now = std::chrono::system_clock::now();
 *   2. const std::time_t t = std::chrono::system_clock::to_time_t(now);
 *   3. std::tm tm_buf{}; 用 localtime_s(&tm_buf, &t)（MSVC 与 MinGW 都提供）
 *      或 localtime_r；localtime 返回的是内部静态对象的指针，多线程下不安全；
 *   4. std::ostringstream os; os << std::put_time(&tm_buf, "%Y-%m-%d %H:%M:%S");
 *
 * 验收：形如 2026-10-01 15:52:07（具体时间当然是当下）。 */
std::string format_now()
{
    return "(TODO stage 3-1: format_now not implemented yet)";
}

/* TODO（阶段 3-2）：当前时间的纪元秒数。
 * 提示：system_clock::to_time_t(system_clock::now()) 再转 long long。 */
long long epoch_seconds()
{
    return -1;
}

/* ==================================================================
 * 阶段 4：atomic 计数与 sleep_for
 * ================================================================== */

/* TODO（阶段 4-1）：跑一遍 workload(n)，把返回值加进计数器，返回计数器的值。
 *
 * 提示：
 *   1. Counter counter; counter.add(workload(n)); return counter.get();
 *   2. 计数器的意义有两个：一是防止整段计算被优化掉（结果要参与输出），
 *      二是演示「共享变量用原子量而不是普通 long」；
 *   3. 单线程里普通 long 也能得到同样的结果，原子量的价值在多线程下，
 *      接口用法见《06-标准库/B-10-内存与并发的基础设施.md》第 5 节。
 *
 * 验收：counter = 200000 对应的那个确定值（见《配置步骤.md》）。 */
long long count_with_atomic(long n)
{
    (void)n;
    return -1;
}

/* TODO（阶段 4-2）：sleep_for 之后量实际过去的时间。
 *
 * 提示：
 *   1. 起点用 steady_clock::now()；
 *   2. std::this_thread::sleep_for(std::chrono::milliseconds(requested_ms))；
 *   3. 终点减起点，转成毫秒的 double 返回；
 *   4. Windows 的定时器分辨率通常在 15 毫秒上下，请求 5 毫秒往往量出十几毫秒，
 *      这是等待类接口的正常现象，不是写错了。
 *
 * 验收：返回值 >= 请求值。 */
double sleep_ms_measured(long long requested_ms)
{
    (void)requested_ms;
    return -1.0;
}

} /* namespace bench */
