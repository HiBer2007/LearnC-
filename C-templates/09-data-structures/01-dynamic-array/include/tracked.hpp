/* tracked.hpp —— 练习模板 01 的计数类型（C++）
 *
 * 版权所有 (C) 2026 HiBer2007，保留所有权利。
 *
 * 本程序是《C 与 C++》教材的一部分，采用与教材文档相同的授权：
 * CC BY-NC-ND 4.0 加附加条款。全文见仓库根目录的 LICENSE 与 许可附加条款.md。
 *
 * 允许在保留本声明的前提下查看、编译、运行本程序用于学习；
 * 不允许二次分发，不允许商用，不允许演绎（修改后再分发），不允许移除署名。
 *
 * 本程序不提供任何担保。
 *
 * ------------------------------------------------------------------
 * 这个头文件里的类型只做一件事：把「构造了多少、析构了多少、搬了几次」
 * 变成可以打印的数字。验收程序靠它判断容器的三件事：
 * 有没有漏掉析构、扩容时有没有漏掉释放、搬迁用的是移动还是拷贝。
 *
 * 两个别名的差别只有一处：
 *     Tracked       移动构造标了 noexcept，容器可以放心移动
 *     SlowTracked   移动构造没有标 noexcept，容器应当退回复制
 *
 * 这两个类的定义与计数器都在 src/counters.cpp 里，练习不需要改它们。
 */
#ifndef TRACKED_HPP
#define TRACKED_HPP

/* 计数器：两种类型共用一份，因此放在非模板的类里 */
struct TrackedCounters {
    static long live;           /* 当前活着的对象个数 */
    static long copies;         /* 拷贝构造次数 */
    static long moves;          /* 移动构造次数 */
    static long throw_after;    /* 再发生这么多次「拷贝或移动构造」就抛异常；-1 表示不抛 */

    /* 三个计数清零，并把 throw_after 置回 -1 */
    static void reset(void);
};

/* 让接下来的第 n 次「拷贝或移动构造」抛异常，n 为负表示关闭。
 * 传 2 表示数到第三次（前两次正常、第三次抛）——阶段 5 用它模拟「搬到一半失败」。 */
void tracked_set_throw_after(long n);

/* 拷贝构造与移动构造里调用的钩子，按 throw_after 的计数决定抛不抛 */
void tracked_maybe_throw(void);

/* NothrowMove 为 true 时移动构造带 noexcept，为 false 时不带 */
template <bool NothrowMove>
class TrackedT {
public:
    TrackedT(void) : value_(0) { ++TrackedCounters::live; }

    explicit TrackedT(int v) : value_(v) { ++TrackedCounters::live; }

    TrackedT(const TrackedT &other) : value_(other.value_) {
        tracked_maybe_throw();          /* 可能抛；抛了就不算构造成功 */
        ++TrackedCounters::live;
        ++TrackedCounters::copies;
    }

    TrackedT(TrackedT &&other) noexcept(NothrowMove) : value_(other.value_) {
        tracked_maybe_throw();          /* NothrowMove 为 false 时这里也允许抛 */
        other.value_ = -1;              /* 被移动之后的值是「有效但未指定」，这里取 -1，便于看出来 */
        ++TrackedCounters::live;
        ++TrackedCounters::moves;
    }

    TrackedT &operator=(const TrackedT &) = default;
    TrackedT &operator=(TrackedT &&) = default;

    ~TrackedT(void) { --TrackedCounters::live; }

    int value(void) const { return value_; }

    bool operator==(const TrackedT &other) const { return value_ == other.value_; }
    bool operator!=(const TrackedT &other) const { return value_ != other.value_; }

private:
    int value_;
};

using Tracked = TrackedT<true>;         /* 移动构造带 noexcept */
using SlowTracked = TrackedT<false>;    /* 移动构造不带 noexcept，容器应当退回复制 */

#endif /* TRACKED_HPP */
