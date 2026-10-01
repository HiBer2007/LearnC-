/* gpio_lab.h —— 练习模板 02-mmio-lab 的接口
 *
 * 阶段 1、2、4 的实现都写在 src/gpio_lab.c 里。
 * 阶段 3 是 Cortex-M 侧的对照实验，代码在 arm/volatile_poll.c。
 */
#ifndef GPIO_LAB_H
#define GPIO_LAB_H

#include <stdint.h>

/* ---------------------------------------------------------------- 阶段 1 */

/* 阶段 1-1
 * 把 pin 对应的输出位**置 1**：读出 ODR、或上这一位、写回。
 * 一次调用应当留下「读 1 次、写 1 次」的记录。 */
void gl_set_pin(uint32_t pin);

/* 阶段 1-2
 * 把 pin 对应的输出位**清 0**：同样用读-改-写。 */
void gl_clear_pin(uint32_t pin);

/* 阶段 1-3
 * 把 pin 对应的输出位**翻转**：读-改-写，用异或。 */
void gl_toggle_pin(uint32_t pin);

/* 阶段 1-4
 * 读输入寄存器 IDR 的第 pin 位，返回 0 或 1。 */
int gl_read_pin(uint32_t pin);

/* ---------------------------------------------------------------- 阶段 2 */

/* 阶段 2-1
 * 只写一次 BSRR 就把输出位置 1，**不许读 ODR**。
 * 做完之后 g_reg_reads 里不该多出任何一次读。 */
void gl_set_pin_fast(uint32_t pin);

/* 阶段 2-2
 * 只写一次 BRR 就把输出位清 0，同样不许读。 */
void gl_clear_pin_fast(uint32_t pin);

/* ---------------------------------------------------------------- 阶段 4 */

/* 阶段 4-1
 * 通用字段写入：把 offset 这个寄存器里 mask 圈出来的那几位换成 value。
 * 例如 gl_write_field(REG_CRL, 0x000000F0u, 4u, 0x3u)
 * 就是把 CRL 的 bit7..4 写成 0011，其余位不动。
 * 这种操作必须读-改-写：一次调用应当留下「读 1 次、写 1 次」。 */
void gl_write_field(uint32_t offset, uint32_t mask, unsigned shift, uint32_t value);

/* 阶段 4-2
 * 通用字段读出：返回 (寄存器 & mask) >> shift。
 * 占位实现返回 0xFFFFFFFF，一看就不对。 */
uint32_t gl_read_field(uint32_t offset, uint32_t mask, unsigned shift);

#endif /* GPIO_LAB_H */
