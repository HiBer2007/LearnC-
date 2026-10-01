/**
 * main_cli.cpp —— 空模板 08 的命令行验收程序（C++）
 *
 * 这个文件**不需要改**：它按 4 个阶段使用 algo.hpp 与 stack.hpp 里的模板，
 * 把结果打印成《配置步骤.md》里的期望输出。
 *
 * 构建与运行：
 *     cmake --preset mingw-gdb
 *     cmake --build --preset mingw-gdb
 *     build\mingw\bin\app_cli.exe
 *
 * 阶段的划分：
 *     阶段 1  函数模板与非类型模板参数
 *     阶段 2  函数模板的全特化
 *     阶段 3  类模板 Stack 与成员模板
 *     阶段 4  显式实例化与「定义必须放在头文件里」
 */
#include <cstddef>
#include <cstdio>
#include <stdexcept>
#include <string>

#include "algo.hpp"
#include "stack.hpp"

/* 容量是编译期常量，这一行在骨架状态下就能编过 */
static_assert(Stack<int, 8>::capacity() == 8, "capacity() 应当是编译期常量");

/* ==================================================================
 * 阶段 1 · 函数模板与非类型模板参数
 * ================================================================== */
static void stage1()
{
    std::printf("=== 阶段 1：函数模板 ===\n");

    std::printf("  max_of(3, 5)         = %d\n", max_of(3, 5));
    std::printf("  max_of(2.5, 1.5)     = %g\n", max_of(2.5, 1.5));
    std::printf("  max_of<double>(3, 5) = %g\n", max_of<double>(3, 5));

    const int values[] = {1, 2, 3, 4, 5};
    std::printf("  sum_of(values)       = %d\n", sum_of(values));
    std::printf("  count_of(values)     = %zu\n", count_of(values));
}

/* ==================================================================
 * 阶段 2 · 全特化
 * ================================================================== */
static void stage2()
{
    std::printf("\n=== 阶段 2：全特化 ===\n");

    const char *a = "ab";
    const char *b = "cd";

    /* 两行调用的是同一份代码：T 都被推导（或指定）为 const char * */
    std::printf("  max_of(a, b)                     = %s\n", max_of(a, b));
    std::printf("  max_of<const char *>(a, b)       = %s\n", max_of<const char *>(a, b));

    std::printf("  没写全特化时，主模板比较的是两个指针的地址，结果不保证是哪一个\n");
}

/* ==================================================================
 * 阶段 3 · 类模板与成员模板
 * ================================================================== */
static void stage3()
{
    std::printf("\n=== 阶段 3：类模板 Stack ===\n");

    Stack<int> st;   /* N 用默认值 8 */
    st.push(1);
    st.push(2);
    st.push(3);
    std::printf("  压入 1 2 3 后 size = %zu，capacity = %zu\n", st.size(), Stack<int>::capacity());
    std::printf("  top = %d\n", st.top());

    std::printf("  弹出顺序：");
    int value = 0;
    while (st.pop(value)) {
        std::printf(" %d", value);
    }
    std::printf("\n");
    std::printf("  空栈再 pop：%s\n", st.pop(value) ? "true" : "false");

    Stack<int, 3> small;
    small.push(1);
    small.push(2);
    small.push(3);
    std::printf("  容量 3 的栈压第 4 个：%s\n", small.push(4) ? "true" : "false");

    Stack<int, 8> from_array;
    const int source[] = {7, 8, 9};
    const std::size_t taken = from_array.push_all(source, 3);
    std::printf("  push_all 压入 %zu 个，size = %zu\n", taken, from_array.size());

    Stack<std::string, 4> words;
    words.push(std::string("alpha"));
    words.push(std::string("beta"));
    std::printf("  字符串栈：size = %zu, top = %s\n", words.size(), words.top().c_str());

    try {
        const Stack<int, 2> nothing;
        (void)nothing.top();
        std::printf("  空栈取 top：没有抛异常（阶段 3-3 还没做完）\n");
    } catch (const std::out_of_range &) {
        std::printf("  空栈取 top：捕获到 std::out_of_range\n");
    }
}

/* ==================================================================
 * 阶段 4 · 显式实例化
 * ================================================================== */
static void stage4()
{
    std::printf("\n=== 阶段 4：显式实例化 ===\n");

    std::printf("  Stack<int, 8>::capacity() 在编译期就是常量：%zu\n", Stack<int, 8>::capacity());

    Stack<double, 2> doubles;
    doubles.push(1.5);
    doubles.push(2.5);
    std::printf("  Stack<double, 2>：size = %zu, top = %g\n", doubles.size(), doubles.top());

    std::printf("  src/instantiations.cpp 里为 Stack<int, 8> 与 Stack<std::string, 4> 做了显式实例化\n");
}

int main()
{
    stage1();
    stage2();
    stage3();
    stage4();
    return 0;
}
