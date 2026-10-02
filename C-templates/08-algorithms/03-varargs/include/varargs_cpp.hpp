/* varargs_cpp.hpp —— 练习模板 03 的 C++ 侧接口（C++）
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
 * C++ 侧的两条路：
 *
 *     阶段 3   sum_all    可变参数模板：取第一个，其余的递归展开
 *     阶段 4   sum_fold   C++17 的折叠表达式，一行写完
 *
 * 两条路都写在头文件里——模板的实现在使用点必须可见。
 */
#ifndef VARARGS_CPP_HPP
#define VARARGS_CPP_HPP

#include <cstddef>
#include <initializer_list>

namespace va {

/* 已给出：展开层数的计数，每展开一层参数包加一 */
struct CppCounters {
    long long expansions = 0;
};

/* 已给出：把计数器清零，返回清零前的值 */
long long reset_expansions(CppCounters &c);

/* 已给出的对照路线：同样求一包数的和，走 <initializer_list>。
 * 大括号里的东西先变成数组再传进来，没有包的展开，也没有类型推导。 */
long long sum_init_list(std::initializer_list<long long> xs);
std::size_t init_list_count(std::initializer_list<long long> xs);

/* 已给出：只有一个参数时，展开到底了 */
template <class T>
long long sum_all(CppCounters &c, T first)
{
    ++c.expansions;
    return static_cast<long long>(first);
}

/* 阶段 3 的 TODO：两个及以上参数时，把第一个与「剩下那些参数的展开结果」加起来。
 * 每展开一层让 c.expansions 加一。 */
template <class T, class... Rest>
long long sum_all(CppCounters &c, T first, Rest... rest)
{
    /* TODO（阶段 3-1，参数包展开）：
     * 这一层的活是两件：把 first 记进来，再把 rest 这一包原样交给下一层。
     * 交给下一层时，参数个数少了一个——递归到只剩一个参数时落进上面那个重载。
     * 每展开一层让 c.expansions 加一。
     * 判据：sum_all(1,2,3,4,5) 是 15、expansions 是 5（见《配置步骤.md》阶段 3）。 */
    ++c.expansions;

    /* 占位实现：下面两行删掉之后，写上你的展开 */
    (void)first;
    (void)sizeof...(rest);
    return 0;
}

/* 阶段 4 的 TODO：用折叠表达式把整包参数一次加起来。
 * 与 sum_all 的差别不只是短：空参数包时它也有确定的值。 */
template <class... Args>
long long sum_fold(Args... args)
{
    /* TODO（阶段 4-1，折叠表达式）：
     * C++17 的折叠表达式把「对包里每个元素做同一个运算」写成一行，
     * 不必再写基线的重载。加法折叠在空包时的值是 0——这一条由判据验。
     * 还要留意**转换发生在哪一步**：包里的元素各自先变成结果类型再相加，
     * 与 sum_all「先各自转换、再加起来」是同一种算法。
     * 判据：sum_fold(1,2,3,4,5) 是 15、sum_fold() 是 0、
     *       sum_fold(2.5, 3.5) 与 sum_all(c, 2.5, 3.5) 逐位相同（都是 5）
     *       （见《配置步骤.md》阶段 4）。 */
    (void)sizeof...(args);
    return 0;       /* 占位实现 */
}

/* 已给出：参数个数，用 sizeof... 直接问编译器 */
template <class... Args>
constexpr std::size_t count_args(Args...) noexcept
{
    return sizeof...(Args);
}

} /* namespace va */

#endif /* VARARGS_CPP_HPP */
