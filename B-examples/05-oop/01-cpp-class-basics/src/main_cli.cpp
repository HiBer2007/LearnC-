/**
 * main_cli.cpp —— 命令行版
 *
 * 用法：
 *   app_cli            用默认长度 10
 *   app_cli 20         用前 20 项斐波那契数
 *   app_cli --check    只跑自测
 *
 * 界面部分只有下面这些 std::cout；算的部分全在 demo 命名空间里，
 * 与 GUI 版共用同一份实现（src/vector_demo.cpp）。
 */
#include "vector_demo.hpp"

#include <cstdlib>
#include <iostream>
#include <string>

namespace {

void print_line(const std::string &label, const IntVector &values)
{
    std::cout << "  " << label << " : " << demo::to_text(values) << "\n";
}

}   /* namespace */

int main(int argc, char *argv[])
{
    std::cout << "示例 07 · 值类型 IntVector（命令行版）\n";

    std::size_t count = 10;
    bool only_check = false;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--check") {
            only_check = true;
            continue;
        }
        const long parsed = std::strtol(arg.c_str(), nullptr, 10);
        if (parsed < 1 || parsed > 30) {
            std::cout << "长度要在 1 到 30 之间，收到：" << arg << "\n";
            return 2;
        }
        count = static_cast<std::size_t>(parsed);
    }

    if (!only_check) {
        const IntVector fib = demo::fibonacci(count);

        std::cout << "\n== 项目输出 ==\n";
        print_line("斐波那契数列", fib);
        print_line("前缀和      ", demo::prefix_sum(fib));
        print_line("每项乘 3    ", fib * 3);
        print_line("每项加 1    ", demo::add_scalar(fib, 1));
        std::cout << "  与 std::vector 对照："
                  << (fib == demo::to_int_vector(demo::fibonacci_with_std(count))
                          ? "结果一致" : "结果不一致")
                  << "（共 " << count << " 项）\n";

        /* const 对象只能调用 const 成员函数 */
        const IntVector &view = fib;
        std::cout << "  长度 " << view.size() << "，首项 " << view[0]
                  << "，末项 " << view.at(view.size() - 1)
                  << "，容量 " << view.capacity() << "\n";

        /* 下标越界：at 会检查，[] 不检查 */
        try {
            (void)fib.at(fib.size());
        } catch (const std::out_of_range &e) {
            std::cout << "  故意越界一次：" << e.what() << "\n";
        }
    }

    const demo::CheckResult result = demo::run_self_tests();
    std::cout << "\n== 自测 ==\n";
    for (const std::string &line : result.lines) {
        std::cout << "  " << line << "\n";
    }
    std::cout << "\n  自测结果：" << result.summary() << "\n";
    return result.all_passed() ? 0 : 1;
}
