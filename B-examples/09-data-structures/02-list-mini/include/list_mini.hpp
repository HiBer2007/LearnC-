/**
 * list_mini.hpp —— 带哨兵的双向链表：迭代器、splice 与节点计数
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
 * 接口与约定：
 *
 *   节点分两层：NodeBase 只有两个链接指针，Node<T> 在它后面接上元素。
 *   哨兵是一个内嵌的 NodeBase 成员，**不是 Node<T>**，因此它一个元素都不构造，
 *   也不占一次堆分配。空链时哨兵的前后指针都指向自己。
 *
 *   迭代器解引用时把 NodeBase* 转成 Node<T>*，因此 begin() 是哨兵的 next，
 *   end() 是哨兵自己，--end() 就是最后一个元素。
 *
 *   插入与删除只改指针、不搬元素，因此别的迭代器都不会失效。
 *   splice 走同一条改指针的路：把一段节点从一条链摘下来接到另一处，
 *   一个元素都不构造、也不析构，两边的 size 各自调整。
 *
 * 面向人的文字一律是 u8"" 字面量，因此本头文件里的 std::string 承载 UTF-8 字节。
 */
#ifndef LIST_MINI_HPP
#define LIST_MINI_HPP

#include <cstddef>
#include <iterator>
#include <new>
#include <string>
#include <type_traits>
#include <vector>

namespace lmini {

/* ================= 计数 ================= */

/** 全局计数。报告与自测都按「先 reset、再做一件事、读增量」的方式用。
    哨兵是内嵌成员，因此创建一条空链不会让这里的任何一个数变化。 */
struct LinkStats {
    std::size_t node_creations = 0;      /**< 分配节点的次数 */
    std::size_t node_destructions = 0;   /**< 归还节点的次数 */
    std::size_t value_constructions = 0; /**< 构造元素的次数 */
    std::size_t value_destructions = 0;  /**< 析构元素的次数 */
    std::size_t link_writes = 0;         /**< 给 prev 或 next 赋值的次数 */
    std::size_t count_steps = 0;         /**< 为了数元素个数走过的步数 */
    std::size_t link_in_calls = 0;       /**< 内部插入路径被调用的次数 */
    std::size_t unlink_calls = 0;        /**< 内部删除路径被调用的次数 */

    void reset();
    std::size_t live_nodes() const { return node_creations - node_destructions; }
};

LinkStats &stats();

/* ================= 节点 ================= */

/** 只有链接的节点。哨兵就是这个类型，因此哨兵不构造元素 */
struct NodeBase {
    NodeBase *prev = nullptr;
    NodeBase *next = nullptr;
};

/** 带元素的节点。元素在原始内存上用定位 new 构造，与链接分开管理 */
template <class T>
struct Node : NodeBase {
    T value;
};

/* ================= 迭代器 ================= */

/** 双向迭代器。五个类型别名齐备，因此标准算法与范围 for 都能用。
    Ref 与 Ptr 决定它是 iterator 还是 const_iterator。 */
template <class T, class Ref, class Ptr>
class ListIterator {
public:
    using iterator_category = std::bidirectional_iterator_tag;
    using value_type = T;
    using difference_type = std::ptrdiff_t;
    using pointer = Ptr;
    using reference = Ref;

    ListIterator() = default;
    explicit ListIterator(NodeBase *node) : node_(node) {}

    /** iterator 转 const_iterator；反向转换会编译失败 */
    template <class OtherRef, class OtherPtr,
              class = typename std::enable_if<std::is_const<Ref>::value
                                              && !std::is_const<OtherRef>::value>::type>
    ListIterator(const ListIterator<T, OtherRef, OtherPtr> &other) : node_(other.node_)
    {
    }

    Ref operator*() const { return static_cast<Node<T> *>(node_)->value; }
    Ptr operator->() const { return &static_cast<Node<T> *>(node_)->value; }

    ListIterator &operator++()
    {
        node_ = node_->next;
        return *this;
    }
    ListIterator operator++(int)
    {
        ListIterator old(*this);
        node_ = node_->next;
        return old;
    }
    ListIterator &operator--()
    {
        node_ = node_->prev;
        return *this;
    }
    ListIterator operator--(int)
    {
        ListIterator old(*this);
        node_ = node_->prev;
        return old;
    }

    bool operator==(const ListIterator &other) const { return node_ == other.node_; }
    bool operator!=(const ListIterator &other) const { return node_ != other.node_; }

    /** 节点地址。演示迭代器稳定性时用它做对照，报告里只打印「变了没有」 */
    const NodeBase *node_address() const { return node_; }

private:
    NodeBase *node_ = nullptr;

    template <class U, class R, class P>
    friend class ListIterator;
    template <class U>
    friend class List;
};

/* ================= 链表 ================= */

template <class T>
class List {
public:
    using value_type = T;
    using iterator = ListIterator<T, T &, T *>;
    using const_iterator = ListIterator<T, const T &, const T *>;
    using size_type = std::size_t;

    List() noexcept { init_sentinel(); }
    ~List() { clear(); }

    List(const List &other)
    {
        init_sentinel();
        copy_from(other);
    }

    /** 移动构造 = 把别的链整条接过来：一个元素都不构造、也不析构 */
    List(List &&other) noexcept
    {
        init_sentinel();
        adopt(other);
    }

    List &operator=(const List &other)
    {
        if (this != &other) {
            List tmp(other);
            swap(tmp);
        }
        return *this;
    }

    List &operator=(List &&other) noexcept
    {
        if (this != &other) {
            clear();
            adopt(other);
        }
        return *this;
    }

    /** 两条链的哨兵是各自的成员，换不了，所以换的是「挂在哨兵上的那一段节点」 */
    void swap(List &other) noexcept
    {
        if (this == &other) {
            return;
        }
        const size_type mine = size_;
        const size_type theirs = other.size_;
        NodeBase *my_first = sentinel_.next;
        NodeBase *their_first = other.sentinel_.next;

        if (mine > 0 && theirs > 0) {
            transfer(my_first, &sentinel_, &other.sentinel_);   /* 我的接到对方后面 */
            transfer(their_first, my_first, &sentinel_);        /* 对方原来的接回我这里 */
        } else if (mine > 0) {
            transfer(my_first, &sentinel_, &other.sentinel_);
        } else if (theirs > 0) {
            transfer(their_first, &other.sentinel_, &sentinel_);
        }
        size_ = theirs;
        other.size_ = mine;
    }

    /* ---------------- 观察 ---------------- */

    size_type size() const noexcept { return size_; }
    bool empty() const noexcept { return size_ == 0; }

    iterator begin() noexcept { return iterator(sentinel_.next); }
    iterator end() noexcept { return iterator(&sentinel_); }
    const_iterator begin() const noexcept { return const_iterator(sentinel_.next); }
    const_iterator end() const noexcept { return const_iterator(sentinel_node()); }
    const_iterator cbegin() const noexcept { return const_iterator(sentinel_.next); }
    const_iterator cend() const noexcept { return const_iterator(sentinel_node()); }

    T &front() noexcept { return static_cast<Node<T> *>(sentinel_.next)->value; }
    const T &front() const noexcept { return static_cast<const Node<T> *>(sentinel_.next)->value; }
    T &back() noexcept { return static_cast<Node<T> *>(sentinel_.prev)->value; }
    const T &back() const noexcept { return static_cast<const Node<T> *>(sentinel_.prev)->value; }

    /* ---------------- 插入与删除 ---------------- */

    /** 在 pos 之前插入，返回指向新元素的迭代器 */
    iterator insert(iterator pos, const T &value)
    {
        Node<T> *fresh = create_node(value);
        link_in(pos.node_, fresh);
        ++size_;
        return iterator(fresh);
    }

    /** 删掉 pos 指向的元素，返回它后面那个位置 */
    iterator erase(iterator pos)
    {
        NodeBase *target = pos.node_;
        NodeBase *next = target->next;
        unlink(target);
        destroy_node(static_cast<Node<T> *>(target));
        --size_;
        return iterator(next);
    }

    void push_back(const T &value) { insert(end(), value); }
    void push_front(const T &value) { insert(begin(), value); }
    void pop_back() { erase(iterator(sentinel_.prev)); }
    void pop_front() { erase(begin()); }

    /** 元素个数归零，节点全部归还 */
    void clear() noexcept
    {
        NodeBase *p = sentinel_.next;
        while (p != &sentinel_) {
            NodeBase *next = p->next;      /* 先记住下一个，再删当前 */
            unlink(p);
            destroy_node(static_cast<Node<T> *>(p));
            p = next;
        }
        size_ = 0;
    }

    /* ---------------- splice ---------------- */

    /** 把 other 整条链接到 pos 之前。元素个数两边都是现成的，因此一步都不用数 */
    void splice(iterator pos, List &other)
    {
        if (this == &other || other.size_ == 0) {
            return;
        }
        const size_type moved = other.size_;
        transfer(other.sentinel_.next, &other.sentinel_, pos.node_);
        size_ += moved;
        other.size_ = 0;
    }

    /** 把一个元素接到 pos 之前 */
    void splice(iterator pos, List &other, iterator it)
    {
        if (pos.node_ == it.node_ || pos.node_ == it.node_->next) {
            return;     /* 已经在目标位置 */
        }
        transfer(it.node_, it.node_->next, pos.node_);
        ++size_;
        --other.size_;
    }

    /** 把 [first, last) 接到 pos 之前。
        指针只改 6 处；元素个数要维护，因此这一步还要走一段数个数——见报告里的两列 */
    void splice(iterator pos, List &other, iterator first, iterator last)
    {
        if (first == last) {
            return;
        }
        if (this == &other && range_contains(first, last, pos.node_)) {
            return;     /* 标准把这种情况定为未定义行为，这里选择不动 */
        }
        const size_type moved = count_range(first.node_, last.node_);
        transfer(first.node_, last.node_, pos.node_);
        size_ += moved;
        other.size_ -= moved;
    }

private:
    /* ---------------- 链接的原子操作 ---------------- */

    static void set_prev(NodeBase *node, NodeBase *target) noexcept
    {
        node->prev = target;
        ++stats().link_writes;
    }

    static void set_next(NodeBase *node, NodeBase *target) noexcept
    {
        node->next = target;
        ++stats().link_writes;
    }

    /** 私有插入路径：所有插入都走这里，空链、头部、尾部不再各写一遍。
        四处指针：新节点的两个，加上前驱与后继各一处 */
    static void link_in(NodeBase *pos, NodeBase *node) noexcept
    {
        ++stats().link_in_calls;
        NodeBase *before = pos->prev;
        set_prev(node, before);
        set_next(node, pos);
        set_next(before, node);
        set_prev(pos, node);
    }

    /** 私有删除路径：两处指针，前驱与后继互指 */
    static void unlink(NodeBase *node) noexcept
    {
        ++stats().unlink_calls;
        set_next(node->prev, node->next);
        set_prev(node->next, node->prev);
    }

    /** 把 [first, last) 从原位置摘下来再接到 pos 之前，共六处指针，与区间长度无关。
        last 可以是某条链的哨兵，那样搬的就是从 first 到最后一个元素。 */
    static void transfer(NodeBase *first, NodeBase *last, NodeBase *pos) noexcept
    {
        if (first == last || pos == last) {
            return;
        }
        NodeBase *before_first = first->prev;   /* 源链上 first 的前驱 */
        NodeBase *before_pos = pos->prev;       /* 目标位置上 pos 的前驱 */
        NodeBase *tail = last->prev;            /* 区间里最后一个节点 */

        set_next(before_first, last);           /* ① 源链的前驱跳过整段 */
        set_prev(last, before_first);           /* ② 区间的后继指回源链前驱 */
        set_next(before_pos, first);            /* ③ 目标前驱指向区间的头 */
        set_prev(first, before_pos);            /* ④ 区间的头指回目标前驱 */
        set_next(tail, pos);                    /* ⑤ 区间的尾指向目标位置 */
        set_prev(pos, tail);                    /* ⑥ 目标位置指回区间的尾 */
    }

    static bool range_contains(iterator first, iterator last, NodeBase *target) noexcept
    {
        for (NodeBase *p = first.node_; p != last.node_; p = p->next) {
            if (p == target) {
                return true;
            }
        }
        return false;
    }

    static size_type count_range(NodeBase *first, NodeBase *last) noexcept
    {
        size_type count = 0;
        for (NodeBase *p = first; p != last; p = p->next) {
            ++count;
            ++stats().count_steps;
        }
        return count;
    }

    /* ---------------- 节点的生死 ---------------- */

    static Node<T> *create_node(const T &value)
    {
        Node<T> *node = static_cast<Node<T> *>(::operator new(sizeof(Node<T>)));
        ++stats().node_creations;
        try {
            new (&node->value) T(value);       /* 先构造元素，成功之后才接进链 */
            ++stats().value_constructions;
        } catch (...) {
            ::operator delete(node);
            ++stats().node_destructions;
            throw;
        }
        node->prev = nullptr;
        node->next = nullptr;
        return node;
    }

    static void destroy_node(Node<T> *node) noexcept
    {
        node->value.~T();
        ++stats().value_destructions;
        ::operator delete(node);
        ++stats().node_destructions;
    }

    /* ---------------- 生命周期 ---------------- */

    void init_sentinel() noexcept
    {
        sentinel_.prev = &sentinel_;
        sentinel_.next = &sentinel_;
        size_ = 0;
    }

    /** 常量成员函数里也要能把哨兵交给迭代器：哨兵本身不会被改动 */
    NodeBase *sentinel_node() const noexcept { return const_cast<NodeBase *>(&sentinel_); }

    /** 把 other 的元素整条接过来：六处指针，元素一个都不动 */
    void adopt(List &other) noexcept
    {
        if (other.size_ == 0) {
            return;
        }
        const size_type moved = other.size_;
        transfer(other.sentinel_.next, &other.sentinel_, &sentinel_);
        size_ += moved;
        other.size_ = 0;
    }

    void copy_from(const List &other)
    {
        for (const T &value : other) {
            push_back(value);
        }
    }

    NodeBase sentinel_;
    size_type size_ = 0;
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

/** 项目输出：哨兵、范围 for、splice、迭代器稳定性、边界情形五段。
    返回多行 UTF-8 文本，末尾带一个换行；由界面层决定怎么显示 */
std::string build_report();

/** 逐项核对插入删除、splice、迭代器稳定性与元素生命周期 */
CheckResult run_self_tests();

}   /* namespace lmini */

#endif /* LIST_MINI_HPP */
