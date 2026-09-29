/**
 * calc.hpp —— 计算函数声明（C++ 版）
 */
#ifndef CALC_HPP
#define CALC_HPP

#include <string>
#include <vector>

/** 求和 */
int add(int a, int b);

/** 阶乘：演示递归调用栈 */
long long factorial(int n);

/** 数组求和 */
long long sum_array(const std::vector<int> &v);

/** 拼接字符串：演示 STL 容器的调试显示 */
std::string join(const std::vector<std::string> &parts, const std::string &sep);

#endif /* CALC_HPP */
