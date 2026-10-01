/**
 * stack.hpp —— 定长栈的类模板（空模板 08）
 *
 * 与 algo.hpp 一样，**定义必须留在这个头文件里**。
 * 类模板的成员函数只有在被调用时才会实例化，因此即使某个成员还没实现，
 * 只要没被调用就不会报错——这也意味着「编译通过」不等于「实现正确」，
 * 必须把阶段 3 的验收程序跑一遍。
 */
#ifndef STACK_HPP
#define STACK_HPP

#include <cstddef>
#include <stdexcept>

/* T 是元素类型，N 是容量；N 有默认值，因此 Stack<int> 就是 Stack<int, 8> */
template <class T, std::size_t N = 8>
class Stack {
public:
    /* TODO（阶段 3-1）
     * 要求：栈满时返回 false 且不写入；否则放进 items_[size_] 并让 size_ 自增，返回 true。
     * 验收：容量 3 的栈压第 4 个元素返回 false；前 3 个都返回 true。 */
    bool push(const T &v)
    {
        (void)v;
        return false;
    }

    /* TODO（阶段 3-2）
     * 要求：栈空时返回 false；否则把栈顶元素写进 out、size_ 自减，返回 true。
     * 验收：阶段 3 的弹出顺序是 3 2 1（后进先出）。 */
    bool pop(T &out)
    {
        (void)out;
        return false;
    }

    /* TODO（阶段 3-3）
     * 要求：返回栈顶元素的**引用**（不拷贝）；栈空时抛 std::out_of_range。
     * 验收：阶段 3 里非空栈的 top 打印 3；空栈取 top 抛出 std::out_of_range 并被接住。
     *       现在的占位版本返回 items_[0]，不会抛异常。 */
    const T &top() const
    {
        return items_[0];
    }

    /* TODO（阶段 3-4）：成员模板
     * 要求：把另一种类型的数组 items 里的 n 个元素依次压栈（要求 U 能转成 T），
     *       返回实际压进去的个数（栈满就停）。
     * 验收：阶段 3 里把 {7,8,9} 压进空栈，返回 3，size 变成 3。
     * 提示：内部直接调用 push(static_cast<T>(items[i]))。 */
    template <class U>
    std::size_t push_all(const U *items, std::size_t n)
    {
        (void)items;
        (void)n;
        return 0;
    }

    /* TODO（阶段 3-5）
     * 要求：size() 返回当前元素个数；empty() 返回「个数是否为 0」。
     * 验收：阶段 3 里压入 3 个之后 size = 3；弹空之后 empty() 为 true。 */
    std::size_t size() const
    {
        return 0;
    }

    bool empty() const
    {
        return true;
    }

    /* 已给出：容量是编译期常量，第 4 阶段的 static_assert 会用到它 */
    static constexpr std::size_t capacity()
    {
        return N;
    }

private:
    /* 值初始化：即使还没压过任何元素，每个元素也有确定的初值 */
    T items_[N] = {};
    std::size_t size_ = 0;
};

#endif /* STACK_HPP */
