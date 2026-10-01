/* semihosting.h —— 最小板上的「打印」通道
 *
 * 最小系统板上没有串口，唯一的输出通道是 semihosting：
 * 程序执行 bkpt 0xAB，调试器（QEMU 或 OpenOCD）截住它，
 * 按 r0 里的功能号办事。SYS_WRITE0（0x04）把 r1 指向的字符串写出去。
 *
 * 这套接口来自 ARM 的 semihosting 规范，与机型无关：
 * QEMU 加 -semihosting-config enable=on,target=native，
 * OpenOCD 加 arm semihosting enable，两边都能看到输出。
 *
 * 写成 static inline 是为了让这个示例只有两个源文件。
 */
#ifndef SEMIHOSTING_H
#define SEMIHOSTING_H

#include <stdint.h>

/* 把一个以 '\0' 结尾的字符串写到调试器那一侧 */
static inline void sh_puts(const char *s) {
    register uint32_t r0 __asm__("r0") = 0x04u; /* SYS_WRITE0 */
    register const char *r1 __asm__("r1") = s;
    __asm__ volatile("bkpt 0xAB" : "+r"(r0) : "r"(r1) : "memory");
}

/* 打印一个无符号十进制数 */
static inline void sh_u32(uint32_t v) {
    char b[12];
    int i = 12;
    b[--i] = '\0';
    if (v == 0u) {
        b[--i] = '0';
    }
    while (v != 0u) {
        b[--i] = (char)('0' + (int)(v % 10u));
        v /= 10u;
    }
    sh_puts(&b[i]);
}

/* 打印 8 位十六进制（不带 0x 前缀） */
static inline void sh_hex32(uint32_t v) {
    static const char hex[] = "0123456789abcdef";
    char b[9];
    int i;
    for (i = 0; i < 8; ++i) {
        b[i] = hex[(v >> (28 - 4 * i)) & 0xFu];
    }
    b[8] = '\0';
    sh_puts(b);
}

#endif /* SEMIHOSTING_H */
