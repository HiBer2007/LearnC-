/**
 * main_cli.cpp —— 命令行版
 *
 * 用法：
 *   app_cli                扫描本示例自带的 data/ 目录
 *   app_cli <路径>         扫描指定目录
 *   app_cli --selftest     只跑自测，不扫描任何目录
 *
 * 界面部分只有下面这些 std::cout；遍历、统计、排版全在 dirscan 命名空间里，
 * 与两份 GUI 共用同一份实现（src/dir_scan.cpp）。
 */
#include "dir_scan.hpp"

#include <filesystem>
#include <iostream>
#include <string>

int main(int argc, char *argv[])
{
    std::cout << "示例 07-standard-library/07-cpp-filesystem-scan · "
                 "目录扫描与属性统计（命令行版）\n";

    std::filesystem::path root = "data";
    bool only_selftest = false;
    for (int i = 1; i < argc; ++i) {
        const std::string argument = argv[i];
        if (argument == "--selftest") {
            only_selftest = true;
            continue;
        }
        root = argument;
    }

    if (!only_selftest) {
        std::cout << "\n== 项目输出 ==\n" << dirscan::build_demo_output(root);
    }

    const dirscan::CheckResult result = dirscan::run_self_tests();
    std::cout << "\n== 自测 ==\n";
    for (const std::string &line : result.lines) {
        std::cout << "  " << line << "\n";
    }
    std::cout << "\n  自测结果：" << result.summary() << "\n";
    return result.all_passed() ? 0 : 1;
}
