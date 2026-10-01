/* hello.c   同一份源码，两种链接方式（--specs=nosys.specs 与 --specs=rdimon.specs）
   编译（真板 / QEMU 通用）：
     arm-none-eabi-gcc -mcpu=cortex-m3 -mthumb -O2 -g -nostartfiles \
       --specs=nosys.specs -T STM32F103C8_FLASH.ld startup_stm32f103xe.s hello.c -o hello_nosys.elf
   结果集中放在 g_result 里，调试器用 mdw 一次读出来。 */
#include <stdio.h>
#include <stdlib.h>

volatile unsigned g_result[8] = { 0x5A5A0000u, 0, 0, 0, 0, 0, 0, 0 };

int main(void)
{
    void *p;

    g_result[0] = 0x5A5A0001u;                              /* 已经进了 main */
    g_result[1] = (unsigned)printf("hello from board\n");    /* printf 的返回值 */
    g_result[2] = (unsigned)fflush(stdout);                  /* 0 = 成功，EOF = 失败 */

    p = malloc(64);
    g_result[3] = (unsigned)(unsigned long)p;                /* malloc 给的地址 */
    if (p != NULL) {
        *(volatile unsigned *)p = 0xA5A5A5A5u;
        g_result[4] = *(volatile unsigned *)p;               /* 真的能读写才算数 */
    }
    g_result[5] = (unsigned)printf("second line\n");
    g_result[6] = (unsigned)fflush(stdout);

    for (;;) {
    }
}
