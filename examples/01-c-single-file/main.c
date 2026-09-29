/**
 * 示例 01 · C 单文件调试
 * 主题：指针、数组、初始化
 *
 * ── 怎么用 ─────────────────────────────────────────────
 *   1. 用 VS Code 打开【本文件夹】（文件 → 打开文件夹）
 *      ⚠️ 不是打开上层的大文件夹，否则用的是上层配置
 *   2. 在行号左边点一下，打个红点（断点）
 *   3. 按 F5，选：
 *        「GDB · 调试当前 .c 文件」      → gcc 编译 + GDB 调试
 *        「VS2022 · 调试当前 .c 文件」   → cl 编译 + VS2022 调试器
 *   4. 左侧「运行和调试」面板看：变量 / 监视 / 调用堆栈
 *
 * ── 建议的断点位置 ──────────────────────────────────────
 *   第 34 行  int arr[N] = {10, 20, 30};   看部分初始化的补 0 行为
 *   第 40 行  int *p = arr;                看指针与数组的关系
 *   第 54 行  int *q = arr + 2;            看指针算术
 */
#include <stdio.h>

#define N 5

/* 数组参数在 C 里会退化成指针，sizeof 拿不到元素个数，所以必须额外传 n */
static void print_array(const int *arr, int n)
{
    for (int i = 0; i < n; ++i) {
        printf("  arr[%d] = %-3d  地址 %p\n", i, arr[i], (const void *)&arr[i]);
    }
}

int main(void)
{
    /* ── 1. 初始化：只写前 3 个，剩下的自动补 0 ── */
    int arr[N] = {10, 20, 30};

    puts("== 1. 数组初始化 ==");
    print_array(arr, N);

    /* ── 2. 指针遍历：*(p + i) 与 p[i] 完全等价 ── */
    int *p = arr;
    puts("\n== 2. 指针遍历 ==");
    for (int i = 0; i < N; ++i) {
        printf("  *(p + %d) = %d\n", i, *(p + i));
    }

    /* ── 3. 通过指针改数组 ── */
    puts("\n== 3. 用指针修改数组 ==");
    *p = 99;            /* 等价于 arr[0] = 99 */
    p[1] = 88;          /* 等价于 arr[1] = 88 */
    print_array(arr, N);

    /* ── 4. 指针算术 ── */
    puts("\n== 4. 指针算术 ==");
    int *q = arr + 2;   /* 指向 arr[2] */
    printf("  *q       = %d\n", *q);
    printf("  q[-1]    = %d\n", q[-1]);
    printf("  q - arr  = %lld   (相差几个元素)\n", (long long)(q - arr));
    printf("  sizeof(arr) = %zu 字节, 元素个数 = %zu\n",
           sizeof(arr), sizeof(arr) / sizeof(arr[0]));

    return 0;
}
