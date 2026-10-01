/**
 * main_cli.cpp —— 命令行版
 *
 * 用法：
 *   app_cli                              读 data/analysis.cfg 与 data/corpus.txt
 *   app_cli --config 配置 --input 文本    换一份配置或一份输入
 *   app_cli --selftest                   只跑自测
 *
 * 界面部分只有下面这些 std::cout；配置、分词、统计、计时、报表全在 capstone 命名空间里。
 */
#include "capstone.hpp"

#include <iostream>
#include <string>

namespace {

void print_usage()
{
    std::cout << "用法：app_cli [--selftest] [--config 配置文件] [--input 输入文件]\n";
}

}   /* namespace */

int main(int argc, char *argv[])
{
    std::string config_path = "data/analysis.cfg";
    std::string input_path = "data/corpus.txt";
    bool only_check = false;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--selftest") {
            only_check = true;
        } else if (arg == "--config" && i + 1 < argc) {
            config_path = argv[++i];
        } else if (arg == "--input" && i + 1 < argc) {
            input_path = argv[++i];
        } else {
            std::cout << "无法识别的参数：" << arg << "\n";
            print_usage();
            return 2;
        }
    }

    std::cout << "示例 06-standard-library/09-stdlib-capstone · 标准库综合流水线（命令行版）\n";

    bool project_ok = true;
    if (!only_check) {
        const capstone::RunResult result = capstone::run(config_path, input_path);

        std::cout << "\n== 项目输出 ==\n";
        for (const std::string &line : result.report) {
            std::cout << line;
        }
        if (result.problems.empty()) {
            std::cout << "\n没有问题。\n";
        } else {
            std::cout << "\n发现 " << result.problems.size() << " 处问题：\n";
            for (const std::string &problem : result.problems) {
                std::cout << "  [问题] " << problem << "\n";
            }
        }
        project_ok = result.ok;
    }

    const capstone::CheckResult checks = capstone::run_self_tests();
    std::cout << "\n== 自测 ==\n";
    for (const std::string &line : checks.lines) {
        std::cout << "  " << line << "\n";
    }
    std::cout << "\n  自测结果：" << checks.summary() << "\n";
    return (project_ok && checks.all_passed()) ? 0 : 1;
}
