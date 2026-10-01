/**
 * vector_demo.hpp —— 项目逻辑，不依赖任何界面
 *
 * 命令行版（src/main_cli.cpp）与 GUI 版（src/main_gui.cpp）都链接它。
 * 这里的函数不打印任何东西，只负责算并返回结果：
 * 该显示成什么样子，由各自的界面决定。
 */
#ifndef VECTOR_DEMO_HPP
#define VECTOR_DEMO_HPP

#include "int_vector.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace demo {

/** 生成前 n 项斐波那契数，装进自己写的容器 */
IntVector fibonacci(std::size_t n);

/** 同一段数列用 std::vector 再算一遍，用于对照 */
std::vector<int> fibonacci_with_std(std::size_t n);

/** 把 std::vector 的内容搬进 IntVector */
IntVector to_int_vector(const std::vector<int> &source);

/** 前缀和：第 i 项是原序列前 i+1 项之和 */
IntVector prefix_sum(const IntVector &values);

/** 每项加 offset，用来演示 operator+ */
IntVector add_scalar(const IntVector &values, int offset);

/** 解析 "1 2 3" 或 "1, 2, 3" 形式的输入。失败时 ok 置 false 并写明原因 */
IntVector parse_numbers(const std::string &text, bool &ok, std::string &error);

/** 把序列转成 "[1, 2, 3]" 这样的文本 */
std::string to_text(const IntVector &values);

/** 自测结果。不打印，交给界面决定怎么显示 */
struct CheckResult {
    int total = 0;
    int passed = 0;
    int failed = 0;
    std::vector<std::string> lines;     /**< 每项一行，例如 "[通过] 3. ……" */

    bool all_passed() const { return failed == 0; }
    std::string summary() const;        /**< "17 项中 17 项通过，全部通过" */
};

/** 逐项核对拷贝、移动、运算符与生存期 */
CheckResult run_self_tests();

}   /* namespace demo */

#endif /* VECTOR_DEMO_HPP */
