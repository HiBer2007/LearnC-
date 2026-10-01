/**
 * sales_report.h —— C 版销售记录报表，核心逻辑（不依赖任何界面）
 *
 * 命令行版（src/main_cli.c）与 Win32 界面版（src/main_gui_win32.c）
 * 都链接它。这里的函数只负责算与拼字符串，不往屏幕上打印任何东西：
 * 该显示成什么样子，由各自的界面决定。
 *
 * 用到的标准库头文件（对应教材 A 段各章）：
 *   <stdio.h>   fopen / fgets / fclose / vsnprintf   —— A-01
 *   <string.h>  strlen / memcpy / strcmp / strspn /
 *               strcspn / strerror                    —— A-02
 *   <stdlib.h>  strtol / strtod / qsort / malloc     —— A-05
 *   <time.h>    timespec_get                         —— A-04
 */
#ifndef SALES_REPORT_H
#define SALES_REPORT_H

#include <stddef.h>

/* ── 容量上限：全部用固定数组，不动态扩容（C 里这是常见做法） ── */
#define SR_NAME_MAX     32    /* 商品名最大长度（含结尾的 '\0'） */
#define SR_DATE_MAX     16    /* 日期字段最大长度 */
#define SR_LINE_MAX    256    /* 一行最多读这么多字节 */
#define SR_MAX_FIELDS    4    /* 每行固定 4 个字段 */
#define SR_MAX_PRODUCTS 32    /* 最多统计多少种商品 */
#define SR_MAX_RECORDS 256    /* 最多保留多少条明细（用于「单笔金额最高」） */
#define SR_REPORT_MAX 8192    /* 报表文本缓冲区大小 */

/* ── 自测结果：每项一行，不打印，由界面决定怎么显示 ───────── */
#define SR_CHECK_MAX      24
#define SR_CHECK_LINE_MAX 128

typedef struct {
    int  total;                                  /* 一共多少项 */
    int  passed;                                 /* 通过多少项 */
    int  failed;                                 /* 不通过多少项 */
    int  count;                                  /* lines 里实际存了几行 */
    char lines[SR_CHECK_MAX][SR_CHECK_LINE_MAX]; /* 形如 "[通过] 3. ……" */
} sr_checks_t;

/* ── 一条明细记录 ─────────────────────────────────────────── */
typedef struct {
    char   date[SR_DATE_MAX];
    char   product[SR_NAME_MAX];
    long   quantity;
    double unit_price;
    double amount;          /* 数量 × 单价，解析时就一次算好 */
} sr_record_t;

/* ── 按商品聚合后的一行 ───────────────────────────────────── */
typedef struct {
    char   name[SR_NAME_MAX];
    int    orders;          /* 出现多少笔 */
    long   quantity;        /* 数量合计 */
    double amount;          /* 金额合计 */
} sr_product_t;

/* ── 整份报表的数据 ───────────────────────────────────────── */
typedef struct {
    int          lines_read;      /* 读进来的总行数（含注释行与空行） */
    int          lines_valid;     /* 解析成功的数据行 */
    int          lines_skipped;   /* 解析失败被跳过的行 */
    int          product_count;   /* 商品种类数 */
    int          record_count;    /* 明细条数 */
    long         total_quantity;
    double       total_amount;
    sr_product_t products[SR_MAX_PRODUCTS];
    /* 明细按「金额降序」排过序，报表第三段只取前三条 */
    sr_record_t  records[SR_MAX_RECORDS];
} sr_report_t;

/**
 * 把一个报表清空到初始状态。
 */
void sr_init(sr_report_t *report);

/**
 * 解析一行文本。成功返回 1 并填好 out；失败返回 0。
 *
 * 判定为失败的情形：去掉空白后字段数不是 4、日期或商品名过长、
 * 数量不是正整数、单价不是非负数字。
 * 空行与以 '#' 开头的行由调用方先筛掉，不走这里。
 */
int sr_parse_line(const char *line, sr_record_t *record);

/**
 * 把一条记录并进报表：同名的商品归到一行，并累加总量与总额。
 * 报表已满时返回 0。
 */
int sr_add_record(sr_report_t *report, const sr_record_t *record);

/**
 * 按金额降序排商品；金额相同时按名称升序，保证输出稳定。
 */
void sr_sort_products_by_amount(sr_report_t *report);

/**
 * 按名称升序排商品。
 */
void sr_sort_products_by_name(sr_report_t *report);

/**
 * 按金额降序排明细，金额相同时按日期、商品名升序。
 * 报表第三段只取前三笔，靠的就是这个顺序。
 */
void sr_sort_records_by_amount(sr_report_t *report);

/**
 * 读文件并聚合。成功返回 1；打不开文件返回 0，
 * 并把原因写进 err（err_size 是缓冲区大小）。
 *
 * 返回时商品已按金额降序排好。
 */
int sr_load(const char *path, sr_report_t *report, char *err, size_t err_size);

/**
 * 把报表拼成文本，写进 out（最多 out_size 字节，含结尾的 '\0'）。
 * 返回写入的字节数（不含结尾的 '\0'）。
 *
 * 同一份数据拼出来的文本逐字节相同：命令行版与界面版共用这一份实现。
 */
size_t sr_format_report(const sr_report_t *report, const char *path,
                        char *out, size_t out_size);

/**
 * 取一个毫秒级的单调时间戳，用来量「这段代码跑了多久」。
 *
 * C 标准库在 Windows 上没有可用的单调钟：C11 的 timespec_get 是
 * UCRT 才有的，本机的 MinGW 走 msvcrt，<time.h> 里根本没有它
 * （实测报 implicit declaration，见《07-标准库/A-04-时间与日期：time.h.md》
 * 第 5 节）。因此这里落到 Windows 自己的 QueryPerformanceCounter，
 * 与 A-04 第 6 节给出的做法一致；非 Windows 上退回 timespec_get。
 */
double sr_now_ms(void);

/**
 * 逐项核对分词、解析、聚合、排序与报表格式。结果写进 out，不打印。
 */
void sr_run_selftest(sr_checks_t *out);

/**
 * 把自测结果拼成一句话，形如「20 项中 20 项通过，全部通过」。
 */
void sr_checks_summary(const sr_checks_t *checks, char *out, size_t out_size);

#endif /* SALES_REPORT_H */
