/**
 * main_cli.cpp —— 命令行版
 *
 * 版权所有 (C) 2026 HiBer2007，保留所有权利。
 *
 * 本程序是《C 与 C++》教材的一部分，采用与教材文档相同的授权：
 * CC BY-NC-ND 4.0 加附加条款。全文见仓库根目录的 LICENSE 与 许可附加条款.md。
 *
 * 允许在保留本声明的前提下查看、编译、运行本程序用于学习；
 * 不允许二次分发，不允许商用，不允许演绎（修改后再分发），不允许移除署名。
 *
 * 本程序不提供任何担保。
 */

/**
 * 用法：
 *   app_cli              出项目输出与自测
 *   app_cli --selftest   只跑自测
 *   app_cli --timing     附一行耗时（随机器负载变化，不进默认输出）
 *
 * 界面部分只有下面这些 std::cout；八段仿真与报告文本全在 ctrl 命名空间里。
 * 核心库内部一律按 UTF-8 处理，输出之前按控制台代码页转换一次：
 * 中文 Windows 的控制台是 936，直接写 UTF-8 字节会显示成乱码。
 */
#include "control_lab.hpp"

#include <chrono>
#include <iostream>
#include <string>
#include <string_view>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace {

#ifdef _WIN32

/** 把 UTF-8 文本换成控制台用的编码。
    控制台代码页是 65001 时原样返回；其余情况先转宽字符再转回去。
    输出被重定向、进程没有控制台时用系统 ANSI 代码页。 */
std::string to_console_encoding(std::string_view utf8)
{
    UINT code_page = GetConsoleOutputCP();
    if (code_page == 0) {
        code_page = GetACP();
    }
    if (code_page == CP_UTF8 || utf8.empty()) {
        return std::string(utf8);
    }

    const int wide_length = MultiByteToWideChar(CP_UTF8, 0, utf8.data(),
                                                static_cast<int>(utf8.size()), nullptr, 0);
    if (wide_length <= 0) {
        return std::string(utf8);
    }
    std::wstring wide(static_cast<std::size_t>(wide_length), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()),
                        wide.data(), wide_length);

    const int narrow_length = WideCharToMultiByte(code_page, 0, wide.data(), wide_length,
                                                  nullptr, 0, nullptr, nullptr);
    if (narrow_length <= 0) {
        return std::string(utf8);
    }
    std::string narrow(static_cast<std::size_t>(narrow_length), '\0');
    WideCharToMultiByte(code_page, 0, wide.data(), wide_length, narrow.data(),
                        narrow_length, nullptr, nullptr);
    return narrow;
}

#else

std::string to_console_encoding(std::string_view utf8)
{
    return std::string(utf8);
}

#endif

/** 所有输出都从这里走，编码转换只在这一处 */
void print_line(const std::string &text)
{
    std::cout << to_console_encoding(text) << "\n";
}

/** 毫秒文本，三位小数 */
std::string milliseconds_text(double seconds)
{
    return ctrl::format_fixed(seconds * 1000.0, 3);
}

}   /* namespace */

int main(int argc, char *argv[])
{
    bool only_check = false;
    bool show_timing = false;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--selftest") {
            only_check = true;
        } else if (arg == "--timing") {
            show_timing = true;
        } else if (arg == "-h" || arg == "--help") {
            print_line(u8"用法：app_cli [--selftest] [--timing]");
            return 0;
        } else {
            print_line(std::string(u8"不认识的参数：") + arg);
            return 2;
        }
    }

    print_line(u8"示例 08-algorithms/13-control · 控制：让一个量停在目标上（命令行版）");

    double report_seconds = 0.0;
    double check_seconds = 0.0;

    if (!only_check) {
        const auto start = std::chrono::steady_clock::now();
        const std::string report = ctrl::build_report();
        report_seconds =
            std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
        print_line(u8"");
        print_line(u8"== 项目输出 ==");
        std::cout << to_console_encoding(report);
    }

    const auto check_start = std::chrono::steady_clock::now();
    const ctrl::CheckResult result = ctrl::run_self_tests();
    check_seconds =
        std::chrono::duration<double>(std::chrono::steady_clock::now() - check_start).count();

    print_line(u8"");
    print_line(u8"== 自测 ==");
    for (const std::string &line : result.lines) {
        print_line("  " + line);
    }
    print_line(u8"");
    print_line(u8"  自测结果：" + result.summary());

    if (show_timing) {
        print_line(u8"");
        std::string text = u8"  本次耗时：";
        if (!only_check) {
            text += u8"项目输出 " + milliseconds_text(report_seconds) + u8" ms，";
        }
        text += u8"自测 " + milliseconds_text(check_seconds) +
                u8" ms（随机器负载变化，不进默认输出）";
        print_line(text);
    }

    return result.all_passed() ? 0 : 1;
}
