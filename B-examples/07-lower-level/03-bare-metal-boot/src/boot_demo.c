/* boot_demo.c —— 03-bare-metal-boot 的核心程序（单文件）
 *
 * 它做的事只有一件：把「Reset_Handler 到底做了什么」一条条验出来。
 *
 * 编译（WSL Ubuntu）：
 *   arm-none-eabi-gcc -mcpu=cortex-m3 -mthumb -O2 -g -Wall -Wextra \
 *     -nostartfiles --specs=nosys.specs \
 *     -T linker/STM32F103C8_FLASH.ld linker/startup_stm32f103xe.s src/boot_demo.c \
 *     -o boot_demo.elf
 *
 * QEMU 里跑：
 *   qemu-system-arm -M netduinoplus2 -kernel boot_demo.elf -nographic \
 *     -semihosting-config enable=on,target=native
 *
 * 真板上跑：见 scripts/flash.ps1。
 *
 * 结果同时写进一个固定的 RAM 地址（0x20004000），
 * 这样即使程序卡住、打印不出来，调试器也能直接读回结论。
 */
#include <stdint.h>

#include "semihosting.h"

/* ==================== 链接脚本给出的符号 ==================== */
/* 这些不是变量，是链接器在脚本里算出来的地址。声明成外部符号即可取地址。 */
extern uint32_t _estack; /* RAM 顶端，向量表第 0 个字 */
extern uint32_t _sidata; /* .data 的 LMA：初值在 flash 里的位置 */
extern uint32_t _sdata;  /* .data 的 VMA 起点（RAM） */
extern uint32_t _edata;  /* .data 的 VMA 终点 */
extern uint32_t _sbss;   /* .bss 起点 */
extern uint32_t _ebss;   /* .bss 终点 */

/* 真实启动文件里的复位入口；取它的地址就能验向量表第 1 个字 */
extern void Reset_Handler(void);

/* ==================== 内存范围（照这块板写） ==================== */
#define FLASH_BASE 0x08000000u
#define FLASH_END 0x08010000u /* 64 KiB */
#define RAM_BASE 0x20000000u
#define RAM_END 0x20005000u /* 20 KiB */

/* 结果区：固定地址，调试器不必查符号表就能读
 *   0x20004000 是 RAM 靠后的一段，本示例的 .bss 只有几十字节、栈也只有几百字节，
 *   因此不会撞上。工程里更稳妥的做法是让链接脚本专门划一段出来。 */
#define RESULT_BASE 0x20004000u
#define RESULTS ((volatile uint32_t *)RESULT_BASE)
#define RESULT_MAGIC 0x600D0703u

#define RESULT_MAGIC_IDX 0u
#define RESULT_PASS_IDX 1u
#define RESULT_TOTAL_IDX 2u
#define RESULT_ESTACK_IDX 3u
#define RESULT_SDATA_IDX 4u
#define RESULT_SIDATA_IDX 5u
#define RESULT_SBSS_IDX 6u
#define RESULT_EBSS_IDX 7u

/* ==================== 三个探针对象 ==================== */
/* 有初值：落在 .data —— 上电时初值还在 flash 里，由 Reset_Handler 搬到 RAM */
volatile uint32_t g_from_data = 0x1234ABCDu;
/* 无初值：落在 .bss —— 不占 flash，由 Reset_Handler 清零 */
volatile uint32_t g_from_bss;
/* 常量：落在 flash，不上 RAM */
const char g_rodata[] = "in-flash";

/* 真实启动文件会调用它们；这里给空实现 */
void SystemInit(void) { }
void _init(void) { }

/* ==================== 自测框架 ==================== */
static int g_pass;
static int g_fail;

static void check(int ok, const char *what) {
    if (ok) {
        ++g_pass;
    } else {
        ++g_fail;
    }
    sh_puts(ok ? "[PASS] " : "[FAIL] ");
    sh_u32((uint32_t)(g_pass + g_fail));
    sh_puts(". ");
    sh_puts(what);
    sh_puts("\r\n");
}

static int in_flash(uint32_t a) { return a >= FLASH_BASE && a < FLASH_END; }
static int in_ram(uint32_t a) { return a >= RAM_BASE && a < RAM_END; }

int main(void) {
    const uint32_t *vectors = (const uint32_t *)FLASH_BASE;
    uint32_t sp_now = (uint32_t)(uintptr_t)&vectors; /* 占位，下面用汇编写真值 */
    uint32_t i;

    (void)sp_now;

    g_pass = 0;
    g_fail = 0;

    sh_puts("boot_demo on Cortex-M3 (STM32F103C8: 64 KiB flash / 20 KiB RAM)\r\n");
    sh_puts("--- 向量表的前 8 个字 ---\r\n");
    for (i = 0u; i < 8u; ++i) {
        sh_puts("  [");
        sh_u32(i);
        sh_puts("] 0x");
        sh_hex32(vectors[i]);
        sh_puts("\r\n");
    }
    sh_puts("--- 自测 ---\r\n");

    /* ---- 向量表：硬件复位后固定从前两个字取栈顶与入口 ---- */
    check(vectors[0] == (uint32_t)(uintptr_t)&_estack,
          "vector[0] is _estack (the linker script put it there)");
    check((vectors[1] & 1u) == 1u, "vector[1] has bit 0 set (Thumb entry)");
    check(vectors[1] == ((uint32_t)(uintptr_t)&Reset_Handler | 1u),
          "vector[1] is Reset_Handler | 1");

    /* ---- 栈顶 ---- */
    check((uint32_t)(uintptr_t)&_estack == RAM_END,
          "_estack equals the top of RAM (0x20005000)");
    check(((uint32_t)(uintptr_t)&_estack & 7u) == 0u,
          "_estack is 8-byte aligned (AAPCS requires it)");

    /* ---- .data：初值从 flash 搬到了 RAM ---- */
    check(g_from_data == 0x1234ABCDu, "g_from_data holds its initial value");
    check(in_ram((uint32_t)(uintptr_t)&g_from_data),
          "g_from_data lives in RAM (that is what .data means)");
    check(*(const uint32_t *)(uintptr_t)&_sidata == 0x1234ABCDu,
          "the same value is still readable in flash at _sidata");
    check((uint32_t)(uintptr_t)&_sidata != (uint32_t)(uintptr_t)&_sdata,
          "_sidata and _sdata differ: that is what AT> FLASH buys");

    /* ---- .bss：不占 flash，启动时清零 ---- */
    check(g_from_bss == 0u, "g_from_bss is zero (cleared by Reset_Handler)");
    check(in_ram((uint32_t)(uintptr_t)&g_from_bss), "g_from_bss lives in RAM");

    /* ---- 段的相对位置 ---- */
    check(in_flash((uint32_t)(uintptr_t)&_sidata),
          "_sidata is inside flash (LMA of .data)");
    check(in_ram((uint32_t)(uintptr_t)&_sdata) &&
              in_ram((uint32_t)(uintptr_t)&_edata),
          ".data VMA range is inside RAM");
    check(in_ram((uint32_t)(uintptr_t)&_sbss) &&
              in_ram((uint32_t)(uintptr_t)&_ebss),
          ".bss range is inside RAM");
    check((uint32_t)(uintptr_t)&_sbss >= (uint32_t)(uintptr_t)&_edata,
          ".bss starts at or after the end of .data (no overlap)");
    check((uint32_t)(uintptr_t)&_ebss <= (uint32_t)(uintptr_t)&_estack,
          ".bss ends below the stack top");

    /* ---- 代码与常量都在 flash 里 ---- */
    check(in_flash((uint32_t)(uintptr_t)&Reset_Handler),
          "Reset_Handler runs from flash");
    check(in_flash((uint32_t)(uintptr_t)&main), "main runs from flash");
    check(in_flash((uint32_t)(uintptr_t)g_rodata),
          "the string literal lives in flash, not in RAM");
    check(g_rodata[0] == 'i' && g_rodata[1] == 'n',
          "the string literal is readable");

    /* ---- 当前栈指针 ---- */
    {
        uint32_t sp;
        __asm__ volatile("mov %0, sp" : "=r"(sp));
        check(in_ram(sp), "the current stack pointer is inside RAM");
        check(sp <= (uint32_t)(uintptr_t)&_estack,
              "the current stack pointer is at or below _estack");
    }

    /* ---- 把结论写进固定地址，调试器可以直接读 ---- */
    RESULTS[RESULT_MAGIC_IDX] = RESULT_MAGIC;
    RESULTS[RESULT_PASS_IDX] = (uint32_t)g_pass;
    RESULTS[RESULT_TOTAL_IDX] = (uint32_t)(g_pass + g_fail);
    RESULTS[RESULT_ESTACK_IDX] = (uint32_t)(uintptr_t)&_estack;
    RESULTS[RESULT_SDATA_IDX] = (uint32_t)(uintptr_t)&_sdata;
    RESULTS[RESULT_SIDATA_IDX] = (uint32_t)(uintptr_t)&_sidata;
    RESULTS[RESULT_SBSS_IDX] = (uint32_t)(uintptr_t)&_sbss;
    RESULTS[RESULT_EBSS_IDX] = (uint32_t)(uintptr_t)&_ebss;

    sh_puts("--- 结果 ---\r\n");
    sh_puts("result block at 0x20004000: pass=");
    sh_u32((uint32_t)g_pass);
    sh_puts(" total=");
    sh_u32((uint32_t)(g_pass + g_fail));
    sh_puts("\r\n");
    sh_puts("_estack=0x");
    sh_hex32((uint32_t)(uintptr_t)&_estack);
    sh_puts("  _sdata=0x");
    sh_hex32((uint32_t)(uintptr_t)&_sdata);
    sh_puts("  _sidata=0x");
    sh_hex32((uint32_t)(uintptr_t)&_sidata);
    sh_puts("\r\n_sbss=0x");
    sh_hex32((uint32_t)(uintptr_t)&_sbss);
    sh_puts("  _ebss=0x");
    sh_hex32((uint32_t)(uintptr_t)&_ebss);
    sh_puts("\r\n");

    sh_u32((uint32_t)(g_pass + g_fail));
    sh_puts(" 项中 ");
    sh_u32((uint32_t)g_pass);
    sh_puts(" 项通过");
    if (g_fail == 0) {
        sh_puts("，全部通过\r\n");
    } else {
        sh_puts("，");
        sh_u32((uint32_t)g_fail);
        sh_puts(" 项失败\r\n");
    }
    sh_puts("ALL DONE\r\n");

    for (;;) {
    }
}
