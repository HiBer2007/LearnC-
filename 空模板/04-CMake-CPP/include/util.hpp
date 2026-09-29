/**
 * util.hpp —— 工具函数声明（C++）
 */
#ifndef UTIL_HPP
#define UTIL_HPP

#include <string>
#include <vector>

/** 求平方 */
int square(int x);

/** 递归求阶乘 */
long long factorial(int n);

/** 向量求和 */
long long sum(const std::vector<int> &v);

/** 拼接字符串（演示 STL 容器的调试显示） */
std::string join(const std::vector<std::string> &parts, const std::string &sep);

#endif /* UTIL_HPP */
