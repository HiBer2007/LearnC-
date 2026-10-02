/* main_cli.cpp —— 练习模板 04 的命令行验收程序（C++）
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
 * 这个文件**不需要改**：它按 5 个阶段调用 st 里的函数，把结果打印成
 * 《配置步骤.md》里的验收输出。你的实现写在 src/searchtool.cpp 里。
 *
 * 构建与运行（在模板目录下）：
 *     cmake --preset mingw-gdb
 *     cmake --build --preset mingw-gdb
 *     build\mingw\bin\app_cli.exe
 *
 * 输出全部写成 ASCII：换一台代码页不是 65001 的机器也照样能读。
 */
#include <iomanip>
#include <iostream>
#include <string>

#include "searchtool.hpp"

namespace {

/* 16 个互不相同的键，间距均匀 */
const int kSorted[16] = {1, 3, 5, 7, 9, 11, 13, 15, 17, 19, 21, 23, 25, 27, 29, 31};

/* 同样 16 个位置，但有一片重复键 */
const int kDup[16] = {2, 5, 5, 5, 8, 11, 11, 17, 20, 20, 20, 20, 33, 41, 41, 50};

/* 间距按 2 的幂增长，插值查找的取点在这种分布上会吃大亏 */
const int kExp[16] = {1, 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024, 2048, 4096, 8192, 16384, 32768};

typedef int (*SearchFn)(const int *, int, int, st::Stats &);

void line(const char *label, long long value)
{
    std::cout << std::left << std::setw(32) << label << ": " << value << "\n";
}

void line(const char *label, const std::string &text)
{
    std::cout << std::left << std::setw(32) << label << ": " << text << "\n";
}

std::string yes_no(bool v)
{
    return v ? "yes" : "no";
}

void print_array(const char *label, const int *a, int n)
{
    std::cout << std::left << std::setw(32) << label << ":";
    for (int i = 0; i < n; ++i) {
        std::cout << " " << a[i];
    }
    std::cout << "\n";
}

/* 逐个查找每一个已经存在的键，把每次的探测次数按顺序打印出来 */
void print_probes_per_key(const char *label, SearchFn f, const int *a, int n)
{
    std::cout << std::left << std::setw(32) << label << ":";
    for (int i = 0; i < n; ++i) {
        st::Stats s;
        (void)f(a, n, a[i], s);
        std::cout << " " << s.probes;
    }
    std::cout << "\n";
}

long long total_probes(SearchFn f, const int *a, int n)
{
    long long total = 0;
    for (int i = 0; i < n; ++i) {
        st::Stats s;
        (void)f(a, n, a[i], s);
        total += s.probes;
    }
    return total;
}

bool all_indexes_correct(SearchFn f, const int *a, int n)
{
    for (int i = 0; i < n; ++i) {
        st::Stats s;
        if (f(a, n, a[i], s) != i) {
            return false;
        }
    }
    return true;
}

void missing_key(const char *label, SearchFn f, const int *a, int n, int key)
{
    st::Stats s;
    const int idx = f(a, n, key, s);
    line((std::string(label) + ", index").c_str(), idx);
    line((std::string(label) + ", probes").c_str(), s.probes);
}

} /* namespace */

int main()
{
    const int n = 16;

    /* ---------------------------------------------------------- 阶段 1 */
    std::cout << "=== Stage 1: binary search, closed interval [lo, hi] ===\n";
    {
        print_array("data, 16 distinct keys", kSorted, n);
        line("sorted", yes_no(st::is_sorted(kSorted, n)));
        print_probes_per_key("probes per key", st::bsearch_closed, kSorted, n);
        line("total probes, 16 hits", total_probes(st::bsearch_closed, kSorted, n));
        line("all 16 indexes correct", yes_no(all_indexes_correct(st::bsearch_closed, kSorted, n)));
        missing_key("missing key 16", st::bsearch_closed, kSorted, n, 16);
    }

    /* ---------------------------------------------------------- 阶段 2 */
    std::cout << "\n=== Stage 2: binary search, half-open interval [lo, hi) ===\n";
    {
        print_probes_per_key("probes per key", st::bsearch_half, kSorted, n);
        line("total probes, 16 hits", total_probes(st::bsearch_half, kSorted, n));
        line("all 16 indexes correct", yes_no(all_indexes_correct(st::bsearch_half, kSorted, n)));
        missing_key("missing key 16", st::bsearch_half, kSorted, n, 16);

        const long long a1 = total_probes(st::bsearch_closed, kSorted, n);
        const long long a2 = total_probes(st::bsearch_half, kSorted, n);
        line("same total as stage 1", yes_no(a1 == a2));
    }

    /* ---------------------------------------------------------- 阶段 3 */
    std::cout << "\n=== Stage 3: lower_bound and upper_bound ===\n";
    {
        print_array("data with duplicates", kDup, n);
    }
    {
        st::Stats s1;
        st::Stats s2;
        const int l5 = st::lower_bound_of(kDup, n, 5, s1);
        const int u5 = st::upper_bound_of(kDup, n, 5, s2);
        line("lower_bound(5)", l5);
        line("upper_bound(5)", u5);
        line("count of 5", u5 - l5);

        st::Stats s3;
        st::Stats s4;
        const int l20 = st::lower_bound_of(kDup, n, 20, s3);
        const int u20 = st::upper_bound_of(kDup, n, 20, s4);
        line("lower_bound(20)", l20);
        line("upper_bound(20)", u20);
        line("count of 20", u20 - l20);
        line("probes, lower_bound(20)", s3.probes);
        line("probes, upper_bound(20)", s4.probes);
    }
    {
        st::Stats s;
        line("lower_bound(99)", st::lower_bound_of(kDup, n, 99, s));
        st::Stats s2;
        line("upper_bound(99)", st::upper_bound_of(kDup, n, 99, s2));
        st::Stats s3;
        line("lower_bound(1)", st::lower_bound_of(kDup, n, 1, s3));
    }
    {
        bool ok_lower = true;
        bool ok_upper = true;
        for (int key = 0; key <= 52; ++key) {
            st::Stats s1;
            st::Stats s2;
            if (st::lower_bound_of(kDup, n, key, s1) != st::std_lower_bound(kDup, n, key)) {
                ok_lower = false;
            }
            if (st::upper_bound_of(kDup, n, key, s2) != st::std_upper_bound(kDup, n, key)) {
                ok_upper = false;
            }
        }
        line("match std::lower_bound (0..52)", yes_no(ok_lower));
        line("match std::upper_bound (0..52)", yes_no(ok_upper));
    }

    /* ---------------------------------------------------------- 阶段 4 */
    std::cout << "\n=== Stage 4: interpolation search, where to probe ===\n";
    {
        std::cout << "uniform data, 16 keys\n";
        line("  binary probes", total_probes(st::bsearch_half, kSorted, n));
        line("  interpolation probes", total_probes(st::interp_search, kSorted, n));
        line("  all 16 indexes correct", yes_no(all_indexes_correct(st::interp_search, kSorted, n)));
    }
    {
        std::cout << "exponential data, 16 keys\n";
        line("  binary probes", total_probes(st::bsearch_half, kExp, n));
        line("  interpolation probes", total_probes(st::interp_search, kExp, n));
        line("  all 16 indexes correct", yes_no(all_indexes_correct(st::interp_search, kExp, n)));
    }
    {
        print_probes_per_key("exponential, probes per key", st::interp_search, kExp, n);
    }

    /* ---------------------------------------------------------- 阶段 5 */
    std::cout << "\n=== Stage 5: linear search as a baseline ===\n";
    {
        line("linear probes, 16 keys", total_probes(st::linear_search, kSorted, n));
        line("binary probes, 16 keys", total_probes(st::bsearch_half, kSorted, n));
        line("interpolation probes, 16 keys", total_probes(st::interp_search, kSorted, n));

        st::Stats s1;
        st::Stats s2;
        st::Stats s3;
        (void)st::linear_search(kSorted, n, 31, s1);
        (void)st::bsearch_half(kSorted, n, 31, s2);
        (void)st::interp_search(kSorted, n, 31, s3);
        line("linear probes, key 31", s1.probes);
        line("binary probes, key 31", s2.probes);
        line("interpolation probes, key 31", s3.probes);
    }

    return 0;
}
