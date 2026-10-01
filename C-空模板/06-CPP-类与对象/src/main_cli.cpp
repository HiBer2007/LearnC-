/**
 * main_cli.cpp —— 空模板 06 的命令行验收程序（C++）
 *
 * 这个文件**不需要改**：它按 4 个阶段调用 MyString，把结果打印成
 * 《配置步骤.md》里的期望输出。你的实现写在 src/mystring.cpp 里。
 * 界面版（src/main_gui_win32.cpp 与 src/main_gui_qt.cpp）用的是同一个核心模块，两边可以对照着看。
 *
 * 构建与运行：
 *     cmake --preset mingw-gdb
 *     cmake --build --preset mingw-gdb
 *     build\mingw\bin\app_cli.exe
 *
 * 阶段的划分：
 *     阶段 1  构造、析构、查询与 append
 *     阶段 2  拷贝构造与拷贝赋值（深拷贝、自赋值）
 *     阶段 3  移动构造与移动赋值（所有权转移，不重新分配）
 *     阶段 4  放进 std::vector，观察容器扩容时用的是移动还是拷贝
 */
#include <cstddef>
#include <cstdio>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

#include "mystring.hpp"

/* ==================================================================
 * 阶段 1 · 构造与析构
 * ================================================================== */
static void stage1()
{
    std::printf("=== 阶段 1：构造与析构 ===\n");
    {
        const MyString empty;
        const MyString hello("hello");

        std::printf("  empty : size = %zu, empty() = %s, c_str = \"%s\"\n",
                    empty.size(), empty.empty() ? "true" : "false", empty.c_str());
        std::printf("  hello : size = %zu, c_str = \"%s\"\n", hello.size(), hello.c_str());

        try {
            std::printf("  hello[0] = '%c'\n", hello.at(0));
            std::printf("  hello[5] = '%c'\n", hello.at(5));   /* 越界，应当抛出 */
            std::printf("  （没有抛异常，说明 at 还没有做越界检查）\n");
        } catch (const std::out_of_range &e) {
            std::printf("  越界被接住：%s\n", e.what());
        }

        std::printf("  live = %d, alloc = %ld\n",
                    MyString::live_count(), MyString::allocation_count());
    }
    std::printf("  作用域结束：live = %d\n", MyString::live_count());
}

/* ==================================================================
 * 阶段 2 · 拷贝构造与拷贝赋值
 * ================================================================== */
static void stage2()
{
    std::printf("\n=== 阶段 2：拷贝构造与拷贝赋值 ===\n");

    MyString a("hello");
    MyString b = a;              /* 拷贝构造：b 与 a 各有各的内存 */
    b.append(" world");          /* 改 b 不该影响 a */

    std::printf("  a = \"%s\" (%zu)\n", a.c_str(), a.size());
    std::printf("  b = \"%s\" (%zu)\n", b.c_str(), b.size());

    MyString c("other");
    c = a;                       /* 拷贝赋值 */
    std::printf("  c = \"%s\" (%zu)\n", c.c_str(), c.size());

    MyString &ref = c;
    c = ref;                     /* 自赋值：两边是同一个对象 */
    std::printf("  自赋值后 c = \"%s\" (%zu)\n", c.c_str(), c.size());

    std::printf("  live = %d, alloc = %ld\n",
                MyString::live_count(), MyString::allocation_count());
}

/* ==================================================================
 * 阶段 3 · 移动构造与移动赋值
 * ================================================================== */
static void stage3()
{
    std::printf("\n=== 阶段 3：移动构造与移动赋值 ===\n");

    MyString big("0123456789");
    MyString target("x");

    const long alloc_before = MyString::allocation_count();
    const long move_before = MyString::move_count();

    MyString taken = std::move(big);      /* 移动构造：接管 big 的内存 */
    std::printf("  big   : size = %zu, c_str = \"%s\"\n", big.size(), big.c_str());
    std::printf("  taken : size = %zu, c_str = \"%s\"\n", taken.size(), taken.c_str());

    target = std::move(taken);            /* 移动赋值 */
    std::printf("  taken : size = %zu\n", taken.size());
    std::printf("  target: size = %zu, c_str = \"%s\"\n", target.size(), target.c_str());

    std::printf("  新增分配 = %ld, 新增移动 = %ld\n",
                MyString::allocation_count() - alloc_before,
                MyString::move_count() - move_before);
}

/* ==================================================================
 * 阶段 4 · 放进 std::vector
 * ================================================================== */
static void stage4()
{
    std::printf("\n=== 阶段 4：放进 vector，看容器怎么搬运 ===\n");

    /* 完成阶段 3 之后，把下面这行取消注释，让编译器替你检查 noexcept：
     *
     * static_assert(std::is_nothrow_move_constructible<MyString>::value,
     *               "移动构造必须标 noexcept，否则容器扩容时会改用拷贝");
     */

    std::vector<MyString> words;
    words.reserve(1);            /* 故意留小，强制扩容两次 */

    const long alloc_before = MyString::allocation_count();
    const long move_before = MyString::move_count();

    words.push_back(MyString("alpha"));
    words.push_back(MyString("beta"));
    words.push_back(MyString("gamma"));

    std::printf("  words =");
    for (std::size_t i = 0; i < words.size(); ++i) {
        std::printf(" \"%s\"", words[i].c_str());
    }
    std::printf("\n  移动次数 = %ld\n", MyString::move_count() - move_before);
    std::printf("  新增分配 = %ld\n", MyString::allocation_count() - alloc_before);
    std::printf("  live = %d\n", MyString::live_count());
}

int main()
{
    stage1();
    stage2();
    stage3();
    stage4();

    std::printf("\n=== 收尾 ===\n");
    std::printf("  live = %d（构造与析构应当配平，最终为 0）\n", MyString::live_count());
    return 0;
}
