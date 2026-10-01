/* regs.c —— 模拟设备的实现（素材，不要改）
 *
 * 这里做的事：把「内存映射 I/O」搬到普通内存里，并给每次访问记一笔账。
 * 设备的时序用一个简单的规则代替时间：
 *   每读一次 IDR，设备往前走一步；第三步时把就绪位 PIN_READY 置起来。
 * 真实设备是靠时间推进的，这里用访问次数代替，好处是结果每次都一样。
 */
#include "regs.h"

#include <string.h>

static uint32_t s_regs[SIM_REG_COUNT];   /* 寄存器区 */
static uint32_t s_step;                  /* 设备内部走了几步 */
static uint32_t s_idr_reads;             /* IDR 一共被读过几次 */

uint32_t g_reg_reads[SIM_REG_COUNT];
uint32_t g_reg_writes[SIM_REG_COUNT];

static int reg_index(uint32_t offset)
{
    if (offset >= SIM_REG_COUNT * 4u) {
        return -1;
    }
    return (int)(offset / 4u);
}

uint32_t sim_read(uint32_t offset)
{
    int i = reg_index(offset);

    if (i < 0) {
        return 0u;
    }
    ++g_reg_reads[i];

    if (offset == REG_IDR) {
        /* 设备往前一步：第三步之后，就绪位读出来就是 1 */
        ++s_step;
        ++s_idr_reads;
        if (s_step >= 3u) {
            s_regs[i] |= PIN_READY;
        }
    }
    return s_regs[i];
}

void sim_write(uint32_t offset, uint32_t value)
{
    int i = reg_index(offset);

    if (i < 0) {
        return;
    }
    ++g_reg_writes[i];

    switch (offset) {
    case REG_BSRR:
        /* 低 16 位：把 ODR 对应位置 1；高 16 位：把 ODR 对应位清 0 */
        s_regs[REG_ODR / 4u] |= (value & 0xFFFFu);
        s_regs[REG_ODR / 4u] &= ~((value >> 16) & 0xFFFFu);
        break;
    case REG_BRR:
        /* 低 16 位：把 ODR 对应位清 0 */
        s_regs[REG_ODR / 4u] &= ~(value & 0xFFFFu);
        break;
    case REG_IDR:
        /* IDR 是只读的，写它没有效果 */
        break;
    default:
        s_regs[i] = value;
        break;
    }
}

uint32_t sim_peek(uint32_t offset)
{
    int i = reg_index(offset);

    return (i < 0) ? 0u : s_regs[i];
}

void sim_access_clear(void)
{
    memset(g_reg_reads, 0, sizeof g_reg_reads);
    memset(g_reg_writes, 0, sizeof g_reg_writes);
}

void sim_device_reset(void)
{
    memset(s_regs, 0, sizeof s_regs);
    s_step = 0u;
    s_idr_reads = 0u;
    sim_access_clear();
}

uint32_t sim_device_odr(void)
{
    return s_regs[REG_ODR / 4u];
}

uint32_t sim_device_idr_reads(void)
{
    return s_idr_reads;
}
