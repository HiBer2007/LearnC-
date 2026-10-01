/* sim_irq.hpp —— 确定性的中断模型（素材，不要改）
 *
 * 真板上的中断是硬件随时打进来的，因此「丢更新」这件事每次跑的数字都不一样。
 * 为了让练习可复现，这里把中断变成两件可控的事：
 *
 *   request_irq()   外设拉了一根中断线（相当于真板上的 NVIC 挂起位）
 *   poll()          给中断一个入口，相当于「主循环的两条指令之间」
 *
 * 关中断（disable_irq）之后，request_irq 照样把挂起位置起来，
 * 但 poll() 什么都不做；等开中断时那个挂起的中断立刻被派发。
 * 这就是真板上 PRIMASK 的行为：中断不会丢，只是晚一点执行。
 *
 * 与真板的对应关系见《06-更底层/10-中断、并发与内存序.md》：
 * 真板上的一次测量是「主循环自增 20 万次 + SysTick 每 1000 周期中断一次」，
 * 普通计数器丢了 1459 次更新，原子计数器一次没丢。
 *
 * 这个头文件与 sim_irq.cpp 都是给定的，不需要改。
 */
#ifndef SIM_IRQ_HPP
#define SIM_IRQ_HPP

namespace sim {

/* 中断服务程序：由 counter.cpp 提供（中断里也要动那个计数器） */
using Handler = void (*)();

/* 复位模型：清掉挂起位、开中断、清零统计，并登记中断服务程序 */
void reset(Handler handler);

/* 外设请求一次中断：置挂起位 */
void request_irq(void);

/* 主循环给中断一个入口：如果挂着中断且没关中断，就调用一次中断服务程序 */
void poll(void);

/* 中断现在允许吗 */
bool irq_enabled(void);

/* 关中断 / 开中断。disable 可以嵌套（内部记层数），
 * enable 只减一层，减到 0 才真正开——真板上的 PRIMASK 就是这样配对的。 */
void disable_irq(void);
void enable_irq(void);

/* 还有挂着没处理的中断吗 */
bool pending(void);

/* 统计：中断服务程序被调用了几次、poll 被调用了几次 */
unsigned isr_calls(void);
unsigned long long poll_calls(void);

/* 统计清零（中断调用次数、poll 次数） */
void clear_stats(void);

}  // namespace sim

#endif  // SIM_IRQ_HPP
