/**
 * main.cpp —— CMake 多文件工程的主程序（C++）
 *
 * ── 怎么用 ─────────────────────────────────────────────
 *   1. 用 VS Code 打开【本文件夹】（文件 → 打开文件夹）
 *   2. 打断点 → 按 F5 → 选：
 *        「GDB · CMake 工程 (MinGW g++)」
 *        「VS2022 · CMake 工程 (MSVC cl)」
 *
 * ── 演示什么 ───────────────────────────────────────────
 *   · 跨文件单步（F11 跳进 src/calc.cpp）
 *   · 递归调用堆栈
 *   · STL 容器的调试显示（vector / string 会被整齐打印）
 *
 * ── 建议的断点位置 ──────────────────────────────────────
 *   第 32 行  add(a, b)             F11 跳进 calc.cpp
 *   第 36 行  factorial(n)          反复 F11 看堆栈叠高
 *   第 45 行  join(words, " ")      看 vector<string> 的内容
 */
#include <iostream>
#include <string>
#include <vector>

#include "calc.hpp"

int main()
{
    const int a = 40;
    const int b = 2;

    std::cout << "=== 1. 基本运算 ===\n";
    std::cout << "add(" << a << ", " << b << ") = " << add(a, b) << "\n";

    std::cout << "\n=== 2. 递归：观察调用堆栈 ===\n";
    for (int n = 0; n <= 8; ++n) {
        std::cout << "factorial(" << n << ") = " << factorial(n) << "\n";
    }

    std::cout << "\n=== 3. STL 容器 ===\n";
    const std::vector<int> nums = {3, 1, 4, 1, 5, 9};
    std::cout << "sum_array(nums) = " << sum_array(nums) << "\n";

    std::cout << "\n=== 4. 字符串拼接 ===\n";
    const std::vector<std::string> words = {"Hello", "C++", "and", "VS Code"};
    std::cout << "join = " << join(words, " ") << "\n";

    std::cout << "\n完成。\n";
    return 0;
}
