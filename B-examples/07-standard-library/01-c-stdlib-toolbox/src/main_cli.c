/**
 * main_cli.c —— 命令行版
 *
 * 用法：
 *   app_cli                          读默认的 data/sales.txt
 *   app_cli <文件>                    读指定文件
 *   app_cli <文件> --out <文件>        同时把报表原样写进另一个文件
 *   app_cli --selftest               只跑内置自测
 *
 * 这里只做三件事：认命令行参数、调核心模块、把结果写到标准输出。
 * 报表文本怎么拼、算得对不对，全在 src/sales_report.c 里，
 * 与 Win32 界面版共用同一份实现。
 *
 * 计时写在标准错误上，这样标准输出里只有报表本身，
 * 与 02-cpp-io-report 的输出可以直接逐字节对比。
 */
#include "sales_report.h"

#include <stdio.h>
#include <string.h>

/* 相对路径：必须在示例目录下运行，或用 .vscode 里配置的 cwd */
#define SR_DEFAULT_INPUT "data/sales.txt"

static int run_selftest(void)
{
    sr_checks_t checks;
    char summary[SR_CHECK_LINE_MAX];
    int i;

    sr_run_selftest(&checks);

    printf("== 自测 ==\n");
    for (i = 0; i < checks.count; ++i) {
        printf("  %s\n", checks.lines[i]);
    }

    sr_checks_summary(&checks, summary, sizeof summary);
    printf("\n  自测结果：%s\n", summary);
    return checks.failed == 0 ? 0 : 1;
}

int main(int argc, char *argv[])
{
    const char *input = SR_DEFAULT_INPUT;
    const char *output = NULL;
    int only_check = 0;
    int i;

    for (i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--selftest") == 0) {
            only_check = 1;
        } else if (strcmp(argv[i], "--out") == 0 && i + 1 < argc) {
            i += 1;
            output = argv[i];
        } else if (argv[i][0] == '-' && argv[i][1] != '\0') {
            printf("不认识的选项：%s\n", argv[i]);
            printf("用法：app_cli [文件] [--out 文件] [--selftest]\n");
            return 2;
        } else {
            input = argv[i];
        }
    }

    if (only_check) {
        return run_selftest();
    }

    printf("示例 07-standard-library/01-c-stdlib-toolbox · C 标准库报表工具（命令行版）\n\n");

    {
        sr_report_t report;
        char text[SR_REPORT_MAX];
        char err[256];
        double started;
        double finished;

        started = sr_now_ms();
        if (!sr_load(input, &report, err, sizeof err)) {
            fprintf(stderr, "读取失败：%s\n", err);
            return 2;
        }
        (void)sr_format_report(&report, input, text, sizeof text);
        finished = sr_now_ms();

        fputs(text, stdout);

        if (output != NULL) {
            /* 用二进制模式写：换行原样保留成 LF，方便与 02 的输出逐字节对比 */
            FILE *sink = fopen(output, "wb");
            if (sink == NULL) {
                fprintf(stderr, "写不了 %s\n", output);
                return 2;
            }
            fwrite(text, 1, strlen(text), sink);
            fclose(sink);
            fprintf(stderr, "[输出] 报表已写入 %s\n", output);
        }

        fprintf(stderr, "[计时] 读文件并生成报表 %.3f 毫秒\n", finished - started);
    }
    return 0;
}
