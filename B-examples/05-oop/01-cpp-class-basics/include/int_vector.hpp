/**
 * int_vector.hpp —— 自己写的一个整型动态数组
 *
 * 对应教材：《05-类与面向对象/02-类是一种类型.md》第 1、4、5 节
 *           《05-类与面向对象/03-构造与析构.md》第 1、2、3、6 节
 *           《05-类与面向对象/04-拷贝与移动.md》第 2、3、4、5 节
 *           《05-类与面向对象/08-运算符重载.md》第 2、3 节
 *
 * 这个类不是要替代 std::vector，而是把「值类型」该有的东西写全：
 *   构造与析构、拷贝与移动、const 成员函数、运算符、静态成员。
 * 头文件只放声明与一两行的短函数，长函数放到 src/int_vector.cpp 里。
 */
#ifndef INT_VECTOR_HPP
#define INT_VECTOR_HPP

#include <cstddef>
#include <initializer_list>
#include <iosfwd>

class IntVector {
public:
    /* ── 构造与析构：对象一出生就得是合法的 ────────────── */

    /** 空数组 */
    IntVector() noexcept;

    /** n 个元素，值都是 fill */
    explicit IntVector(std::size_t n, int fill = 0);

    /** {1, 2, 3} 这样的写法 */
    IntVector(std::initializer_list<int> init);

    /** 拷贝构造：按 other 的样子再造一份，两份之后互不影响 */
    IntVector(const IntVector &other);

    /** 移动构造：把 other 的缓冲区搬过来，不复制元素 */
    IntVector(IntVector &&other) noexcept;

    ~IntVector();

    /* ── 赋值 ──────────────────────────────────────────── */

    IntVector &operator=(const IntVector &other);
    IntVector &operator=(IntVector &&other) noexcept;

    /* ── 观察：都是 const 成员函数，const 对象也能调用 ── */

    std::size_t size() const noexcept { return size_; }
    std::size_t capacity() const noexcept { return capacity_; }
    bool empty() const noexcept { return size_ == 0; }
    const int *data() const noexcept { return data_; }

    /* ── 访问元素 ──────────────────────────────────────── */

    int &operator[](std::size_t index) noexcept { return data_[index]; }
    const int &operator[](std::size_t index) const noexcept { return data_[index]; }

    /** 带越界检查的访问，越界时抛 std::out_of_range */
    int &at(std::size_t index);
    const int &at(std::size_t index) const;

    /* ── 修改 ──────────────────────────────────────────── */

    void push_back(int value);
    void clear() noexcept { size_ = 0; }
    void swap(IntVector &other) noexcept;

    /** 预留容量。只要不越界，就先分配好，避免反复搬运元素 */
    void reserve(std::size_t needed);

    /* ── 复合赋值运算符：写成成员函数 ──────────────────── */

    IntVector &operator+=(const IntVector &rhs);
    IntVector &operator*=(int factor);

    /* ── static 成员：属于类，不属于某个对象 ───────────── */

    /** 当前活着的对象个数。拷贝、移动、析构写错了，这个数立刻不对 */
    static std::size_t live_count() noexcept { return live_count_; }

    /** 累计的堆分配次数。移动不分配，这个数就能证明 */
    static std::size_t allocations() noexcept { return allocations_; }
    static void reset_allocations() noexcept { allocations_ = 0; }

private:
    int *data_ = nullptr;               /* 类内初始化器：不写构造函数也不会是野指针 */
    std::size_t size_ = 0;
    std::size_t capacity_ = 0;

    static std::size_t live_count_;
    static std::size_t allocations_;

    static int *allocate(std::size_t n);
    static void release(int *p) noexcept;
};

/* ── 非成员运算符：两边都能隐式转换时写成非成员 ────────── */

/** 值传递 + 复合赋值 + 返回值：拷贝发生在参数上，正好用上移动 */
IntVector operator+(IntVector lhs, const IntVector &rhs);
IntVector operator*(IntVector lhs, int factor);

bool operator==(const IntVector &lhs, const IntVector &rhs) noexcept;
bool operator!=(const IntVector &lhs, const IntVector &rhs) noexcept;

std::ostream &operator<<(std::ostream &os, const IntVector &v);

#endif /* INT_VECTOR_HPP */
