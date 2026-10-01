/* regs.h —— 模拟的寄存器区（练习模板 02-mmio-lab 的素材）
 *
 * 这块区域与 STM32F103 的 GPIOA 排布一致（偏移 0x00 到 0x18），
 * 但它在普通内存里，不在 0x40010800。真实代码里的写法是
 *
 *     #define GPIOA_ODR (*(volatile uint32_t *)0x4001080Cu)
 *
 * 这里换成两个访问函数，好处是**每次访问都被记一笔**，
 * 于是「读-改-写」与「一次写完成」的差别可以数出来：
 *
 *     sim_read(REG_ODR)           读一次 ODR，g_reg_reads[REG_ODR/4] 加一
 *     sim_write(REG_BSRR, mask)   写一次 BSRR，g_reg_writes[REG_BSRR/4] 加一
 *
 * 不要改这个头文件里的偏移与位定义：各阶段的验收标准按它们写。
 */
#ifndef REGS_H
#define REGS_H

#include <stdint.h>

/* 寄存器偏移：与 STM32F103 的 GPIOA 一致 */
#define REG_CRL   0x00u   /* 端口配置低寄存器 */
#define REG_CRH   0x04u   /* 端口配置高寄存器 */
#define REG_IDR   0x08u   /* 输入数据寄存器（只读） */
#define REG_ODR   0x0Cu   /* 输出数据寄存器 */
#define REG_BSRR  0x10u   /* 置位/复位寄存器（只写） */
#define REG_BRR   0x14u   /* 复位寄存器（只写） */
#define REG_LCKR  0x18u   /* 配置锁定寄存器 */

#define SIM_REG_COUNT 7u

/* 引脚位定义：位 0 是设备侧的就绪输入，位 1 与位 2 是两根输出 */
#define PIN_READY  (1u << 0)
#define PIN_OUT_1  (1u << 1)
#define PIN_OUT_2  (1u << 2)

/* 访问计数：下标是「偏移 / 4」 */
extern uint32_t g_reg_reads[SIM_REG_COUNT];
extern uint32_t g_reg_writes[SIM_REG_COUNT];

/* 读一个寄存器 */
uint32_t sim_read(uint32_t offset);

/* 写一个寄存器。BSRR 与 BRR 由设备按真实语义处理：
 *   BSRR 的低 16 位把 ODR 对应位置 1，高 16 位把 ODR 对应位清 0；
 *   BRR  的低 16 位把 ODR 对应位清 0。
 * 这两种写法都**不需要先读 ODR**。 */
void sim_write(uint32_t offset, uint32_t value);

/* 看一个寄存器当前的值，**不算一次访问**。
 * 这是调试器视角（真板上相当于 OpenOCD 的 mdw），用来看结果不干扰计数。 */
uint32_t sim_peek(uint32_t offset);

/* 把访问计数清零（只清计数，不动寄存器内容） */
void sim_access_clear(void);

/* 设备的复位：清空寄存器区与计数，内部时序归零 */
void sim_device_reset(void);

/* 设备侧看到的输出电平（这是设备自己的视角，不算一次寄存器访问） */
uint32_t sim_device_odr(void);

/* 设备从复位起一共被读过几次 IDR（用来解释就绪位为什么会出现） */
uint32_t sim_device_idr_reads(void);

#endif /* REGS_H */
