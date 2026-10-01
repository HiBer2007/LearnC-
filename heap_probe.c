/* heap_probe.c   自己实现 _sbrk（照 sysmem.c 的写法），验证裸机上的「brk 系统调用」
   编译：
     arm-none-eabi-gcc -mcpu=cortex-m3 -mthumb -O2 -g -nostartfiles --specs=nosys.specs \
       -T STM32F103C8_FLASH.ld startup_stm32f103xe.s heap_probe.c -o heap_ok.elf
   去掉边界检查的版本：同一份源码加 -DNO_LIMIT_CHECK */
#include <errno.h>
#include <stddef.h>
#include <stdint.h>

extern uint8_t _end;              /* 链接脚本给的堆起点：.bss 之后 */
extern uint8_t _estack;           /* 链接脚本给的 RAM 顶端 */
extern uint32_t _Min_Stack_Size;  /* 链接脚本给栈预留的字节数 */

volatile unsigned g_result[8] = { 0x5A5A0000u, 0, 0, 0, 0, 0, 0, 0 };
static uint8_t *heap_end;

/* 裸机上的 _sbrk：没有内核可问，只能自己在 RAM 里往上挪一个高水位标记 */
void *_sbrk(ptrdiff_t incr)
{
    if (heap_end == NULL) {
        heap_end = &_end;
    }
#if defined(NO_LIMIT_CHECK)
    uint8_t *prev = heap_end;
    heap_end += incr;
    return prev;
#else
    const uint8_t *max_heap =
        (const uint8_t *)((uint32_t)&_estack - (uint32_t)&_Min_Stack_Size);
    if (heap_end + incr > max_heap) {
        errno = ENOMEM;
        return (void *)-1;
    }
    uint8_t *prev = heap_end;
    heap_end += incr;
    return prev;
#endif
}

int main(void)
{
    unsigned n = 0, last = 0, refused = 0;

    g_result[0] = (unsigned)(uintptr_t)&_end;
    g_result[1] = (unsigned)(uintptr_t)&_estack;
    g_result[2] = (unsigned)&_Min_Stack_Size;

    for (;;) {
        void *p = _sbrk(1024);
        if (p == (void *)-1) {
            refused = 1;
            break;
        }
        last = (unsigned)(uintptr_t)p;
        ++n;
        if (n >= 40) {          /* 保险：最多要 40 KiB，别真的跑飞 */
            break;
        }
    }
    g_result[3] = n;            /* 被接受了几次 1 KiB 的请求 */
    g_result[4] = last;         /* 最后交出去的那块地址 */
    g_result[5] = refused;      /* 1 = 触到边界被拒 */

    g_result[6] = 1;            /* 打算往最后那块写一个字 */
    *(volatile unsigned *)last = 0x600D600Du;
    g_result[7] = 0x600D600Du;  /* 写成功了才会到这里 */

    for (;;) {
    }
}
