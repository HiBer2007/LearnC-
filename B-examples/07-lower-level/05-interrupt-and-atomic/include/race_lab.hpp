/* race_lab.hpp —— 示例 07-lower-level/05-interrupt-and-atomic 的核心接口
 *
 * 实验模型：一个「主循环」与一个「中断」同时给同一个计数器加一。
 * 两边各做固定次数的读-改-写，然后数一数最终值少了多少。
 *
 * 三种写法：
 *   plain     直接 ++：读-改-写不是一步，两边的操作会互相盖掉
 *   spinlock  两边都先拿一把自旋锁（临界区）
 *   atomic    用原子量的 fetch_add，一条指令做完
 *
 * 「关中断」（Cortex-M 上的 cpsid i / cpsie i）在主机上没有对应物，
 * 它在 board/race_board.c 里演示。
 */
#ifndef RACE_LAB_HPP
#define RACE_LAB_HPP

#include <cstdint>
#include <iosfwd>

namespace rl {

enum class mode {
    plain = 0,   /* 不加保护 */
    spinlock = 1, /* 临界区：自旋锁 */
    atomic_ = 2   /* 原子量 */
};

const char *mode_name(mode m);

struct result {
    const char *name;
    std::uint64_t main_ops; /* 主循环做了多少次自增 */
    std::uint64_t isr_ops;  /* 「中断」做了多少次自增 */
    std::uint64_t expected; /* 应有的值 = main_ops + isr_ops */
    std::uint64_t actual;   /* 计数器最终值 */
    std::uint64_t lost;     /* expected - actual */
    double ms;              /* 耗时 */
};

/* 两边同时开工，各做固定次数的自增，等两边都结束再读计数器。 */
result run(mode m, std::uint64_t main_ops, std::uint64_t isr_ops);

void print_result(std::ostream &os, const result &r);
void print_explanation(std::ostream &os);

/* 自测：返回失败的项数（0 表示全部通过）。 */
int self_test(std::ostream &os);

} /* namespace rl */

#endif /* RACE_LAB_HPP */
