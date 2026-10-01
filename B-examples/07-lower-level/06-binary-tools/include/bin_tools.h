/* bin_tools.h —— 示例 07-lower-level/06-binary-tools 的核心接口
 *
 * 把 nm / objdump / size / readelf 的输出读成结论：
 *   · 跑工具，把标准输出收进缓冲区；
 *   · 解析四种常见输出；
 *   · 从解析结果里说出「这段话是什么意思」。
 *
 * 解析函数与「跑工具」分开，是为了让自测能拿内嵌的样例文本跑，
 * 不依赖本机装没装这些工具。
 */
#ifndef BIN_TOOLS_H
#define BIN_TOOLS_H

#include <stddef.h>
#include <stdio.h>

/* ==================== 跑工具 ==================== */

/* 跑一个程序并把它的标准输出（含标准错误）读进 out。
 * 返回 0 表示成功；out 一定以 '\0' 结尾。 */
int bt_run(const char *exe, const char *const argv[], char *out, size_t cap);

/* 本机的工具路径由构建系统在编译期填进来；没找到时是空串。 */
const char *bt_tool_path(const char *name); /* name: "objdump"/"nm"/"size"/"readelf" */

/* 当前进程自己的可执行文件路径。默认就是拿它当分析对象。 */
const char *bt_self_path(void);

/* ==================== objdump -h：段表 ==================== */

#define BT_NAME_MAX 32
#define BT_SYM_MAX 128

typedef struct {
    char name[BT_NAME_MAX];
    unsigned long long size;
    unsigned long long vma;
    unsigned long long lma;
    unsigned long long file_off;
    int has_contents; /* 有 CONTENTS：占文件 */
    int is_alloc;     /* 有 ALLOC：占内存 */
    int is_code;      /* 有 CODE */
    int is_readonly;  /* 有 READONLY */
} bt_section;

int bt_parse_objdump_h(const char *text, bt_section *out, int max, int *count);
void bt_conclude_sections(FILE *out, const bt_section *s, int n, const char *what);

/* ==================== readelf -S：ELF 的段表 ==================== */

int bt_parse_readelf_sections(const char *text, bt_section *out, int max, int *count);

/* ==================== nm：符号表 ==================== */

typedef struct {
    char name[BT_SYM_MAX];
    char type; /* nm 的类型字母 */
    unsigned long long addr;
} bt_symbol;

typedef struct {
    int total;
    int text;      /* T / t：代码段 */
    int data;      /* D / d / G / g：.data */
    int bss;       /* B / b：.bss */
    int rodata;    /* R / r：只读数据 */
    int undefined_; /* U：未定义 */
    int weak;      /* W / w / V / v：弱符号 */
    int local;     /* 小写字母：局部符号 */
    int other;
} bt_symbol_stats;

int bt_parse_nm(const char *text, bt_symbol *out, int max, int *count);
void bt_classify(const bt_symbol *s, int n, bt_symbol_stats *st);
void bt_conclude_symbols(FILE *out, const bt_symbol_stats *st, const char *what);

/* ==================== size ==================== */

typedef struct {
    char name[64];
    unsigned long long text;
    unsigned long long data;
    unsigned long long bss;
    unsigned long long dec;
    unsigned long long hex;
    int has_dec_hex; /* 有 dec/hex 两列时置 1 */
} bt_size_row;

int bt_parse_size(const char *text, bt_size_row *out, int max, int *count);
void bt_conclude_size(FILE *out, const bt_size_row *rows, int n, const char *what);

/* ==================== 自测 ==================== */

int bt_self_test(FILE *out);

#endif /* BIN_TOOLS_H */
