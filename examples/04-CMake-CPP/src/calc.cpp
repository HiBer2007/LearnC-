/**
 * calc.cpp —— 计算函数实现（C++ 版）
 *
 * 调试重点：这些函数在另一个文件里，
 * 在 main.cpp 里按 F11（单步进入）会跳到这个文件。
 */
#include "calc.hpp"

int add(int a, int b)
{
    int sum = a + b;   /* <- 在这行打断点 */
    return sum;
}

long long factorial(int n)
{
    if (n <= 1) {
        return 1;
    }
    return static_cast<long long>(n) * factorial(n - 1);
}

long long sum_array(const std::vector<int> &v)
{
    long long total = 0;
    for (std::size_t i = 0; i < v.size(); ++i) {
        total += v[i];   /* <- 打断点后单步，看 i 和 total */
    }
    return total;
}

std::string join(const std::vector<std::string> &parts, const std::string &sep)
{
    std::string out;
    for (std::size_t i = 0; i < parts.size(); ++i) {
        if (i > 0) {
            out += sep;
        }
        out += parts[i];
    }
    return out;
}
