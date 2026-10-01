/* main_cli.cpp —— 05-interrupt-and-atomic 的命令行版
 *
 * 手工编译：
 *   g++ -std=c++17 -O2 -Wall -Wextra -Iinclude src/race_lab.cpp src/main_cli.cpp -o app_cli.exe
 */
#include "race_lab.hpp"

#include <cstdlib>
#include <cstring>
#include <iostream>

namespace {

const char *const k_usage =
    "用法：app_cli [选项]\n"
    "  （无选项）     三种写法各跑一次并对照，最后跑自测\n"
    "  --selftest     只跑自测\n"
    "  --ops N        每边做多少次自增（默认 2000000）\n"
    "  --help         显示这段文字\n";

} /* namespace */

int main(int argc, char **argv) {
    std::uint64_t ops = 2000000u;
    bool only_selftest = false;

    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--selftest") == 0) {
            only_selftest = true;
        } else if (std::strcmp(argv[i], "--ops") == 0 && i + 1 < argc) {
            ops = std::strtoull(argv[++i], nullptr, 10);
            if (ops == 0u) {
                ops = 1u;
            }
        } else if (std::strcmp(argv[i], "--help") == 0) {
            std::cout << k_usage;
            return 0;
        } else {
            std::cerr << "无法识别的参数：" << argv[i] << "\n\n" << k_usage;
            return 2;
        }
    }

    if (only_selftest) {
        return rl::self_test(std::cout) == 0 ? 0 : 1;
    }

    std::cout << "示例 06-lower-level/05-interrupt-and-atomic · 中断与原子\n\n";
    std::cout << "== 主循环与「中断」同时给一个计数器加一 ==\n";
    rl::print_explanation(std::cout);
    std::cout << "\n";
    rl::print_result(std::cout, rl::run(rl::mode::plain, ops, ops));
    rl::print_result(std::cout, rl::run(rl::mode::spinlock, ops, ops));
    rl::print_result(std::cout, rl::run(rl::mode::atomic_, ops, ops));

    std::cout << "\n";
    return rl::self_test(std::cout) == 0 ? 0 : 1;
}
