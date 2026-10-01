/* main.c —— 应用程序（已给出，不需要改）
 *
 * 它只报三件事，三件事都要靠链接脚本与启动文件配合才能成立：
 *   1. Reset_Handler 真的跑到了，并且进了 main；
 *   2. .data 里的全局量拿到了初值（初值原本在 flash 里，是 Reset_Handler 搬到 RAM 的）；
 *   3. .bss 里的全局量是 0（Reset_Handler 清过）。
 *
 * 另外打印三个由链接脚本定义的符号，用来看 .data 的两套地址：
 *   _sidata  .data 在 flash 里的装载地址（LMA）
 *   _sdata   .data 在 RAM 里的运行地址（VMA）
 *   _ebss    .bss 的结束地址
 *
 * 编译与运行见 build.sh；输出走 semihosting，不依赖任何外设。
 */
#include <stdint.h>

/* 有初值的全局：初值放在 flash 里，运行时要被搬到 RAM */
volatile uint32_t g_from_data = 0x1234ABCDu;

/* 无初值的全局：运行时应当是 0 */
volatile uint32_t g_from_bss;
volatile uint32_t g_another_bss;

/* 这三个符号由链接脚本定义，它们的「地址」就是脚本里算出来的值 */
extern uint32_t _sidata;
extern uint32_t _sdata;
extern uint32_t _ebss;

/* semihosting：调试通道，QEMU 直接接到标准输出，不依赖任何外设。
 * r0 / r1 用汇编显式装好：用 register 变量绑寄存器在 -O2 下第二次调用会失效。 */
static void sh_puts(const char *s)
{
    __asm__ volatile ("movs r0, #4\n\t"      /* SYS_WRITE0 */
                      "mov  r1, %0\n\t"
                      "bkpt 0xAB\n\t"
                      : : "r"(s) : "r0", "r1", "memory");
}

static void sh_hex32(uint32_t v)
{
    static const char hex[] = "0123456789abcdef";
    char buf[11];
    int  i;

    buf[0] = '0';
    buf[1] = 'x';
    for (i = 0; i < 8; ++i) {
        buf[2 + i] = hex[(v >> (28 - 4 * i)) & 0xFu];
    }
    buf[10] = '\0';
    sh_puts(buf);
}

static void sh_dec(uint32_t v)
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

/* 启动文件要求这两个符号存在 */
void SystemInit(void) { }
void _init(void) { }

int main(void)
{
    sh_puts("linker-script-lab: Reset_Handler ran, we are in main\r\n");

    sh_puts("g_from_data = ");
    sh_hex32(g_from_data);
    sh_puts("  (expect 0x1234abcd)\r\n");

    sh_puts("g_from_bss  = ");
    sh_hex32(g_from_bss);
    sh_puts("  (expect 0x00000000)\r\n");

    sh_puts("g_another_bss = ");
    sh_hex32(g_another_bss);
    sh_puts("  (expect 0x00000000)\r\n");

    sh_puts("_sidata = ");
    sh_hex32((uint32_t)(uintptr_t)&_sidata);
    sh_puts("  (flash: 0x0800....)\r\n");

    sh_puts("_sdata  = ");
    sh_hex32((uint32_t)(uintptr_t)&_sdata);
    sh_puts("  (RAM:   0x2000....)\r\n");

    sh_puts("_ebss   = ");
    sh_hex32((uint32_t)(uintptr_t)&_ebss);
    sh_puts("\r\n");

    sh_puts("done\r\n");

    for (;;) {
        /* 停在这里，别让 main 返回 */
    }
}
