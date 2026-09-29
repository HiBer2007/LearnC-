/**
 * main.cpp —— 空模板 04 的主程序（C++）
 *
 * 先用命令行确认这个工程本身能编译：
 *     cmake -S . -B build -G Ninja -DCMAKE_CXX_COMPILER=g++
 *     cmake --build build
 *     build\bin\app.exe
 *
 * 能跑通之后再照《配置步骤.md》配 VS Code。
 */
#include <iostream>
#include <string>
#include <vector>

#include "util.hpp"

int main()
{
    for (int i = 1; i <= 5; ++i) {
        std::cout << "square(" << i << ") = " << square(i) << "\n";   /* <- F11 跳进 util.cpp */
    }

    std::cout << "factorial(8) = " << factorial(8) << "\n";          /* <- 反复 F11 看堆栈 */

    const std::vector<int> nums = {3, 1, 4, 1, 5};
    std::cout << "sum = " << sum(nums) << "\n";                      /* <- 展开 nums 看容器内容 */

    const std::vector<std::string> words = {"Hello", "C++", "VS Code"};
    std::cout << "join = " << join(words, " ") << "\n";

    return 0;
}
