/**
 * main_cli.c —— 练习模板 01 的命令行验收程序（C）
 *
 * 这个文件**不需要改**：它按 5 个阶段调用 textstats.c 里的函数，
 * 把结果打印成《配置步骤.md》里的验收输出。你的实现写在 src/textstats.c 里。
 *
 * 构建与运行（在模板目录下）：
 *     cmake --preset mingw-gdb
 *     cmake --build --preset mingw-gdb
 *     build\mingw\bin\app_cli.exe
 *     build\mingw\bin\app_cli.exe data\sample.txt
 *
 * 阶段的划分：
 *     阶段 1  读文件与数行
 *     阶段 2  分词与词频
 *     阶段 3  qsort 排序
 *     阶段 4  两种计时
 *     阶段 5  报表输出
 */
#include <stdio.h>
#include <string.h>

#include "textstats.h"

#define TS_REPORT_CAP (16 * 1024)

/* 阶段 4 把同一段工作量重复多少遍。数字越大，clock() 越容易读出可比的数 */
#define TS_REPEAT     20000L

/* 骨架自带的兜底样例：文件打不开时用它，好让阶段 2 到阶段 5 仍然能跑起来。
 * 阶段 1 做对之后，这一段就不会再被用到。 */
static const char kFallback[] =
    "the quick brown fox jumps over the lazy dog\n"
    "the fox is quick and the dog is lazy\n";

static TsStats g_stats;
static char    g_report[TS_REPORT_CAP];

int main(int argc, char **argv)
{
    const char *path = (argc > 1) ? argv[1] : "data/sample.txt";
    char   err[256] = "";
    char  *text = NULL;
    size_t size = 0;
    double cpu_ms = 0.0;
    double wall_s = 0.0;
    long   i = 0;
    long   shown = 0;

    /* ---------------------------------------------------------- 阶段 1 */
    printf("=== Stage 1: read file ===\n");
    printf("path    : %s\n", path);

    text = ts_read_file(path, &size, err, sizeof(err));
    if (text == NULL) {
        printf("open failed : %s\n", err);
        printf("fallback    : built-in two-line sample (so the later stages still run)\n");
        text = (char *)kFallback;
        size = strlen(kFallback);
    }

    g_stats.bytes = (long)size;
    ts_count_lines(text, size, &g_stats.lines);
    printf("bytes   : %ld\n", g_stats.bytes);
    printf("lines   : %ld\n", g_stats.lines);

    /* ---------------------------------------------------------- 阶段 2 */
    printf("\n=== Stage 2: tokenize and count ===\n");
    if (ts_collect_words(text, size, g_stats.entries, TS_MAX_UNIQUE,
                         &g_stats.unique, &g_stats.words) != 0) {
        printf("ts_collect_words returned -1 (stage 2 is not done yet)\n");
    }
    printf("words   : %ld\n", g_stats.words);
    printf("unique  : %ld\n", g_stats.unique);

    /* ---------------------------------------------------------- 阶段 3 */
    printf("\n=== Stage 3: sort with qsort ===\n");
    ts_sort_entries(g_stats.entries, (size_t)g_stats.unique);
    shown = (g_stats.unique < 5) ? g_stats.unique : 5;
    for (i = 0; i < shown; ++i) {
        printf("  %2ld  %-14s %4ld\n", i + 1,
               g_stats.entries[i].word, g_stats.entries[i].count);
    }

    /* ---------------------------------------------------------- 阶段 4 */
    printf("\n=== Stage 4: timing ===\n");
    printf("repeat       : %ld\n", TS_REPEAT);
    ts_measure(text, size, TS_REPEAT, &cpu_ms, &wall_s);
    printf("cpu_ms       : %.3f\n", cpu_ms);
    printf("wall_s       : %.3f\n", wall_s);
    printf("note         : clock() 是 CPU 时间（毫秒级），time() 只有 1 秒分辨率\n");

    /* ---------------------------------------------------------- 阶段 5 */
    printf("\n=== Stage 5: report ===\n");
    snprintf(g_stats.path, sizeof(g_stats.path), "%s", path);
    ts_write_report(&g_stats, g_report, sizeof(g_report));
    fputs(g_report, stdout);

    if (text != kFallback) {
        ts_free_text(text);
    }
    ts_free_stats(&g_stats);
    return 0;
}
