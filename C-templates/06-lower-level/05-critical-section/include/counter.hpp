/* counter.hpp —— 练习模板 05-critical-section 的接口
 *
 * 共享计数器在 counter.cpp 里，中断服务程序也定义在那里。
 * 四种自增方式对应四个阶段：
 *
 *   阶段 1  bump_unprotected()   读—改—写三步，中间给中断留了入口：丢更新
 *   阶段 2  bump_irq_guard()     用 IrqGuard 关中断（RAII，恢复之前的状态）
 *   阶段 3  bump_critical()      用 CriticalSection 临界区（可嵌套）
 *   阶段 4  bump_atomic()        用 std::atomic 的读-改-写，不关中断
 *
 * 还有两个只用来量代价的版本，见阶段 4。
 */
#ifndef COUNTER_HPP
#define COUNTER_HPP

#include <cstdint>

namespace lab {

/* 把计数器清零（中断模型也要跟着复位，见 counter.cpp） */
void reset(void);

/* 计数器当前值，读的时候用 relaxed 语义就够：这里只关心最后那个数 */
std::uint32_t value(void);

/* 中断服务程序被调用了几次（用来算「期望值」） */
unsigned isr_calls(void);

/* ---------------------------------------------------------------- 阶段 1 */

/* 无保护的自增：读出来、给中断留一个入口、再写回去。
 * 中断正好落在中间时，那一次增量就丢了。 */
void bump_unprotected(void);

/* ---------------------------------------------------------------- 阶段 2 */

/* 关中断：构造时保存「之前是开还是关」，析构时恢复回去。
 * 不能无脑开中断——要是调用者本来就关着中断，析构把中断打开就错了。 */
class IrqGuard {
public:
    IrqGuard(void);
    ~IrqGuard(void);
    IrqGuard(const IrqGuard &) = delete;
    IrqGuard &operator=(const IrqGuard &) = delete;

private:
    /* 构造时记下「之前中断是开还是关」，析构时照原样恢复 */
    bool was_enabled_;
};

void bump_irq_guard(void);

/* ---------------------------------------------------------------- 阶段 3 */

/* 临界区：与 IrqGuard 用同一个原语，但额外维护一个嵌套层数，
 * 只有最外层退出时才真的开中断。 */
class CriticalSection {
public:
    CriticalSection(void);
    ~CriticalSection(void);
    CriticalSection(const CriticalSection &) = delete;
    CriticalSection &operator=(const CriticalSection &) = delete;

    /* 当前嵌套了几层（0 表示不在临界区里） */
    static unsigned depth(void);
};

void bump_critical(void);

/* ---------------------------------------------------------------- 阶段 4 */

/* 原子自增：一条读-改-写完成，中间不给中断留缝。
 * relaxed 只保证原子性，seq_cst 还要保证全局顺序，两者代价不同。 */
void bump_atomic_relaxed(void);
void bump_atomic_seq_cst(void);

}  // namespace lab

#endif  // COUNTER_HPP
