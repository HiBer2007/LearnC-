/**
 * vector_demo.cpp —— 项目逻辑的实现
 *
 * 这里没有任何界面代码：不包含 <windows.h>，也不打印。
 * 命令行版和 GUI 版都在这个基础上接自己的输入输出。
 */
#include "vector_demo.hpp"

#include <cctype>
#include <cstddef>
#include <cstdlib>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace demo {

namespace {

/* 自测的小工具：把每一项的结果记下来，不打印 */
class Checker {
public:
    void check(bool ok, const std::string &what, const std::string &detail = std::string())
    {
        ++result_.total;
        std::ostringstream line;
        if (ok) {
            ++result_.passed;
            line << "[通过] " << result_.total << ". " << what;
        } else {
            ++result_.failed;
            line << "[不符] " << result_.total << ". " << what;
            if (!detail.empty()) {
                line << "（" << detail << "）";
            }
        }
        result_.lines.push_back(line.str());
    }

    CheckResult take() { return std::move(result_); }

private:
    CheckResult result_;
};

}   /* namespace */

std::string CheckResult::summary() const
{
    std::ostringstream os;
    os << total << " 项中 " << passed << " 项通过";
    if (failed == 0) {
        os << "，全部通过";
    } else {
        os << "，" << failed << " 项不符";
    }
    return os.str();
}

IntVector fibonacci(std::size_t n)
{
    IntVector result;
    result.reserve(n);              /* 预留容量，避免反复扩容 */
    int a = 1;
    int b = 1;
    for (std::size_t i = 0; i < n; ++i) {
        result.push_back(a);
        const int next = a + b;
        a = b;
        b = next;
    }
    return result;
}

std::vector<int> fibonacci_with_std(std::size_t n)
{
    std::vector<int> result;
    result.reserve(n);
    int a = 1;
    int b = 1;
    for (std::size_t i = 0; i < n; ++i) {
        result.push_back(a);
        const int next = a + b;
        a = b;
        b = next;
    }
    return result;
}

IntVector to_int_vector(const std::vector<int> &source)
{
    IntVector result;
    result.reserve(source.size());
    for (const int value : source) {
        result.push_back(value);
    }
    return result;
}

IntVector prefix_sum(const IntVector &values)
{
    IntVector result;
    result.reserve(values.size());
    int running = 0;
    for (std::size_t i = 0; i < values.size(); ++i) {
        running += values[i];
        result.push_back(running);
    }
    return result;
}

IntVector add_scalar(const IntVector &values, int offset)
{
    IntVector result;
    result.reserve(values.size());
    for (std::size_t i = 0; i < values.size(); ++i) {
        result.push_back(values[i] + offset);
    }
    return result;
}

IntVector parse_numbers(const std::string &text, bool &ok, std::string &error)
{
    IntVector result;
    ok = true;
    error.clear();

    const char *cursor = text.c_str();
    while (*cursor != '\0') {
        /* 跳过空格、逗号、分号等分隔符。
           中文逗号是 UTF-8 多字节字符，因此把 0x80 以上的字节一并当分隔符跳过 */
        while (*cursor != '\0'
               && (std::isspace(static_cast<unsigned char>(*cursor)) != 0
                   || *cursor == ',' || *cursor == ';'
                   || static_cast<unsigned char>(*cursor) >= 0x80)) {
            ++cursor;
        }
        if (*cursor == '\0') {
            break;
        }

        char *end = nullptr;
        const long value = std::strtol(cursor, &end, 10);
        if (end == cursor) {
            ok = false;
            error = std::string("这里不是整数：") + cursor;
            return IntVector{};
        }
        if (value < -1000000L || value > 1000000L) {
            ok = false;
            error = "数值超出范围（-1000000 到 1000000）";
            return IntVector{};
        }
        result.push_back(static_cast<int>(value));
        cursor = end;

        if (result.size() > 64) {
            ok = false;
            error = "最多 64 个数";
            return IntVector{};
        }
    }

    if (result.empty()) {
        ok = false;
        error = "没有读到任何整数";
    }
    return result;
}

std::string to_text(const IntVector &values)
{
    std::ostringstream os;
    os << values;                   /* 用的是 IntVector 自己的 operator<< */
    return os.str();
}

CheckResult run_self_tests()
{
    Checker c;

    /* 1–2. 两种构造函数 */
    const IntVector init_list{1, 2, 3};
    c.check(init_list.size() == 3 && init_list[0] == 1 && init_list[2] == 3,
            "初始化列表构造 {1, 2, 3}", to_text(init_list));
    const IntVector filled(4, 7);
    c.check(filled.size() == 4 && filled[0] == 7 && filled[3] == 7,
            "IntVector(4, 7) 造出 4 个 7", to_text(filled));

    /* 3. 拷贝构造：两份之后互不影响 */
    IntVector original{10, 20, 30};
    IntVector copied(original);
    copied[0] = 99;
    c.check(original[0] == 10 && copied[0] == 99,
            "拷贝构造出的对象是独立的一份",
            "原件 " + to_text(original) + "，副本 " + to_text(copied));

    /* 4. 拷贝赋值 */
    IntVector assigned;
    assigned = original;
    c.check(assigned == original && assigned.data() != original.data(),
            "拷贝赋值后内容相同，缓冲区不同");

    /* 5. 自赋值不能把自己拆掉 */
    assigned = assigned;
    c.check(assigned == original, "自赋值 v = v 之后内容不变", to_text(assigned));

    /* 6. 移动构造：资源搬走，源对象变成空 */
    IntVector source{1, 2, 3, 4};
    const std::size_t allocations_before = IntVector::allocations();
    IntVector moved(std::move(source));
    c.check(moved.size() == 4 && source.size() == 0 && source.data() == nullptr,
            "移动构造后源对象变空", "源 size = " + std::to_string(source.size()));

    /* 7. 移动不复制元素，因此不产生新的堆分配 */
    c.check(IntVector::allocations() == allocations_before,
            "移动构造没有产生新的堆分配",
            "分配次数 " + std::to_string(allocations_before) + " → "
                + std::to_string(IntVector::allocations()));

    /* 8. 移动赋值同理 */
    IntVector target{9, 9};
    const std::size_t allocations_before_move_assign = IntVector::allocations();
    target = std::move(moved);
    c.check(target.size() == 4 && moved.size() == 0
                && IntVector::allocations() == allocations_before_move_assign,
            "移动赋值同样是搬走，不是复制");

    /* 9. 下标越界抛异常 */
    bool caught = false;
    try {
        (void)target.at(target.size());
    } catch (const std::out_of_range &) {
        caught = true;
    }
    c.check(caught, "at() 越界抛出 std::out_of_range");

    /* 10. 长度不同的向量相加要报错，而不是越界读写 */
    bool length_error_caught = false;
    try {
        IntVector bad{1, 2};
        bad += target;
    } catch (const std::length_error &) {
        length_error_caught = true;
    }
    c.check(length_error_caught, "长度不同的向量相加抛出 std::length_error");

    /* 11. 运算符 */
    const IntVector a{1, 2, 3};
    const IntVector b{10, 20, 30};
    c.check(to_text(a + b) == "[11, 22, 33]", "operator+ 逐项相加", to_text(a + b));
    c.check(to_text(a * 3) == "[3, 6, 9]", "operator* 逐项乘系数", to_text(a * 3));
    c.check(a == IntVector{1, 2, 3} && a != b, "operator== 与 operator!=");

    /* 12. 扩容后元素仍在，容量按倍增长 */
    IntVector growing;
    for (int i = 0; i < 10; ++i) {
        growing.push_back(i);
    }
    c.check(growing.size() == 10 && growing[0] == 0 && growing[9] == 9
                && growing.capacity() >= growing.size(),
            "push_back 触发扩容后元素完好",
            "size = " + std::to_string(growing.size()) + "，capacity = "
                + std::to_string(growing.capacity()));

    /* 13. const 对象只能用 const 成员函数，这里核对它读得对 */
    const IntVector &const_ref = growing;
    c.check(const_ref.size() == 10 && const_ref[3] == 3 && const_ref.at(4) == 4,
            "const 引用能读，不能改");

    /* 14. 输入解析 */
    bool ok = false;
    std::string error;
    const IntVector parsed = parse_numbers("1, 2  3;4", ok, error);
    c.check(ok && parsed.size() == 4 && parsed[3] == 4, "解析 \"1, 2  3;4\"",
            ok ? to_text(parsed) : error);
    const IntVector bad_parse = parse_numbers("1 x 3", ok, error);
    c.check(!ok && bad_parse.size() == 0 && !error.empty(), "非法输入会被拒绝", error);

    /* 15. 与 std::vector 对照 */
    const IntVector fib = fibonacci(12);
    c.check(fib == to_int_vector(fibonacci_with_std(12)),
            "自己写的容器与 std::vector 算出的数列一致");

    /* 16. 生存期：所有对象都在作用域结束时析构 */
    const std::size_t live_before = IntVector::live_count();
    {
        IntVector local_a{1};
        IntVector local_b(local_a);
        IntVector local_c(std::move(local_b));
        std::vector<IntVector> pool(3);
        c.check(IntVector::live_count() == live_before + 6,
                "作用域内 6 个对象都活着",
                "live = " + std::to_string(IntVector::live_count()));
    }
    c.check(IntVector::live_count() == live_before,
            "离开作用域后全部析构，没有对象泄漏",
            "live = " + std::to_string(IntVector::live_count()));

    return c.take();
}

}   /* namespace demo */
