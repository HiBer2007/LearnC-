/* main_cli.c —— 命令行验收程序（已经写好，不需要改）
 *
 * 它按阶段调用 include/layout_probe.h 里的接口，把结果打印出来。
 * 每个阶段的验收标准见《配置步骤.md》。
 *
 * 编译与运行：
 *   cmake --preset mingw-gdb
 *   cmake --build --preset mingw-gdb
 *   build\mingw\bin\app_cli.exe
 */
#include "layout_probe.h"
#include "shapes.h"

#include <stdio.h>

#define MAX_SAMPLES 16
#define MAX_SHAPES  4

static void print_samples(const LpSample *s, int n)
{
    int i;

    printf("  %-14s %-8s %s\n", "name", "kind", "address");
    for (i = 0; i < n; ++i) {
        printf("  %-14s %-8s 0x%016llx\n", s[i].name, s[i].kind,
               (unsigned long long)s[i].addr);
    }
}

static void print_gap(const LpSample *s, int n, const char *from, const char *to)
{
    long gap = lp_section_gap(s, n, from, to);

    printf("  %-8s -> %-8s : %+ld\n", from, to, gap);
}

static void print_shapes(const LpShape *sh, int n)
{
    int i;

    printf("  %-10s %5s %6s %6s %6s %6s %6s\n",
           "name", "size", "align", "tag", "value", "flags", "kind");
    for (i = 0; i < n; ++i) {
        char o[4][8];
        const size_t off[4] = { sh[i].off_tag, sh[i].off_value,
                                sh[i].off_flags, sh[i].off_kind };
        int k;

        for (k = 0; k < 4; ++k) {
            if (off[k] == (size_t)-1) {
                snprintf(o[k], sizeof o[k], "--");
            } else {
                snprintf(o[k], sizeof o[k], "%zu", off[k]);
            }
        }
        printf("  %-10s %5zu %6zu %6s %6s %6s %6s\n",
               sh[i].name, sh[i].size, sh[i].align,
               o[0], o[1], o[2], o[3]);
    }
}

int main(void)
{
    LpSample samples[MAX_SAMPLES];
    LpShape  shapes[MAX_SHAPES];
    char     report[512];
    int      n;
    int      i;

    /* ---------------------------------------------------------- 阶段 1 */
    printf("=== Stage 1: object addresses ===\n");
    n = lp_probe_addresses(samples, MAX_SAMPLES);
    if (n <= 0) {
        printf("  lp_probe_addresses returned %d (stage 1 is not done yet)\n", n);
    } else {
        print_samples(samples, n);
    }

    /* ---------------------------------------------------------- 阶段 2 */
    printf("\n=== Stage 2: section gaps ===\n");
    if (n <= 0) {
        printf("  (no samples: stage 1 is not done yet)\n");
        printf("  lp_section_gap returned %ld\n",
               lp_section_gap(samples, 0, "text", "rodata"));
    } else {
        print_gap(samples, n, "text", "rodata");
        print_gap(samples, n, "rodata", "data");
        print_gap(samples, n, "data", "bss");
        print_gap(samples, n, "bss", "heap");
    }
    printf("  delta(bss, data) = %+ld\n",
           lp_address_delta(&lp_g_bss, &lp_g_data));
    printf("  delta(text, data) = %+ld\n",
           lp_address_delta((const void *)&lp_code_anchor, &lp_g_data));
    printf("  --- 本次运行的绝对地址：再跑一次，把这一段与上一次逐行对照 ---\n");
    if (n > 0) {
        print_samples(samples, n);
    }

    /* ---------------------------------------------------------- 阶段 3 */
    printf("\n=== Stage 3: struct layout ===\n");
    n = lp_describe_shapes(shapes, MAX_SHAPES);
    if (n <= 0) {
        printf("  lp_describe_shapes returned %d (stage 3 is not done yet)\n", n);
    } else {
        print_shapes(shapes, n);
    }

    /* ---------------------------------------------------------- 阶段 4 */
    printf("\n=== Stage 4: alignment self-check ===\n");
    n = lp_align_check(report, sizeof report);
    if (n <= 0) {
        printf("  lp_align_check returned %d (stage 4 is not done yet)\n", n);
    } else {
        fputs(report, stdout);
    }

    (void)i;
    return 0;
}
