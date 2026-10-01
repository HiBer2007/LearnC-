/**
 * main_cli.cpp —— 练习模板 03 的命令行验收程序（C++）
 *
 * 这个文件**不需要改**：它按 4 个阶段调用 texttool.cpp 里的函数，
 * 把结果打印成《配置步骤.md》里的验收输出。你的实现写在 src/texttool.cpp 里。
 *
 * 构建与运行（在模板目录下）：
 *     cmake --preset mingw-gdb
 *     cmake --build --preset mingw-gdb
 *     build\mingw\bin\app_cli.exe
 *
 * 阶段的划分：
 *     阶段 1  std::string 的构造与容量（短串优化）
 *     阶段 2  查找、替换、切分、修剪
 *     阶段 3  std::string_view 的零拷贝切分
 *     阶段 4  数字与字符串互转、UTF-8 字节与码点计数
 */
#include <cstddef>
#include <iomanip>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

#include "texttool.hpp"

/* 两个汉字（各 3 字节）加三个 ASCII 字母，共 9 字节、5 个码点。
 * 十六进制转义是「贪婪」的：'\x87' 后面直接跟 'a' 会被当成同一个转义，
 * 因此这里写成两段字面量再拼接。 */
static std::string utf8_sample()
{
    return std::string("\xE4\xB8\xAD\xE6\x96\x87") + "abc";
}

/* 已给出：按 probe 的结果打印一行 */
static void print_capacity(const char *label, const std::string &s)
{
    const tt::Capacity c = tt::probe(s);
    std::cout << std::left << std::setw(14) << label
              << ": size = " << std::right << std::setw(3) << c.size
              << "   capacity = " << std::setw(3) << c.capacity
              << "   inside_object = " << (c.inside_object ? "yes" : "no") << "\n";
}

int main()
{
    /* ---------------------------------------------------------- 阶段 1 */
    std::cout << "=== Stage 1: string capacity and SSO ===\n";
    const std::string short_s = "hello";
    const std::string long_s(40, 'x');
    std::string       reserved = long_s;
    reserved.reserve(64);

    print_capacity("short", short_s);
    print_capacity("long", long_s);
    print_capacity("reserved(64)", reserved);

    /* ---------------------------------------------------------- 阶段 2 */
    std::cout << "\n=== Stage 2: find, replace, split, trim ===\n";
    const std::string line = "alpha,beta,gamma,delta,epsilon";
    const std::vector<std::string> tokens = tt::split(line, ',');

    std::cout << "count \"a\"      : " << tt::count_occurrences(line, "a") << "\n";
    std::cout << "split count    : " << tokens.size() << "\n";
    std::cout << "tokens         :";
    for (const std::string &t : tokens) {
        std::cout << " [" << t << "]";
    }
    std::cout << "\n";
    std::cout << "trim           : [" << tt::trim("  padded  ") << "]\n";
    std::cout << "lower          : [" << tt::to_lower("Mixed Case") << "]\n";
    std::cout << "replace_all    : [" << tt::replace_all(line, "a", "A") << "]\n";

    /* ---------------------------------------------------------- 阶段 3 */
    std::cout << "\n=== Stage 3: string_view, zero copy ===\n";
    std::string text = line;   /* 原串要活到本阶段结束，视图才安全 */
    const std::vector<std::string_view> views = tt::split_view(text, ',');

    bool same = (views.size() == tokens.size());
    for (std::size_t i = 0; same && i < views.size(); ++i) {
        same = (views[i] == tokens[i]);
    }

    std::cout << "split_view count : " << views.size() << "\n";
    std::cout << "same as split    : " << (same ? "yes" : "no") << "\n";
    std::cout << "chars copied by split      : " << tt::copied_chars(tokens) << "\n";
    std::cout << "chars copied by split_view : " << tt::copied_chars(views) << "\n";
    std::cout << "trim_view        : [" << tt::trim_view("  padded  ") << "]\n";

    /* ---------------------------------------------------------- 阶段 4 */
    std::cout << "\n=== Stage 4: numbers and UTF-8 ===\n";
    const tt::Numbers n = tt::probe_numbers();
    const tt::Utf8    u = tt::count_utf8(utf8_sample());

    std::cout << "stoi(\"42\")      : " << n.i << "\n";
    std::cout << "stod(\"3.5\")     : " << n.d << "\n";
    std::cout << "to_string(42)   : [" << n.back << "]\n";
    std::cout << "stoi(\"7x\")      : " << n.prefix << "   (prefix parse, no exception)\n";
    std::cout << "stoi(\"hello\")   : "
              << (n.bad_caught ? "caught invalid_argument" : "(no exception)") << "\n";
    std::cout << "utf8 bytes      : " << u.bytes << "\n";
    std::cout << "utf8 codepoints : " << u.codepoints << "\n";

    return 0;
}
