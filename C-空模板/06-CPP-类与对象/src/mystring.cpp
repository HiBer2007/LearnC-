/**
 * mystring.cpp —— MyString 的实现（空模板 06）
 *
 * 骨架里的每个成员函数都是**能编译、能运行**的占位版本：
 * 它们不释放内存、不做深拷贝，因此运行结果会与《配置步骤.md》里的期望值不符。
 * 把每个 TODO 换成真正的实现，逐阶段对照验收标准。
 *
 * 调试建议：在 alloc_raw 里打断点，观察每个阶段到底分配了几次内存。
 */
#include "mystring.hpp"

#include <cstring>
#include <stdexcept>

/* ==================================================================
 * 内存管理的小工具（已给出）
 * ================================================================== */

char *MyString::alloc_raw(std::size_t n)
{
    ++allocation_count_;
    return new char[n + 1];
}

char *MyString::alloc_copy(const char *s, std::size_t n)
{
    char *buf = alloc_raw(n);
    for (std::size_t i = 0; i < n; ++i) {
        buf[i] = s[i];
    }
    buf[n] = '\0';
    return buf;
}

/* ==================================================================
 * 计数（已给出）
 * ================================================================== */

int  MyString::live_count_ = 0;
long MyString::allocation_count_ = 0;
long MyString::move_count_ = 0;

int  MyString::live_count() { return live_count_; }
long MyString::allocation_count() { return allocation_count_; }
long MyString::move_count() { return move_count_; }

/* ==================================================================
 * 阶段 1 · 构造、析构、查询
 * ================================================================== */

/* 空串的写法已经给出，可当作其余构造函数的模板 */
MyString::MyString() : data_(nullptr), size_(0)
{
    ++live_count_;
}

/* TODO（阶段 1-1）
 * 要求：算出 s 的长度，用 alloc_copy 拿一块自己的内存，累计 live_count_。
 * 提示：长度用 std::strlen（<cstring> 已经包含）。
 *       注意 size_ 要在初始化列表里填好，data_ 也在列表里初始化。
 * 验收：阶段 1 打印 hello : size = 5, c_str = hello。
 *       若打印 size = 0，说明初始化列表里没填 size_，或忘了调用 alloc_copy。
 */
MyString::MyString(const char *s) : data_(nullptr), size_(0)
{
    (void)s;
    ++live_count_;
}

/* TODO（阶段 1-2）
 * 要求：释放自己持有的内存，并让 live_count_ 自减。
 * 提示：delete[] 一个 nullptr 是合法的，不必额外判断。
 * 验收：每个阶段结束时 live 回到 0；若 live 只增不减，说明这里没写完。
 */
MyString::~MyString()
{
    /* TODO */
}

/* TODO（阶段 1-3）
 * 要求：size() 返回 size_；empty() 返回「长度是否为 0」；
 *       c_str() 返回可当 C 字符串用的指针（空串返回 ""，不要返回 nullptr）。
 * 验收：阶段 1 打印 empty : size = 0, empty() = true 与 hello 的两行。
 */
std::size_t MyString::size() const
{
    return 0;
}

bool MyString::empty() const
{
    return true;
}

const char *MyString::c_str() const
{
    return "";
}

/* TODO（阶段 1-4）
 * 要求：下标越界时抛 std::out_of_range，消息写成 "MyString::at 下标越界"。
 * 验收：阶段 1 里 at(0) 得到 'h'，at(5) 抛出的异常被 main 接住并打印消息。
 *       现在的占位版本直接返回 '\0'，两行都会走成「没有抛异常」。
 */
char MyString::at(std::size_t i) const
{
    (void)i;
    return '\0';
}

/* TODO（阶段 1-5）
 * 要求：把 s 接到末尾，重新分配一块 size_ + strlen(s) 的内存，
 *       复制原来的内容与 s，再释放旧内存。
 * 提示：用 alloc_raw(size_ + add) 拿新块，它会把分配次数记上；
 *       不要忘了更新 data_ 与 size_。
 * 验收：阶段 2 里 b.append(" world") 之后，b 为 "hello world"（长度 11）。
 */
void MyString::append(const char *s)
{
    (void)s;
}

/* TODO（阶段 1-6）
 * 要求：释放内存并回到空串状态（data_ = nullptr、size_ = 0）。
 * 验收：阶段 2 里调用 clear 之后 size 变成 0，原内容不再打印出来。
 */
void MyString::clear()
{
    /* TODO */
}

/* ==================================================================
 * 阶段 2 · 拷贝构造与拷贝赋值
 * ================================================================== */

/* TODO（阶段 2-1）
 * 要求：深拷贝 —— 为 other 的内容单独分配一块内存，两个对象互不影响。
 * 验收：阶段 2 里 b 追加内容后，a 仍然是 "hello"。
 * 反例实验：把这里改成逐成员拷贝（data_ = other.data_; size_ = other.size_;），
 *       跑一次会看到程序在析构时崩溃（同一块内存被 delete[] 两次）。
 */
MyString::MyString(const MyString &other) : data_(nullptr), size_(0)
{
    (void)other;
    ++live_count_;
}

/* TODO（阶段 2-2）
 * 要求：把 other 的内容拷到当前对象上，返回 *this。
 *       必须处理**自赋值**（other 就是自己）：先分配新内存，再释放旧内存，
 *       这样即使自赋值也不会取到已经释放的内容。
 * 验收：阶段 2 的 c 打印 hello，且自赋值那一行不崩、内容不变。
 */
MyString &MyString::operator=(const MyString &other)
{
    (void)other;
    return *this;
}

/* ==================================================================
 * 阶段 3 · 移动构造与移动赋值
 * ================================================================== */

/* TODO（阶段 3-1）
 * 要求：把 other 的内存**接管**过来，再把 other 置成空串；
 *       最后给这两个函数补上 noexcept（头文件里也标）。
 * 验收：阶段 3 里 big 变成空串、taken 拿到内容；
 *       新增分配 = 0，新增移动 = 2。
 *       若新增分配不是 0，说明移动里又分配了一次内存（那就是拷贝）。
 */
MyString::MyString(MyString &&other) : data_(nullptr), size_(0)
{
    (void)other;
    ++live_count_;
}

/* TODO（阶段 3-2）
 * 要求：处理自赋值，释放自己原有的内存，接管 other 的内存与长度，
 *       把 other 置成空串，返回 *this。
 * 验收：阶段 3 里 target 拿到内容，taken 变成空串。
 */
MyString &MyString::operator=(MyString &&other)
{
    (void)other;
    return *this;
}
