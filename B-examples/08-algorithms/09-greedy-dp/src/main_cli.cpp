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
 *
 * 界面部分只有下面这些 std::cout；算法实验与报告文本全在 gdp 命名空间里。
 * 核心库内部一律按 UTF-8 处理，输出之前按控制台代码页转换一次：
 * 中文 Windows 的控制台是 936，直接写 UTF-8 字节会显示成乱码。
 */
#include "greedy_dp.hpp"

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

}   /* namespace */

int main(int argc, char *argv[])
{
    bool only_check = false;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--selftest") {
            only_check = true;
        } else if (arg == "-h" || arg == "--help") {
            print_line(u8"用法：app_cli [--selftest]");
            return 0;
        } else {
            print_line(std::string(u8"不认识的参数：") + arg);
            return 2;
        }
    }

    print_line(u8"示例 08-algorithms/09-greedy-dp · 贪心与动态规划：同一道题的两条路（命令行版）");

    if (!only_check) {
        print_line(u8"");
        print_line(u8"== 项目输出 ==");
        std::cout << to_console_encoding(gdp::build_report());
    }

    const gdp::CheckResult result = gdp::run_self_tests();
    print_line(u8"");
    print_line(u8"== 自测 ==");
    for (const std::string &line : result.lines) {
        print_line("  " + line);
    }
    print_line(u8"");
    print_line(u8"  自测结果：" + result.summary());
    return result.all_passed() ? 0 : 1;
}
