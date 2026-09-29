/**
 * util.c —— 工具函数实现
 *
 * 配好调试器后，在 main.c 里按 F11 单步进入，
 * 执行点会「跳」到这个文件——这就是多文件调试的核心体验。
 */
#include "util.h"

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
    return (long long)n * factorial(n - 1);
}
