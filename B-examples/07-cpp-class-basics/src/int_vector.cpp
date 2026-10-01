/**
 * int_vector.cpp —— IntVector 的实现
 *
 * 用到这个类的代码只需要包含 int_vector.hpp；
 * 这里怎么分配、怎么搬运，都留在本文件里，改实现不必重新编译使用者。
 */
#include "int_vector.hpp"

#include <algorithm>
#include <ostream>
#include <stdexcept>
#include <utility>

/* ── 静态成员的定义：类里只是声明，存储要在这里给出 ────── */
std::size_t IntVector::live_count_ = 0;
std::size_t IntVector::allocations_ = 0;

/* ── 私有的内存管理 ────────────────────────────────────── */

int *IntVector::allocate(std::size_t n)
{
    if (n == 0) {
        return nullptr;
    }
    ++allocations_;
    return new int[n]{};        /* 花括号：新数组一律先清零 */
}

void IntVector::release(int *p) noexcept
{
    delete[] p;                 /* delete[] 与 new[] 配对 */
}

void IntVector::reserve(std::size_t needed)
{
    if (needed <= capacity_) {
        return;
    }
    std::size_t new_capacity = capacity_ == 0 ? 4 : capacity_;
    while (new_capacity < needed) {
        new_capacity *= 2;      /* 成倍增长，避免每加一个元素就搬一次 */
    }

    int *fresh = allocate(new_capacity);
    for (std::size_t i = 0; i < size_; ++i) {
        fresh[i] = data_[i];
    }
    release(data_);

    data_ = fresh;
    capacity_ = new_capacity;
}

/* ── 构造与析构 ────────────────────────────────────────── */

IntVector::IntVector() noexcept
{
    ++live_count_;
}

IntVector::IntVector(std::size_t n, int fill)
    : data_(allocate(n)), size_(n), capacity_(n)
{
    ++live_count_;
    for (std::size_t i = 0; i < size_; ++i) {
        data_[i] = fill;
    }
}

IntVector::IntVector(std::initializer_list<int> init)
    : data_(allocate(init.size())), size_(init.size()), capacity_(init.size())
{
    ++live_count_;
    std::size_t i = 0;
    for (const int value : init) {
        data_[i++] = value;
    }
}

IntVector::IntVector(const IntVector &other)
    : data_(allocate(other.size_)), size_(other.size_), capacity_(other.size_)
{
    ++live_count_;
    for (std::size_t i = 0; i < size_; ++i) {
        data_[i] = other.data_[i];
    }
}

/* 移动构造：只搬三个成员，不碰元素，因此标 noexcept，也不产生分配 */
IntVector::IntVector(IntVector &&other) noexcept
    : data_(other.data_), size_(other.size_), capacity_(other.capacity_)
{
    ++live_count_;
    other.data_ = nullptr;
    other.size_ = 0;
    other.capacity_ = 0;
}

IntVector::~IntVector()
{
    release(data_);
    --live_count_;
}

/* ── 赋值 ──────────────────────────────────────────────── */

/* 拷贝并交换：先复制出一份，成功了再换进来。
   若复制过程中抛异常，*this 仍是原来的样子。 */
IntVector &IntVector::operator=(const IntVector &other)
{
    if (this != &other) {               /* 自赋值：v = v 不能把自己拆了 */
        IntVector copy(other);
        swap(copy);                     /* copy 析构时释放旧缓冲区 */
    }
    return *this;
}

IntVector &IntVector::operator=(IntVector &&other) noexcept
{
    if (this != &other) {
        release(data_);
        data_ = other.data_;
        size_ = other.size_;
        capacity_ = other.capacity_;
        other.data_ = nullptr;
        other.size_ = 0;
        other.capacity_ = 0;
    }
    return *this;
}

/* ── 访问与修改 ────────────────────────────────────────── */

int &IntVector::at(std::size_t index)
{
    if (index >= size_) {
        throw std::out_of_range("IntVector::at 下标越界");
    }
    return data_[index];
}

const int &IntVector::at(std::size_t index) const
{
    if (index >= size_) {
        throw std::out_of_range("IntVector::at 下标越界");
    }
    return data_[index];
}

void IntVector::push_back(int value)
{
    reserve(size_ + 1);
    data_[size_++] = value;
}

void IntVector::swap(IntVector &other) noexcept
{
    std::swap(data_, other.data_);
    std::swap(size_, other.size_);
    std::swap(capacity_, other.capacity_);
}

IntVector &IntVector::operator+=(const IntVector &rhs)
{
    if (rhs.size_ != size_) {
        throw std::length_error("IntVector 相加要求两边长度相同");
    }
    for (std::size_t i = 0; i < size_; ++i) {
        data_[i] += rhs.data_[i];
    }
    return *this;
}

IntVector &IntVector::operator*=(int factor)
{
    for (std::size_t i = 0; i < size_; ++i) {
        data_[i] *= factor;
    }
    return *this;
}

/* ── 非成员运算符 ──────────────────────────────────────── */

IntVector operator+(IntVector lhs, const IntVector &rhs)
{
    lhs += rhs;
    return lhs;                 /* 返回局部对象，编译器直接构造在调用处 */
}

IntVector operator*(IntVector lhs, int factor)
{
    lhs *= factor;
    return lhs;
}

bool operator==(const IntVector &lhs, const IntVector &rhs) noexcept
{
    if (lhs.size() != rhs.size()) {
        return false;
    }
    for (std::size_t i = 0; i < lhs.size(); ++i) {
        if (lhs[i] != rhs[i]) {
            return false;
        }
    }
    return true;
}

bool operator!=(const IntVector &lhs, const IntVector &rhs) noexcept
{
    return !(lhs == rhs);
}

std::ostream &operator<<(std::ostream &os, const IntVector &v)
{
    os << '[';
    for (std::size_t i = 0; i < v.size(); ++i) {
        os << v[i];
        if (i + 1 < v.size()) {
            os << ", ";
        }
    }
    return os << ']';
}
