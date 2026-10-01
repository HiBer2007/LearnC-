/* race_lab.cpp —— 05-interrupt-and-atomic 的核心实现
 *
 * 编译：由 CMakeLists.txt 编成静态库 core，不直接编译这个文件。
 * 手工编译：
 *   g++ -std=c++17 -O2 -Wall -Wextra -Iinclude src/race_lab.cpp src/main_cli.cpp -o app_cli.exe
 */
#include "race_lab.hpp"

#include <atomic>
#include <chrono>
#include <iomanip>
#include <ostream>
#include <thread>

namespace rl {
namespace {

/* 被争抢的那个计数器，三种写法各一份，互不干扰 */
volatile std::uint64_t g_plain;
std::atomic<std::uint64_t> g_atomic{0};

/* 临界区用的自旋锁 */
std::atomic_flag g_lock = ATOMIC_FLAG_INIT;

/* 「中断」那边做了多少次 */
std::atomic<std::uint64_t> g_isr_ops{0};

/* 两边同时开工的发令枪 */
std::atomic<bool> g_go{false};

void lock_acquire(void) {
    while (g_lock.test_and_set(std::memory_order_acquire)) {
        /* 自旋。真实的中断处理里不能这么写：如果主循环正拿着锁，
         * 中断会一直等它放开，而它又要等中断返回——死锁。 */
    }
}

void lock_release(void) { g_lock.clear(std::memory_order_release); }

void bump(mode m) {
    switch (m) {
    case mode::plain:
        /* volatile 只保证「每次都真的读写内存」，
         * 不保证「读-改-写」是一步。中间随时可能被另一边插进来。 */
        g_plain = g_plain + 1u;
        break;
    case mode::spinlock:
        lock_acquire();
        g_plain = g_plain + 1u;
        lock_release();
        break;
    case mode::atomic_:
        g_atomic.fetch_add(1u, std::memory_order_seq_cst);
        break;
    }
}

/* 「中断」那一侧：它不受主循环控制，随时可能插进来 */
void isr_thread(mode m, std::uint64_t ops) {
    while (!g_go.load(std::memory_order_acquire)) {
        /* 等发令枪 */
    }
    for (std::uint64_t i = 0; i < ops; ++i) {
        bump(m);
    }
    g_isr_ops.store(ops, std::memory_order_release);
}

} /* namespace */

const char *mode_name(mode m) {
    switch (m) {
    case mode::plain:
        return "不加保护";
    case mode::spinlock:
        return "临界区（自旋锁）";
    case mode::atomic_:
        return "原子量 fetch_add";
    }
    return "?";
}

result run(mode m, std::uint64_t main_ops, std::uint64_t isr_ops) {
    result r{};
    r.name = mode_name(m);
    r.main_ops = main_ops;
    r.isr_ops = isr_ops;

    g_plain = 0u;
    g_atomic.store(0u, std::memory_order_seq_cst);
    g_lock.clear(std::memory_order_release);
    g_isr_ops.store(0u, std::memory_order_release);
    g_go.store(false, std::memory_order_release);

    const auto t0 = std::chrono::steady_clock::now();

    std::thread isr(isr_thread, m, isr_ops);
    g_go.store(true, std::memory_order_release);

    for (std::uint64_t i = 0; i < main_ops; ++i) {
        bump(m);
    }

    isr.join();
    const auto t1 = std::chrono::steady_clock::now();

    r.actual = (m == mode::atomic_) ? g_atomic.load(std::memory_order_seq_cst)
                                    : static_cast<std::uint64_t>(g_plain);
    r.expected = main_ops + isr_ops;
    r.lost = (r.expected > r.actual) ? (r.expected - r.actual) : 0u;
    r.ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    return r;
}

void print_result(std::ostream &os, const result &r) {
    os << "  " << std::left << std::setw(20) << r.name << std::right << " 期望 "
       << std::setw(10) << r.expected << "  实测 " << std::setw(10) << r.actual
       << "  丢了 " << std::setw(8) << r.lost << "  " << std::fixed
       << std::setprecision(1) << r.ms << " ms\n";
}

void print_explanation(std::ostream &os) {
    os << "  主循环与「中断」各做同样次数的自增，两边同时开工。\n"
          "  不加保护时，一次自增要读内存、加一、写回三步；\n"
          "  两边的三步交错在一起，后写回的那次把前一次的结果盖掉了。\n"
          "  临界区把这三步圈起来，原子量把这三步合成一条指令，两边都不丢。\n"
          "  主机上没有「关中断」这一说：那是 cpsid i / cpsie i 管的事，\n"
          "  Cortex-M 那一份（board/race_board.c）里有。\n";
}

/* ==================== 自测 ==================== */

namespace {

int g_pass;
int g_fail;

void report(std::ostream &os, bool ok, const char *what) {
    if (ok) {
        ++g_pass;
    } else {
        ++g_fail;
    }
    os << "  [" << (ok ? "通过" : "失败") << "] " << (g_pass + g_fail) << ". " << what
       << "\n";
}

void report_skip(std::ostream &os, const char *what) {
    ++g_pass;
    os << "  [跳过] " << (g_pass + g_fail) << ". " << what << "\n";
}

} /* namespace */

int self_test(std::ostream &os) {
    const std::uint64_t ops = 2000000u;
    g_pass = 0;
    g_fail = 0;
    os << "== 自测 ==\n";

    const result plain = run(mode::plain, ops, ops);
    const result spin = run(mode::spinlock, ops, ops);
    const result atom = run(mode::atomic_, ops, ops);

    report(os, plain.actual <= plain.expected,
           "不加保护：结果不会超过期望值（只会少，不会多）");
    report(os, spin.actual == spin.expected,
           "临界区：两边各 200 万次，一次都没丢");
    report(os, atom.actual == atom.expected,
           "原子量：两边各 200 万次，一次都没丢");
    report(os, plain.expected == spin.expected && spin.expected == atom.expected,
           "三种写法的期望值相同（同样的工作量才谈得上对照）");
    report(os, plain.ms > 0.0 && spin.ms > 0.0 && atom.ms > 0.0,
           "三次实验都真的跑起来了");

    /* 丢更新是竞态的结果，不是必然：单核或调度把两边错开时就可能一次不丢。
     * 因此这里只报告，不拿它当失败条件。 */
    if (plain.lost > 0u) {
        report(os, true, "不加保护：这一次确实丢了更新（竞态复现出来了）");
    } else {
        report_skip(os, "不加保护：这一次没丢更新（本机把两边错开了，不是代码对了）");
    }
    report(os, spin.actual >= atom.actual - 1u && atom.actual >= spin.actual - 1u,
           "两种修法的最终值一致（都是 400 万）");

    os << "\n  自测结果：" << (g_pass + g_fail) << " 项中 " << g_pass << " 项通过";
    if (g_fail == 0) {
        os << "，全部通过\n";
    } else {
        os << "，" << g_fail << " 项失败\n";
    }
    return g_fail;
}

} /* namespace rl */
