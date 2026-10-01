/**
 * textstats.c —— 练习模板 01 的核心逻辑（C 标准库综合）
 *
 * 5 个阶段的实现都写在这个文件里。每个函数上面标了 TODO，
 * 骨架给的是占位实现：能编译、能运行、结果明显不对（多为 -1）。
 * 各阶段的任务、验收标准与自查方法见同目录《配置步骤.md》。
 *
 * 构建与运行：
 *     cmake --preset mingw-gdb
 *     cmake --build --preset mingw-gdb
 *     build\mingw\bin\app_cli.exe
 */
#include "textstats.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ==================================================================
 * 阶段 1：把整个文件读进来，并数出行数
 * ================================================================== */

/* TODO（阶段 1-1）：打开文件、量出大小、一次读进 malloc 出来的缓冲区。
 *
 * 提示：
 *   1. fopen(path, "rb")：二进制模式读到的字节数与文件长度一致；
 *   2. fseek(f, 0, SEEK_END) + ftell(f) 得到长度，再用 fseek(f, 0, SEEK_SET) 回到开头；
 *   3. 缓冲区多分配 1 字节并写 '\0'，后面就能当 C 字符串用；
 *   4. 任何一步失败都要把原因写进 err（用 strerror(errno) 拼一句），返回 NULL；
 *      缓冲区分配失败时不要漏掉 fclose。
 *
 * 验收：data/sample.txt 的 bytes 与 lines 对得上《配置步骤.md》阶段 1 的数字；
 *       传一个不存在的路径时，程序打印 open failed 而不是崩溃。 */
char *ts_read_file(const char *path, size_t *out_size, char *err, size_t err_cap)
{
    (void)path;

    if (out_size != NULL) {
        *out_size = 0;
    }
    if (err != NULL && err_cap > 0) {
        snprintf(err, err_cap, "TODO stage 1-1: ts_read_file not implemented yet");
    }
    return NULL;
}

/* TODO（阶段 1-2）：数行数。
 *
 * 提示：'\n' 的个数就是行数；若最后一个字节不是 '\n' 且文件非空，最后一行
 *       没有以换行结束，也要算一行。空文件是 0 行。
 *
 * 验收：data/sample.txt 打印 15。 */
void ts_count_lines(const char *text, size_t size, long *out_lines)
{
    (void)text;
    (void)size;

    if (out_lines != NULL) {
        *out_lines = -1;    /* 占位值：一眼就能看出还没实现 */
    }
}

/* ==================================================================
 * 阶段 2：分词与计数
 * ================================================================== */

/* TODO（阶段 2-1）：把词挑出来，统计每个词出现多少次。
 *
 * 提示：
 *   1. 用一个循环扫过 text 的每个字节；isalpha((unsigned char)c) 为真就攒一个词，
 *      遇到别的字符就把当前攒下的词收尾。注意 isalpha 的实参要转 unsigned char；
 *   2. 收尾时用 tolower 把词统一成小写；词长超过 TS_MAX_WORD-1 的部分截断；
 *   3. 表里已有的词就把 count 加一（strcmp 找），没有就新加一条；
 *      表满（unique == cap）时不再新加，但 words 仍要照数；
 *   4. 返回 0 表示成功，返回 -1 表示出错。
 *
 * 验收：data/sample.txt 的 words 与 unique 对得上《配置步骤.md》阶段 2 的数字。 */
int ts_collect_words(const char *text, size_t size,
                     TsEntry *entries, size_t cap,
                     long *out_unique, long *out_words)
{
    (void)text;
    (void)size;
    (void)entries;
    (void)cap;

    if (out_unique != NULL) {
        *out_unique = 0;
    }
    if (out_words != NULL) {
        *out_words = -1;
    }
    return -1;
}

/* ==================================================================
 * 阶段 3：排序
 * ================================================================== */

/* TODO（阶段 3-1）：用 qsort 排序：次数降序；次数相同时按字典序升序。
 *
 * 提示：
 *   1. 写一个比较函数 int cmp(const void *a, const void *b)，里面把两个 void *
 *      转回 const TsEntry *，先用 count 比，count 相等再用 strcmp 比 word；
 *   2. 比较函数必须返回 int，直接相减 long 会在 64 位下被截断，用两个 if 分别返回 -1/1；
 *   3. 名次由调用方按数组顺序打印，因此这里排完序即可。
 *
 * 验收：阶段 3 打印的前 5 名与《配置步骤.md》一致。 */
void ts_sort_entries(TsEntry *entries, size_t n)
{
    (void)entries;
    (void)n;

    /* 占位实现：什么都不做。 */
}

/* ==================================================================
 * 阶段 4：计时
 * ================================================================== */

/* TODO（阶段 4-1）：同一段工作量，两个时间来源各测一次。
 *
 * 提示：
 *   1. CPU 时间：clock() 返回处理器时间，除以 CLOCKS_PER_SEC 再乘 1000 得到毫秒；
 *   2. 墙上时间：time(NULL) 取起点与终点，用 difftime 相减得到秒；
 *   3. 中间那段工作量是「把整份文本再分词 repeat 遍」，
 *      用 ts_collect_words 完成，结果写进一个临时数组，别写到调用方的表里；
 *   4. clock() 测的是进程占用的 CPU 时间，等待 I/O 或多线程时与墙上时间差别很大；
 *      time() 的分辨率只有 1 秒，这段工作量根本测不出来，两行数字对比正是重点。
 *
 * 本机的 MinGW 链接的是 msvcrt，没有 C11 的 timespec_get 与 TIME_UTC，
 * 因此这里不用它；原因与替代方案见《06-标准库/A-04-时间与日期：time.h.md》第 5.2 小节。
 *
 * 验收：cpu_ms 是随 repeat 变大的正数，wall_s 打出 0.000（数值每次运行都不同，不比对具体数字）。 */
void ts_measure(const char *text, size_t size, long repeat,
                double *out_cpu_ms, double *out_wall_s)
{
    (void)text;
    (void)size;
    (void)repeat;

    if (out_cpu_ms != NULL) {
        *out_cpu_ms = -1.0;
    }
    if (out_wall_s != NULL) {
        *out_wall_s = -1.0;
    }
}

/* ==================================================================
 * 阶段 5：报表
 * ================================================================== */

/* TODO（阶段 5-1）：按下面的格式拼出报表。
 *
 * 格式（每一行的字段名与冒号位置都要对上，柱状条是 '#' 重复 min(count, 40) 次）：
 *
 *     file    : data/sample.txt
 *     bytes   : 963
 *     lines   : 15
 *     words   : 145
 *     unique  : 103
 *     top 10  :
 *        1  the             9  #########
 *
 * 提示：
 *   1. 用 snprintf 一段一段往 out 里写，每次把已写长度累加，剩余空间传 cap - written；
 *   2. 每条的名次用 "%2d"、词用 "%-14s"、次数用 "%4ld"，后面跟两个空格再跟柱状条；
 *   3. 只打印前 TS_TOP_N 条，unique 不足 10 条就有多少打多少；
 *   4. 返回写出的字符数（不含结尾的 '\0'）。
 *
 * 验收：与《配置步骤.md》阶段 5 的报表逐字节一致。 */
int ts_write_report(const TsStats *stats, char *out, size_t cap)
{
    (void)stats;

    if (out == NULL || cap == 0) {
        return 0;
    }
    return snprintf(out, cap, "(TODO stage 5-1: ts_write_report not implemented yet)\n");
}

/* ==================================================================
 * 已给出：内存释放
 * ================================================================== */

void ts_free_text(char *text)
{
    free(text);
}

void ts_free_stats(TsStats *stats)
{
    if (stats != NULL) {
        memset(stats, 0, sizeof(*stats));
    }
}

/* ==================================================================
 * 已给出：把阶段 1 到阶段 3 串起来，界面版与阶段 5 用它
 * ================================================================== */

int ts_analyze(const char *path, TsStats *stats, char *err, size_t err_cap)
{
    char  *text = NULL;
    size_t size = 0;

    if (stats == NULL) {
        return -1;
    }

    memset(stats, 0, sizeof(*stats));
    snprintf(stats->path, sizeof(stats->path), "%s", (path != NULL) ? path : "");

    text = ts_read_file(path, &size, err, err_cap);
    if (text == NULL) {
        return -1;
    }

    stats->bytes = (long)size;
    ts_count_lines(text, size, &stats->lines);

    if (ts_collect_words(text, size, stats->entries, TS_MAX_UNIQUE,
                         &stats->unique, &stats->words) != 0) {
        ts_free_text(text);
        return -1;
    }

    ts_sort_entries(stats->entries, (size_t)stats->unique);
    ts_free_text(text);
    return 0;
}
