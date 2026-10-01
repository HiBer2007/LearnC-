/**
 * main_cli.cpp —— 命令行版
 *
 * 用法：
 *   app_cli              走一遍演示，再跑自测
 *   app_cli --selftest   只跑自测
 *
 * 界面部分只有下面这些 std::cout；资源与回调的逻辑全在 registry 命名空间里。
 *
 * 核心库流出来的一直是 UTF-8 字节，输出之前按控制台代码页转换一次：
 * 中文 Windows 的控制台是 936，控制台是 65001 时原样写出。
 */
#include "resource_registry.hpp"

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

void print_usage()
{
    print_line(u8"用法：");
    print_line(u8"  app_cli              演示 + 自测");
    print_line(u8"  app_cli --selftest   只跑自测");
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
            print_usage();
            return 0;
        } else {
            print_line(std::string(u8"不认识的参数：") + arg);
            print_usage();
            return 2;
        }
    }

    print_line(u8"示例 06-standard-library/04-cpp-smart-pointers · "
               u8"智能指针与回调注册表（命令行版）");

    if (!only_check) {
        print_line(u8"");
        print_line(u8"== 项目输出 ==");
        std::cout << to_console_encoding(registry::build_report());
    }

    const registry::CheckResult result = registry::run_self_tests();
    print_line(u8"");
    print_line(u8"== 自测 ==");
    for (const std::string &line : result.lines) {
        print_line("  " + line);
    }
    print_line(u8"");
    print_line(u8"  自测结果：" + result.summary());
    return result.all_passed() ? 0 : 1;
}
