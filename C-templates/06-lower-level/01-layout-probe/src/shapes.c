/* shapes.c —— 阶段 3、4 的 TODO 都在这个文件里
 *
 * 素材是 include/shapes.h 里的三个类型：LpPlain、LpPacked、LpAligned。
 * 用 sizeof、_Alignof、offsetof 把真实布局报出来，不要照着直觉填。
 */
#include "layout_probe.h"
#include "shapes.h"

#include <stdio.h>
#include <string.h>

/* ================================================================ 阶段 3 */

/* 阶段 3-1
 * 三种排布各填一条，返回填了几条。
 * LpPlain 与 LpPacked 有 tag / value / flags / kind 四个成员；
 * LpAligned 没有这四个成员，它的四个偏移填 (size_t)-1，驱动会打印成 "--"。
 */
int lp_describe_shapes(LpShape *out, int cap)
{
    /* TODO 3-1：用 sizeof / _Alignof / offsetof 取真实值。 */
    (void)out;
    (void)cap;
    return -1;
}

/* ================================================================ 阶段 4 */

/* 阶段 4-1
 * 把自检结果写成文字放进 buf，最多写 cap 字节（含结尾的 '\0'），返回写出的字符数。
 *
 * 至少要报出这三件事，每件一行：
 *   align  : 一个 LpAligned 变量的地址对 16 取余是不是 0
 *   align  : 一个 uint32_t 变量的地址对 4 取余是不是 0
 *   read   : 从 buf+1 这个非对齐地址读一个 uint32_t，与对齐读比较是否一致
 * 最后再写一行总结：几项通过、几项失败。
 */
int lp_align_check(char *buf, size_t cap)
{
    /* TODO 4-1：占位实现，写一行提示就返回 */
    const char *todo = "(TODO 4-1: lp_align_check is not implemented yet)\n";
    size_t n = strlen(todo);

    if (cap == 0) {
        return 0;
    }
    if (n > cap - 1) {
        n = cap - 1;
    }
    memcpy(buf, todo, n);
    buf[n] = '\0';
    return (int)n;
}
