/**
 * sales_report.hpp —— C++ 版销售记录报表，核心逻辑（不依赖任何界面）
 *
 * 与 01-c-stdlib-toolbox 做同一件事：读同一份 data/sales.txt，
 * 出同一份报表文本，逐字节相同。两边的差别只在写法：
 *
 *   读文件    01 用 fopen + fgets           这里用 std::ifstream + getline
 *   分字段    01 用 strspn / strcspn         这里用 std::istringstream
 *   数字转换  01 用 strtol / strtod          这里用 std::stol / std::stod
 *   存数据    01 用固定长度数组              这里用 std::vector
 *   排序      01 用 qsort                    这里用 std::sort
 *   拼文本    01 用 snprintf                 这里用 std::ostringstream + <iomanip>
 *   计时      01 用 QueryPerformanceCounter  这里用 std::chrono::steady_clock
 *
 * 命令行版（src/main_cli.cpp）与两份界面版（Win32、Qt）都链接它。
 * 这里的函数不往屏幕上打印任何东西，只负责算并返回结果。
 */
#ifndef SALES_REPORT_HPP
#define SALES_REPORT_HPP

#include <cstddef>
#include <string>
#include <vector>

namespace sales {

/** 与 C 版一致的上限：超过就不再收，避免异常数据把内存吃光 */
constexpr std::size_t kMaxProducts = 32;
constexpr std::size_t kMaxRecords = 256;
constexpr std::size_t kNameMax = 32;

/** 一条明细记录 */
struct Record {
    std::string date;
    std::string product;
    long        quantity = 0;
    double      unit_price = 0.0;
    double      amount = 0.0;       /**< 数量 × 单价，解析时就一次算好 */
};

/** 按商品聚合后的一行 */
struct Product {
    std::string name;
    int         orders = 0;         /**< 出现多少笔 */
    long        quantity = 0;       /**< 数量合计 */
    double      amount = 0.0;       /**< 金额合计 */
};

/** 整份报表的数据。load 返回时，两个 vector 都已排好序 */
struct Report {
    int                  lines_read = 0;      /**< 读进来的总行数（含注释行与空行） */
    int                  lines_valid = 0;     /**< 解析成功的数据行 */
    int                  lines_skipped = 0;   /**< 解析失败被跳过的行 */
    long                 total_quantity = 0;
    double               total_amount = 0.0;
    std::vector<Product> products;            /**< 按金额降序 */
    std::vector<Record>  records;             /**< 按金额降序，报表第三段只取前三条 */
};

/** 自测结果。不打印，交给界面决定怎么显示 */
struct CheckResult {
    int                      total = 0;
    int                      passed = 0;
    int                      failed = 0;
    std::vector<std::string> lines;           /**< 每项一行，形如 "[通过] 3. ……" */

    bool all_passed() const { return failed == 0; }
    std::string summary() const;              /**< "20 项中 20 项通过，全部通过" */
};

/** 按空白把一个行拆成若干字段。返回空 vector 表示这一行没有内容 */
std::vector<std::string> split_fields(const std::string &line);

/** 日期只认 YYYY-MM-DD */
bool is_date(const std::string &text);

/**
 * 解析一行文本。成功返回 true 并填好 out；失败返回 false。
 *
 * 判定为失败的情形：字段数不是 4、日期格式不对、商品名过长、
 * 数量不是正整数、单价不是非负数字。
 * 空行与以 '#' 开头的行由调用方先筛掉，不走这里。
 */
bool parse_line(const std::string &line, Record &out);

/**
 * 把一条记录并进报表：同名的商品归到一行，并累加总量与总额。
 * 商品种类或明细条数超过上限时返回 false。
 */
bool add_record(Report &report, const Record &record);

/** 按金额降序排商品；金额相同时按名称升序，保证输出稳定 */
void sort_products_by_amount(Report &report);

/** 按名称升序排商品 */
void sort_products_by_name(Report &report);

/** 按金额降序排明细，金额相同时按日期、商品名升序 */
void sort_records_by_amount(Report &report);

/**
 * 读文件并聚合。打不开或读出错时 ok 置 false，原因写进 error。
 * 成功时 ok 置 true，返回的 Report 已经排好序。
 */
Report load(const std::string &path, bool &ok, std::string &error);

/**
 * 把报表拼成文本。
 *
 * 宽度用 <iomanip> 的 setw 指定。它按「字符数」补齐，而这里的
 * 中文常量在 GBK 执行字符集下每个汉字正好是 2 个字节、也是 2 个字符，
 * 于是 setw(16) 与 C 版的 "%-16s" 补出来一样宽。
 */
std::string format_report(const Report &report, const std::string &path);

/**
 * 取一个毫秒级的单调时间戳。
 *
 * 这里用 std::chrono::steady_clock：它在标准里就规定了只往前走，
 * 不受系统对时影响。C 版没有这个条件，只能落到平台 API
 * （QueryPerformanceCounter），两边的差别见 01 的 README。
 */
double now_ms();

/** 逐项核对分词、解析、聚合、排序与报表格式 */
CheckResult run_self_tests();

}   /* namespace sales */

#endif /* SALES_REPORT_HPP */
