/**
 * util.h —— 工具函数声明
 *
 * 故意把实现放到另一个 .c 文件里，
 * 这样配好调试器之后可以练习「跨文件单步」（F11 跳到 util.c）。
 */
#ifndef UTIL_H
#define UTIL_H

/** 求平方 */
int square(int x);

/** 递归求阶乘 */
long long factorial(int n);

#endif /* UTIL_H */
