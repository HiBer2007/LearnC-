/* tool_fixtures.c —— 内嵌的样例输出，供自测使用
 *
 * 这些都是真实工具的真实输出，从本示例与 03-bare-metal-boot 的产物上抄下来的。
 * 把样例内嵌进来，自测就不依赖本机装没装 binutils——解析逻辑可以被稳定地验证。
 */
#include "bin_tools.h"

/* objdump -h 一段真实输出（03 示例那份裸机镜像的节选） */
const char bt_fixture_objdump_h[] =
    "\n"
    "Sections:\n"
    "Idx Name          Size      VMA       LMA       File off  Algn\n"
    "  0 .isr_vector   000001e4  08000000  08000000  00001000  2**0\n"
    "                  CONTENTS, ALLOC, LOAD, READONLY, DATA\n"
    "  1 .text         000005a4  080001e4  080001e4  000011e4  2**2\n"
    "                  CONTENTS, ALLOC, LOAD, READONLY, CODE\n"
    "  2 .data         00000004  20000000  08000ca0  00002000  2**2\n"
    "                  CONTENTS, ALLOC, LOAD, DATA\n"
    "  3 .bss          00000028  20000004  08000ca4  00002004  2**2\n"
    "                  ALLOC\n"
    "  4 .ARM.attributes 0000002f  00000000  00000000  00002204  2**0\n"
    "                  CONTENTS, READONLY\n";

/* nm 一段真实输出：有代码、有数据、有未定义、有弱符号、有局部符号 */
const char bt_fixture_nm[] =
    "20005000 R _estack\n"
    "20000004 B _sbss\n"
    "20000000 D _sdata\n"
    "08000ca0 A _sidata\n"
    "00000000 T main\n"
    "00000000 T Reset_Handler\n"
    "00000000 W SystemInit\n"
    "00000000 T ll_self_test\n"
    "00000000 t same_image\n"
    "00000000 U printf\n"
    "00000000 U memcpy\n"
    "0800079c R g_rodata\n";

/* size 的两种格式：GNU 的有 dec/hex 两列，Berkeley 的没有 */
const char bt_fixture_size_gnu[] =
    "   text\t   data\t    bss\t    dec\t    hex\tfilename\n"
    "   3232\t      4\t   1548\t   4784\t   12b0\tboot_demo.elf\n";

const char bt_fixture_size_multi[] =
    "   text    data     bss     dec     hex filename\n"
    "   3232       4    1548    4784    12b0 boot_demo.elf\n"
    "  46016     272    2976   49264    c070 app_cli.exe\n";

/* readelf -S 一段真实输出（节选） */
const char bt_fixture_readelf_s[] =
    "There are 6 section headers, starting at offset 0x22d4:\n"
    "\n"
    "Section Headers:\n"
    "  [Nr] Name              Type            Addr     Off    Size   ES Flg Lk Inf Al\n"
    "  [ 0]                   NULL            00000000 000000 000000 00      0   0  0\n"
    "  [ 1] .isr_vector       PROGBITS        08000000 001000 0001e4 00   A  0   0  1\n"
    "  [ 2] .text             PROGBITS        080001e4 0011e4 0005a4 00  AX  0   0  4\n"
    "  [ 3] .data             PROGBITS        20000000 002000 000004 00  WA  0   0  4\n"
    "  [ 4] .bss              NOBITS          20000004 002004 000028 00  WA  0   0  4\n"
    "  [ 5] .comment          PROGBITS        00000000 00202c 000011 01  MS  0   0  1\n";
