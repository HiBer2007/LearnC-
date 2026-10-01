/* volatile_poll.c —— 阶段 3：Cortex-M 上的 volatile 对照实验
 *
 * 做的事：SysTick 每 1 ms 中断一次，中断里给 g_ticks 加一，
 * 到第三次之后把 g_ready 置 1；主循环在 while (g_ready == 0) 里等它。
 *
 * 编译两个版本（只差一个宏，见 run_qemu.sh）：
 *   -DUSE_VOLATILE=1  g_ready 带 volatile：主循环每次都真的去读
 *   -DUSE_VOLATILE=0  少了 volatile：编译器把读提到循环外，循环变成 b .
 *
 * 这个文件不参与主机侧的 CMake 构建，它是交叉编译的素材。
 */
#include <stdint.h>

#ifndef USE_VOLATILE
#define USE_VOLATILE 1
#endif

#if USE_VOLATILE
#define SHARED volatile          /* 中断与主循环共用的对象：必须让编译器知道它会变 */
#else
#define SHARED
#endif

/* 中断里改写、主循环里读的标志 */
SHARED uint32_t g_ready;

/* 中断里数节拍 */
volatile uint32_t g_ticks;

/* 内核寄存器：SysTick（Cortex-M3 内核自带，不是板级外设） */
#define SYST_CSR (*(volatile uint32_t *)0xE000E010u)
#define SYST_RVR (*(volatile uint32_t *)0xE000E014u)
#define SYST_CVR (*(volatile uint32_t *)0xE000E018u)

/* semihosting：调试通道，QEMU 直接接到标准输出，不依赖任何外设。
 *
 * 这里把 r0 / r1 用汇编显式装好，而不是用 register 变量绑寄存器：
 * 后者在 -O2 下会在第二次调用时被优化掉（实测：同一份代码 -O0 正常、
 * -O2 第二次 bkpt 时 r0 不是 4，QEMU 报 Unsupported SemiHosting SWI）。 */
static void sh_puts(const char *s)
{
    __asm__ volatile ("movs r0, #4\n\t"      /* SYS_WRITE0 */
                      "mov  r1, %0\n\t"
                      "bkpt 0xAB\n\t"
                      : : "r"(s) : "r0", "r1", "memory");
}

static void sh_u32(uint32_t v)
{
    char b[12];
    int  i = 12;

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

/* 启动文件的向量表把 SysTick_Handler 声明成 weak，
 * 这里给一个强定义就覆盖了它——《06-更底层/06-符号与链接属性.md》第 4 节的做法。 */
void SysTick_Handler(void)
{
    ++g_ticks;
    if (g_ticks >= 3u) {
        g_ready = 1u;
        SYST_CSR = 0u;       /* 够了就停表：不停的话 ticks 会随打印时机漂移 */
    }
}

static void systick_start(void)
{
    SYST_RVR = 8000u;        /* 8 MHz 的内核时钟下大约 1 ms 一次 */
    SYST_CVR = 0u;
    SYST_CSR = 7u;           /* ENABLE | TICKINT | 使用内核时钟 */
}

/* 启动文件要求这两个符号存在 */
void SystemInit(void) { }
void _init(void) { }

int main(void)
{
    sh_puts("stage 3: waiting for the SysTick interrupt\r\n");
    systick_start();

    while (g_ready == 0u) {
        /* 空转等中断；带 volatile 时这一句会反复读内存 */
    }

    SYST_CSR = 0u;           /* 先关掉 SysTick：semihosting 打印期间不希望被打断 */

    sh_puts("ready: g_ready == 1\r\n");
    sh_puts("ticks: ");
    sh_u32(g_ticks);
    sh_puts("\r\n");

    for (;;) {
        /* 停在这里，别让 main 返回 */
    }
}
