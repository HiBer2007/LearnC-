/* main_cli.cpp —— 练习模板 02 的命令行验收程序（C++）
 *
 * 版权所有 (C) 2026 HiBer2007，保留所有权利。
 *
 * 本程序是《C 与 C++》教材的一部分，采用与教材文档相同的授权：
 * CC BY-NC-ND 4.0 加附加条款。全文见仓库根目录的 LICENSE 与 许可附加条款.md。
 *
 * 允许在保留本声明的前提下查看、编译、运行本程序用于学习；
 * 不允许二次分发，不允许商用，不允许演绎（修改后再分发），不允许移除署名。
 *
 * 本程序不提供任何担保。
 *
 * ------------------------------------------------------------------
 * 这个文件**不需要改**：它按 4 个阶段调用 memo 里的函数，把结果打印成
 * 《配置步骤.md》里的验收输出。你的实现写在 src/memo.cpp 里。
 *
 * 构建与运行（在模板目录下）：
 *     cmake --preset mingw-gdb
 *     cmake --build --preset mingw-gdb
 *     build\mingw\bin\app_cli.exe
 *
 * 输出全部写成 ASCII：换一台代码页不是 65001 的机器也照样能读。
 */
#include <cstddef>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

#include "memo.hpp"

namespace {

void line(const char *label, long long value)
{
    std::cout << std::left << std::setw(32) << label << ": " << value << "\n";
}

void print_array(const char *label, const int *a, int n)
{
    std::cout << std::left << std::setw(32) << label << ": ";
    for (int i = 0; i < n; ++i) {
        std::cout << (i == 0 ? "" : " ") << a[i];
    }
    std::cout << "\n";
}

/* 固定种子的伪随机数组：重跑逐位相同 */
void fill_pseudo_random(int *a, int n, unsigned int seed)
{
    unsigned int x = seed;
    for (int i = 0; i < n; ++i) {
        x = x * 1103515245U + 12345U;
        a[i] = static_cast<int>((x >> 16) % 41U) - 20;   /* -20 .. 20 */
    }
}

long long fib_naive_calls(int n)
{
    memo::Counters c;
    (void)memo::fib_naive(n, c);
    return c.calls;
}

/* 缓存里有几格已经不再是「还没算过」的标记（-1 就是那个标记） */
long long filled_cells(const std::vector<long long> &cache)
{
    long long n = 0;
    for (std::size_t i = 0; i < cache.size(); ++i) {
        if (cache[i] != -1) {
            ++n;
        }
    }
    return n;
}

struct SubarrayResult {
    long long value = 0;
    long long calls = 0;
    long long iterations = 0;
    long long depth_max = 0;
};

SubarrayResult run_dc(const int *a, int n)
{
    memo::Counters c;
    SubarrayResult r;
    r.value = memo::max_subarray_dc(a, 0, n, c);
    r.calls = c.calls;
    r.iterations = c.iterations;
    r.depth_max = c.depth_max;
    return r;
}

void subarray_case(const char *name, const int *a, int n)
{
    line((std::string(name) + ", linear").c_str(), memo::max_subarray_linear(a, n));
    const SubarrayResult r = run_dc(a, n);
    line((std::string(name) + ", divide and conquer").c_str(), r.value);
    line((std::string(name) + ", dc calls").c_str(), r.calls);
    line((std::string(name) + ", dc scan iterations").c_str(), r.iterations);
}

void queens_case(int n)
{
    memo::QueensBoard b;
    memo::n_queens(n, b);
    line((std::string("n_queens(") + std::to_string(n) + ")").c_str(), b.solutions);
    line((std::string("n_queens(") + std::to_string(n) + ") nodes").c_str(), b.nodes);
    line((std::string("n_queens(") + std::to_string(n) + ") pruned").c_str(), b.pruned);
}

} /* namespace */

int main()
{
    /* ---------------------------------------------------------- 阶段 1 */
    std::cout << "=== Stage 1: memoization, cache read and write ===\n";
    {
        memo::Counters c;
        const long long v = memo::fib_naive(25, c);
        line("fib_naive(25)", v);
        line("fib_naive(25) calls", c.calls);
        line("fib_naive(25) depth max", c.depth_max);
        line("fib_naive(30) calls", fib_naive_calls(30));
    }
    {
        std::vector<long long> cache = memo::make_cache(25);
        memo::Counters c;
        const long long v = memo::fib_memo(25, cache, c);
        line("fib_memo(25)", v);
        line("fib_memo(25) calls", c.calls);
        line("fib_memo(25) hits", c.hits);
        line("fib_memo(25) depth max", c.depth_max);
    }
    {
        std::vector<long long> cache = memo::make_cache(30);
        memo::Counters c;
        const long long v = memo::fib_memo(30, cache, c);
        line("fib_memo(30)", v);
        line("fib_memo(30) calls", c.calls);
        line("fib_memo(30) hits", c.hits);
    }

    /* ---------------------------------------------------------- 阶段 2 */
    std::cout << "\n=== Stage 2: divide and conquer, the merge step ===\n";
    {
        const int a[] = {2, -3, 4, -1, -2, 1, 5, -3};
        print_array("data A", a, 8);
        subarray_case("A", a, 8);

        const int b[] = {-5, -2, -9, -1, -7};
        print_array("data B (all negative)", b, 5);
        subarray_case("B", b, 5);

        int c[32];
        fill_pseudo_random(c, 32, 20260808U);
        print_array("data C (fixed random)", c, 32);
        subarray_case("C", c, 32);
    }

    /* ---------------------------------------------------------- 阶段 3 */
    std::cout << "\n=== Stage 3: backtracking, choose and undo ===\n";
    {
        queens_case(6);
        queens_case(7);
    }

    /* ---------------------------------------------------------- 阶段 4 */
    std::cout << "\n=== Stage 4: the same recursion tree, three uses ===\n";
    {
        line("fib(25), naive tree nodes", fib_naive_calls(25));

        std::vector<long long> cache = memo::make_cache(25);
        memo::Counters c;
        (void)memo::fib_memo(25, cache, c);
        line("fib(25), memo calls", c.calls);
        line("fib(25), memo hits", c.hits);
        line("fib(25), cache filled cells", filled_cells(cache));

        line("fib(25), distinct subproblems", memo::count_distinct_calls(25));

        std::vector<long long> cache30 = memo::make_cache(30);
        memo::Counters c30;
        (void)memo::fib_memo(30, cache30, c30);
        line("fib(30), naive tree nodes", fib_naive_calls(30));
        line("fib(30), memo calls", c30.calls);
        line("fib(30), cache filled cells", filled_cells(cache30));
        line("fib(30), distinct subproblems", memo::count_distinct_calls(30));
    }

    return 0;
}
