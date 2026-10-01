/**
 * util.cpp —— 工具函数实现（C++）
 *
 * 配好调试器后，在 main.cpp 里按 F11 会跳到这个文件。
 */
#include "util.hpp"

int square(int x)
{
    int result = x * x;   /* <- 建议在这行打断点 */
    return result;
}

long long factorial(int n)
{
    if (n <= 1) {
        return 1;
    }
    return static_cast<long long>(n) * factorial(n - 1);
}

long long sum(const std::vector<int> &v)
{
    long long total = 0;
    for (int x : v) {
        total += x;   /* <- 单步时看 total 的变化 */
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
