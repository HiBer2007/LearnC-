/* counter.cpp —— 阶段 1 到阶段 4 的 TODO 都在这个文件里
 *
 * 共享计数器与中断服务程序都定义在这里。中断服务程序是给定的：
 * 它统计自己被调用了几次，并给计数器加一。
 *
 * 交付状态：四个 bump 函数与两个 RAII 类的构造函数、析构函数都是空壳，
 * 能编译、能链接、能跑，但计数器的值一直是 0。
 */
#include "counter.hpp"
#include "sim_irq.hpp"

#include <atomic>

namespace lab {
namespace {

std::atomic<std::uint32_t> g_counter{0u};
unsigned                   g_isr_calls = 0;
unsigned                   g_cs_depth = 0;

/* 中断服务程序：中断里也自增同一个计数器。
 * 这一步是给定的：竞争来自主循环那一侧怎么保护。 */
void isr_bump(void)
{
    ++g_isr_calls;
    g_counter.fetch_add(1u, std::memory_order_relaxed);
}

}  // namespace

void reset(void)
{
    g_counter.store(0u, std::memory_order_relaxed);
    g_isr_calls = 0;
    g_cs_depth = 0;
    sim::reset(&isr_bump);
}

std::uint32_t value(void)
{
    return g_counter.load(std::memory_order_relaxed);
}

unsigned isr_calls(void)
{
    return g_isr_calls;
}

/* ================================================================ 阶段 1 */

void bump_unprotected(void)
{
    /* TODO 1-1：三步都在这里写
     *   1) 把计数器的值读进一个局部变量；
     *   2) 调用 sim::poll()，给中断一个入口；
     *   3) 把「局部变量 + 1」写回计数器。
     * 第 2 步与第 3 步之间被中断插进来时，那一次增量就丢了。 */
}

/* ================================================================ 阶段 2 */

IrqGuard::IrqGuard(void)
    : was_enabled_(false)
{
    /* TODO 2-1：记下现在中断是不是开着的，然后关中断 */
}

IrqGuard::~IrqGuard(void)
{
    /* TODO 2-2：只有之前是开着的才开回去 */
}

void bump_irq_guard(void)
{
    /* TODO 2-3：用 IrqGuard 包住阶段 1 那三步 */
}

/* ================================================================ 阶段 3 */

CriticalSection::CriticalSection(void)
{
    /* TODO 3-1：层数加一；从 0 变 1 时才真的关中断 */
}

CriticalSection::~CriticalSection(void)
{
    /* TODO 3-2：层数减一；减到 0 时才真的开中断 */
}

unsigned CriticalSection::depth(void)
{
    return g_cs_depth;
}

void bump_critical(void)
{
    /* TODO 3-3：用 CriticalSection 包住阶段 1 那三步 */
}

/* ================================================================ 阶段 4 */

void bump_atomic_relaxed(void)
{
    /* TODO 4-1：用 fetch_add 一步完成自增（relaxed 语义），随后照样调用 sim::poll() */
}

void bump_atomic_seq_cst(void)
{
    /* TODO 4-2：同上，内存序换成 seq_cst */
}

}  // namespace lab
