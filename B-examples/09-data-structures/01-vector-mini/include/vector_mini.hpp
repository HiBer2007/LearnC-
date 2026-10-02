/**
 * vector_mini.hpp —— 手写动态数组：增长策略、异常安全与搬迁方式
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
 */

/**
 * 接口与不变量：
 *
 *   Vector<T> 有三个指针：data_（元素区起点）、size_（元素个数）、cap_（缓冲区容量）。
 *   三条不变量由 invariants_ok() 检查：
 *     1. size_ <= cap_；
 *     2. cap_ == 0 与 data_ == nullptr 同时成立或同时不成立；
 *     3. [data_, data_ + size_) 里全是活对象，[data_ + size_, data_ + cap_) 是未构造的原始内存。
 *
 *   分配与构造是两件事：缓冲区用 ::operator new 拿（只给内存），
 *   元素用定位 new 在缓冲区上构造。释放时也必须分两步：先逐个析构元素，再还内存。
 *
 *   reserve() 与 shrink_to_fit() 都走同一条搬迁路径 reallocate()，
 *   它保证强异常安全：抛异常时容器状态与调用前逐位一致。
 *
 * 面向人的文字一律是 u8"" 字面量，因此本头文件里的 std::string 承载 UTF-8 字节，
 * 与 -fexec-charset 无关。显示成哪种编码由界面层决定。
 */
#ifndef VECTOR_MINI_HPP
#define VECTOR_MINI_HPP

#include <cstddef>
#include <exception>
#include <new>
#include <string>
#include <utility>
#include <vector>

namespace vmini {

/* ================= 全局分配计数 ================= */

/** 缓冲区层面的计数。只统计 Vector 自己的缓冲区，不含元素内部的分配。
    用全局对象是为了让报告与自测都能读到同一份数，不必每个容器各带一套。 */
struct AllocStats {
    std::size_t allocations = 0;    /**< ::operator new 的次数 */
    std::size_t deallocations = 0;  /**< ::operator delete 的次数 */
    std::size_t live_bytes = 0;     /**< 当前还活着的缓冲区字节数 */
    std::size_t peak_bytes = 0;     /**< live_bytes 的历史最大值 */
    std::size_t reallocations = 0;  /**< 扩容次数（含 shrink 引起的一次） */
    std::size_t relocated = 0;      /**< 累计搬走的元素个数 */

    void reset();
};

AllocStats &stats();

/* ================= 增长策略 ================= */

/** 容量不够时按哪种倍数长。选哪个是工程决定，见 README 的对照表。 */
enum class Growth {
    Double,          /**< 2 倍：搬移次数最少，但内存几乎无法重用 */
    OneAndHalf,      /**< 1.5 倍：释放的块攒起来够放下新块 */
    PlusOne,         /**< 每次加一：反面教材，插入 n 个元素是 O(n²) */
    PlusTenPercent,  /**< 每次加 10%：比加一好，搬移总量仍是约 5n 以上 */
};

const char *growth_name(Growth policy);

/** 从 current 长到「至少 needed」需要的新容量。
    current 为 0 时从 1 起步；小容量下整数除法会让倍数失效，届时退化成加一。 */
std::size_t next_capacity(Growth policy, std::size_t current, std::size_t needed);

/* ================= 搬迁实验用的类型 ================= */

/** 三个测试类型共用的计数器。
    throw_after 大于 0 时，第 throw_after 次拷贝（或可能抛的移动）抛出 test_failure。 */
struct RelocationCounters {
    static std::size_t copies;
    static std::size_t moves;
    static std::size_t throws;
    static int throw_after;

    static void reset();
    static void note_copy();              /**< 计一次拷贝，可能抛 */
    static void note_move();              /**< 计一次可能抛的移动 */
    static void count_move_noexcept() noexcept;  /**< 计一次不抛的移动，只加数 */
};

struct test_failure : std::exception {
    const char *what() const noexcept override { return "vmini::test_failure"; }
};

/** 只有拷贝构造，且不标 noexcept：搬迁时只能拷贝 */
class CopyOnly {
public:
    explicit CopyOnly(int value = 0) : value_(value) {}
    CopyOnly(const CopyOnly &other) : value_(other.value_) { RelocationCounters::note_copy(); }
    ~CopyOnly() = default;
    CopyOnly &operator=(const CopyOnly &) = delete;
    int value() const { return value_; }

private:
    int value_;
};

/** 有移动构造，但**没标 noexcept**：容器会当成「移动可能抛」，退回复制 */
class ThrowingMover {
public:
    explicit ThrowingMover(int value = 0) : value_(value) {}
    ThrowingMover(const ThrowingMover &other) : value_(other.value_) { RelocationCounters::note_copy(); }
    ThrowingMover(ThrowingMover &&other) : value_(other.value_) { RelocationCounters::note_move(); }
    ~ThrowingMover() = default;
    ThrowingMover &operator=(const ThrowingMover &) = delete;
    ThrowingMover &operator=(ThrowingMover &&) = delete;
    int value() const { return value_; }

private:
    int value_;
};

/** 移动构造标了 noexcept：搬迁时走移动，一次拷贝都不做 */
class NoThrowMover {
public:
    explicit NoThrowMover(int value = 0) : value_(value) {}
    NoThrowMover(const NoThrowMover &other) : value_(other.value_) { RelocationCounters::note_copy(); }
    NoThrowMover(NoThrowMover &&other) noexcept : value_(other.value_)
    {
        RelocationCounters::count_move_noexcept();
    }
    ~NoThrowMover() = default;
    NoThrowMover &operator=(const NoThrowMover &) = delete;
    NoThrowMover &operator=(NoThrowMover &&) = delete;
    int value() const { return value_; }

private:
    int value_;
};

/* ================= 动态数组 ================= */

template <class T>
class Vector {
public:
    using value_type = T;
    using iterator = T *;
    using const_iterator = const T *;

    explicit Vector(Growth policy = Growth::Double) noexcept : growth_(policy) {}
    ~Vector() { release_all(); }

    Vector(const Vector &other) : growth_(other.growth_) { copy_from(other); }

    Vector(Vector &&other) noexcept
        : data_(other.data_), size_(other.size_), cap_(other.cap_), growth_(other.growth_),
          reallocations_(other.reallocations_), relocated_(other.relocated_)
    {
        other.data_ = nullptr;
        other.size_ = 0;
        other.cap_ = 0;
    }

    Vector &operator=(const Vector &other)
    {
        if (this != &other) {
            Vector tmp(other);      /* 先复制成功，再换过来：赋值也是强异常安全 */
            swap(tmp);
        }
        return *this;
    }

    Vector &operator=(Vector &&other) noexcept
    {
        if (this != &other) {
            release_all();
            data_ = other.data_;
            size_ = other.size_;
            cap_ = other.cap_;
            growth_ = other.growth_;
            reallocations_ = other.reallocations_;
            relocated_ = other.relocated_;
            other.data_ = nullptr;
            other.size_ = 0;
            other.cap_ = 0;
        }
        return *this;
    }

    void swap(Vector &other) noexcept
    {
        std::swap(data_, other.data_);
        std::swap(size_, other.size_);
        std::swap(cap_, other.cap_);
        std::swap(growth_, other.growth_);
        std::swap(reallocations_, other.reallocations_);
        std::swap(relocated_, other.relocated_);
    }

    /* ---------------- 观察 ---------------- */

    std::size_t size() const noexcept { return size_; }
    std::size_t capacity() const noexcept { return cap_; }
    bool empty() const noexcept { return size_ == 0; }

    T *data() noexcept { return data_; }
    const T *data() const noexcept { return data_; }

    Growth growth() const noexcept { return growth_; }
    std::size_t reallocation_count() const noexcept { return reallocations_; }
    std::size_t relocated_count() const noexcept { return relocated_; }

    iterator begin() noexcept { return data_; }
    iterator end() noexcept { return size_ == 0 ? data_ : data_ + size_; }
    const_iterator begin() const noexcept { return data_; }
    const_iterator end() const noexcept { return size_ == 0 ? data_ : data_ + size_; }
    const_iterator cbegin() const noexcept { return data_; }
    const_iterator cend() const noexcept { return size_ == 0 ? data_ : data_ + size_; }

    T &operator[](std::size_t index) noexcept { return data_[index]; }
    const T &operator[](std::size_t index) const noexcept { return data_[index]; }
    T &front() noexcept { return data_[0]; }
    T &back() noexcept { return data_[size_ - 1]; }
    const T &back() const noexcept { return data_[size_ - 1]; }

    /** 三条不变式是否仍然成立 */
    bool invariants_ok() const noexcept
    {
        if (size_ > cap_) {
            return false;
        }
        if ((cap_ == 0) != (data_ == nullptr)) {
            return false;
        }
        return cap_ == 0 || data_ != nullptr;
    }

    /* ---------------- 改变元素个数 ---------------- */

    /** 只增不减：n 不大于当前容量时什么都不做 */
    void reserve(std::size_t n)
    {
        if (n > cap_) {
            reallocate(n);
        }
    }

    /** 请求语义：把容量降到 size()。容量已经是 size() 时什么都不做 */
    void shrink_to_fit()
    {
        if (size_ == cap_) {
            return;
        }
        if (size_ == 0) {
            release_buffer();
            return;
        }
        reallocate(size_);
    }

    /** 元素个数归零，**不释放缓冲区** */
    void clear() noexcept
    {
        for (std::size_t i = 0; i < size_; ++i) {
            data_[i].~T();
        }
        size_ = 0;
    }

    /** 扩大时多出来的元素默认构造；缩小时把尾部析构掉 */
    void resize(std::size_t n)
    {
        if (n < size_) {
            for (std::size_t i = n; i < size_; ++i) {
                data_[i].~T();
            }
            size_ = n;
            return;
        }
        if (n > size_) {
            reserve(n);
            std::size_t built = size_;
            try {
                for (; built < n; ++built) {
                    new (data_ + built) T();
                }
            } catch (...) {
                for (std::size_t i = size_; i < built; ++i) {
                    data_[i].~T();
                }
                throw;      /* size_ 没动过，容器与调用前一致 */
            }
            size_ = n;
        }
    }

    T &push_back(const T &value) { return emplace_back(value); }
    T &push_back(T &&value) { return emplace_back(std::move(value)); }

    /** 就地构造。容量不够时先扩容——扩容抛了则容器一个字节都没变 */
    template <class... Args>
    T &emplace_back(Args &&...args)
    {
        if (size_ == cap_) {
            reallocate(next_capacity(growth_, cap_, size_ + 1));
        }
        new (data_ + size_) T(std::forward<Args>(args)...);   /* 这一步抛则 size_ 不动 */
        ++size_;
        return data_[size_ - 1];
    }

    void pop_back() noexcept
    {
        --size_;
        data_[size_].~T();
    }

private:
    /* ---------------- 搬迁：整个类里唯一会抛的地方 ---------------- */

    /** 换一块 new_cap 大小的缓冲区，把元素搬过去。
        抛异常时：新缓冲区里已构造的元素析构掉、内存还回去，自己的三个指针一个都不动。 */
    void reallocate(std::size_t new_cap)
    {
        T *fresh = static_cast<T *>(::operator new(new_cap * sizeof(T)));
        note_alloc(new_cap * sizeof(T));

        std::size_t built = 0;
        try {
            for (; built < size_; ++built) {
                /* 移动构造不抛才敢移动：移动搬到一半抛异常时旧元素已经被搬空，
                   强保证就做不到了。move_if_noexcept 正是按这个条件选的。 */
                new (fresh + built) T(std::move_if_noexcept(data_[built]));
            }
        } catch (...) {
            for (std::size_t i = 0; i < built; ++i) {
                fresh[i].~T();
            }
            ::operator delete(fresh);
            note_free(new_cap * sizeof(T));
            throw;      /* 容器状态与调用前逐位一致 */
        }

        /* 全部搬完，到这里才动自己的指针 */
        for (std::size_t i = 0; i < size_; ++i) {
            data_[i].~T();
        }
        if (data_ != nullptr) {
            ::operator delete(data_);
            note_free(cap_ * sizeof(T));
        }
        data_ = fresh;
        cap_ = new_cap;
        ++reallocations_;
        relocated_ += size_;
        ++stats().reallocations;
        stats().relocated += size_;
        /* 这两个 ++ 不会抛：走到这里搬迁已经成功 */
    }

    void copy_from(const Vector &other)
    {
        if (other.size_ == 0) {
            return;
        }
        data_ = static_cast<T *>(::operator new(other.size_ * sizeof(T)));
        note_alloc(other.size_ * sizeof(T));
        cap_ = other.size_;
        std::size_t built = 0;
        try {
            for (; built < other.size_; ++built) {
                new (data_ + built) T(other.data_[built]);
            }
        } catch (...) {
            for (std::size_t i = 0; i < built; ++i) {
                data_[i].~T();
            }
            ::operator delete(data_);
            note_free(cap_ * sizeof(T));
            data_ = nullptr;
            cap_ = 0;
            throw;
        }
        size_ = other.size_;
    }

    void release_buffer() noexcept
    {
        if (data_ != nullptr) {
            ::operator delete(data_);
            note_free(cap_ * sizeof(T));
            data_ = nullptr;
        }
        cap_ = 0;
    }

    void release_all() noexcept
    {
        for (std::size_t i = 0; i < size_; ++i) {
            data_[i].~T();
        }
        size_ = 0;
        release_buffer();
    }

    static void note_alloc(std::size_t bytes) noexcept
    {
        ++stats().allocations;
        stats().live_bytes += bytes;
        if (stats().live_bytes > stats().peak_bytes) {
            stats().peak_bytes = stats().live_bytes;
        }
    }

    static void note_free(std::size_t bytes) noexcept
    {
        ++stats().deallocations;
        stats().live_bytes -= bytes;
    }

    T *data_ = nullptr;         /* 元素区起点 */
    std::size_t size_ = 0;      /* 元素个数；元素区终点是 data_ + size_ */
    std::size_t cap_ = 0;       /* 缓冲区容量；缓冲区终点是 data_ + cap_ */
    Growth growth_ = Growth::Double;
    std::size_t reallocations_ = 0;
    std::size_t relocated_ = 0;
};

/* ================= 报告与自测 ================= */

/** 自测结果。不打印，交给界面决定怎么显示 */
struct CheckResult {
    std::size_t total = 0;
    std::size_t passed = 0;
    std::size_t failed = 0;
    std::vector<std::string> lines;     /**< 每项一行，例如 "[通过] 3. ……" */

    bool all_passed() const { return failed == 0; }
    std::string summary() const;        /**< "16 项中 16 项通过，全部通过" */
};

/** 项目输出：增长策略、异常安全、搬迁方式、迭代器失效四段。
    返回多行 UTF-8 文本，末尾带一个换行；由界面层决定怎么显示 */
std::string build_report();

/** 逐项核对增长、扩容、异常安全、搬迁方式与元素生命周期 */
CheckResult run_self_tests();

}   /* namespace vmini */

#endif /* VECTOR_MINI_HPP */
