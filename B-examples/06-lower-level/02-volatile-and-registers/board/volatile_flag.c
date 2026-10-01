/* volatile_flag.c —— 02-volatile-and-registers 的真板 / QEMU 演示
 *
 * 一段三行的等待循环，只差一个 volatile，行为完全不同。
 *
 * 编译（WSL Ubuntu；本目录里有 stm32f103c8_min.ld 与 startup_min.s）：
 *   # 触发器一：SysTick 中断置位（QEMU 与真板都能自动跑完）
 *   arm-none-eabi-gcc -mcpu=cortex-m3 -mthumb -O2 -g -nostartfiles --specs=nosys.specs \
 *     -T stm32f103c8_min.ld startup_min.s volatile_flag.c -o flag_systick_vol.elf
 *   # 同一个文件加一个宏，去掉 volatile
 *   ... -DNO_VOLATILE ... -o flag_systick_nv.elf
 *
 *   # 触发器二：调试器用 mww 改内存（照真板实测第六节的做法）
 *   ... 不加 -DTRIGGER_SYSTICK ... -o flag_mww_vol.elf
 *   ... -DNO_VOLATILE ...        -o flag_mww_nv.elf
 *
 * 结果全部写进 g_results[]，程序跑完停在死循环里，从 RAM 读回即可：
 *   g_results[0] 1 = 跳出了等待循环，0 = 还在等
 *   g_results[1] 等待了多少个内核周期（DWT_CYCCNT 差值）
 *   g_results[2] g_flag 的地址（调试器 mww 要用它）
 *   g_results[3] 标识字 0x600D0702，确认读到的就是本程序
 *   g_results[4] SysTick 中断进来过多少次
 */
#include <stdint.h>

/* ==================== 内核与外设寄存器 ==================== */
#define DEMCR      (*(volatile uint32_t *)0xE000EDFCu)
#define DWT_CTRL   (*(volatile uint32_t *)0xE0001000u)
#define DWT_CYCCNT (*(volatile uint32_t *)0xE0001004u)
#define SYST_CSR   (*(volatile uint32_t *)0xE000E010u)
#define SYST_RVR   (*(volatile uint32_t *)0xE000E014u)
#define SYST_CVR   (*(volatile uint32_t *)0xE000E018u)

/* ==================== semihosting ==================== */
/* 最小板上没有串口，打印走 semihosting：bkpt 0xAB + SYS_WRITE0，
 * QEMU 直接接到 stdout，OpenOCD 打在它自己的控制台上。 */
static void sh_puts(const char *s) {
    register uint32_t r0 __asm__("r0") = 0x04u;
    register const char *r1 __asm__("r1") = s;
    __asm__ volatile("bkpt 0xAB" : "+r"(r0) : "r"(r1) : "memory");
}

static void sh_u32(uint32_t v) {
    char b[12];
    int i = 12;
    b[--i] = '\0';
    if (v == 0u) {
        b[--i] = '0';
    }
    while (v != 0u) {
        b[--i] = (char)('0' + (v % 10u));
        v /= 10u;
    }
    sh_puts(&b[i]);
}

static void sh_hex32(uint32_t v) {
    static const char hex[] = "0123456789abcdef";
    char b[11];
    int i;
    for (i = 0; i < 8; ++i) {
        b[i] = hex[(v >> (28 - 4 * i)) & 0xFu];
    }
    b[8] = '\0';
    sh_puts(b);
}

/* ==================== 被等的那个标志 ==================== */

/* 这一对 #ifdef 就是整个演示：下面那一行去掉 volatile，程序就再也出不来。
 * 加 -DNO_VOLATILE 编译出的镜像，除了这一个关键字之外与另一份逐字节同源。 */
#ifdef NO_VOLATILE
uint32_t g_flag;
#else
volatile uint32_t g_flag;
#endif

volatile uint32_t g_results[8];
volatile uint32_t g_ticks;

/* SysTick 中断：在中断里改 g_flag。中断什么时候进来，编译器不知道。 */
void SysTick_Handler(void) {
    ++g_ticks;
    g_flag = 1u;
}

void SystemInit(void) { }

int main(void) {
    uint32_t t0;

    /* DWT 周期计数器：用它量等待了多久 */
    DEMCR |= (1u << 24);
    DWT_CYCCNT = 0u;
    DWT_CTRL |= 1u;

    g_results[0] = 0u;
    g_results[1] = 0u;
    g_results[2] = (uint32_t)(uintptr_t)&g_flag;
    g_results[3] = 0x600D0702u;
    g_results[4] = 0u;

#ifdef TRIGGER_SYSTICK
    sh_puts("trigger: SysTick\r\n");
#else
    sh_puts("trigger: debugger (mww)\r\n");
#endif

    sh_puts("waiting for g_flag at 0x");
    sh_hex32(g_results[2]);
    sh_puts("\r\n");

#ifdef TRIGGER_SYSTICK
    /* 使能必须放在所有打印之后：semihosting 的 bkpt 要花掉成千上万个周期，
     * 若在打印之前就把 SysTick 打开，中断会在打印期间就把标志置上，
     * 循环一次都不转就出去了，看不到要演示的现象。
     * SysTick 每 8000 个内核周期中断一次（8 MHz 下约 1 ms）。
     * 真板上它会一直跑：主循环即使卡住，中断也照进不误——
     * 因此 g_results[4]（中断次数）会一直涨，这是「内存改了，程序不知道」的旁证。 */
    SYST_RVR = 8000u - 1u;
    SYST_CVR = 0u;
    SYST_CSR = 7u;
#endif

    /* ======== 演示的核心：这三行 ======== */
    t0 = DWT_CYCCNT;
    while (g_flag == 0u) {
        /* 空循环。带 volatile 时每次都真去读；
         * 不带时 -O2 把读提到循环之外，这里就变成了 b . ——跳到自身。 */
    }
    g_results[0] = 1u;
    g_results[1] = DWT_CYCCNT - t0;
    /* =================================== */

    sh_puts("escaped, cycles = ");
    sh_u32(g_results[1]);
    sh_puts("\r\nALL DONE\r\n");

    for (;;) {
    }
}
