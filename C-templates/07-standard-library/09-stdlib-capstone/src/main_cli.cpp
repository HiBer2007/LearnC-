/**
 * main_cli.cpp —— 练习模板 09 的命令行驱动（毕业练习）
 *
 * 两种用法：
 *     app_cli.exe              按 5 个阶段跑一遍流水线，打印每一步的结果
 *     app_cli.exe --selftest   跑 31 项自测，打印逐项结果与汇总
 *
 * 这个文件**不需要改**：它只调用 include/pipeline.hpp 里的接口。
 * 你的实现写在 src/pipeline.cpp 里。
 *
 * 构建与运行（**在模板目录下**执行，数据文件是相对路径 data/app.log）：
 *     cmake --preset mingw-gdb
 *     cmake --build --preset mingw-gdb
 *     build\mingw\bin\app_cli.exe
 *     build\mingw\bin\app_cli.exe --selftest
 */
#include <cstring>
#include <iomanip>
#include <iostream>
#include <memory>
#include <string>

#include "pipeline.hpp"

namespace {

int g_pass = 0;
int g_fail = 0;

void check(bool ok, const std::string &name)
{
    if (ok) {
        ++g_pass;
        std::cout << "  [ok]   " << name << "\n";
    } else {
        ++g_fail;
        std::cout << "  [FAIL] " << name << "\n";
    }
}

const char *kSampleText =
    "2026-10-01T08:15:02 INFO  worker=1 task=parse ms=12 ok=1\n"
    "2026-10-01T08:15:03 WARN  worker=2 task=flush ms=48 ok=0\n"
    "\n"
    "2026-10-01T08:15:04 ERROR worker=1 task=write ms=310 ok=0\n"
    "this line has no fields at all\n";

/* 31 项自测：覆盖 5 个阶段的每一件事 */
int run_selftest()
{
    const std::string text = kSampleText;
    std::string err;

    std::cout << "=== selftest ===\n";

    /* 阶段 1 */
    const std::string real = pl::read_text_file("data/app.log", err);
    check(!real.empty(), "read_text_file reads data/app.log");
    check(pl::count_lines(real) == 24, "data/app.log has 24 lines");
    check(pl::read_text_file("data/no_such.log", err).empty() && !err.empty(),
          "read_text_file reports a missing file through err");
    check(pl::count_lines(text) == 5, "count_lines counts 5 lines in the sample");
    check(pl::count_lines(std::string()) == 0, "count_lines of an empty text is 0");

    /* 阶段 2 */
    const auto good = pl::parse_line("2026-10-01T08:15:02 INFO  worker=1 task=parse ms=12 ok=1");
    check(good.has_value(), "parse_line accepts a valid line");
    check(good.has_value() && good->worker == 1, "parse_line reads worker=1");
    check(good.has_value() && good->task == "parse", "parse_line reads task=parse");
    check(good.has_value() && good->ms == 12, "parse_line reads ms=12");
    check(good.has_value() && good->ok, "parse_line reads ok=1 as true");
    check(!pl::parse_line("2026-10-01T08:21:00 DEBUG worker=4 task=flush ms=10 ok=1").has_value(),
          "parse_line rejects an unknown level");
    check(!pl::parse_line("2026-10-01T08:20:00 INFO  worker=3 task=parse ms=abc ok=1").has_value(),
          "parse_line rejects a non numeric ms");
    check(!pl::parse_line("2026-10-01T08:22:00 INFO  worker=5 ms=10 ok=1").has_value(),
          "parse_line rejects a line with a missing field");
    check(!pl::parse_line("short").has_value(), "parse_line rejects a short line");

    const pl::ParseResult pr = pl::parse_log(text);
    check(pr.total_lines == 4, "parse_log counts 4 non empty lines");
    check(pr.bad_lines == 1, "parse_log finds 1 bad line");
    check(pr.records.size() == 3, "parse_log collects 3 records");
    check(pr.bad_samples.size() == 1, "parse_log keeps the first bad sample");
    check(pl::parse_log(std::string()).records.empty(), "parse_log of an empty text is empty");

    /* 阶段 3 */
    const pl::Summary s = pl::summarize(pr.records);
    check(s.info == 1, "summarize counts 1 INFO");
    check(s.warn == 1, "summarize counts 1 WARN");
    check(s.error == 1, "summarize counts 1 ERROR");
    check(s.max_ms == 310, "summarize finds max_ms = 310");
    check(s.median_ms == 48.0, "summarize finds median_ms = 48");
    check(s.by_task.size() == 3, "summarize groups by 3 tasks");
    check(pl::sample_ms(pr.records, 2026u, 5).size() == 5, "sample_ms returns 5 samples");

    /* 阶段 4 */
    check(pl::count_records_atomic(pr.records) == 3, "count_records_atomic counts 3");
    const pl::Timings t = pl::measure_pipeline(text);
    check(t.total_ms >= 0.0 && t.parse_ms >= 0.0, "measure_pipeline returns non negative times");

    /* 阶段 5 */
    const std::string report = pl::format_report(pr, s, t);
    check(report.find("lines") != std::string::npos, "format_report contains a lines row");
    check(report.find("parse") != std::string::npos, "format_report contains the task table");
    check(report.size() > 100, "format_report is not a placeholder");

    std::cout << "\n" << (g_pass + g_fail) << " 项中 " << g_pass << " 项通过";
    if (g_fail == 0) {
        std::cout << "，全部通过\n";
    } else {
        std::cout << "，" << g_fail << " 项失败\n";
    }
    return g_fail;
}

} /* namespace */

int main(int argc, char **argv)
{
    if (argc > 1 && std::strcmp(argv[1], "--selftest") == 0) {
        return run_selftest();
    }

    const std::string path = (argc > 1) ? argv[1] : "data/app.log";

    /* ---------------------------------------------------------- 阶段 1 */
    std::cout << "=== Stage 1: read and count ===\n";
    std::string err;
    std::string text = pl::read_text_file(path, err);
    if (text.empty()) {
        std::cout << "read failed : " << err << "\n";
        std::cout << "fallback    : built-in 4-line sample (so the later stages still run)\n";
        text = kSampleText;
    }
    std::cout << "file             : " << path << "\n";
    std::cout << "bytes            : " << text.size() << "\n";
    std::cout << "lines            : " << pl::count_lines(text) << "\n";

    /* ---------------------------------------------------------- 阶段 2 */
    std::cout << "\n=== Stage 2: parse ===\n";
    std::unique_ptr<pl::ParseResult> parsed(new pl::ParseResult(pl::parse_log(text)));
    parsed->path = path;
    std::cout << "total lines      : " << parsed->total_lines << "\n";
    std::cout << "records          : " << parsed->records.size() << "\n";
    std::cout << "bad lines        : " << parsed->bad_lines << "\n";
    for (std::size_t i = 0; i < parsed->bad_samples.size(); ++i) {
        std::cout << "bad sample " << (i + 1) << "     : " << parsed->bad_samples[i] << "\n";
    }

    /* ---------------------------------------------------------- 阶段 3 */
    std::cout << "\n=== Stage 3: summarize and sample ===\n";
    const pl::Summary summary = pl::summarize(parsed->records);
    std::cout << std::fixed << std::setprecision(2);
    std::cout << "INFO/WARN/ERROR  : " << summary.info << " / " << summary.warn
              << " / " << summary.error << "\n";
    std::cout << "ms mean/median/max : " << summary.mean_ms << " / "
              << summary.median_ms << " / " << summary.max_ms << "\n";
    std::cout << std::defaultfloat << std::setprecision(6);
    std::cout << "by task          :";
    for (const auto &kv : summary.by_task) {
        std::cout << " " << kv.first << "=" << kv.second;
    }
    std::cout << "\n";
    std::cout << "sample seed=2026 :";
    for (long ms : pl::sample_ms(parsed->records, 2026u, 5)) {
        std::cout << " " << ms;
    }
    std::cout << "\n";

    /* ---------------------------------------------------------- 阶段 4 */
    std::cout << "\n=== Stage 4: timings and counting ===\n";
    std::cout << "records (atomic) : " << pl::count_records_atomic(parsed->records) << "\n";
    const pl::Timings timings = pl::measure_pipeline(text);
    std::cout << std::fixed << std::setprecision(3);
    std::cout << "timings          : parse = " << timings.parse_ms
              << " ms, report = " << timings.report_ms
              << " ms, total = " << timings.total_ms << " ms\n";
    std::cout << std::defaultfloat << std::setprecision(6);

    /* ---------------------------------------------------------- 阶段 5 */
    std::cout << "\n=== Stage 5: report ===\n";
    std::cout << pl::format_report(*parsed, summary, timings);

    return 0;
}
