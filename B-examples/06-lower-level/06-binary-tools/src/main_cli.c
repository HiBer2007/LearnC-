/* main_cli.c —— 06-binary-tools 的命令行版
 *
 * 默认分析对象是它自己：跑一遍 objdump -h / nm / size，把输出读成结论。
 * 也可以指定别的文件——交叉编译出来的 .elf 就是最合适的第二个对象。
 *
 * 手工编译：
 *   gcc -std=c17 -O2 -Wall -Wextra -Iinclude src/bin_tools.c src/tool_fixtures.c \
 *       src/main_cli.c -o bin_tools.exe
 */
#include "bin_tools.h"

#include <stdlib.h>
#include <string.h>

static const char *const k_usage =
    "用法：bin_tools [选项] [子命令]\n"
    "  子命令（不给就全做）：\n"
    "    size       跑 size，把三段字节数读成结论\n"
    "    sections   跑 objdump -h，把段表读成结论\n"
    "    symbols    跑 nm，把符号表读成结论\n"
    "    headers    跑 readelf -S（对 ELF 才有意义；PE 上会报错，属正常）\n"
    "    selftest   只跑自测\n"
    "  选项：\n"
    "    --file PATH   要分析的文件（默认是本程序自己）\n"
    "    --help        显示这段文字\n";

#define BUF_CAP 65536

static void do_size(const char *file) {
    char *buf = (char *)malloc(BUF_CAP);
    const char *argv[3];
    bt_size_row rows[8];
    int n = 0;
    if (buf == NULL) {
        return;
    }
    argv[0] = file;
    argv[1] = NULL;
    printf("== size %s ==\n", file);
    if (bt_run(bt_tool_path("size"), argv, buf, BUF_CAP) != 0) {
        printf("  （%s）\n", buf);
        free(buf);
        return;
    }
    bt_parse_size(buf, rows, 8, &n);
    if (n == 0) {
        printf("  没能从输出里解析出数据，原始输出：\n%s\n", buf);
    } else {
        bt_conclude_size(stdout, rows, n, "text 与 data 要占 flash，data 与 bss 要占 RAM。");
    }
    free(buf);
}

static void do_sections(const char *file) {
    char *buf = (char *)malloc(BUF_CAP);
    const char *argv[3];
    bt_section secs[64];
    int n = 0;
    if (buf == NULL) {
        return;
    }
    argv[0] = "-h";
    argv[1] = file;
    argv[2] = NULL;
    printf("== objdump -h %s ==\n", file);
    if (bt_run(bt_tool_path("objdump"), argv, buf, BUF_CAP) != 0) {
        printf("  （%s）\n", buf);
        free(buf);
        return;
    }
    bt_parse_objdump_h(buf, secs, 64, &n);
    if (n == 0) {
        printf("  没能从输出里解析出段表，原始输出：\n%s\n", buf);
    } else {
        bt_conclude_sections(stdout, secs, n,
                             "同名的段如果有两个地址，说明它上电时要被搬一次（flash → RAM）。");
    }
    free(buf);
}

static void do_symbols(const char *file) {
    char *buf = (char *)malloc(BUF_CAP);
    const char *argv[3];
    bt_symbol syms[4096];
    bt_symbol_stats st;
    int n = 0;
    if (buf == NULL) {
        return;
    }
    argv[0] = file;
    argv[1] = NULL;
    printf("== nm %s ==\n", file);
    if (bt_run(bt_tool_path("nm"), argv, buf, BUF_CAP) != 0) {
        printf("  （%s）\n", buf);
        free(buf);
        return;
    }
    bt_parse_nm(buf, syms, 4096, &n);
    bt_classify(syms, n, &st);
    bt_conclude_symbols(stdout, &st,
                        "U 是链接期的驱动力：每有一个 U，就一定要有一处定义来配它。");
    free(buf);
}

static void do_headers(const char *file) {
    char *buf = (char *)malloc(BUF_CAP);
    const char *argv[4];
    bt_section secs[64];
    int n = 0;
    if (buf == NULL) {
        return;
    }
    argv[0] = "-S";
    argv[1] = file;
    argv[2] = NULL;
    printf("== readelf -S %s ==\n", file);
    if (bt_run(bt_tool_path("readelf"), argv, buf, BUF_CAP) != 0 ||
        strstr(buf, "Section Headers") == NULL) {
        printf("  readelf 只认 ELF。用它看 PE 会直接报错，这是正常的：\n");
        printf("  PE 那边的对应工具是 objdump -p 或 MSVC 的 dumpbin /headers。\n");
        free(buf);
        return;
    }
    bt_parse_readelf_sections(buf, secs, 64, &n);
    if (n == 0) {
        printf("  没能从输出里解析出段表，原始输出：\n%s\n", buf);
    } else {
        bt_conclude_sections(stdout, secs, n,
                             "NOBITS 的段（.bss）在文件里不占字节，只在内存里占。");
    }
    free(buf);
}

int main(int argc, char **argv) {
    const char *file = NULL;
    const char *cmd = NULL;
    int i;

    for (i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--file") == 0 && i + 1 < argc) {
            file = argv[++i];
        } else if (strcmp(argv[i], "--help") == 0) {
            fputs(k_usage, stdout);
            return 0;
        } else if (argv[i][0] == '-') {
            fprintf(stderr, "无法识别的参数：%s\n\n%s", argv[i], k_usage);
            return 2;
        } else {
            cmd = argv[i];
        }
    }

    if (cmd != NULL && strcmp(cmd, "selftest") == 0) {
        return bt_self_test(stdout) == 0 ? 0 : 1;
    }
    if (cmd != NULL && strcmp(cmd, "size") != 0 && strcmp(cmd, "sections") != 0 &&
        strcmp(cmd, "symbols") != 0 && strcmp(cmd, "headers") != 0) {
        fprintf(stderr, "无法识别的子命令：%s\n\n%s", cmd, k_usage);
        return 2;
    }

    if (file == NULL) {
        file = bt_self_path();
    }

    printf("示例 06-lower-level/06-binary-tools · 把工具的输出读成结论\n");
    printf("工具路径（构建时确定）：\n");
    printf("  objdump = %s\n", bt_tool_path("objdump")[0] ? bt_tool_path("objdump") : "（没找到）");
    printf("  nm      = %s\n", bt_tool_path("nm")[0] ? bt_tool_path("nm") : "（没找到）");
    printf("  size    = %s\n", bt_tool_path("size")[0] ? bt_tool_path("size") : "（没找到）");
    printf("  readelf = %s\n", bt_tool_path("readelf")[0] ? bt_tool_path("readelf") : "（没找到）");
    printf("\n");

    if (cmd == NULL || strcmp(cmd, "size") == 0) {
        do_size(file);
    }
    if (cmd == NULL || strcmp(cmd, "sections") == 0) {
        do_sections(file);
    }
    if (cmd == NULL || strcmp(cmd, "symbols") == 0) {
        do_symbols(file);
    }
    if (cmd == NULL || strcmp(cmd, "headers") == 0) {
        do_headers(file);
    }

    printf("\n");
    return bt_self_test(stdout) == 0 ? 0 : 1;
}
