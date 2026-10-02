/* varargs_cpp.cpp —— 练习模板 03 的 C++ 侧实现（C++17）
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
 * 可变参数模板是模板，实现必须写在使用点可见的地方，因此阶段 3 与阶段 4 的
 * 代码都在 include/varargs_cpp.hpp 里，本文件只放两件不是模板的东西：
 * 展开层数的清零，以及 <initializer_list> 那条对照路线。
 */
#include "varargs_cpp.hpp"

#include <initializer_list>

namespace va {

long long reset_expansions(CppCounters &c)
{
    const long long old = c.expansions;
    c.expansions = 0;
    return old;
}

/* 已给出的对照：同样求一包数的和，走 <initializer_list> 这条路。
 * 大括号里的东西先变成数组再传进来，因此没有「包」的展开，也没有类型推导。 */
long long sum_init_list(std::initializer_list<long long> xs)
{
    long long s = 0;
    for (long long v : xs) {
        s += v;
    }
    return s;
}

std::size_t init_list_count(std::initializer_list<long long> xs)
{
    return xs.size();
}

} /* namespace va */
