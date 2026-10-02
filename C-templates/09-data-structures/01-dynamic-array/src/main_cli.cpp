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
 * 这个文件**不需要改**：它按 5 个阶段调用 dyn::DynArray，把结果打印成
 * 《配置步骤.md》里的验收输出。你的实现写在 include/dynarray.hpp 里。
 *
 * 构建与运行（在模板目录下）：
 *     cmake --preset mingw-gdb
 *     cmake --build --preset mingw-gdb
 *     build\mingw\bin\app_cli.exe
 *
 * 输出全部写成 ASCII：换一台代码页不是 65001 的机器也照样能读。
 */
#include <cstddef>
#include <exception>
#include <iostream>

#include "dynarray.hpp"
#include "tracked.hpp"

namespace {

/* 打印一个 int 数组的内容；超过 8 个就只打印前 8 个 */
void print_int_values(const dyn::DynArray<int> &a)
{
    const std::size_t show = a.size() < 8 ? a.size() : 8;
    std::cout << "values          : [";
    for (std::size_t i = 0; i < show; ++i) {
        std::cout << (i == 0 ? "" : " ") << a[i];
    }
    if (a.size() > show) {
        std::cout << " ... " << a.size() << " items";
    }
    std::cout << "]\n";
}

/* 逐个比对 0 到 n-1，判断内容与追加顺序是否一致 */
bool holds_sequence(const dyn::DynArray<int> &a, int n)
{
    if (a.size() != static_cast<std::size_t>(n)) {
        return false;
    }
    for (int i = 0; i < n; ++i) {
        if (a[static_cast<std::size_t>(i)] != i) {
            return false;
        }
    }
    return true;
}

} /* namespace */

int main()
{
    /* ---------------------------------------------------------- 阶段 1 */
    std::cout << "=== Stage 1: append while capacity lasts ===\n";
    {
        dyn::DynArray<int> a(4);        /* 容量 4，里面是 4 个 0 */
        a[0] = 1;
        a[1] = 2;
        a.pop_back();                   /* 元素个数降到 3 */
        a.pop_back();                   /* 元素个数降到 2，容量仍是 4 */
        a.reset_stats();
        a.push_back(3);
        a.push_back(4);
        std::cout << "size = " << a.size() << "   capacity = " << a.capacity()
                  << "   grow_calls = " << a.grow_calls() << "\n";
        print_int_values(a);
    }

    /* ---------------------------------------------------------- 阶段 2 */
    std::cout << "\n=== Stage 2: finish the grow ===\n";
    {
        dyn::DynArray<int> b;           /* 容量 0：第一次 push_back 就要扩容 */
        dyn::AllocStats::reset();
        for (int i = 0; i < 8; ++i) {
            b.push_back(i * 10);
        }
        std::cout << "size            : " << b.size() << "\n";
        std::cout << "capacity        : " << b.capacity() << "\n";
        std::cout << "grow_calls      : " << b.grow_calls() << "\n";
        std::cout << "relocations     : " << b.relocations() << "\n";
        std::cout << "allocs          : " << dyn::AllocStats::allocs << "\n";
        std::cout << "deallocs        : " << dyn::AllocStats::deallocs << "\n";
        print_int_values(b);
    }

    /* ---------------------------------------------------------- 阶段 3 */
    std::cout << "\n=== Stage 3: relocate (move or copy) ===\n";
    {
        TrackedCounters::reset();
        dyn::DynArray<Tracked> c;
        for (int i = 0; i < 8; ++i) {
            c.push_back(Tracked(i * 10));
        }
        std::cout << "Tracked     : size = " << c.size()
                  << "   moves = " << TrackedCounters::moves
                  << "   copies = " << TrackedCounters::copies << "\n";
        std::cout << "Tracked     : values = [";
        for (std::size_t i = 0; i < c.size(); ++i) {
            std::cout << (i == 0 ? "" : " ") << c[i].value();
        }
        std::cout << "]\n";

        TrackedCounters::reset();
        dyn::DynArray<SlowTracked> d;
        for (int i = 0; i < 8; ++i) {
            d.push_back(SlowTracked(i * 10));
        }
        std::cout << "SlowTracked : size = " << d.size()
                  << "   moves = " << TrackedCounters::moves
                  << "   copies = " << TrackedCounters::copies << "\n";
        std::cout << "SlowTracked : values = [";
        for (std::size_t i = 0; i < d.size(); ++i) {
            std::cout << (i == 0 ? "" : " ") << d[i].value();
        }
        std::cout << "]\n";
    }

    /* ---------------------------------------------------------- 阶段 4 */
    std::cout << "\n=== Stage 4: growth policy ===\n";
    {
        dyn::AllocStats::reset();
        dyn::DynArray<int> e;
        for (int i = 0; i < 1000; ++i) {
            e.push_back(i);
        }
        std::cout << "size              : " << e.size() << "\n";
        std::cout << "capacity          : " << e.capacity() << "\n";
        std::cout << "grow_calls        : " << e.grow_calls() << "\n";
        std::cout << "relocated_elements: " << e.relocated_elements() << "\n";
        std::cout << "allocs            : " << dyn::AllocStats::allocs << "\n";
        std::cout << "deallocs          : " << dyn::AllocStats::deallocs << "\n";
        std::cout << "sequence 0..999   : " << (holds_sequence(e, 1000) ? "ok" : "wrong") << "\n";
    }

    /* ---------------------------------------------------------- 阶段 5 */
    std::cout << "\n=== Stage 5: exception safety on grow ===\n";
    {
        TrackedCounters::reset();
        dyn::AllocStats::reset();

        dyn::DynArray<SlowTracked> f(3);        /* 容量 3，正好装满 */
        f[0] = SlowTracked(1);
        f[1] = SlowTracked(2);
        f[2] = SlowTracked(3);

        const long live_before = TrackedCounters::live;
        const long buffers_before = dyn::AllocStats::allocs - dyn::AllocStats::deallocs;

        tracked_set_throw_after(2);             /* 数到第三次拷贝/移动构造时抛 */
        bool caught = false;
        try {
            f.push_back(SlowTracked(4));        /* 触发扩容，要搬走 3 个元素 */
        } catch (const std::exception &) {
            caught = true;
        }
        tracked_set_throw_after(-1);

        const bool unchanged = (f.size() == 3) && (f[0].value() == 1) &&
                               (f[1].value() == 2) && (f[2].value() == 3);
        const bool returned = (dyn::AllocStats::allocs - dyn::AllocStats::deallocs) == buffers_before;

        std::cout << "caught            : " << (caught ? "yes" : "no") << "\n";
        std::cout << "content unchanged : " << (unchanged ? "yes" : "no") << "\n";
        std::cout << "live before/after : " << live_before << " / " << TrackedCounters::live << "\n";
        std::cout << "buffer returned   : " << (returned ? "yes" : "no") << "\n";
    }

    return 0;
}
