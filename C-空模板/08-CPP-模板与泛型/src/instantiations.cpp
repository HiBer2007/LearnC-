/**
 * instantiations.cpp —— 显式实例化（空模板 08）
 *
 * 模板的定义留在头文件里，这个文件只是提前告诉编译器「这几个类型组合请生成代码」。
 * 好处是同一个实例只生成一次，多个源文件包含同一个头文件时不必各自生成一份
 * （《05-类与面向对象/11-模板.md》第 5.2 小节）。
 */
#include <string>

#include "stack.hpp"

/* 已给出：显式实例化 Stack<int, 8> */
template class Stack<int, 8>;

/* TODO（阶段 4-1）
 * 要求：再补一行，为 Stack<std::string, 4> 做显式实例化。
 * 验收：程序能编过、能运行，输出与阶段 3 完全一致（显式实例化不改变行为）。
 * 自查实验：补完之后，把 Stack::push 的定义从头文件挪进一个 .cpp（不加入构建），
 *       链接会报
 *           undefined reference to `bool Stack<std::string, 4ul>::push(std::string const&)'
 *       这就是「模板的定义必须能被调用点看到」的直接后果，实验完记得改回来。
 */
