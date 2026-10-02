/* counters.cpp —— 练习模板 01 的计数器定义（C++）
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
 *
 * ------------------------------------------------------------------
 * 头文件里只留声明，静态成员与两个钩子函数的定义放在这个文件里。
 * 这个文件是计数器，不是练习内容，不需要改。
 */
#include <stdexcept>

#include "dynarray.hpp"
#include "tracked.hpp"

/* ---------------------------------------------------------- 对象计数 */

long TrackedCounters::live = 0;
long TrackedCounters::copies = 0;
long TrackedCounters::moves = 0;
long TrackedCounters::throw_after = -1;

void TrackedCounters::reset(void)
{
    live = 0;
    copies = 0;
    moves = 0;
    throw_after = -1;
}

void tracked_set_throw_after(long n)
{
    TrackedCounters::throw_after = n;
}

void tracked_maybe_throw(void)
{
    if (TrackedCounters::throw_after < 0) {
        return;
    }
    if (TrackedCounters::throw_after == 0) {
        TrackedCounters::throw_after = -1;      /* 只抛一次，免得后续操作一直被它打断 */
        throw std::runtime_error("TrackedT: injected construction failure");
    }
    --TrackedCounters::throw_after;
}

/* ---------------------------------------------------------- 分配计数 */

long dyn::AllocStats::allocs = 0;
long dyn::AllocStats::deallocs = 0;
long dyn::AllocStats::bytes = 0;

void dyn::AllocStats::reset(void)
{
    allocs = 0;
    deallocs = 0;
    bytes = 0;
}
