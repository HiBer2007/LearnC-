/**
 * main.c —— CMake 多文件工程的主程序
 *
 * ── 怎么用 ─────────────────────────────────────────────
 *   1. 用 VS Code 打开【本文件夹】（文件 → 打开文件夹）
 *   2. 在行号左边点一下打断点
 *   3. 按 F5，选：
 *        「GDB · CMake 工程 (MinGW gcc)」
 *        「VS2022 · CMake 工程 (MSVC cl)」
 *      会自动完成「CMake 配置 → 编译 → 启动调试」
 *
 * ── 这个示例演示什么 ────────────────────────────────────
 *   · 跨文件单步（F11 进 add()，会跳到 src/calc.c）
 *   · 递归的调用堆栈（factorial）
 *   · 数组 / 指针变量的观察
 *
 * ── 建议的断点位置 ──────────────────────────────────────
 *   第 34 行  add(a, b)         按 F11 会跳进 src/calc.c
 *   第 39 行  factorial(n)      按 F11 反复进，看堆栈越叠越高
 *   第 44 行  sum_array(...)    进去后看 i 和 total 的变化
 */
#include <stdio.h>

#include "calc.h"

#define N 6

int main(void)
{
    int a = 40;
    int b = 2;

    printf("=== 1. 基本运算 ===\n");
    printf("add(%d, %d) = %d\n", a, b, add(a, b));
    printf("sub(%d, %d) = %d\n", a, b, sub(a, b));

    printf("\n=== 2. 递归：观察调用堆栈 ===\n");
    for (int n = 0; n <= 5; ++n) {
        printf("factorial(%d) = %lld\n", n, factorial(n));
    }

    printf("\n=== 3. 数组与指针 ===\n");
    int arr[N] = {3, 1, 4, 1, 5, 9};
    printf("sum_array(arr, %d) = %lld\n", N, sum_array(arr, N));

    const int *p = arr;          /* p 指向数组首元素 */
    for (int i = 0; i < N; ++i) {
        printf("  p[%d] = %d\n", i, p[i]);
    }

    printf("\n完成。\n");
    return 0;
}
