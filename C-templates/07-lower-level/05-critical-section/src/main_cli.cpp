/* main_cli.cpp —— 命令行验收程序（已经写好，不需要改）
 *
 * 它做两件事：
 *   1. 每个阶段跑一轮「主循环自增 + 中断也自增」，把期望值与实际值并排打印；
 *      丢了多少次更新一眼就能看出来。
 *   2. 四种保护方式各量一次代价（这一轮不请求中断，量的是保护本身的开销）。
 *
 * 编译与运行：
 *   cmake --preset mingw-gdb
 *   cmake --build --preset mingw-gdb
 *   build\mingw\bin\app_cli.exe
 */
#include "counter.hpp"
#include "sim_irq.hpp"

#include <chrono>
#include <cstdio>

namespace {

/* 一轮对照实验的规模：主循环 4000 次，每 8 次迭代来一次中断 */
constexpr unsigned kIterations = 4000u;
constexpr unsigned kIrqEvery = 8u;

/* 代价测量的规模：每种方式 200 万次 */
constexpr unsigned kCostIterations = 2000000u;

void run_round(const char *title, void (*bump)(void))
{
    unsigned expected;
    unsigned actual;
    unsigned isr;

    lab::reset();
    sim::clear_stats();

    for (unsigned i = 0u; i < kIterations; ++i) {
        if ((i % kIrqEvery) == 0u) {
            sim::request_irq();
        }
        bump();
    }
    sim::poll();                       /* 把最后挂着的那个处理掉 */

    isr = lab::isr_calls();
    expected = kIterations + isr;      /* 主循环每次加一，中断每次也加一 */
    actual = lab::value();

    std::printf("=== %s ===\n", title);
    std::printf("  iterations : %u\n", kIterations);
    std::printf("  isr calls  : %u\n", isr);
    std::printf("  expected   : %u\n", expected);
    std::printf("  actual     : %u\n", actual);
    std::printf("  lost       : %u\n", expected - actual);
}

double measure_ns(void (*bump)(void))
{
    auto t0 = std::chrono::steady_clock::now();
    for (unsigned i = 0u; i < kCostIterations; ++i) {
        bump();
    }
    auto t1 = std::chrono::steady_clock::now();
    double ns = std::chrono::duration<double, std::nano>(t1 - t0).count();
    return ns / static_cast<double>(kCostIterations);
}

struct CostEntry {
    const char *name;
    void (*bump)(void);
};

}  // namespace

int main(void)
{
    std::printf("=== 阶段 1：无保护的自增（读—改—写，中间给中断留了入口）===\n");
    run_round("unprotected", &lab::bump_unprotected);

    std::printf("\n=== 阶段 2：关中断（IrqGuard，恢复之前的状态）===\n");
    run_round("irq guard", &lab::bump_irq_guard);

    std::printf("\n=== 阶段 3：临界区（CriticalSection，可嵌套）===\n");
    run_round("critical section", &lab::bump_critical);
    {
        /* 临界区可以嵌套：进两层再一层层出来，depth() 应当先升后降 */
        unsigned before;
        unsigned outer_depth;
        unsigned inner_depth;
        unsigned after;

        before = lab::CriticalSection::depth();
        {
            lab::CriticalSection outer;
            outer_depth = lab::CriticalSection::depth();
            {
                lab::CriticalSection inner;
                inner_depth = lab::CriticalSection::depth();
            }
        }
        after = lab::CriticalSection::depth();
        std::printf("  nesting depth: %u -> %u -> %u -> %u   (expect 0 -> 1 -> 2 -> 0)\n",
                    before, outer_depth, inner_depth, after);
    }

    std::printf("\n=== 阶段 4：原子自增（relaxed 与 seq_cst）===\n");
    run_round("atomic relaxed", &lab::bump_atomic_relaxed);
    run_round("atomic seq_cst", &lab::bump_atomic_seq_cst);

    /* ---------------------------------------------------------- 代价对照 */
    std::printf("\n=== 代价对照：%u 次自增，不请求中断 ===\n", kCostIterations);
    {
        const CostEntry table[] = {
            { "unprotected      ", &lab::bump_unprotected },
            { "irq guard        ", &lab::bump_irq_guard },
            { "critical section ", &lab::bump_critical },
            { "atomic relaxed   ", &lab::bump_atomic_relaxed },
            { "atomic seq_cst   ", &lab::bump_atomic_seq_cst },
        };

        for (const CostEntry &e : table) {
            lab::reset();
            std::printf("  %s : %6.2f ns/op\n", e.name, measure_ns(e.bump));
        }
    }

    std::printf("\n（骨架状态下每个 bump 都是空壳：计数器的值一直是 0，代价也接近 0）\n");
    return 0;
}
