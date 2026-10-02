/* dynarray.hpp —— 练习模板 01 的核心接口（C++）
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
 * 一个自己管的动态数组：三个指针加一处扩容路径，分成 5 个阶段。
 * 五个阶段的题目、验收标准与自查方法见同目录《配置步骤.md》。
 *
 *     阶段 1  append_at_end()     容量够的时候，把元素追加到 end_ 那一格
 *     阶段 2  grow_and_push()     里「换指针、析构旧的、还内存」那一段
 *     阶段 3  relocate()          把元素搬到新缓冲区：拷贝还是移动
 *     阶段 4  grow_capacity()     扩容策略：下一次要多少格
 *     阶段 5  grow_and_push()     里 catch 那一段：搬到一半抛了怎么办
 *
 * 类模板的成员函数要写在使用点可见的地方，因此这个文件既有声明也有实现。
 * 计数器的定义在 src/counters.cpp 里，那个文件不需要改。
 */
#ifndef DYNARRAY_HPP
#define DYNARRAY_HPP

#include <cstddef>
#include <new>
#include <utility>

namespace dyn {

/* 分配统计：分配与释放分开数，用来判断有没有漏掉释放 */
struct AllocStats {
    static long allocs;         /* allocate 被调用了几次 */
    static long deallocs;       /* deallocate 被调用了几次 */
    static long bytes;          /* 还欠着的字节数：分配时加上、释放时减去 */

    static void reset(void);
};

/* 最小分配器：allocate 只给原始内存，deallocate 按当初申请的数量还回去。
 * 注意 deallocate 的第二个实参不能省——有些分配器要靠它判断该还给哪条空闲链。 */
template <class T>
struct SimpleAllocator {
    T *allocate(std::size_t n) {
        ++AllocStats::allocs;
        AllocStats::bytes += static_cast<long>(n * sizeof(T));
        return static_cast<T *>(::operator new(n * sizeof(T)));
    }

    void deallocate(T *p, std::size_t n) {
        ++AllocStats::deallocs;
        AllocStats::bytes -= static_cast<long>(n * sizeof(T));
        ::operator delete(p);
    }
};

template <class T>
class DynArray {
public:
    DynArray(void) = default;

    /* n 个默认构造的元素，容量正好是 n */
    explicit DynArray(std::size_t n)
        : begin_(alloc_.allocate(n)), end_(begin_ + n), cap_(begin_ + n)
    {
        /* 默认构造不会抛，因此这一段没有异常处理；扩容路径上的异常见阶段 5 */
        for (std::size_t i = 0; i < n; ++i) {
            new (begin_ + i) T();
        }
    }

    ~DynArray(void) { release_all(); }

    DynArray(const DynArray &other) { append_all(other); }
    DynArray(DynArray &&other) noexcept { take_over(other); }

    DynArray &operator=(const DynArray &other) {
        if (this != &other) {
            release_all();
            append_all(other);
        }
        return *this;
    }

    DynArray &operator=(DynArray &&other) noexcept {
        if (this != &other) {
            release_all();
            take_over(other);
        }
        return *this;
    }

    std::size_t size(void) const { return static_cast<std::size_t>(end_ - begin_); }
    std::size_t capacity(void) const { return static_cast<std::size_t>(cap_ - begin_); }
    bool empty(void) const { return begin_ == end_; }

    T &operator[](std::size_t i) { return begin_[i]; }
    const T &operator[](std::size_t i) const { return begin_[i]; }
    T *data(void) { return begin_; }

    /* 追加一个元素：容量够就走阶段 1，不够就走扩容路径 */
    void push_back(const T &value) {
        if (end_ != cap_) {
            append_at_end(value);
            return;
        }
        grow_and_push(value);
    }

    /* 去掉最后一个元素；已经空了就什么也不做 */
    void pop_back(void) {
        if (begin_ != end_) {
            --end_;
            end_->~T();
        }
    }

    /* 元素个数归零，容量不变（要释放内存得另想办法，本模板不提供 reserve） */
    void clear(void) {
        while (begin_ != end_) {
            --end_;
            end_->~T();
        }
    }

    /* 统计：验收程序用它判断扩容与搬迁的次数，你不需要改 */
    std::size_t grow_calls(void) const { return grow_calls_; }
    std::size_t relocations(void) const { return relocations_; }
    std::size_t relocated_elements(void) const { return relocated_elements_; }
    void reset_stats(void) { grow_calls_ = relocations_ = relocated_elements_ = 0; }

private:
    /* 阶段 1：容量够时的追加 */
    void append_at_end(const T &value);

    /* 阶段 2 与阶段 5：扩容路径的两段 */
    void grow_and_push(const T &value);

    /* 阶段 3：搬迁；built 是出参，每成功构造一个就加一 */
    void relocate(T *fresh, std::size_t n, std::size_t &built);

    /* 阶段 4：扩容策略 */
    std::size_t grow_capacity(void) const;

    /* 已给出：析构 [begin_, end_) 里的元素，并把缓冲区还回去 */
    void release_all(void) {
        clear();
        if (begin_ != nullptr) {
            alloc_.deallocate(begin_, capacity());
        }
        begin_ = end_ = cap_ = nullptr;
    }

    /* 已给出：把另一个数组的元素逐个追加进来（拷贝构造与拷贝赋值用它） */
    void append_all(const DynArray &other) {
        for (std::size_t i = 0; i < other.size(); ++i) {
            push_back(other[i]);
        }
    }

    /* 已给出：接管另一个数组的缓冲区，并把它置空 */
    void take_over(DynArray &other) noexcept {
        begin_ = other.begin_;
        end_ = other.end_;
        cap_ = other.cap_;
        other.begin_ = other.end_ = other.cap_ = nullptr;
    }

    SimpleAllocator<T> alloc_{};
    T *begin_ = nullptr;        /* 元素区起点 */
    T *end_ = nullptr;          /* 元素区终点，也就是 size 的位置 */
    T *cap_ = nullptr;          /* 缓冲区终点，也就是 capacity 的位置 */
    std::size_t grow_calls_ = 0;
    std::size_t relocations_ = 0;
    std::size_t relocated_elements_ = 0;
};

/* ==================================================================
 * 阶段 1-1：容量够时的追加
 * ================================================================== */

/* TODO（阶段 1-1）：把 value 追加到 end_ 指着的那一格上。
 *
 * 调用前提：end_ != cap_，也就是容量还够，不必扩容。
 *
 * 提示：
 *   1. end_ 与 cap_ 之间那段是**未构造的原始内存**，里面没有对象，
 *      因此不能赋值，只能在那里构造一个对象；
 *   2. 构造用放置构造（placement new，声明在 <new> 里），它在指定地址上起一个对象；
 *   3. 构造成功之后再把 end_ 往前挪一格。顺序反了，下一步就会把还没有对象的格子
 *      当成已构造的对象用。
 *
 * 判据（见《配置步骤.md》阶段 1）：容量 4、当时装了 2 个元素的数组追加两次之后，
 *   size 从 2 变成 4，capacity 仍是 4，grow_calls 仍是 0，四个值分别是 1、2、3、4。
 *   占位实现什么也不做，因此 size 会停在 2。 */
template <class T>
void DynArray<T>::append_at_end(const T &value)
{
    (void)value;    /* 占位实现：元素追加不进去 */
}

/* ==================================================================
 * 阶段 2 与阶段 5：扩容路径
 * ================================================================== */

/* TODO（阶段 2-1）：把下面标着「阶段 2-1」的那处占位换成你的实现。
 *
 * 要做的事与顺序：
 *   ① 到这里时，新缓冲区已经备好、旧元素也已经搬完（那是阶段 3 的活）；
 *   ② 把这一次要加的元素放进新缓冲区里已搬元素之后；
 *   ③ 到这一步才可以改 begin_、end_、cap_。改早了，旧数据就找不回来了；
 *   ④ 改完指针再处置旧缓冲区：逐个析构旧元素，然后按**旧的容量**把内存还回去。
 *      释放时给的第二个实参必须等于当初申请时给的那个数。
 *
 * 判据（见《配置步骤.md》阶段 2）：连续 8 次 push_back 之后 size = 8；
 *   占位实现不动指针，size 会停在 0，而 AllocStats 的「分配次数」会一直涨、释放次数为 0。
 *
 * 常见写法错误：把旧缓冲区当成新缓冲区释放（先改指针、再调用 release_all 之类），
 *   那会把正在用的那块内存还回去，后面的 push_back 就会读写已释放的内存。
 *
 * TODO（阶段 5-1）：把下面标着「阶段 5-1」的那处占位换成异常安全的收尾。
 *
 * 异常从哪里来：relocate 搬到一半，某个元素的拷贝构造抛了。
 * 抛出时的状态是「新缓冲区里有几个构造好的元素，旧缓冲区一个都没动」。
 * 要做到强异常安全：异常传出去之后，容器的 size、capacity、内容与调用 push_back
 * 之前完全一样，并且新缓冲区申请的那块内存要还回去。
 *
 * 提示：catch 里做两件事——把新缓冲区里**已经构造好的那几个**析构掉、
 *   把新缓冲区还回去，然后把异常继续往外抛（不要吞掉，也不要换成别的类型）。
 *   清理时不要碰 begin_ 那一侧，旧缓冲区还是容器的家。
 *   已经构造好的个数落在 built 这个计数里：grow_and_push 声明了它，
 *   relocate 每成功构造一个就把它加一，catch 直接读它即可。
 *
 * 判据（见《配置步骤.md》阶段 5）：注入一次失败之后，
 *   「content unchanged = yes」「live before/after」两个数相等、「buffer returned = yes」。 */
template <class T>
void DynArray<T>::grow_and_push(const T &value)
{
    ++grow_calls_;
    const std::size_t n = size();
    const std::size_t fresh_cap = grow_capacity();
    T *fresh = alloc_.allocate(fresh_cap);

    std::size_t built = 0;      /* 新缓冲区里已经构造好了几个；catch 也要读它，因此声明在 try 外面 */
    try {
        ++relocations_;
        relocated_elements_ += n;
        relocate(fresh, n, built);
        new (fresh + n) T(value);
        ++built;

        /* ---- 阶段 2-1：换指针、析构旧元素、还旧缓冲区，写在这里 ---- */
        /* 占位实现：什么都不做，三个指针还指着旧缓冲区，size 因此不变 */

    } catch (...) {
        /* ---- 阶段 5-1：收拾新缓冲区，写在这里 ---- */
        throw;      /* 占位实现：直接往外抛，已经构造的元素与那块内存都留在了原地 */
    }
}

/* ==================================================================
 * 阶段 3-1：搬迁
 * ================================================================== */

/* TODO（阶段 3-1）：把旧缓冲区 [begin_, end_) 里的 n 个元素搬到 fresh 上。
 *
 * 关键点：
 *   1. fresh 是未构造的原始内存，因此要逐个构造，不能赋值；
 *   2. 用移动还是拷贝，取决于元素的移动构造会不会抛：
 *      不抛就用移动（快），可能抛就退回复制（慢，但旧元素保持原样，
 *      第 5 阶段要的强异常安全才有得谈）。std::move_if_noexcept（<utility> 里）
 *      就是替你判断这件事的：移动可能抛时，它给出的是 const 左值引用；
 *      也可以按 std::is_nothrow_move_constructible_v<T> 分成两个分支，效果相同；
 *   3. 本模板给的两个类型正好各占一边：Tracked 的移动带 noexcept，SlowTracked 不带。
 *      搬完之后 TrackedCounters::moves 与 copies 会告诉你各搬了几次；
 *   4. built 是出参，每成功构造出一个元素就把它加一。阶段 5 靠这个数知道
 *      抛出去的时候新缓冲区里有几个元素要收拾，因此不能漏。
 *
 * 判据（见《配置步骤.md》阶段 3）：0、10、20、……、70 这八个数各追加一次之后，
 *   内容与追加顺序一致（占位实现把它们全变成 0）。
 *   用 Tracked 时 moves 随搬迁次数增长；用 SlowTracked 时 moves 为 0、copies 增长。
 *
 * 这个函数不管「搬一半抛了怎么办」，那是阶段 5 的事：这里只管搬。 */
template <class T>
void DynArray<T>::relocate(T *fresh, std::size_t n, std::size_t &built)
{
    for (std::size_t i = 0; i < n; ++i) {
        new (fresh + i) T();    /* 占位实现：默认构造 n 个空对象，元素原来的值全丢 */
        ++built;
    }
}

/* ==================================================================
 * 阶段 4-1：扩容策略
 * ================================================================== */

/* TODO（阶段 4-1）：返回「这一次要申请多少格」。
 *
 * 调用前提：容量已经用完（end_ == cap_），这一次插入必须扩容。
 *
 * 提示：
 *   1. 每次只加一格，n 次插入就要搬走约 n²/2 个元素；成倍增长把这件事摊平到常数。
 *      倍数要在 1.5 与 2 这类值里挑一个（取舍见《09-高阶数据结构/B-01-手写：动态数组.md》
 *      第 2.2 小节：1.5 换内存重用，2 换搬移次数）；
 *   2. 第一次扩容时 capacity() 是 0，别忘了给一个下限（例如至少 1 格），
 *      否则容量永远涨不起来；
 *   3. 定下常数之后，在下面写一行注释说明为什么选它。
 *
 * 判据（见《配置步骤.md》阶段 4）：连续 1000 次 push_back 之后，
 *   grow_calls 落在十几次这个量级，relocated_elements 与它同量级；
 *   占位实现是每次加一格，两个数都会接近 1000。 */
template <class T>
std::size_t DynArray<T>::grow_capacity(void) const
{
    return capacity() + 1;      /* 占位实现：每次只加一格 */
}

} /* namespace dyn */

#endif /* DYNARRAY_HPP */
