/* varargs.h —— 练习模板 03 的 C 侧接口（C）
 *
 * 版权所有 (C) 2026 HiBer2007，保留所有权利。
 *
 * 本程序是《C 与 C++》教材的一部分，采用与教材文档相同的授权：
 * CC BY-NC-ND 4.0 加附加条款。全文见仓库根目录的 LICENSE 与 许可附加条款.md。
 *
 * 允许在保留本声明的前提下查看、编译、运行本程序用于学习；
 * 不允许二次分发，不允许商用，不允许演绎（修改后再分发），不允许移除署名。
 *
 * 本程序不提供任何担保。
 *
 * ------------------------------------------------------------------
 * 不定参数在 C 与 C++ 里是两套东西：
 *
 *     C  侧（本文件 + src/varargs.c）      <stdarg.h> 的 va_list 家族
 *     C++ 侧（varargs_cpp.hpp + .cpp）     可变参数模板与折叠表达式
 *
 * 阶段 1 与阶段 2 是 C 侧的，阶段 3 与阶段 4 是 C++ 侧的。
 * 这个头文件被 C 与 C++ 两种源文件包含，因此带 extern "C" 守卫。
 */
#ifndef VARARGS_H
#define VARARGS_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 已给出：取参次数的计数。阶段 1 与阶段 2 每做一次 va_arg 就加一，
 * 验收程序用它判断「取了几次」是否符合预期。 */
void va_reset_counters(void);
long long va_arg_calls(void);

/* ---------- 阶段 1：va_list 的类型与步进 ---------- */

/* count 个 int 的和。循环的次数已经给定，留空的是「取下一个参数」那一步。
 *
 * 注意：调用方传进来的实参经过默认实参提升——char、short 一律变成 int，
 * float 变成 double。因此这里能取到的类型只有提升之后的那些。 */
long long va_sum_ints(int count, ...);

/* 已给出：把 char、short、int 混着传进来，全部按 int 取出来写进 out（空格分隔）。
 * 它演示的是「可变参数列表里没有 char，也没有 short」。 */
void va_show_promotions(char *out, size_t cap, int count, ...);

/* ---------- 阶段 2：自己写一个 printf 的子集 ---------- */

/* 支持 %d、%s、%c、%% 四种写法。返回值是「本来需要写多少个字符」，
 * 与缓冲够不够大无关；输出始终以 '\0' 结尾（cap 大于 0 时）。
 * 留空的是「按格式符决定取什么类型的参数」那一步。 */
int va_format(char *out, size_t cap, const char *fmt, ...);

/* 已给出：往缓冲里放一个字符 / 一串字符 / 一个十进制整数。
 * 三个都守住 cap，写不下就不写，但 written 照常累加。 */
void va_put_char(char *out, size_t cap, size_t *used, int *written, char ch);
void va_put_str(char *out, size_t cap, size_t *used, int *written, const char *s);
void va_put_int(char *out, size_t cap, size_t *used, int *written, int value);

#ifdef __cplusplus
}
#endif

#endif /* VARARGS_H */
