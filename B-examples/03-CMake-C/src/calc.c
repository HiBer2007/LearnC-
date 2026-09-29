/**
 * calc.c —— 计算函数的实现
 *
 * 调试重点：这里的函数在另一个 .c 文件里，
 * 在 main.c 里按 F11（单步进入）会「跳」到这个文件来，
 * 这就是多文件工程调试和单文件最大的不同。
 */
#include "calc.h"

int add(int a, int b)
{
    int sum = a + b;   /* <- 在这行打断点，能看到 a、b 的值 */
    return sum;
}

int sub(int a, int b)
{
    int diff = a - b;
    return diff;
}

long long factorial(int n)
{
    if (n <= 1) {
        return 1;
    }
    /* 递归：在「调用堆栈」面板里能看到一层层压栈的过程 */
    return (long long)n * factorial(n - 1);
}

long long sum_array(const int *arr, int n)
{
    long long total = 0;
    for (int i = 0; i < n; ++i) {
        total += arr[i];   /* <- 打断点后单步，观察 i 和 total 的变化 */
    }
    return total;
}
