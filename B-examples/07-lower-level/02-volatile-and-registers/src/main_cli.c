/* main_cli.c —— 02-volatile-and-registers 的命令行版
 *
 * 只做三件事：认参数、调 core、把结果按顺序打出来。
 *
 * 手工编译：
 *   gcc -std=c17 -O2 -Wall -Wextra -Iinclude src/volatile_regs.c src/main_cli.c -o app_cli.exe
 */
#include "volatile_regs.h"

#include <stdlib.h>
#include <string.h>

static const char *const k_usage =
    "用法：app_cli [选项]\n"
    "  （无选项）      打印寄存器表、两种写法的对照，最后跑自测\n"
    "  --selftest      只跑自测\n"
    "  --delay MS      「设备」等多少毫秒才置位（默认 50）\n"
    "  --quiet         不打前两段，只跑自测\n"
    "  --help          显示这段文字\n";

static void print_probe(FILE *out, const char *title, vr_probe_result r) {
    fprintf(out, "  %-28s 结果=%s  自旋=%lu 圈  用时=%u ms\n", title,
            r.escaped ? "读到了，跳出来" : "没读到，一直等",
            (unsigned long)r.spins, r.waited_ms);
}

int main(int argc, char **argv) {
    unsigned delay_ms = 50u;
    int only_selftest = 0;
    int quiet = 0;
    int i;
    vr_probe_result v;
    vr_probe_result p;

    for (i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--selftest") == 0) {
            only_selftest = 1;
        } else if (strcmp(argv[i], "--quiet") == 0) {
            quiet = 1;
        } else if (strcmp(argv[i], "--delay") == 0 && i + 1 < argc) {
            delay_ms = (unsigned)strtoul(argv[++i], NULL, 10);
            if (delay_ms == 0u) {
                delay_ms = 1u;
            }
        } else if (strcmp(argv[i], "--help") == 0) {
            fputs(k_usage, stdout);
            return 0;
        } else {
            fprintf(stderr, "无法识别的参数：%s\n\n%s", argv[i], k_usage);
            return 2;
        }
    }

    if (only_selftest) {
        return vr_self_test(stdout) == 0 ? 0 : 1;
    }

    if (!quiet) {
        printf("示例 07-lower-level/02-volatile-and-registers · 寄存器与 volatile\n\n");
        vr_print_reg_table(stdout);

        printf("\n== 同一个等待循环，两种写法 ==\n");
        printf("  三行源码只差一个关键字，起一个新线程去等，本线程 %u ms 后把状态位置 1。\n",
               delay_ms);
        v = vr_probe_flag(1, delay_ms, 10000u);
        p = vr_probe_flag(0, delay_ms, 10000u);
        print_probe(stdout, "带 volatile：", v);
        print_probe(stdout, "不带 volatile：", p);
#if defined(__OPTIMIZE__)
        printf("  结论：带 volatile 的读到了；不带的那一份把读提到了循环之外，\n"
               "        内存已经改了，循环里读到的仍是旧值。\n");
#else
        printf("  结论：本档没有开优化，两份都读到了。\n"
               "        要看出差别得开优化：用 mingw-release 预设再跑一次。\n");
#endif

        printf("\n== 重复读同一个字 100 次 ==\n");
        vr_dr_set(0x00000001u);
        printf("  带 volatile 的和   = %lu\n", (unsigned long)vr_sum_reads(1, 100u));
        printf("  不带 volatile 的和 = %lu\n", (unsigned long)vr_sum_reads(0, 100u));
        printf("  两个和相同——所以这类错不会通过「结果对不对」发现；\n"
               "  差别在真读了几次：不带 volatile 的版本在优化之后只读一次。\n");
        printf("  看汇编：tools\\asm-compare.ps1。\n");
    }

    printf("\n");
    return vr_self_test(stdout) == 0 ? 0 : 1;
}
