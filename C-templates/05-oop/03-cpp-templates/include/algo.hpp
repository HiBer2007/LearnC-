/**
 * algo.hpp —— 泛型算法（空模板 08）
 *
 * 这个头文件里全是模板，因此**没有对应的 .cpp**：模板的声明与定义必须放在
 * 头文件里，编译器在看到调用点时才知道用什么类型实例化它。
 * 第 4 阶段会让你亲手验证一次「把定义搬进 .cpp 会怎样」。
 *
 * 构建与运行：
 *     cmake --preset mingw-gdb
 *     cmake --build --preset mingw-gdb
 *     build\mingw\bin\app_cli.exe
 */
#ifndef ALGO_HPP
#define ALGO_HPP

#include <cstddef>
#include <cstring>

/* ==================================================================
 * 阶段 1 · 函数模板
 * ================================================================== */

/* TODO（阶段 1-1）
 * 要求：实现主模板，返回 a 与 b 中较大的一个。
 * 提示：一行就够：return a > b ? a : b;
 * 验收：阶段 1 里 max_of(3, 5) 得到 5，max_of(2.5, 1.5) 得到 2.5。
 */
template <class T>
T max_of(T a, T b)
{
    (void)b;
    return a;
}

/* TODO（阶段 1-2）
 * 要求：实现带**非类型模板参数**的求和函数：N 由数组长度自动推出来。
 * 提示：循环累加 arr[0] 到 arr[N-1]，初值写成 T{}（对 int 就是 0）。
 * 验收：阶段 1 里 sum_of({1,2,3,4,5}) 得到 15。
 */
template <class T, std::size_t N>
T sum_of(const T (&arr)[N])
{
    (void)arr;
    return T{};
}

/* 已给出：数组长度也是编译期常量，注意它的形参没有名字 */
template <class T, std::size_t N>
constexpr std::size_t count_of(const T (&)[N])
{
    return N;
}

/* ==================================================================
 * 阶段 2 · 全特化
 * ================================================================== */

/* TODO（阶段 2-1）
 * 要求：为 max_of 写一份针对 const char * 的**全特化**，用 std::strcmp 按字典序比较。
 * 写法（本章节第 6.1 小节）：
 *     template <>
 *     const char *max_of<const char *>(const char *a, const char *b)
 *     {
 *         ...
 *     }
 * 两处容易错：
 *   1. 尖括号里必须写 <const char *>，写成 <> 或别的类型都会报「与主模板不匹配」；
 *   2. 返回类型必须与主模板**逐字一致**（主模板返回 T，这里 T 就是 const char *），
 *      写成 const char *const & 一类的形式会编译失败。
 * 验收：阶段 2 里 max_of("ab", "cd") 得到 cd；
 *       没写特化时走主模板，比较的是两个指针的地址，会得到 ab。
 * 说明：本章节第 6.1 小节还讲了一种情况——如果同时存在同名的**非模板**函数，
 *       重载决议会优先选非模板那一份，只有显式写出 max_of<const char *>(...) 才落到特化。
 *       这一条留作选做实验，见《配置步骤.md》。
 */

#endif /* ALGO_HPP */
