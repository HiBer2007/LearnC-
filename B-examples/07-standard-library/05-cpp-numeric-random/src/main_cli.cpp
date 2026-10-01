/**
 * main_cli.cpp —— 命令行版
 *
 * 用法：
 *   app_cli                 用默认参数出一份报表，随后跑自测
 *   app_cli --seed 7        换一个种子（默认 20240601）
 *   app_cli --count 2000    换抽样规模（默认 4000）
 *   app_cli --selftest      只跑自测
 *
 * 界面部分只有下面这些 std::cout；算的部分全在 demo 命名空间里，
 * 与 Win32 版共用同一份实现（src/stats_demo.cpp）。
 */
#include "stats_demo.hpp"

#include <cstdlib>
#include <iostream>
#include <string>

namespace {

bool parse_count(const std::string &text, std::size_t &count)
{
    const long parsed = std::strtol(text.c_str(), nullptr, 10);
    if (parsed < 1 || parsed > 1000000) {
        return false;
    }
    count = static_cast<std::size_t>(parsed);
    return true;
}

}   /* namespace */

int main(int argc, char *argv[])
{
    std::cout << "示例 07-standard-library/05-cpp-numeric-random · 随机数与统计（命令行版）\n";

    demo::Options options;
    bool only_check = false;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--selftest") {
            only_check = true;
            continue;
        }
        if ((arg == "--seed" || arg == "--count") && i + 1 < argc) {
            const std::string value = argv[++i];
            std::string error;
            if (arg == "--seed" && !demo::parse_seed(value, options.seed, error)) {
                std::cout << "种子有误：" << error << "\n";
                return 2;
            }
            if (arg == "--count" && !parse_count(value, options.count)) {
                std::cout << "抽样规模要在 1 到 1000000 之间，收到：" << value << "\n";
                return 2;
            }
            continue;
        }
        std::cout << "用法：app_cli [--selftest] [--seed N] [--count N]\n";
        return 2;
    }

    if (!only_check) {
        const demo::Report report = demo::make_report(options);
        std::cout << "\n== 项目输出 ==\n" << report.data_text << "\n"
                  << report.stats_text << "\n" << report.histogram_text << "\n"
                  << report.rand_text;
    }

    const demo::CheckResult result = demo::run_self_tests();
    std::cout << "\n== 自测 ==\n";
    for (const std::string &line : result.lines) {
        std::cout << "  " << line << "\n";
    }
    std::cout << "\n  自测结果：" << result.summary() << "\n";
    return result.all_passed() ? 0 : 1;
}
