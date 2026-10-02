/* varargs.c —— 练习模板 03 的 C 侧实现（C23）
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
 * 本文件里有 2 处 TODO：
 *
 *     阶段 1-1   va_sum_ints   从 ap 里取下一个参数
 *     阶段 2-1   va_format     按格式符取不同类型（也要用到阶段 1 的那一步）
 */
#include "varargs.h"

#include <stdarg.h>

static long long g_arg_calls = 0;

void va_reset_counters(void)
{
    g_arg_calls = 0;
}

long long va_arg_calls(void)
{
    return g_arg_calls;
}

/* ==================================================================
 * 已给出的三个输出工具：都守住 cap，写不下就不写，但 written 照常累加
 * ================================================================== */

void va_put_char(char *out, size_t cap, size_t *used, int *written, char ch)
{
    if (out != NULL && used != NULL && *used + 1 < cap) {
        out[(*used)++] = ch;
    }
    if (written != NULL) {
        ++(*written);
    }
}

void va_put_str(char *out, size_t cap, size_t *used, int *written, const char *s)
{
    for (const char *p = s; *p != '\0'; ++p) {
        va_put_char(out, cap, used, written, *p);
    }
}

void va_put_int(char *out, size_t cap, size_t *used, int *written, int value)
{
    char tmp[12];
    int n = 0;
    unsigned int u;

    if (value < 0) {
        va_put_char(out, cap, used, written, '-');
        u = 0U - (unsigned int)value;
    } else {
        u = (unsigned int)value;
    }

    do {
        tmp[n++] = (char)('0' + (int)(u % 10U));
        u /= 10U;
    } while (u != 0U);

    while (n > 0) {
        va_put_char(out, cap, used, written, tmp[--n]);
    }
}

/* ==================================================================
 * 阶段 1：va_list 的类型与步进
 * ================================================================== */

long long va_sum_ints(int count, ...)
{
    va_list ap;
    long long sum = 0;

    va_start(ap, count);

    for (int i = 0; i < count; ++i) {
        /* TODO（阶段 1-1）：
         * 从 ap 里取出下一个参数，累加进 sum。
         * 要决定两件事：**取出来算什么类型**，以及**取完之后 ap 怎么往前走**
         * （第二件事由这一步本身完成，不必自己挪 ap）。
         * 每取一次让 g_arg_calls 加一，验收程序要读它。
         * 调用方传进来的都是 int；char 与 short 进列表之前已经被提升成 int，
         * 所以这里写 char 或 short 都是错的。
         * 判据：va_sum_ints(5, 1, 2, 3, 4, 5) 是 15、va_arg_calls() 是 5
         *       （见《配置步骤.md》阶段 1）。 */

        /* 占位实现：写完上面那一步之后，循环里这些占位行删掉 */
        (void)ap;
        (void)i;
    }

    va_end(ap);
    return sum;     /* 占位实现：一个也没取，和是 0 */
}

void va_show_promotions(char *out, size_t cap, int count, ...)
{
    va_list ap;
    size_t used = 0;
    int written = 0;

    if (cap > 0 && out != NULL) {
        out[0] = '\0';
    }

    va_start(ap, count);
    for (int i = 0; i < count; ++i) {
        /* 传进来的是 char、short、int 三种宽度，取的时候一律按 int ——
         * 这就是默认实参提升在可变参数列表里的样子 */
        const int v = va_arg(ap, int);
        if (i > 0) {
            va_put_char(out, cap, &used, &written, ' ');
        }
        va_put_int(out, cap, &used, &written, v);
    }
    va_end(ap);

    if (cap > 0 && out != NULL) {
        out[used < cap ? used : cap - 1] = '\0';
    }
}

/* ==================================================================
 * 阶段 2：自己写一个 printf 的子集
 * ================================================================== */

int va_format(char *out, size_t cap, const char *fmt, ...)
{
    va_list ap;
    size_t used = 0;
    int written = 0;

    if (cap > 0 && out != NULL) {
        out[0] = '\0';
    }

    va_start(ap, fmt);

    for (const char *p = fmt; *p != '\0'; ++p) {
        if (*p != '%') {
            va_put_char(out, cap, &used, &written, *p);
            continue;
        }

        ++p;                            /* 跨过 '%' */
        if (*p == '\0') {
            break;                      /* 格式串以孤立的 '%' 结尾 */
        }
        if (*p == '%') {
            va_put_char(out, cap, &used, &written, '%');
            continue;
        }

        /* TODO（阶段 2-1）：
         * 按 *p 这个格式符决定**取什么类型**的参数，取出来之后交给上面三个工具：
         *     'd'  整数        's'  字符串        'c'  字符
         * 取参同样要用阶段 1 那一步，并且每取一次让 g_arg_calls 加一。
         * 三个格式符各自要取的类型并不一样；'c' 传来的同样是提升之后的类型。
         * 格式符不认识时，原样把 '%' 与这个字符都放进输出，不要取参数。
         * 判据：见《配置步骤.md》阶段 2 —— 四段输出与 written、va_arg_calls 都要对上：
         *       完整的一段是 `n=42 s=text c=X pct=%`、written 是 21、取参 3 次；
         *       缓冲只有 8 字节时文本被截断而 written 仍是 21；
         *       不认识的格式符原样输出且不取参数。 */

        (void)ap;                       /* 占位实现：一个参数也不取 */
    }

    if (cap > 0 && out != NULL) {
        out[used < cap ? used : cap - 1] = '\0';
    }

    va_end(ap);
    return written;
}
