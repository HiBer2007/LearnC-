/**
 * main_cli.cpp —— 命令行版
 *
 * 用法：
 *   app_cli                  读 data/sample_text.txt，出项目输出与自测
 *   app_cli <文件路径>        读指定的文本文件
 *   app_cli --selftest       只跑自测
 *
 * 界面部分只有下面这些 std::cout；文本处理全在 text 命名空间里，
 * 与将来可能有的界面版共用同一份实现。
 *
 * 核心库内部一律按 UTF-8 处理，输出之前按控制台代码页转换一次：
 * 中文 Windows 的控制台是 936，直接写 UTF-8 字节会显示成乱码。
 */
#include "text_tools.hpp"

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

void print_usage()
{
    print_line(u8"用法：");
    print_line(u8"  app_cli               读 data/sample_text.txt");
    print_line(u8"  app_cli <文件路径>     读指定的文本文件");
    print_line(u8"  app_cli --selftest    只跑自测");
}

}   /* namespace */

int main(int argc, char *argv[])
{
    std::string path = "data/sample_text.txt";
    bool path_given = false;
    bool only_check = false;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--selftest") {
            only_check = true;
        } else if (arg == "-h" || arg == "--help") {
            print_usage();
            return 0;
        } else if (!arg.empty() && arg[0] == '-') {
            print_line(std::string(u8"不认识的参数：") + arg);
            print_usage();
            return 2;
        } else if (path_given) {
            print_line(u8"最多给一个文件路径");
            return 2;
        } else {
            path = arg;
            path_given = true;
        }
    }

    print_line(u8"示例 06-standard-library/03-cpp-string-text · "
               u8"std::string 与 string_view 文本处理（命令行版）");

    if (!only_check) {
        bool ok = false;
        std::string error;
        const std::string content = text::read_text_file(path, ok, error);

        print_line(u8"");
        print_line(u8"== 项目输出 ==");
        if (ok) {
            std::cout << to_console_encoding(text::build_report(content, path));
        } else {
            print_line(error);
            print_line(u8"  在示例目录下运行，或者用 app_cli <文件路径> 指定一个文本文件");
        }
    }

    const text::CheckResult result = text::run_self_tests();
    print_line(u8"");
    print_line(u8"== 自测 ==");
    for (const std::string &line : result.lines) {
        print_line("  " + line);
    }
    print_line(u8"");
    print_line(u8"  自测结果：" + result.summary());
    return result.all_passed() ? 0 : 1;
}
