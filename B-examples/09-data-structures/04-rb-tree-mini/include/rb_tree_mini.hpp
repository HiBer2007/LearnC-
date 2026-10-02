/**
 * rb_tree_mini.hpp —— 红黑树：插入、删除、不变式校验与带颜色的按层打印
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
 * 要维持的四条不变式（check() 逐条检查）：
 *
 *   1. 每个节点是红或黑；
 *   2. 根是黑的；
 *   3. 红节点的孩子必须是黑的；
 *   4. 从任一节点到它所有空叶子的路径上，黑节点个数相同。
 *
 * 两个实现上的决定：
 *
 *   **空叶子合成一个黑哨兵**（nil_）。所有「没有孩子」的位置都指向它，
 *   它的颜色是黑、左右与父都指向自己。这样叔叔、兄弟这两个角色永远不是空指针，
 *   修复代码里就不必到处判断「兄弟是不是空的」——删除修复少一半分支。
 *
 *   **节点带父指针**。迭代器的 ++ 与 -- 才是常数时间：有右子树就找右子树里最左的，
 *   否则往上走到「我是我父亲的左孩子」为止。代价是每个节点多 8 字节。
 *
 * 面向人的文字一律是 u8"" 字面量，因此本头文件里的 std::string 承载 UTF-8 字节。
 */
#ifndef RB_TREE_MINI_HPP
#define RB_TREE_MINI_HPP

#include <cstddef>
#include <iterator>
#include <string>
#include <vector>

namespace rbmini {

/* ================= 计数与颜色 ================= */

struct RbStats {
    std::size_t comparisons = 0;       /**< 键与键比较的次数 */
    std::size_t rotations = 0;         /**< 旋转次数（左旋加右旋） */
    std::size_t recolors = 0;          /**< 改颜色的次数 */
    std::size_t insert_fix_steps = 0;  /**< 插入修复循环转过的圈数 */
    std::size_t erase_fix_steps = 0;   /**< 删除修复循环转过的圈数 */
    std::size_t successor_steps = 0;   /**< 迭代器 ++ 走过的步数 */
    std::size_t checks = 0;            /**< 校验器跑过的次数 */
    std::size_t failures = 0;          /**< 校验器报出的不变式破坏次数 */

    void reset();
};

RbStats &stats();

enum class Color : unsigned char { Red, Black };

const char *color_name(Color color);

/* ================= 节点 ================= */

namespace detail {

struct RbNode {
    int key = 0;
    Color color = Color::Black;
    RbNode *parent = nullptr;
    RbNode *left = nullptr;
    RbNode *right = nullptr;
};

}   /* namespace detail */

/* ================= 红黑树 ================= */

class RbTree {
public:
    RbTree();
    ~RbTree();

    RbTree(const RbTree &) = delete;
    RbTree &operator=(const RbTree &) = delete;

    bool insert(int key);           /**< 键已经存在时返回 false */
    bool contains(int key) const;
    bool erase(int key);            /**< 键不在树里时返回 false */

    std::size_t size() const noexcept { return size_; }
    bool empty() const noexcept { return size_ == 0; }

    /** 层数：只有根时是 1，空树是 0 */
    int height() const;

    /** 从根到空叶子的黑节点个数（空叶子算一个黑） */
    int black_height() const;

    /** 校验报告。ok 为假时 first_problem 写明是哪一条被破坏了 */
    struct CheckReport {
        bool ok = true;
        std::string first_problem;
        std::size_t nodes = 0;
        std::size_t red_nodes = 0;
        std::size_t black_nodes = 0;
        int height = 0;
        int black_height = 0;
        bool ordered = true;        /**< 中序是否严格递增 */
    };

    CheckReport check() const;

    /** 按层打印，每个节点带上颜色（B=黑 R=红）。位置按中序名次排，因此左子树总在左边 */
    std::string to_levels() const;

    /** 中序序列 */
    std::vector<int> inorder() const;

    /* ---------------- 中序迭代器 ---------------- */

    /** 双向迭代器：++ 是「下一个」，-- 是「上一个」，--end() 是最大的那个键 */
    class Iterator {
    public:
        using iterator_category = std::bidirectional_iterator_tag;
        using value_type = int;
        using difference_type = std::ptrdiff_t;
        using pointer = const int *;
        using reference = const int &;

        Iterator() = default;

        reference operator*() const { return node_->key; }
        pointer operator->() const { return &node_->key; }

        Iterator &operator++();
        Iterator operator++(int)
        {
            Iterator old(*this);
            ++(*this);
            return old;
        }
        Iterator &operator--();
        Iterator operator--(int)
        {
            Iterator old(*this);
            --(*this);
            return old;
        }

        bool operator==(const Iterator &other) const { return node_ == other.node_; }
        bool operator!=(const Iterator &other) const { return node_ != other.node_; }

    private:
        friend class RbTree;
        Iterator(detail::RbNode *node, detail::RbNode *nil, const detail::RbNode *root)
            : node_(node), nil_(nil), root_(root)
        {
        }

        detail::RbNode *node_ = nullptr;
        detail::RbNode *nil_ = nullptr;
        const detail::RbNode *root_ = nullptr;
    };

    Iterator begin() { return Iterator(minimum(root_), nil_, root_); }
    Iterator end() { return Iterator(nil_, nil_, root_); }
    Iterator begin() const { return Iterator(minimum(root_), nil_, root_); }
    Iterator end() const { return Iterator(nil_, nil_, root_); }

private:
    using Node = detail::RbNode;

    Node *find_node(int key) const;
    Node *minimum(Node *subtree) const;
    Node *maximum(Node *subtree) const;

    void rotate_left(Node *x);
    void rotate_right(Node *x);
    void insert_fixup(Node *z);
    void erase_fixup(Node *x);
    void transplant(Node *u, Node *v);

    void set_color(Node *node, Color color);
    static void destroy_subtree(Node *nil, Node *node);
    static int walk_check(const Node *nil, const Node *node, CheckReport &report, int depth,
                          bool &height_known);

    Node *nil_ = nullptr;       /**< 共用的黑哨兵，代表所有空叶子 */
    Node *root_ = nullptr;
    std::size_t size_ = 0;
};

/* ================= 朴素搜索树：只用来做退化对照 ================= */

class PlainBst {
public:
    PlainBst() = default;
    ~PlainBst();
    PlainBst(const PlainBst &) = delete;
    PlainBst &operator=(const PlainBst &) = delete;

    bool insert(int key);
    bool contains(int key) const;
    std::size_t size() const noexcept { return size_; }
    int height() const;

private:
    struct Node {
        int key = 0;
        Node *left = nullptr;
        Node *right = nullptr;
    };

    static void destroy_subtree(Node *node);
    static int height_of(const Node *node);

    Node *root_ = nullptr;
    std::size_t size_ = 0;
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

/** 项目输出：插入、按层打印、删除、随机插删、退化对照、中序迭代器六段。
    返回多行 UTF-8 文本，末尾带一个换行；由界面层决定怎么显示 */
std::string build_report();

/** 逐项核对插入、删除、不变式、迭代器与退化对照 */
CheckResult run_self_tests();

}   /* namespace rbmini */

#endif /* RB_TREE_MINI_HPP */
