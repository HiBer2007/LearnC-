/* main_cli.cpp —— 练习模板 01 的命令行验收程序（C++）
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
 * 这个文件**不需要改**：它按 4 个阶段调用 rec 里的函数，把结果打印成
 * 《配置步骤.md》里的验收输出。你的实现写在 src/recursion.cpp 里。
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

#include "recursion.hpp"

namespace {

void line(const char *label, long long value)
{
    std::cout << std::left << std::setw(32) << label << ": " << value << "\n";
}

void line(const char *label, const std::string &text)
{
    std::cout << std::left << std::setw(32) << label << ": " << text << "\n";
}

/* 把一次调用的返回值与三个结构量一起带出来 */
struct Result {
    long long value = 0;
    long long calls = 0;
    long long iterations = 0;
    long long depth_max = 0;
};

Result run_sum_first(const int *a, int n)
{
    rec::Counters c;
    Result r;
    r.value = rec::sum_first(a, n, c);
    r.calls = c.calls;
    r.iterations = c.iterations;
    r.depth_max = c.depth_max;
    return r;
}

Result run_sum_tail(const int *a, int n)
{
    rec::Counters c;
    Result r;
    r.value = rec::sum_tail(a, n, 0, c);
    r.calls = c.calls;
    r.iterations = c.iterations;
    r.depth_max = c.depth_max;
    return r;
}

Result run_sum_loop(const int *a, int n)
{
    rec::Counters c;
    Result r;
    r.value = rec::sum_loop(a, n, c);
    r.calls = c.calls;
    r.iterations = c.iterations;
    r.depth_max = c.depth_max;
    return r;
}

long long fib_calls(int n)
{
    rec::Counters c;
    (void)rec::fib_naive(n, c);
    return c.calls;
}

long long fib_depth(int n)
{
    rec::Counters c;
    (void)rec::fib_naive(n, c);
    return c.depth_max;
}

std::string yes_no(bool v)
{
    return v ? "yes" : "no";
}

void print_vector(const int *a, int n)
{
    for (int i = 0; i < n; ++i) {
        std::cout << (i == 0 ? "" : " ") << a[i];
    }
    std::cout << "\n";
}

void stage4_pair(int n)
{
    const std::vector<rec::Move> r = rec::hanoi_recursive(n, 'A', 'B', 'C');
    const std::vector<rec::Move> s = rec::hanoi_stack(n, 'A', 'B', 'C');

    line((std::string("hanoi_recursive(") + std::to_string(n) + ") moves").c_str(),
         static_cast<long long>(r.size()));
    line((std::string("hanoi_stack(") + std::to_string(n) + ") moves").c_str(),
         static_cast<long long>(s.size()));
    line((std::string("hanoi(") + std::to_string(n) + ") same").c_str(),
         yes_no(rec::same_moves(r, s)));
}

} /* namespace */

int main()
{
    const int a[8] = {1, 2, 3, 4, 5, 6, 7, 8};

    std::vector<int> big(2048);
    for (std::size_t i = 0; i < big.size(); ++i) {
        big[i] = static_cast<int>(i) + 1;
    }

    /* ---------------------------------------------------------- 阶段 1 */
    std::cout << "=== Stage 1: base case, step, combine ===\n";
    {
        std::cout << std::left << std::setw(32) << "array" << ": ";
        print_vector(a, 8);

        const Result r0 = run_sum_first(a, 0);
        line("sum_first(n=0)", r0.value);
        line("sum_first(n=0) calls", r0.calls);

        const Result r1 = run_sum_first(a, 1);
        line("sum_first(n=1)", r1.value);

        const Result r8 = run_sum_first(a, 8);
        line("sum_first(n=8)", r8.value);
        line("sum_first(n=8) calls", r8.calls);
        line("sum_first(n=8) depth max", r8.depth_max);

        const Result rb = run_sum_first(big.data(), static_cast<int>(big.size()));
        line("sum_first(n=2048)", rb.value);
        line("sum_first(n=2048) calls", rb.calls);
        line("sum_first(n=2048) depth max", rb.depth_max);
    }

    /* ---------------------------------------------------------- 阶段 2 */
    std::cout << "\n=== Stage 2: how big is the recursion tree ===\n";
    {
        rec::Counters c;
        const long long v = rec::fib_naive(5, c);
        line("fib_naive(5)", v);
        line("fib_naive(5) calls", c.calls);

        line("fib_naive(10) calls", fib_calls(10));
        line("fib_naive(20) calls", fib_calls(20));
        line("fib_naive(20) depth max", fib_depth(20));

        line("fib_tree_nodes(5)", rec::fib_tree_nodes(5));
        line("fib_tree_nodes(10)", rec::fib_tree_nodes(10));
        line("fib_tree_nodes(20)", rec::fib_tree_nodes(20));

        std::cout << "fib(4) tree:\n";
        const long long drawn = rec::fib_tree_draw(4, std::cout);
        line("fib(4) tree drawn, lines", drawn);
        line("fib_naive(4) calls", fib_calls(4));
    }

    /* ---------------------------------------------------------- 阶段 3 */
    std::cout << "\n=== Stage 3: tail recursion, and what it really is ===\n";
    {
        const Result t8 = run_sum_tail(a, 8);
        line("sum_tail(n=8)", t8.value);
        line("sum_tail(n=8) calls", t8.calls);
        line("sum_tail(n=8) depth max", t8.depth_max);

        const Result tb = run_sum_tail(big.data(), static_cast<int>(big.size()));
        line("sum_tail(n=2048)", tb.value);
        line("sum_tail(n=2048) calls", tb.calls);
        line("sum_tail(n=2048) depth max", tb.depth_max);

        const Result l8 = run_sum_loop(a, 8);
        line("sum_loop(n=8)", l8.value);
        line("sum_loop(n=8) iterations", l8.iterations);
        line("sum_loop(n=8) depth max", l8.depth_max);

        const Result lb = run_sum_loop(big.data(), static_cast<int>(big.size()));
        line("sum_loop(n=2048)", lb.value);
        line("sum_loop(n=2048) iterations", lb.iterations);
        line("sum_loop(n=2048) depth max", lb.depth_max);
    }

    /* ---------------------------------------------------------- 阶段 4 */
    std::cout << "\n=== Stage 4: recursion with an explicit stack ===\n";
    {
        stage4_pair(3);
        stage4_pair(10);
        stage4_pair(16);

        std::cout << "hanoi(3), recursive moves:\n";
        rec::print_moves(rec::hanoi_recursive(3, 'A', 'B', 'C'), std::cout, 16);

        std::cout << "hanoi(3), stack moves:\n";
        rec::print_moves(rec::hanoi_stack(3, 'A', 'B', 'C'), std::cout, 16);
    }

    return 0;
}
