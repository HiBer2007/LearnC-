/* main_cli.cpp —— 01-layout-and-align 的命令行版
 *
 * 它只做三件事：认参数、调 core、把结果按顺序打出来。
 * 一行布局逻辑都没有。
 *
 * 手工编译：
 *   g++ -std=c++17 -O2 -Wall -Wextra -Iinclude src/layout_lab.cpp src/main_cli.cpp -o app_cli.exe
 */
#include "layout_lab.hpp"

#include <cstdlib>
#include <cstring>
#include <iostream>

namespace {

const char *const kUsage =
    "用法：app_cli [选项]\n"
    "  （无选项）        打印地址空间、结构体布局、伪共享对照，最后跑自测\n"
    "  --selftest        只跑自测\n"
    "  --iterations N    伪共享测量的迭代次数（默认 20000000）\n"
    "  --no-timing       跳过伪共享计时，只打印布局\n"
    "  --help            显示这段文字\n";

} /* namespace */

int main(int argc, char **argv) {
    bool only_selftest = false;
    bool no_timing = false;
    std::uint64_t iterations = 20000000u;

    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--selftest") == 0) {
            only_selftest = true;
        } else if (std::strcmp(argv[i], "--no-timing") == 0) {
            no_timing = true;
        } else if (std::strcmp(argv[i], "--iterations") == 0 && i + 1 < argc) {
            iterations = std::strtoull(argv[++i], nullptr, 10);
            if (iterations == 0u) {
                iterations = 1u;
            }
        } else if (std::strcmp(argv[i], "--help") == 0) {
            std::cout << kUsage;
            return 0;
        } else {
            std::cerr << "无法识别的参数：" << argv[i] << "\n\n" << kUsage;
            return 2;
        }
    }

    if (only_selftest) {
        return ll::run_self_test(std::cout) == 0 ? 0 : 1;
    }

    std::cout << "示例 07-lower-level/01-layout-and-align · 布局与对齐\n\n";

    ll::print_address_map(std::cout, ll::take_address_map());
    std::cout << "\n";
    ll::print_layout_table(std::cout);
    std::cout << "\n";
    ll::print_offset_table(std::cout);

    if (!no_timing) {
        std::cout << "\n";
        ll::print_sharing(std::cout, ll::measure_false_sharing(iterations));
    }

    std::cout << "\n";
    return ll::run_self_test(std::cout) == 0 ? 0 : 1;
}
