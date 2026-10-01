/* layout_probe.h —— 练习模板 07-lower-level/01-layout-probe 的公共接口
 *
 * 这个头文件给出各阶段的函数原型与数据结构。
 * 阶段 1 与阶段 2 的实现写在 src/layout_probe.c 里，阶段 3 与阶段 4 写在 src/shapes.c 里。
 * 探测用的对象定义在 src/probe_data.c 里，那是固定的素材，不要改。
 */
#ifndef LAYOUT_PROBE_H
#define LAYOUT_PROBE_H

#include <stddef.h>
#include <stdint.h>

/* ---------------------------------------------------------------- 阶段 1 */

/* 一个探测点：对象名、它落在哪一段、它的地址 */
typedef struct {
    const char *name;    /* 对象名，用于打印 */
    const char *kind;    /* 段名：text / rodata / data / bss / stack / heap */
    uintptr_t   addr;    /* 地址；取不到时写 0 */
} LpSample;

/* 阶段 1-1
 * 把六类对象各取一个填进 out，最多 cap 条，返回真正填了几条。
 * 素材对象的声明见本文件下方；栈与堆的对象要在函数内部现取。 */
int lp_probe_addresses(LpSample *out, int cap);

/* ---------------------------------------------------------------- 阶段 2 */

/* 阶段 2-1
 * 在已经采到的样本里找第一段名等于 from、to 的两条，返回 to 的地址减 from 的地址。
 * 找不到就返回 0。 */
long lp_section_gap(const LpSample *s, int n, const char *from, const char *to);

/* 阶段 2-2
 * 返回两个地址之间的距离（字节）。
 * 注意实参可能是两个互不相关的对象：C 里对它们直接做指针减法没有定义，
 * 要先转成 uintptr_t 再减。 */
long lp_address_delta(const void *a, const void *b);

/* ---------------------------------------------------------------- 阶段 3 */

/* 一个结构体的布局：大小、对齐、四个成员的偏移 */
typedef struct {
    const char *name;      /* 类型名 */
    size_t      size;      /* sizeof */
    size_t      align;     /* _Alignof */
    size_t      off_tag;   /* offsetof(..., tag) */
    size_t      off_value; /* offsetof(..., value) */
    size_t      off_flags; /* offsetof(..., flags) */
    size_t      off_kind;  /* offsetof(..., kind) */
} LpShape;

/* 阶段 3-1
 * 把 LpPlain、LpPacked、LpAligned 三种排布填进 out，返回填了几条。
 * 三个类型的定义在 shapes.h 里。 */
int lp_describe_shapes(LpShape *out, int cap);

/* ---------------------------------------------------------------- 阶段 4 */

/* 阶段 4-1
 * 把阶段 4 的自检结果写成一行行文字放进 buf，返回写出的字符数。
 * 自检至少包含两项：
 *   1) 采样到的对象地址是否满足该类型的对齐要求；
 *   2) 从「非对齐的地址」读一个 uint32_t，读到的值与对齐读是否一致。
 * 具体格式见《配置步骤.md》阶段 4 的验收标准。 */
int lp_align_check(char *buf, size_t cap);

/* ---------------------------------------------------------------- 素材 */

/* 六类探测对象，定义在 src/probe_data.c —— 只读不改 */
extern int         lp_g_data;      /* 有初值的全局：.data */
extern int         lp_g_bss;       /* 无初值的全局：.bss */
extern const int   lp_g_rodata;    /* 只读全局：.rodata */
extern const char  lp_g_msg[];     /* 只读字符数组：.rodata */
void lp_code_anchor(void);         /* 一个空函数：.text */

#endif /* LAYOUT_PROBE_H */
