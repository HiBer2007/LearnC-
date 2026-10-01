/**
 * textstats.h —— 练习模板 01 的核心接口（C）
 *
 * 接口已经定好，src/main_cli.c 按 5 个阶段调用它们，窗口版调用 ts_analyze。
 * 你要做的是在 src/textstats.c 里把标了 TODO 的函数实现出来，
 * 每完成一个阶段就重新构建、运行一次，对照《配置步骤.md》的验收输出。
 *
 * 五个阶段与函数的对应关系：
 *     阶段 1   ts_read_file、ts_count_lines
 *     阶段 2   ts_collect_words
 *     阶段 3   ts_sort_entries
 *     阶段 4   ts_measure
 *     阶段 5   ts_write_report
 */
#ifndef TEXTSTATS_H
#define TEXTSTATS_H

#include <stddef.h>

/* 一个词最长 63 个字符（另留一个 '\0'），最多记 256 个不同的词 */
#define TS_MAX_WORD   64
#define TS_MAX_UNIQUE 256

/* 报表里给出前多少名 */
#define TS_TOP_N      10

typedef struct {
    char word[TS_MAX_WORD];
    long count;              /* 这个词在全文里出现了多少次 */
} TsEntry;

typedef struct {
    char    path[512];       /* 输入文件路径，报表里要打印出来 */
    long    bytes;           /* 文件字节数 */
    long    lines;           /* 行数 */
    long    words;           /* 词的总个数（含重复） */
    long    unique;          /* 不同词的个数 */
    TsEntry entries[TS_MAX_UNIQUE];
} TsStats;

/* ---------- 阶段 1：打开、读取、数行 ---------- */

/* 把整个文件读进一块 malloc 出来的缓冲区，成功时返回首地址并把字节数写进 *out_size；
 * 失败时返回 NULL，并把原因写进 err（err 可以为 NULL）。调用方用 ts_free_text 释放。 */
char *ts_read_file(const char *path, size_t *out_size, char *err, size_t err_cap);

/* 数行数：'\n' 的个数；最后一个字节不是 '\n' 且文件非空时再加一。 */
void ts_count_lines(const char *text, size_t size, long *out_lines);

/* ---------- 阶段 2：分词与计数 ---------- */

/* 统计词频。词的规则：连续的字母（isalpha）算一个词，统一转小写，
 * 超过 TS_MAX_WORD-1 个字符的部分截断；其余字符一律当分隔符。
 * entries 是调用方给的数组，最多放 cap 条；返回 0 表示成功。 */
int ts_collect_words(const char *text, size_t size,
                     TsEntry *entries, size_t cap,
                     long *out_unique, long *out_words);

/* ---------- 阶段 3：排序 ---------- */

/* 次数降序；次数相同时按字典序升序。 */
void ts_sort_entries(TsEntry *entries, size_t n);

/* ---------- 阶段 4：计时 ---------- */

/* 把同一段工作量（把整份文本再分词 repeat 遍）各测一次：
 *   *out_cpu_ms   进程占用的 CPU 时间，用 clock()，单位毫秒
 *   *out_wall_s   墙上时间，用 time() 与 difftime()，单位秒
 * 两个来源的分辨率差得很远：clock() 是毫秒级，time() 只有 1 秒。
 * 为什么测耗时不该用 clock()、本机的 MinGW 为什么没有 timespec_get，
 * 见《07-标准库/A-04-时间与日期：time.h.md》第 5.2 小节与第 6 节。 */
void ts_measure(const char *text, size_t size, long repeat,
                double *out_cpu_ms, double *out_wall_s);

/* ---------- 阶段 5：报表 ---------- */

/* 把报表写进 out（最多 cap 字节，含结尾的 '\0'），返回写出的字符数（不含 '\0'）。
 * 格式见《配置步骤.md》阶段 5 的验收输出，必须逐字节一致。 */
int ts_write_report(const TsStats *stats, char *out, size_t cap);

/* ---------- 已给出：内存释放与串联 ---------- */

void ts_free_text(char *text);
void ts_free_stats(TsStats *stats);

/* 把阶段 1 到阶段 3 串起来，界面版与阶段 5 用它。已实现，不必改。 */
int ts_analyze(const char *path, TsStats *stats, char *err, size_t err_cap);

#endif /* TEXTSTATS_H */
