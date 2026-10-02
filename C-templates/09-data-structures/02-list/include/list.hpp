/* list.hpp —— 练习模板 02 的核心接口（C++）
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
 * 带哨兵的双向链表，分成 4 个阶段。题目、验收标准与自查方法见同目录《配置步骤.md》。
 *
 *     阶段 1  insert()   把新节点接进链：改哪几个指针
 *     阶段 2  erase()    把节点摘下来：摘之前记住什么
 *     阶段 3  clear()、~List()   析构顺序：先记住下一个，再删当前
 *     阶段 4  splice()   把另一条链整条摘过来接到 pos 之前
 *
 * 类模板的成员函数要写在使用点可见的地方，因此这个文件既有声明也有实现。
 * 计数器的定义在 src/list_stats.cpp 里，那个文件不需要改。
 *
 * 关于哨兵：本模板的哨兵节点里也放了一个 T，因此要求 T 可以默认构造。
 * 标准库把「链接」与「值」拆成两层来省掉这次构造，见
 * 《09-高阶数据结构/B-02-手写：链表.md》第 2 节。
 */
#ifndef LIST_HPP
#define LIST_HPP

#include <cstddef>
#include <new>
#include <utility>

#include "list_stats.hpp"

namespace dsl {

template <class T>
class List {
private:
    struct Node {
        Node *prev = nullptr;
        Node *next = nullptr;
        T value;

        explicit Node(const T &v) : value(v) {}
    };

public:
    /* 双向迭代器：只包一个节点指针。
     * 链表没有「扩容」这回事，因此除了被删掉的那个节点，别的迭代器都不会失效。 */
    class iterator {
    public:
        iterator(void) = default;

        T &operator*(void) const { return node_->value; }
        T *operator->(void) const { return &node_->value; }

        iterator &operator++(void) {
            node_ = node_->next;
            return *this;
        }

        iterator &operator--(void) {
            node_ = node_->prev;
            return *this;
        }

        bool operator==(const iterator &other) const { return node_ == other.node_; }
        bool operator!=(const iterator &other) const { return node_ != other.node_; }

    private:
        friend class List;

        explicit iterator(Node *n) : node_(n) {}

        Node *node_ = nullptr;
    };

    /* 建一个哨兵，让它自环：空链就是「哨兵的 next 指向自己」 */
    List(void) : sentinel_(create_node(T())) {
        sentinel_->next = sentinel_;
        sentinel_->prev = sentinel_;
    }

    ~List(void);                                    /* 阶段 3-2 */

    /* 本模板只做「节点搬家」这一类操作，不做深拷贝 */
    List(const List &) = delete;
    List &operator=(const List &) = delete;

    iterator begin(void) { return iterator(sentinel_->next); }
    iterator end(void) { return iterator(sentinel_); }

    std::size_t size(void) const { return size_; }
    bool empty(void) const { return size_ == 0; }

    iterator insert(iterator pos, const T &value);  /* 阶段 1-1 */
    iterator erase(iterator pos);                   /* 阶段 2-1 */
    void clear(void);                               /* 阶段 3-1 */
    void splice(iterator pos, List &other);         /* 阶段 4-1 */

    /* 已给出：尾插就是「在 end() 之前插入」 */
    void push_back(const T &value) { insert(end(), value); }

    /* 已给出：检查哨兵、双向链接与 size() 是否自洽。
     * 返回 nullptr 表示一切正常，否则返回一句说明。
     * 每次改动链之后都可以调用它——它比盯着指针猜要快。 */
    const char *check(void) const {
        const Node *prev = sentinel_;
        const Node *p = sentinel_->next;
        std::size_t counted = 0;

        while (p != sentinel_) {
            if (counted > 1000000U) {
                return "chain longer than a million nodes: it is probably a cycle";
            }
            if (p->prev != prev) {
                return "node->prev does not match the node before it";
            }
            prev = p;
            p = p->next;
            ++counted;
        }
        if (sentinel_->prev != prev) {
            return "sentinel->prev does not match the last node";
        }
        if (counted != size_) {
            return "size() differs from the number of nodes on the chain";
        }
        return nullptr;
    }

private:
    /* 已给出：建立与销毁节点，计数都在这里发生 */
    Node *create_node(const T &value) {
        void *raw = ::operator new(sizeof(Node));
        ++ListStats::nodes_created;
        ++ListStats::values_constructed;
        return new (raw) Node(value);
    }

    void destroy_node(Node *node) {
        ++ListStats::nodes_destroyed;
        ++ListStats::values_destroyed;
        node->~Node();
        ::operator delete(static_cast<void *>(node));
    }

    Node *sentinel_ = nullptr;      /* 不存数据，串在链的两端 */
    std::size_t size_ = 0;
};

/* ==================================================================
 * 阶段 1-1：插入
 * ================================================================== */

/* TODO（阶段 1-1）：把新节点接进 pos 之前，并返回指向新节点的迭代器。
 *
 * 提示：
 *   1. pos 指向的那个节点是 cur，cur->prev 是它的前驱；
 *   2. 新节点自己的两个指针先接好，再让前驱指向新节点、让 cur 指回新节点。
 *      顺序反了会在中途出现「别人的指针指着还没有接好的节点」；
 *   3. 节点要先建好再改指针：建立节点可能抛异常（拷贝构造抛），
 *      先改指针再建节点，抛出去时链就断成两截了；
 *   4. size_ 要跟着加一。插入点之前的迭代器仍然有效，插入点之后的也没失效
 *      （链表只改指针，不搬元素），这一点与动态数组相反。
 *
 * 判据（见《配置步骤.md》阶段 1）：从头尾各插入、再在中间插入，
 *   check() 返回 nullptr，内容与插入顺序一致，节点数 = 元素个数 + 1（哨兵）。 */
template <class T>
typename List<T>::iterator List<T>::insert(iterator pos, const T &value)
{
    Node *fresh = create_node(value);
    ++size_;
    (void)pos;
    /* 占位实现：节点建好了、计数也加了，只是没有接进链里。
     * check() 会报「size() 与链上的节点数不一致」，这一处正是留给你的。 */
    return iterator(fresh);
}

/* ==================================================================
 * 阶段 2-1：删除
 * ================================================================== */

/* TODO（阶段 2-1）：把 pos 指向的节点从链上摘下来，销毁它，并返回它的后继。
 *
 * 提示：
 *   1. 先把它从链上摘掉：让前驱的 next 指向后继、让后继的 prev 指向前驱；
 *   2. 摘下来之后再销毁节点：销毁会释放内存，先去读它的 next 就是读已释放的内存；
 *   3. size_ 减一。
 *
 * 判据（见《配置步骤.md》阶段 2）：删掉头、中间、尾各一个之后，
 *   check() 返回 nullptr，剩下的内容与顺序正确，销毁的节点数与元素数对得上。 */
template <class T>
typename List<T>::iterator List<T>::erase(iterator pos)
{
    Node *node = pos.node_;
    Node *next = node->next;
    --size_;
    /* 占位实现：计数减了，但节点还挂在链上，也没有销毁。
     * check() 会报「size() 与链上的节点数不一致」。 */
    return iterator(next);
}

/* ==================================================================
 * 阶段 3-1 与 3-2：清空与析构
 * ================================================================== */

/* TODO（阶段 3-1）：清空所有元素，但哨兵留着（链回到「哨兵自环」的状态）。
 *
 * 提示：顺着链走，**每处理一个节点之前先记住它的下一个**——
 *   销毁之后那个节点的内存就还回去了，再去读它的 next 是读已释放的内存。
 *
 * TODO（阶段 3-2）：析构函数。把 clear() 做一遍，然后连哨兵一起销毁。
 *
 * 判据（见《配置步骤.md》阶段 3）：clear() 之后 size() 是 0、check() 正常、
 *   节点数回到 1（只剩哨兵）；list 对象离开作用域之后，未销毁的节点数是 0
 *   （哨兵漏掉的话这个数就是 1）。 */
template <class T>
void List<T>::clear(void)
{
    /* 占位实现：什么都不做，元素还挂在链上 */
}

template <class T>
List<T>::~List(void)
{
    /* 占位实现：什么都不做，链上的节点与哨兵都漏掉了 */
}

/* ==================================================================
 * 阶段 4-1：整条链的转移
 * ================================================================== */

/* TODO（阶段 4-1）：把 other 的**整条链**摘下来，接到 pos 之前。
 *
 * 这件事的特别之处：元素一个也不重新构造、一个也不析构，节点地址不变，
 * 因此指向它们的迭代器与引用仍然有效，只是换了所属的容器。这就是常数时间的来路
 * （见《09-高阶数据结构/B-02-手写：链表.md》第 5 节）。
 *
 * 提示：
 *   1. 要改的指针只有四个：pos 的前驱指到 other 的第一个节点、other 的第一个节点
 *      指回 pos 的前驱、other 的最后一个节点指到 pos、pos 指回 other 的最后一个节点；
 *   2. other 的哨兵要变回自环（它现在没有任何节点了）；
 *   3. **两个容器的 size 都要改**：一个加、一个归零；
 *   4. other 是空链时什么也不做；`&other == this` 时也不做（自己搬给自己不是转移）。
 *
 * 判据（见《配置步骤.md》阶段 4）：dst 的 size 变成两边之和、src 变成 0、
 *   内容正确，**第一个节点的地址与转移前相同**，
 *   并且这次调用里「构造过的元素数」与「析构过的元素数」都没有增加。 */
template <class T>
void List<T>::splice(iterator pos, List &other)
{
    (void)pos;
    (void)other;
    /* 占位实现：什么都不做，两条链各自原样不动 */
}

} /* namespace dsl */

#endif /* LIST_HPP */
