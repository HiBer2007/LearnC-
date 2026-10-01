/* layout_probe.c —— 阶段 1、2 的 TODO 都在这个文件里
 *
 * 交付状态：下面的函数都是占位实现，能编译、能跑，但结果不对。
 * 每做完一个 TODO，重新构建一次，对照《配置步骤.md》里该阶段的验收标准。
 */
#include "layout_probe.h"

#include <stdlib.h>
#include <string.h>

/* ================================================================ 阶段 1 */

/* 阶段 1-1
 * 六类对象各取一个，填进 out：
 *   .text    函数 lp_code_anchor 的地址
 *   .rodata  lp_g_rodata 与 lp_g_msg 的地址
 *   .data    lp_g_data 的地址
 *   .bss     lp_g_bss 的地址
 *   stack    函数内部一个自动变量的地址
 *   heap     malloc 拿到的地址（填完记得 free）
 * 最多填 cap 条，返回真正填的条数：素材有 6 个探测点（rodata 两个），加栈与堆共 8 条。
 */
int lp_probe_addresses(LpSample *out, int cap)
{
    /* TODO 1-1：把每条样本的 name、kind、addr 填好。
     * 下面这一行是占位实现：一条都不填，返回 -1，好让驱动打印「阶段 1 未完成」。 */
    (void)out;
    (void)cap;
    return -1;
}

/* ================================================================ 阶段 2 */

/* 阶段 2-1
 * 在 s[0..n) 里找第一段名等于 from、to 的两条样本，返回 to 的地址减 from 的地址。
 * 段名用 strcmp 比较；任一段找不到就返回 0。
 */
long lp_section_gap(const LpSample *s, int n, const char *from, const char *to)
{
    /* TODO 2-1 */
    (void)s;
    (void)n;
    (void)from;
    (void)to;
    return 0;
}

/* 阶段 2-2
 * 返回两个地址之间的距离（可能为负）。
 * 两个实参可能是互不相关的对象，指针直接相减没有定义，先转 uintptr_t。
 */
long lp_address_delta(const void *a, const void *b)
{
    /* TODO 2-2 */
    (void)a;
    (void)b;
    return 0;
}
