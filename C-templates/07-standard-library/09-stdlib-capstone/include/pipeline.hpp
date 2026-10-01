/**
 * pipeline.hpp —— 练习模板 09 的核心接口（C++，毕业练习）
 *
 * 这是一条完整的流水线：读文件 -> 解析成记录 -> 统计与抽样 -> 计时 -> 报表，
 * 把前面几个模板里练过的东西串起来。接口已经定好，
 * 你要做的是在 src/pipeline.cpp 里把标了 TODO 的函数实现出来。
 *
 * 五个阶段的对应关系：
 *     阶段 1   read_text_file、count_lines          <filesystem> 与 <fstream>
 *     阶段 2   parse_line、parse_log                optional / variant 与 string_view
 *     阶段 3   summarize、sample_ms                 <numeric>、<random>、unique_ptr
 *     阶段 4   measure_pipeline、count_records_atomic   <chrono> 与 <atomic>
 *     阶段 5   format_report                        <iomanip> 与错误处理
 */
#ifndef PIPELINE_HPP
#define PIPELINE_HPP

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace pl {

/* 报表里给出最慢的多少条 */
constexpr int kTopSlow = 5;

/* 一条日志记录：时间戳、级别、任务名、工作线程、耗时、是否成功 */
struct Record {
    std::string ts;
    std::string level;    /* INFO / WARN / ERROR */
    std::string task;
    int         worker = 0;
    long        ms = 0;
    bool        ok = false;
};

/* 一次解析的全部结果 */
struct ParseResult {
    std::string                path;             /* 输入文件路径，由调用方填，报表里要打印 */
    std::vector<Record>        records;
    long                       total_lines = 0;   /* 非空行数（含不合法行） */
    long                       bad_lines = 0;     /* 解析失败的行数 */
    std::vector<std::string>   bad_samples;       /* 前 3 条不合法行的原文 */
};

/* ==================================================================
 * 阶段 1：读取与数行
 * ================================================================== */

/* 用 <filesystem> 确认文件存在、用 std::ifstream 读全文。
 * 失败时返回空串并把原因写进 err。 */
std::string read_text_file(const std::string &path, std::string &err);

/* 数行数：'\n' 的个数；最后一个字节不是 '\n' 且文本非空时再加一 */
long count_lines(const std::string &text);

/* ==================================================================
 * 阶段 2：解析
 * ================================================================== */

/* 解析一行。格式（字段之间是空白，级别与任务名不带空格）：
 *     2026-10-01T08:15:02 INFO  worker=1 task=parse ms=12 ok=1
 * 任何一处不合法（字段个数不对、级别不认识、ms 不是数字、ok 不是 0/1）都返回 nullopt。
 * 级别只认 INFO / WARN / ERROR。 */
std::optional<Record> parse_line(std::string_view line);

/* 逐行解析。空行跳过（不计入 total_lines）；其余每行都计入，
 * 其中解析失败的计入 bad_lines，并把前 3 条原文放进 bad_samples。 */
ParseResult parse_log(const std::string &text);

/* ==================================================================
 * 阶段 3：统计与抽样
 * ================================================================== */

struct Summary {
    long   info = 0;
    long   warn = 0;
    long   error = 0;
    double mean_ms = 0.0;
    double median_ms = 0.0;
    long   max_ms = 0;
    /* 任务名 -> 条数，按任务名升序 */
    std::vector<std::pair<std::string, long>> by_task;
};

/* 级别计数、耗时的均值与中位数、最大值，以及按任务的条数。
 * 记录为空时返回全 0。 */
Summary summarize(const std::vector<Record> &records);

/* 用固定种子的 std::mt19937 从记录里随机抽 n 条的耗时（可以有重复）。
 * 记录为空或 n 为 0 时返回空表。 */
std::vector<long> sample_ms(const std::vector<Record> &records, unsigned seed, std::size_t n);

/* ==================================================================
 * 阶段 4：计时与计数
 * ================================================================== */

struct Timings {
    double parse_ms = 0.0;    /* 解析整段文本 */
    double report_ms = 0.0;   /* 拼报表 */
    double total_ms = 0.0;    /* 从读到文本到报表拼完 */
};

/* 用 steady_clock 给三个阶段计时。parse_log 要真的跑一遍（结果不用返回）。 */
Timings measure_pipeline(const std::string &text);

/* 用 std::atomic<long long> 数出记录条数（同时起到防止整段被优化掉的作用） */
long count_records_atomic(const std::vector<Record> &records);

/* ==================================================================
 * 阶段 5：报表
 * ================================================================== */

/* 把解析结果、统计量与耗时渲染成一段报表。格式见《配置步骤.md》阶段 5：
 *
 *     file      : data/app.log
 *     lines     : 24
 *     records   : 20
 *     bad lines : 4
 *     INFO/WARN/ERROR : 14 / 3 / 3
 *     ms mean/median/max : 12.35 / 10.00 / 310
 *     by task   :
 *       flush   : 3
 *       parse   : 14
 *       write   : 3
 *     slowest   :
 *         1  write     310  ERROR
 *     timings   : parse = 0.412 ms, report = 0.031 ms, total = 0.470 ms
 *
 * 文件路径由调用方在拼装前填进 ParseResult（本函数不读文件）。
 * 具体对齐宽度见验收输出，逐字节比对。 */
std::string format_report(const ParseResult &pr, const Summary &s, const Timings &t);

} /* namespace pl */

#endif /* PIPELINE_HPP */
