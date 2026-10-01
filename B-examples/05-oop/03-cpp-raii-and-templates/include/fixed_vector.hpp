/**
 * fixed_vector.hpp —— 一个小容器模板，外加几个配套的小算法
 *
 * 对应教材：《05-类与面向对象/10-模板.md》第 2、4、6 节
 *
 *   FixedVector<T, N>   定容容器：不碰堆，满了就拒绝，不会越界
 *   sum_of / max_of     函数模板：任何有 begin/end 与 value_type 的容器都能用
 *   to_text             函数模板 + 两个全特化：bool 与 double 要特殊写法
 *
 * 模板必须把实现放在头文件里：编译器要为每一种类型各生成一份代码，
 * 只看到声明是生成不出来的（这一点见教材第 5 节「实例化」）。
 */
#ifndef FIXED_VECTOR_HPP
#define FIXED_VECTOR_HPP

#include <cstddef>
#include <iomanip>
#include <sstream>
#include <string>

template <typename T, std::size_t Capacity>
class FixedVector {
public:
    using value_type = T;               /* 配套算法要用到这个名字 */

    /** 追加一个元素。满了返回 false，不覆盖也不越界 */
    bool push_back(const T &value)
    {
        if (size_ >= Capacity) {
            return false;
        }
        data_[size_++] = value;
        return true;
    }

    std::size_t size() const noexcept { return size_; }
    bool empty() const noexcept { return size_ == 0; }
    static constexpr std::size_t capacity() noexcept { return Capacity; }

    const T &operator[](std::size_t index) const noexcept { return data_[index]; }
    T &operator[](std::size_t index) noexcept { return data_[index]; }

    /* 有 begin/end 就能写范围 for，也能交给下面的函数模板 */
    const T *begin() const noexcept { return data_; }
    const T *end() const noexcept { return data_ + size_; }

    void clear() noexcept { size_ = 0; }

private:
    T data_[Capacity]{};                /* 定容：整块就在对象里，不碰堆 */
    std::size_t size_ = 0;
};

/** 求和。value_type 从容器自己身上取，因此换成 std::vector 也能用 */
template <typename Container>
typename Container::value_type sum_of(const Container &container)
{
    typename Container::value_type total{};
    for (const auto &item : container) {
        total += item;
    }
    return total;
}

/** 最大值。空容器返回 value_type{} */
template <typename Container>
typename Container::value_type max_of(const Container &container)
{
    typename Container::value_type best{};
    bool first = true;
    for (const auto &item : container) {
        if (first || item > best) {
            best = item;
            first = false;
        }
    }
    return best;
}

/** 通用版本：能用 << 输出就能转成文本 */
template <typename T>
std::string to_text(const T &value)
{
    std::ostringstream os;
    os << value;
    return os.str();
}

/** 全特化一：bool 默认打成 1 或 0，这里改成是或否 */
template <>
std::string to_text<bool>(const bool &value);

/** 全特化二：double 默认六位有效数字，这里固定三位小数 */
template <>
std::string to_text<double>(const double &value);

#endif /* FIXED_VECTOR_HPP */
