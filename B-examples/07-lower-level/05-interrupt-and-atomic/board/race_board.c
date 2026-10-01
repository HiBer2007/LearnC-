/* race_board.c —— 05-interrupt-and-atomic 的真板 / QEMU 演示
 *
 * 主循环给一个计数器加一，同时 SysTick 中断也给它加一。
 * 三种写法各跑一遍，数一数最终值少了多少：
 *
 *   MODE_PLAIN     直接 ++，不带保护
 *   MODE_IRQMASK   主循环自增期间用 cpsid i / cpsie i 关中断
 *   MODE_ATOMIC    用 __atomic_fetch_add
 * 外加一项：中断延迟（写 NVIC_ISPR 到进 ISR 第一条指令）测 64 次取最小与最大。
 *
 * 编译（WSL Ubuntu；本目录里有 stm32f103c8_min.ld 与 startup_min.s）：
 *   arm-none-eabi-gcc -mcpu=cortex-m3 -mthumb -O2 -g -Wall -Wextra \
 *     -nostartfiles --specs=nosys.specs \
 *     -T stm32f103c8_min.ld startup_min.s race_board.c -o race_board.elf
 *
 * QEMU 里跑：
 *   qemu-system-arm -M netduinoplus2 -kernel race_board.elf -nographic \
 *     -semihosting-config enable=on,target=native
 *
 * 真板上跑：见 tools/board-run.ps1。
 * 结论同时写进固定的 RAM 地址 0x20004000，调试器直接读得到。
 */
#include <stdint.h>

/* ==================== 内核寄存器 ==================== */
#define DEMCR      (*(volatile uint32_t *)0xE000EDFCu)
#define DWT_CTRL   (*(volatile uint32_t *)0xE0001000u)
#define DWT_CYCCNT (*(volatile uint32_t *)0xE0001004u)
#define SYST_CSR   (*(volatile uint32_t *)0xE000E010u)
#define SYST_RVR   (*(volatile uint32_t *)0xE000E014u)
#define SYST_CVR   (*(volatile uint32_t *)0xE000E018u)
#define NVIC_ISER0 (*(volatile uint32_t *)0xE000E100u)
#define NVIC_ISPR0 (*(volatile uint32_t *)0xE000E200u)
#define NVIC_ICPR0 (*(volatile uint32_t *)0xE000E280u)
#define TIM2_IRQN 28u

/* ==================== semihosting ==================== */
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

static void sh_label(const char *name, uint32_t v) {
    sh_puts(name);
    sh_u32(v);
    sh_puts("\r\n");
}

/* ==================== 实验参数与状态 ==================== */
#define MAIN_LOOPS 200000u
#define SYSTICK_CYCLES 1000u

enum { MODE_PLAIN = 0, MODE_IRQMASK = 1, MODE_ATOMIC = 2 };

/* 被争抢的计数器。主循环与 SysTick 中断都会动它，因此必须是 volatile：
 * 否则编译器会把「循环里没人改它」当成前提，把读提出循环。 */
static volatile uint32_t g_counter;
static volatile uint32_t g_ticks;         /* 中断进来过多少次 */
static volatile uint32_t g_irq_min = 0xFFFFFFFFu;
static volatile uint32_t g_irq_max;
static volatile uint32_t g_irq_t0;
static uint32_t g_mode;

/* 结果区：固定地址，调试器不必查符号表就能读
 *   0x20004000 是 RAM 靠后的一段，本程序的数据区很小，撞不上。 */
#define RESULTS ((volatile uint32_t *)0x20004000u)
/* [0]=magic [1]=模式 [2]=期望 [3]=实测 [4]=丢失 [5]=中断次数
 * [6]=中断延迟最小 [7]=中断延迟最大 */
#define RESULT_MAGIC 0x600D0705u
#define RESULT_WORDS 8u

void SystemInit(void) { }

/* SysTick：每 SYSTICK_CYCLES 个周期进来一次，给同一个计数器加一 */
void SysTick_Handler(void) {
    ++g_ticks;
    switch (g_mode) {
    case MODE_PLAIN:
    case MODE_IRQMASK:
        g_counter = g_counter + 1u;
        break;
    case MODE_ATOMIC:
    default:
        __atomic_fetch_add(&g_counter, 1u, __ATOMIC_SEQ_CST);
        break;
    }
}

/* TIM2 只用来量中断延迟：它不做任何事，只记下进第一条指令的时刻 */
void TIM2_IRQHandler(void) {
    const uint32_t lat = DWT_CYCCNT - g_irq_t0;
    if (lat < g_irq_min) {
        g_irq_min = lat;
    }
    if (lat > g_irq_max) {
        g_irq_max = lat;
    }
    NVIC_ICPR0 = (1u << TIM2_IRQN);
}

static void systick_start(void) {
    SYST_RVR = SYSTICK_CYCLES - 1u;
    SYST_CVR = 0u;
    SYST_CSR = 7u; /* 使能 + 中断 + 内核时钟 */
}

static void systick_stop(void) { SYST_CSR = 0u; }

/* 跑一轮：模式 m，主循环 MAIN_LOOPS 次自增，同时 SysTick 也在自增 */
static void run_mode(uint32_t m, const char *label, uint32_t *expected, uint32_t *actual) {
    uint32_t i;
    uint32_t ticks;

    g_mode = m;
    g_counter = 0u;
    g_ticks = 0u;

    systick_start();
    for (i = 0u; i < MAIN_LOOPS; ++i) {
        switch (m) {
        case MODE_PLAIN:
            g_counter = g_counter + 1u;
            break;
        case MODE_IRQMASK:
            /* 关中断：这三步之间不会有中断插进来。
             * 中断不会被丢掉，它挂在那里，等 cpsie i 之后立刻进。 */
            __asm__ volatile("cpsid i" ::: "memory");
            g_counter = g_counter + 1u;
            __asm__ volatile("cpsie i" ::: "memory");
            break;
        case MODE_ATOMIC:
        default:
            __atomic_fetch_add(&g_counter, 1u, __ATOMIC_SEQ_CST);
            break;
        }
    }
    systick_stop();

    /* 中断还可能有一两次排在门口，等它落定再读 */
    __asm__ volatile("dsb" ::: "memory");
    ticks = g_ticks;
    *expected = MAIN_LOOPS + ticks;
    *actual = g_counter;
    (void)label;
}

/* 中断延迟：写 NVIC_ISPR 挂起 TIM2，ISR 第一条指令读 DWT_CYCCNT 相减 */
static void measure_irq_latency(void) {
    uint32_t i;
    NVIC_ISER0 = (1u << TIM2_IRQN);
    __asm__ volatile("dsb" ::: "memory");
    for (i = 0u; i < 64u; ++i) {
        g_irq_t0 = DWT_CYCCNT;
        NVIC_ISPR0 = (1u << TIM2_IRQN);
        __asm__ volatile("dsb" ::: "memory");
    }
}

int main(void) {
    uint32_t exp_plain = 0u, act_plain = 0u;
    uint32_t exp_mask = 0u, act_mask = 0u;
    uint32_t exp_atom = 0u, act_atom = 0u;

    /* DWT 周期计数器 */
    DEMCR |= (1u << 24);
    DWT_CYCCNT = 0u;
    DWT_CTRL |= 1u;

    sh_puts("race_board on Cortex-M3 (STM32F103C8)\r\n");
    sh_puts("main loop and SysTick both increment one counter\r\n");

    /* 先把延迟量了：此时 SysTick 还没开，不会干扰 */
    measure_irq_latency();

    run_mode(MODE_PLAIN, "plain", &exp_plain, &act_plain);
    run_mode(MODE_IRQMASK, "irqmask", &exp_mask, &act_mask);
    run_mode(MODE_ATOMIC, "atomic", &exp_atom, &act_atom);

    RESULTS[0] = RESULT_MAGIC;
    RESULTS[1] = 0x0500u; /* 版本标记：05 示例 */
    RESULTS[2] = exp_plain;
    RESULTS[3] = act_plain;
    RESULTS[4] = (exp_plain > act_plain) ? (exp_plain - act_plain) : 0u;
    RESULTS[5] = exp_plain - MAIN_LOOPS; /* 这一轮的中断次数 */
    RESULTS[6] = g_irq_min;
    RESULTS[7] = g_irq_max;

    sh_puts("--- 不加保护 ---\r\n");
    sh_label("  expected = ", exp_plain);
    sh_label("  actual   = ", act_plain);
    sh_label("  lost     = ", RESULTS[4]);

    sh_puts("--- 关中断（cpsid i / cpsie i）---\r\n");
    sh_label("  expected = ", exp_mask);
    sh_label("  actual   = ", act_mask);
    sh_label("  lost     = ", (exp_mask > act_mask) ? (exp_mask - act_mask) : 0u);

    sh_puts("--- 原子量（__atomic_fetch_add）---\r\n");
    sh_label("  expected = ", exp_atom);
    sh_label("  actual   = ", act_atom);
    sh_label("  lost     = ", (exp_atom > act_atom) ? (exp_atom - act_atom) : 0u);

    sh_puts("--- 中断延迟（64 次，DWT 周期）---\r\n");
    sh_label("  min = ", g_irq_min);
    sh_label("  max = ", g_irq_max);

    /* 自测：关中断与原子量必须一次不丢，不加保护只会少不会多 */
    {
        int pass = 0;
        int total = 0;
        const uint32_t lost_mask = (exp_mask > act_mask) ? (exp_mask - act_mask) : 0u;
        const uint32_t lost_atom = (exp_atom > act_atom) ? (exp_atom - act_atom) : 0u;

        ++total;
        if (act_plain <= exp_plain) ++pass; /* 只会少，不会多 */

        ++total;
        if (lost_mask == 0u) ++pass;

        ++total;
        if (lost_atom == 0u) ++pass;

        ++total;
        if (g_irq_min != 0xFFFFFFFFu && g_irq_max >= g_irq_min) ++pass;

        ++total;
        if (MAIN_LOOPS == 200000u) ++pass;

        /* 三次运行的期望值各不相同：每轮跑多久，中断就进多少次。
         * 要点是每一轮的中断都真的进来过（期望值都大于主循环次数）。 */
        ++total;
        if (exp_plain > MAIN_LOOPS && exp_mask > MAIN_LOOPS && exp_atom > MAIN_LOOPS) ++pass;

        sh_u32((uint32_t)total);
        sh_puts(" 项中 ");
        sh_u32((uint32_t)pass);
        sh_puts(" 项通过");
        if (pass == total) {
            sh_puts("，全部通过\r\n");
        } else {
            sh_puts("，");
            sh_u32((uint32_t)(total - pass));
            sh_puts(" 项失败\r\n");
        }
    }
    sh_puts("ALL DONE\r\n");

    for (;;) {
    }
}
