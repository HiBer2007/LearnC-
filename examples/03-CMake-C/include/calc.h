/**
 * calc.h —— 计算函数声明
 *
 * 这个文件本身不参与编译，只是被 main.c 和 calc.c 各自 #include 一次。
 * 头文件的作用是让「调用方」和「实现方」看到同一份函数签名。
 */
#ifndef CALC_H
#define CALC_H

/** 求和：演示最基本的函数单步进入 */
int add(int a, int b);

/** 求差 */
int sub(int a, int b);

/** 阶乘：演示递归调用栈 */
long long factorial(int n);

/** 数组求和：演示指针 / 数组参数的调试 */
long long sum_array(const int *arr, int n);

#endif /* CALC_H */
