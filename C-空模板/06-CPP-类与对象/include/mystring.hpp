/**
 * mystring.hpp —— 自己写的字符串类（空模板 06）
 *
 * 类的接口已经定好，main.cpp 按这份接口写好了 4 个阶段的验收程序。
 * 你要做的是在 src/mystring.cpp 里把标了 TODO 的成员函数实现出来。
 *
 * 约定（main.cpp 依赖这些约定，实现时必须满足）：
 *   1. 空串的 data_ 为 nullptr、size_ 为 0；c_str() 返回 ""（不是 nullptr）
 *   2. 每个对象各自持有一块 new char[] 内存（深拷贝）
 *   3. 移动之后，源对象回到「空串」状态
 *   4. 构造与析构配平：程序结束时 live_count() 为 0
 */
#ifndef MYSTRING_HPP
#define MYSTRING_HPP

#include <cstddef>

class MyString {
public:
    /* ---------- 阶段 1：构造、析构、查询 ---------- */

    MyString();                          /* 空串 */
    explicit MyString(const char *s);    /* 从 C 字符串构造 */
    ~MyString();

    std::size_t size() const;
    bool empty() const;
    const char *c_str() const;

    /* 越界时抛 std::out_of_range，main.cpp 会接住它 */
    char at(std::size_t i) const;

    void append(const char *s);          /* 追加内容，必要时重新分配 */
    void clear();                        /* 变回空串 */

    /* ---------- 阶段 2：拷贝 ---------- */

    MyString(const MyString &other);              /* 深拷贝 */
    MyString &operator=(const MyString &other);   /* 深拷贝，自赋值安全 */

    /* ---------- 阶段 3：移动 ---------- */

    /* TODO（阶段 3-1）：这两个移动操作必须标 noexcept，否则阶段 4 会看到
     * 标准容器改用拷贝来搬运元素。骨架里先没有标。 */
    MyString(MyString &&other);
    MyString &operator=(MyString &&other);

    /* ---------- 计数（已实现，供验收使用） ---------- */

    static int live_count();             /* 当前活着的对象个数 */
    static long allocation_count();      /* 累计 new char[] 次数 */
    static long move_count();            /* 移动构造 + 移动赋值的调用次数 */

private:
    /* 内存管理的小工具，已给出，直接调用即可 */
    static char *alloc_raw(std::size_t n);                  /* 分配 n+1 字节 */
    static char *alloc_copy(const char *s, std::size_t n);  /* 分配并复制 n 个字符 */

    char       *data_;
    std::size_t size_;

    static int  live_count_;
    static long allocation_count_;
    static long move_count_;
};

#endif /* MYSTRING_HPP */
