/**
 * sales_report.c —— C 版销售记录报表，核心逻辑
 *
 * 这里不出现任何界面代码：命令行版与 Win32 界面版都调用本文件，
 * 因此两边算出来的报表逐字节相同。
 *
 * 演示到的标准库（对应教材 A 段各章）：
 *   A-01  <stdio.h>   fopen / fgets / fclose / snprintf / vsnprintf
 *   A-02  <string.h>  strlen / memcpy / memset / strcmp / strspn / strcspn
 *   A-04  <time.h>    timespec_get
 *   A-05  <stdlib.h>  strtol / strtod / qsort
 */
#include "sales_report.h"

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#if defined(_WIN32)
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>      /* QueryPerformanceCounter：Windows 上的单调钟 */
#endif

/* ── 报表版式 ─────────────────────────────────────────────────
 *
 * 宽度一律按「字节数」算，这一点在本工程里同时就是「显示列数」：
 * 源文件是 UTF-8，构建时用 -fexec-charset=GBK 把窄字符串转成 GBK
 * 写进 exe，一个汉字正好 2 字节，而它在控制台上也正好占 2 列。
 * 于是 %-16s 这类宽度说明符既按字节补齐，看起来也齐。
 *
 * 六个格式串的列宽必须两两对得上，改一个就要改一排：
 *   第 1 段  4 + 2 + 16 + 2 + 6 + 2 + 6 + 2 + 11 + 2 + 8 = 61 列
 *   第 2 段      2 + 16 + 2 + 6 + 2 + 6 + 2 + 11 + 2 + 8 = 55 列
 *   第 3 段  4 + 2 + 10 + 2 + 16 + 2 + 6 + 2 + 11       = 55 列
 * ────────────────────────────────────────────────────────────── */
#define SR_FMT_HEAD1 "%4s  %-16s  %6s  %6s  %11s  %8s\n"
#define SR_FMT_ROW1  "%4d  %-16s  %6d  %6ld  %11.2f  %7.2f%%\n"
#define SR_FMT_SUM   "%4s  %-16s  %6d  %6ld  %11.2f  %7.2f%%\n"
#define SR_RULE1     "----  ----------------  ------  ------  -----------  --------"

#define SR_FMT_HEAD2 "%-16s  %6s  %6s  %11s  %8s\n"
#define SR_FMT_ROW2  "%-16s  %6d  %6ld  %11.2f  %7.2f%%\n"
#define SR_RULE2     "----------------  ------  ------  -----------  --------"

#define SR_FMT_HEAD3 "%4s  %-10s  %-16s  %6s  %11s\n"
#define SR_FMT_ROW3  "%4d  %-10s  %-16s  %6ld  %11.2f\n"
#define SR_RULE3     "----  ----------  ----------------  ------  -----------"

/* 让编译器按 printf 的规则检查本文件里的自定义格式化函数。
   这是 GCC 的扩展，非 GCC 编译器上展开成空。 */
#if defined(__GNUC__)
#  define SR_PRINTF_LIKE(fmt_index, arg_index) \
          __attribute__((format(printf, fmt_index, arg_index)))
#else
#  define SR_PRINTF_LIKE(fmt_index, arg_index)
#endif

/* ── 小工具 ───────────────────────────────────────────────── */

/** 带长度上限的字符串复制：永远以 '\0' 收尾，不越界。 */
static void sr_copy_string(char *dst, size_t cap, const char *src)
{
    size_t length;

    if (dst == NULL || cap == 0) {
        return;
    }
    if (src == NULL) {
        dst[0] = '\0';
        return;
    }
    length = strlen(src);
    if (length >= cap) {
        length = cap - 1;
    }
    memcpy(dst, src, length);
    dst[length] = '\0';
}

SR_PRINTF_LIKE(4, 5)
static size_t sr_appendf(char *out, size_t cap, size_t used,
                         const char *format, ...)
{
    va_list args;
    int written;

    if (out == NULL || cap == 0 || used >= cap) {
        return used;
    }

    va_start(args, format);
    written = vsnprintf(out + used, cap - used, format, args);
    va_end(args);

    if (written < 0) {
        return cap;                             /* 编码出错：当作已写满 */
    }
    if ((size_t)written >= cap - used) {
        return cap;                             /* 被截断：当作已写满 */
    }
    return used + (size_t)written;
}

/**
 * 按空白把一行拆成若干字段：就地写入 '\0'，fields 指向各字段的开头。
 * 返回字段数；字段数超过 max_fields 时返回 -1。
 *
 * 这里用的是 strspn / strcspn 的「跳过一段、走过一段」写法，
 * 不修改调用方传进来的原串之外的东西，也不需要 strtok 的静态状态。
 */
static int sr_split_fields(char *line, char *fields[], int max_fields)
{
    int count = 0;
    char *cursor = line;

    while (*cursor != '\0') {
        cursor += strspn(cursor, " \t\r\n");    /* 跳过前导空白 */
        if (*cursor == '\0') {
            break;
        }
        if (count == max_fields) {
            return -1;
        }
        fields[count] = cursor;
        count += 1;

        cursor += strcspn(cursor, " \t\r\n");   /* 走到这个字段的末尾 */
        if (*cursor == '\0') {
            break;
        }
        *cursor = '\0';
        cursor += 1;
    }
    return count;
}

/** 日期只认 YYYY-MM-DD：C 里没有现成的日期解析，只能逐字符看。 */
static int sr_is_date(const char *text)
{
    int i;

    if (text == NULL || strlen(text) != 10) {
        return 0;
    }
    for (i = 0; i < 10; ++i) {
        if (i == 4 || i == 7) {
            if (text[i] != '-') {
                return 0;
            }
        } else if (text[i] < '0' || text[i] > '9') {
            return 0;
        }
    }
    return 1;
}

/* ── 解析与聚合 ───────────────────────────────────────────── */

void sr_init(sr_report_t *report)
{
    if (report == NULL) {
        return;
    }
    memset(report, 0, sizeof *report);
}

int sr_parse_line(const char *line, sr_record_t *record)
{
    char buffer[SR_LINE_MAX];
    char *fields[SR_MAX_FIELDS];
    char *end = NULL;
    long quantity;
    double price;
    size_t length;
    int count;

    if (line == NULL || record == NULL) {
        return 0;
    }

    length = strlen(line);
    if (length >= sizeof buffer) {
        return 0;                               /* 这一行太长，直接跳过 */
    }
    memcpy(buffer, line, length + 1);           /* 分词会改缓冲区，先抄一份 */

    count = sr_split_fields(buffer, fields, SR_MAX_FIELDS);
    if (count != SR_MAX_FIELDS) {
        return 0;
    }
    if (!sr_is_date(fields[0])) {
        return 0;
    }
    if (strlen(fields[1]) >= sizeof record->product) {
        return 0;
    }

    /* strtol / strtod 都要看三件事：有没有转换成功、有没有走到串尾、errno 变没变 */
    errno = 0;
    quantity = strtol(fields[2], &end, 10);
    if (end == fields[2] || *end != '\0' || errno != 0 || quantity <= 0) {
        return 0;
    }

    errno = 0;
    price = strtod(fields[3], &end);
    if (end == fields[3] || *end != '\0' || errno != 0 || price < 0.0) {
        return 0;
    }

    sr_copy_string(record->date, sizeof record->date, fields[0]);
    sr_copy_string(record->product, sizeof record->product, fields[1]);
    record->quantity = quantity;
    record->unit_price = price;
    record->amount = (double)quantity * price;
    return 1;
}

int sr_add_record(sr_report_t *report, const sr_record_t *record)
{
    sr_product_t *slot = NULL;
    int i;

    if (report == NULL || record == NULL) {
        return 0;
    }

    for (i = 0; i < report->product_count; ++i) {
        if (strcmp(report->products[i].name, record->product) == 0) {
            slot = &report->products[i];
            break;
        }
    }

    if (slot == NULL) {
        if (report->product_count >= SR_MAX_PRODUCTS) {
            return 0;                           /* 商品种类超上限 */
        }
        slot = &report->products[report->product_count];
        report->product_count += 1;
        sr_copy_string(slot->name, sizeof slot->name, record->product);
        slot->orders = 0;
        slot->quantity = 0;
        slot->amount = 0.0;
    }

    slot->orders += 1;
    slot->quantity += record->quantity;
    slot->amount += record->amount;

    report->total_quantity += record->quantity;
    report->total_amount += record->amount;
    report->lines_valid += 1;

    if (report->record_count < SR_MAX_RECORDS) {
        report->records[report->record_count] = *record;
        report->record_count += 1;
    }
    return 1;
}

/* ── 排序：qsort 的比较函数 ───────────────────────────────── */

static int sr_cmp_amount_desc(const void *lhs, const void *rhs)
{
    const sr_product_t *a = (const sr_product_t *)lhs;
    const sr_product_t *b = (const sr_product_t *)rhs;

    if (a->amount > b->amount) {
        return -1;
    }
    if (a->amount < b->amount) {
        return 1;
    }
    return strcmp(a->name, b->name);            /* 金额并列时按名称升序 */
}

static int sr_cmp_name_asc(const void *lhs, const void *rhs)
{
    const sr_product_t *a = (const sr_product_t *)lhs;
    const sr_product_t *b = (const sr_product_t *)rhs;

    return strcmp(a->name, b->name);
}

static int sr_cmp_record_amount_desc(const void *lhs, const void *rhs)
{
    const sr_record_t *a = (const sr_record_t *)lhs;
    const sr_record_t *b = (const sr_record_t *)rhs;
    int by_name;

    if (a->amount > b->amount) {
        return -1;
    }
    if (a->amount < b->amount) {
        return 1;
    }
    by_name = strcmp(a->date, b->date);
    if (by_name != 0) {
        return by_name;
    }
    return strcmp(a->product, b->product);
}

void sr_sort_products_by_amount(sr_report_t *report)
{
    if (report == NULL || report->product_count < 2) {
        return;
    }
    qsort(report->products, (size_t)report->product_count,
          sizeof report->products[0], sr_cmp_amount_desc);
}

void sr_sort_products_by_name(sr_report_t *report)
{
    if (report == NULL || report->product_count < 2) {
        return;
    }
    qsort(report->products, (size_t)report->product_count,
          sizeof report->products[0], sr_cmp_name_asc);
}

void sr_sort_records_by_amount(sr_report_t *report)
{
    if (report == NULL || report->record_count < 2) {
        return;
    }
    qsort(report->records, (size_t)report->record_count,
          sizeof report->records[0], sr_cmp_record_amount_desc);
}

/* ── 读文件 ───────────────────────────────────────────────── */

int sr_load(const char *path, sr_report_t *report, char *err, size_t err_size)
{
    FILE *stream;
    char line[SR_LINE_MAX];

    if (path == NULL || report == NULL) {
        return 0;
    }

    stream = fopen(path, "r");
    if (stream == NULL) {
        if (err != NULL && err_size > 0) {
            snprintf(err, err_size, "打不开文件 %s：%s", path, strerror(errno));
        }
        return 0;
    }

    sr_init(report);

    while (fgets(line, (int)sizeof line, stream) != NULL) {
        const char *head;
        sr_record_t record;

        report->lines_read += 1;

        /* 空行与注释行既不算有效数据行，也不算跳过行 */
        head = line + strspn(line, " \t\r\n");
        if (*head == '\0' || *head == '#') {
            continue;
        }

        if (!sr_parse_line(line, &record)) {
            report->lines_skipped += 1;
            continue;
        }
        if (!sr_add_record(report, &record)) {
            report->lines_skipped += 1;
        }
    }

    if (ferror(stream) != 0) {
        if (err != NULL && err_size > 0) {
            snprintf(err, err_size, "读取 %s 的过程中出错", path);
        }
        fclose(stream);
        return 0;
    }
    fclose(stream);

    sr_sort_products_by_amount(report);
    sr_sort_records_by_amount(report);
    return 1;
}

/* ── 拼报表 ───────────────────────────────────────────────── */

size_t sr_format_report(const sr_report_t *report, const char *path,
                        char *out, size_t out_size)
{
    /* 第二段要按名称升序，而 report 里的商品已按金额排过，
       因此复制一份来排，不动调用方传进来的数据。 */
    sr_product_t by_name[SR_MAX_PRODUCTS];
    size_t used = 0;
    double average;
    int i;

    if (out == NULL || out_size == 0) {
        return 0;
    }
    out[0] = '\0';
    if (report == NULL) {
        return 0;
    }
    if (path == NULL) {
        path = "(未指定)";
    }

    used = sr_appendf(out, out_size, used,
                      "==================== 销售记录报表 ====================\n");
    used = sr_appendf(out, out_size, used, "%-16s: %s\n", "输入文件", path);
    used = sr_appendf(out, out_size, used, "%-16s: %d\n", "读取行数", report->lines_read);
    used = sr_appendf(out, out_size, used, "%-16s: %d\n", "有效数据行", report->lines_valid);
    used = sr_appendf(out, out_size, used, "%-16s: %d\n", "跳过行数", report->lines_skipped);
    used = sr_appendf(out, out_size, used, "%-16s: %d\n", "商品种类", report->product_count);
    used = sr_appendf(out, out_size, used, "%-16s: %ld\n", "总数量", report->total_quantity);
    used = sr_appendf(out, out_size, used, "%-16s: %.2f\n", "总金额", report->total_amount);

    average = report->total_quantity > 0
        ? report->total_amount / (double)report->total_quantity
        : 0.0;
    used = sr_appendf(out, out_size, used, "%-16s: %.3f\n", "平均单价", average);

    /* 第 1 段：按金额降序 */
    used = sr_appendf(out, out_size, used, "\n[1] 按金额降序\n");
    used = sr_appendf(out, out_size, used, SR_FMT_HEAD1,
                      "排名", "商品", "订单数", "数量", "金额", "占比");
    used = sr_appendf(out, out_size, used, "%s\n", SR_RULE1);
    for (i = 0; i < report->product_count; ++i) {
        const sr_product_t *item = &report->products[i];
        double share = report->total_amount > 0.0
            ? item->amount * 100.0 / report->total_amount
            : 0.0;
        used = sr_appendf(out, out_size, used, SR_FMT_ROW1,
                          i + 1, item->name, item->orders, item->quantity,
                          item->amount, share);
    }
    used = sr_appendf(out, out_size, used, SR_FMT_SUM, "", "合计",
                      report->lines_valid, report->total_quantity,
                      report->total_amount, 100.0);

    /* 第 2 段：按商品名升序 */
    memcpy(by_name, report->products, sizeof by_name);
    qsort(by_name, (size_t)report->product_count,
          sizeof by_name[0], sr_cmp_name_asc);

    used = sr_appendf(out, out_size, used, "\n[2] 按商品名升序\n");
    used = sr_appendf(out, out_size, used, SR_FMT_HEAD2,
                      "商品", "订单数", "数量", "金额", "占比");
    used = sr_appendf(out, out_size, used, "%s\n", SR_RULE2);
    for (i = 0; i < report->product_count; ++i) {
        const sr_product_t *item = &by_name[i];
        double share = report->total_amount > 0.0
            ? item->amount * 100.0 / report->total_amount
            : 0.0;
        used = sr_appendf(out, out_size, used, SR_FMT_ROW2,
                          item->name, item->orders, item->quantity,
                          item->amount, share);
    }

    /* 第 3 段：单笔金额最高的前三笔 */
    used = sr_appendf(out, out_size, used, "\n[3] 单笔金额最高的前三笔\n");
    used = sr_appendf(out, out_size, used, SR_FMT_HEAD3,
                      "排名", "日期", "商品", "数量", "金额");
    used = sr_appendf(out, out_size, used, "%s\n", SR_RULE3);
    {
        int limit = report->record_count < 3 ? report->record_count : 3;
        for (i = 0; i < limit; ++i) {
            const sr_record_t *item = &report->records[i];
            used = sr_appendf(out, out_size, used, SR_FMT_ROW3,
                              i + 1, item->date, item->product,
                              item->quantity, item->amount);
        }
    }

    used = sr_appendf(out, out_size, used,
                      "\n====================== 报表结束 ======================\n");
    return used;
}

/* ── 计时 ─────────────────────────────────────────────────────
 *
 * 为什么不用 <time.h> 里的东西：
 *   clock() 在标准里量的是处理器时间，在本机的 MinGW 上却是墙上时间
 *   （A-04 第 6 节有实测），语义依平台而异，不能拿来量「跑了多久」。
 *   C11 的 timespec_get 是 UCRT 才有的，本机的 MinGW 走 msvcrt，
 *   <time.h> 里没有它的声明（A-04 第 5 节讲了原因）。
 *   于是只剩平台自己的单调钟：Windows 上是 QueryPerformanceCounter。
 * ────────────────────────────────────────────────────────────── */

double sr_now_ms(void)
{
#if defined(_WIN32)
    LARGE_INTEGER counter;
    LARGE_INTEGER frequency;

    if (QueryPerformanceFrequency(&frequency) == 0
        || QueryPerformanceCounter(&counter) == 0
        || frequency.QuadPart == 0) {
        return 0.0;
    }
    return (double)counter.QuadPart * 1000.0 / (double)frequency.QuadPart;
#else
    struct timespec mark;

    mark.tv_sec = 0;
    mark.tv_nsec = 0;
    if (timespec_get(&mark, TIME_UTC) != TIME_UTC) {
        return 0.0;
    }
    return (double)mark.tv_sec * 1000.0 + (double)mark.tv_nsec / 1000000.0;
#endif
}

/* ── 内置自测 ─────────────────────────────────────────────── */

SR_PRINTF_LIKE(3, 4)
static void sr_check(sr_checks_t *checks, int ok, const char *format, ...)
{
    /* 比 SR_CHECK_LINE_MAX 小一截：拼出来的那一行要留得下前缀，
       留够了 gcc 的 -Wformat-truncation 才不会报警。 */
    char title[96];
    va_list args;

    if (checks == NULL) {
        return;
    }

    va_start(args, format);
    vsnprintf(title, sizeof title, format, args);
    va_end(args);

    checks->total += 1;
    if (ok) {
        checks->passed += 1;
    } else {
        checks->failed += 1;
    }

    if (checks->count < SR_CHECK_MAX) {
        snprintf(checks->lines[checks->count], SR_CHECK_LINE_MAX,
                 "[%s] %d. %s", ok ? "通过" : "不通过", checks->total, title);
        checks->count += 1;
    }
}

static int sr_near(double value, double expected)
{
    double diff = value - expected;

    if (diff < 0.0) {
        diff = -diff;
    }
    return diff < 0.005;
}

void sr_run_selftest(sr_checks_t *out)
{
    sr_report_t report;
    sr_record_t record;
    char buffer[SR_LINE_MAX];
    char text[SR_REPORT_MAX];
    char *fields[SR_MAX_FIELDS];
    int count;

    if (out == NULL) {
        return;
    }
    memset(out, 0, sizeof *out);

    /* 1–3：分词 */
    sr_copy_string(buffer, sizeof buffer, "2026-01-05  keyboard  3  199.00");
    count = sr_split_fields(buffer, fields, SR_MAX_FIELDS);
    sr_check(out, count == 4
                  && strcmp(fields[0], "2026-01-05") == 0
                  && strcmp(fields[3], "199.00") == 0,
             "split_fields 把一行拆成 4 个字段");

    sr_copy_string(buffer, sizeof buffer, "2026-01-05\tkeyboard\t 3\t199.00");
    count = sr_split_fields(buffer, fields, SR_MAX_FIELDS);
    sr_check(out, count == 4
                  && strcmp(fields[1], "keyboard") == 0
                  && strcmp(fields[2], "3") == 0,
             "split_fields 把制表符与连续空白都当作分隔");

    sr_copy_string(buffer, sizeof buffer, "a b c d e");
    count = sr_split_fields(buffer, fields, SR_MAX_FIELDS);
    sr_check(out, count == -1, "split_fields 见到第 5 个字段返回 -1");

    /* 4–5：解析正常行 */
    sr_check(out, sr_parse_line("2026-01-05  keyboard  3  199.00", &record) == 1
                  && record.quantity == 3
                  && sr_near(record.unit_price, 199.00)
                  && strcmp(record.product, "keyboard") == 0,
             "parse_line 解析正常行，数量与单价都对");

    sr_check(out, sr_near(record.amount, 597.00),
             "parse_line 同时算出金额 3 × 199.00 = 597.00");

    /* 6–10：各类坏行都该被拒 */
    sr_check(out, sr_parse_line("2026-01-20  mouse  1", &record) == 0,
             "parse_line 拒绝只有 3 个字段的行");

    sr_check(out, sr_parse_line("2026-01-19  usb-hub  -  49.90", &record) == 0,
             "parse_line 拒绝数量不是数字的行");

    sr_check(out, sr_parse_line("2026-01-19  webcam  2  abc", &record) == 0,
             "parse_line 拒绝单价不是数字的行");

    sr_check(out, sr_parse_line("2026-01-19  webcam  0  329.00", &record) == 0
                  && sr_parse_line("2026-01-19  webcam  -2  329.00", &record) == 0,
             "parse_line 拒绝数量为 0 或负数的行");

    sr_check(out, sr_parse_line("2026/01/05  keyboard  3  199.00", &record) == 0
                  && sr_parse_line("20260105  keyboard  3  199.00", &record) == 0,
             "parse_line 只认 YYYY-MM-DD 形式的日期");

    /* 11–14：聚合。造一份小数据，手算得出的答案写在断言里。
       keyboard 7 × 199.00 = 1393.00
       usb-hub  7 × 199.00 = 1393.00   （与 keyboard 金额并列，用来验并列规则）
       monitor  2 × 899.00 = 1798.00
       mouse   12 ×  59.50 =  714.00                                */
    sr_init(&report);
    {
        static const char *const lines[] = {
            "2026-01-05  keyboard  3  199.00",
            "2026-01-07  keyboard  4  199.00",
            "2026-01-06  monitor   2  899.00",
            "2026-01-09  mouse    12   59.50",
            "2026-01-11  usb-hub   7  199.00"
        };
        size_t i;
        for (i = 0; i < sizeof lines / sizeof lines[0]; ++i) {
            if (sr_parse_line(lines[i], &record)) {
                (void)sr_add_record(&report, &record);
            }
        }
    }

    sr_check(out, report.product_count == 4,
             "add_record 把 5 条记录归成 4 种商品，同名商品并成一行");

    sr_check(out, report.products[0].orders == 2
                  && report.products[0].quantity == 7,
             "add_record 累加订单数（keyboard 2 笔）与数量（7 件）");

    sr_check(out, sr_near(report.products[0].amount, 1393.00),
             "add_record 累加金额（keyboard 1393.00）");

    sr_check(out, sr_near(report.total_amount, 5298.00)
                  && report.total_quantity == 28,
             "add_record 累加总量 28 与总额 5298.00");

    /* 15–17：排序 */
    sr_sort_products_by_amount(&report);
    sr_check(out, strcmp(report.products[0].name, "monitor") == 0
                  && strcmp(report.products[3].name, "mouse") == 0,
             "sort_products_by_amount 把金额最大的排到最前");

    sr_check(out, sr_near(report.products[1].amount, report.products[2].amount)
                  && strcmp(report.products[1].name, "keyboard") == 0
                  && strcmp(report.products[2].name, "usb-hub") == 0,
             "金额并列时按名称升序，输出顺序稳定");

    qsort(report.products, (size_t)report.product_count,
          sizeof report.products[0], sr_cmp_name_asc);
    sr_check(out, strcmp(report.products[0].name, "keyboard") == 0
                  && strcmp(report.products[1].name, "monitor") == 0
                  && strcmp(report.products[2].name, "mouse") == 0
                  && strcmp(report.products[3].name, "usb-hub") == 0,
             "按名称升序排出来是 keyboard、monitor、mouse、usb-hub");

    /* 18–20：报表文本 */
    sr_sort_products_by_amount(&report);
    sr_sort_records_by_amount(&report);
    (void)sr_format_report(&report, "data/sales.txt", text, sizeof text);

    sr_check(out, strncmp(text, "==================== 销售记录报表", 18) == 0,
             "format_report 的头一行是报表标题");

    sr_check(out, strstr(text, "monitor") != NULL
                  && strstr(text, "合计") != NULL
                  && strstr(text, "[3] 单笔金额最高的前三笔") != NULL,
             "format_report 里有商品名、合计行与第三段标题");

    sr_check(out, strstr(text, "报表结束") != NULL
                  && text[0] != '\0'
                  && strlen(text) < sizeof text,
             "format_report 以「报表结束」收尾且没有把缓冲区写满");
}

void sr_checks_summary(const sr_checks_t *checks, char *out, size_t out_size)
{
    if (out == NULL || out_size == 0) {
        return;
    }
    if (checks == NULL) {
        sr_copy_string(out, out_size, "没有自测结果");
        return;
    }
    snprintf(out, out_size, "%d 项中 %d 项通过，%s",
             checks->total, checks->passed,
             checks->failed == 0 ? "全部通过" : "有未通过项");
}
