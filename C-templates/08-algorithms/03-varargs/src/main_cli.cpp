/* main_cli.cpp —— 练习模板 03 的命令行验收程序（C++）
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
 *
 * ------------------------------------------------------------------
 * 这个文件**不需要改**：它按 4 个阶段调用 C 侧与 C++ 侧的函数，
 * 把结果打印成《配置步骤.md》里的验收输出。
 * 你的实现写在 src/varargs.c 与 include/varargs_cpp.hpp 里。
 *
 * 构建与运行（在模板目录下）：
 *     cmake --preset mingw-gdb
 *     cmake --build --preset mingw-gdb
 *     build\mingw\bin\app_cli.exe
 *
 * 输出全部写成 ASCII：换一台代码页不是 65001 的机器也照样能读。
 */
#include <iomanip>
#include <iostream>
#include <string>

#include "varargs.h"
#include "varargs_cpp.hpp"

namespace {

void line(const char *label, long long value)
{
    std::cout << std::left << std::setw(32) << label << ": " << value << "\n";
}

void line(const char *label, const std::string &text)
{
    std::cout << std::left << std::setw(32) << label << ": [" << text << "]\n";
}

} /* namespace */

int main()
{
    /* ---------------------------------------------------------- 阶段 1 */
    std::cout << "=== Stage 1: the C va_list, type and step ===\n";
    {
        va_reset_counters();
        const long long sum = va_sum_ints(5, 1, 2, 3, 4, 5);
        line("sum of 5 ints", sum);
        line("va_arg calls", va_arg_calls());
    }
    {
        char buf[64];
        va_show_promotions(buf, sizeof buf, 4, 'A', static_cast<short>(7), 'z', -3);
        line("promotions: A, short 7, z, -3", std::string(buf));

        /* 同一批实参走一遍求和：char 与 short 在进列表前已经提升成 int */
        va_reset_counters();
        const long long mixed = va_sum_ints(4, 'A', static_cast<short>(7), 'z', -3);
        line("sum of mixed widths", mixed);
        line("va_arg calls", va_arg_calls());
    }

    /* ---------------------------------------------------------- 阶段 2 */
    std::cout << "\n=== Stage 2: a printf subset of our own ===\n";
    {
        char buf[64];
        va_reset_counters();
        const int written = va_format(buf, sizeof buf, "n=%d s=%s c=%c pct=%%", 42, "text", 'X');
        line("fmt n=%d s=%s c=%c pct=%%", std::string(buf));
        line("written", written);
        line("va_arg calls", va_arg_calls());
    }
    {
        char small[8];
        va_reset_counters();
        const int written = va_format(small, sizeof small, "n=%d s=%s c=%c pct=%%", 42, "text", 'X');
        line("same fmt, buffer of 8", std::string(small));
        line("written (buffer of 8)", written);
        line("va_arg calls", va_arg_calls());
    }
    {
        char buf[64];
        va_reset_counters();
        const int written = va_format(buf, sizeof buf, "a=%q b=%d", 7);
        line("unknown specifier %q", std::string(buf));
        line("written (unknown specifier)", written);
        line("va_arg calls", va_arg_calls());
    }
    {
        char buf[64];
        va_reset_counters();
        const int written = va_format(buf, sizeof buf, "%d", -1234);
        line("negative int", std::string(buf));
        line("written (negative int)", written);
        line("va_arg calls", va_arg_calls());
    }

    /* ---------------------------------------------------------- 阶段 3 */
    std::cout << "\n=== Stage 3: variadic templates, expanding the pack ===\n";
    {
        va::CppCounters c;
        const long long s = va::sum_all(c, 1, 2, 3, 4, 5);
        line("sum_all(1,2,3,4,5)", s);
        line("sum_all expansions", c.expansions);
        line("count_args(1,2,3,4,5)", static_cast<long long>(va::count_args(1, 2, 3, 4, 5)));
    }
    {
        va::CppCounters c;
        const long long s = va::sum_all(c, 2.5, 3.5);
        line("sum_all(2.5, 3.5)", s);
        line("sum_all expansions", c.expansions);
    }
    {
        line("sum_init_list{1,2,3,4,5}",
             va::sum_init_list({1, 2, 3, 4, 5}));
        line("init_list_count{1,2,3,4,5}",
             static_cast<long long>(va::init_list_count({1, 2, 3, 4, 5})));
    }

    /* ---------------------------------------------------------- 阶段 4 */
    std::cout << "\n=== Stage 4: fold expressions ===\n";
    {
        line("sum_fold(1,2,3,4,5)", va::sum_fold(1, 2, 3, 4, 5));
        line("sum_fold() empty pack", va::sum_fold());
        line("sum_fold(2.5, 3.5)", va::sum_fold(2.5, 3.5));

        va::CppCounters c;
        const long long s = va::sum_all(c, 1, 2, 3, 4, 5);
        const bool same = (va::sum_fold(1, 2, 3, 4, 5) == s);
        line("same as sum_all (ints)", std::string(same ? "yes" : "no"));

        va::CppCounters c2;
        const long long sd = va::sum_all(c2, 2.5, 3.5);
        const bool same_d = (va::sum_fold(2.5, 3.5) == sd);
        line("same as sum_all (doubles)", std::string(same_d ? "yes" : "no"));
    }

    return 0;
}
