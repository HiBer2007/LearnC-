/* volatile_regs.h —— 示例 06-lower-level/02-volatile-and-registers 的核心接口
 *
 * 三件事：
 *   · 一块「模拟的寄存器区」，用来看清内存映射 I/O 的写法；
 *   · 同一个等待循环的两种写法（带 volatile / 不带），对照它们的行为差别；
 *   · 重复读同一个字，对照「每次真读」与「只读一次」。
 *
 * 主机上没有真正的外设寄存器，因此这里用一个普通全局量加一个线程，
 * 模拟「编译器看不见的写」。带 volatile 的那一份每次都真去读，读得到；
 * 不带的那一份在 -O2 下把读提出了循环，改内存也读不到。
 */
#ifndef VOLATILE_REGS_H
#define VOLATILE_REGS_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

/* ==================== 模拟的寄存器区 ==================== */

/* 四个寄存器，模型照 STM32 的 GPIO 那一组写：
 * 每个寄存器就是一个固定地址上的 32 位字。 */
enum vr_reg_id {
    VR_REG_CR = 0, /* 控制寄存器：可读可写 */
    VR_REG_SR = 1, /* 状态寄存器：只读，由设备改 */
    VR_REG_DR = 2, /* 数据寄存器：可读可写 */
    VR_REG_IR = 3, /* 中断标志：写 1 清一位 */
    VR_REG_COUNT = 4
};

typedef struct {
    const char *name;   /* 寄存器名 */
    uintptr_t addr;     /* 它在进程里的地址 */
    uint32_t reset;     /* 复位值 */
    const char *access; /* 访问属性 */
} vr_reg_info;

const vr_reg_info *vr_reg_table(size_t *count);
void vr_print_reg_table(FILE *out);

/* ==================== 两种等待写法 ==================== */

/* 等到状态寄存器的第 0 位置 1 为止。
 * spin_limit 是自旋上限：到了就返回 0，避免程序真的卡死。
 * 返回 0 表示「上限到了还没等到」，大于 0 表示「第几圈等到的」。 */
uint32_t vr_wait_volatile(uint32_t spin_limit);
uint32_t vr_wait_plain(uint32_t spin_limit);

/* ==================== 带看门狗的探针 ==================== */

typedef struct {
    int use_volatile;  /* 用的是哪一种写法 */
    int escaped;       /* 1 = 等到了，跳出了循环 */
    int timed_out;     /* 1 = 看门狗到点它还没出来 */
    uint32_t spins;    /* 跳出来时转了多少圈 */
    unsigned waited_ms; /* 从起跑到出结果经过了多少毫秒 */
} vr_probe_result;

/* 在一个新线程里跑等待循环，本线程睡 delay_ms 之后把状态位置 1。
 * timeout_ms 是看门狗上限。 */
vr_probe_result vr_probe_flag(int use_volatile, unsigned delay_ms, unsigned timeout_ms);

/* ==================== 重复读同一个字 ==================== */

/* 读同一个地址 n 次再相加。带 volatile 时每次都真读；
 * 不带时编译器会把它折成「读一次再乘 n」。 */
uint32_t vr_sum_reads(int use_volatile, uint32_t n);

/* 给数据寄存器写一个值（带 volatile 与不带 volatile 的两份一起写）。 */
void vr_dr_set(uint32_t value);

/* ==================== 自测 ==================== */

int vr_self_test(FILE *out);

#endif /* VOLATILE_REGS_H */
