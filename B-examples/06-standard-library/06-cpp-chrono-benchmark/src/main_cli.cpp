/**
 * main_cli.cpp —— 命令行版
 *
 * 用法：
 *   app_cli                     跑全部三个用例
 *   app_cli --case prime-count  只跑一个用例，可以重复给
 *   app_cli --repeat 21         正式测量 21 轮（默认 11）
 *   app_cli --warmups 3         预热 3 轮（默认 2）
 *   app_cli --selftest          只跑自测
 *
 * 界面部分只有下面这些 std::cout；计时、统计与文本都在 bench 命名空间里
 * （src/benchmark.cpp）。
 */
#include "benchmark.hpp"

#include <cstdlib>
#include <iostream>
#include <string>

namespace {

bool parse_size(const std::string &text, std::size_t low, std::size_t high,
                std::size_t &value)
{
    const long parsed = std::strtol(text.c_str(), nullptr, 10);
    if (parsed < static_cast<long>(low) || parsed > static_cast<long>(high)) {
        return false;
    }
    value = static_cast<std::size_t>(parsed);
    return true;
}

}   /* namespace */

int main(int argc, char *argv[])
{
    std::cout << "示例 06-standard-library/06-cpp-chrono-benchmark · <chrono> 小基准（命令行版）\n";

    bench::Options options;
    bool only_check = false;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--selftest") {
            only_check = true;
            continue;
        }
        if ((arg == "--case" || arg == "--repeat" || arg == "--warmups") && i + 1 < argc) {
            const std::string value = argv[++i];
            if (arg == "--case") {
                const bench::Case *found = nullptr;
                if (!bench::find_case(value, found)) {
                    std::cout << "没有这个用例：" << value
                              << "\n可选：sum-of-squares、prime-count、matrix-multiply\n";
                    return 2;
                }
                options.cases.push_back(value);
                continue;
            }
            std::size_t parsed = 0;
            const std::size_t high = (arg == "--repeat") ? 1000 : 100;
            if (!parse_size(value, 1, high, parsed)) {
                std::cout << arg << " 要在 1 到 " << high << " 之间，收到：" << value << "\n";
                return 2;
            }
            if (arg == "--repeat") {
                options.repeats = parsed;
            } else {
                options.warmups = parsed;
            }
            continue;
        }
        std::cout << "用法：app_cli [--selftest] [--case 名字] [--repeat N] [--warmups N]\n";
        return 2;
    }

    if (!only_check) {
        const bench::Report report = bench::make_report(options);
        std::cout << "\n== 项目输出 ==\n" << report.method_text << "\n"
                  << report.table_text << "\n" << report.detail_text;
    }

    const bench::CheckResult result = bench::run_self_tests();
    std::cout << "\n== 自测 ==\n";
    for (const std::string &line : result.lines) {
        std::cout << "  " << line << "\n";
    }
    std::cout << "\n  自测结果：" << result.summary() << "\n";
    return result.all_passed() ? 0 : 1;
}
