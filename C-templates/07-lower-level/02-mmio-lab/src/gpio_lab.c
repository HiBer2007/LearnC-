/* gpio_lab.c —— 阶段 1、2、4 的 TODO 都在这个文件里
 *
 * 交付状态：下面的函数都是空壳或明显的占位值，能编译、能跑，
 * 但寄存器不会被改动，访问计数一直是 0。
 * 每做完一个 TODO，重新构建一次，对照《配置步骤.md》里该阶段的验收标准。
 */
#include "gpio_lab.h"
#include "regs.h"

/* ================================================================ 阶段 1 */

/* 阶段 1-1：读 ODR、置位、写回 */
void gl_set_pin(uint32_t pin)
{
    /* TODO 1-1 */
    (void)pin;
}

/* 阶段 1-2：读 ODR、清位、写回 */
void gl_clear_pin(uint32_t pin)
{
    /* TODO 1-2 */
    (void)pin;
}

/* 阶段 1-3：读 ODR、异或、写回 */
void gl_toggle_pin(uint32_t pin)
{
    /* TODO 1-3 */
    (void)pin;
}

/* 阶段 1-4：读 IDR 的第 pin 位 */
int gl_read_pin(uint32_t pin)
{
    /* TODO 1-4：占位实现，一律返回 0 */
    (void)pin;
    return 0;
}

/* ================================================================ 阶段 2 */

/* 阶段 2-1：只写 BSRR，不许读 ODR */
void gl_set_pin_fast(uint32_t pin)
{
    /* TODO 2-1 */
    (void)pin;
}

/* 阶段 2-2：只写 BRR，不许读 ODR */
void gl_clear_pin_fast(uint32_t pin)
{
    /* TODO 2-2 */
    (void)pin;
}

/* ================================================================ 阶段 4 */

/* 阶段 4-1：把 mask 圈出来的位换成 value */
void gl_write_field(uint32_t offset, uint32_t mask, unsigned shift, uint32_t value)
{
    /* TODO 4-1 */
    (void)offset;
    (void)mask;
    (void)shift;
    (void)value;
}

/* 阶段 4-2：读出 mask 圈出来的那几位 */
uint32_t gl_read_field(uint32_t offset, uint32_t mask, unsigned shift)
{
    /* TODO 4-2：占位实现，返回一个一眼就知道不对的值 */
    (void)offset;
    (void)mask;
    (void)shift;
    return 0xFFFFFFFFu;
}
