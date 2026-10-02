/* rb_tree.hpp —— 练习模板 04 的核心接口（C++）
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
 * 红黑树，只做插入（删除用「标记删除」顶着，这是《09-高阶数据结构/B-04-手写：平衡树.md》
 * 第 4.3 小节给的顺序）。分成 4 个阶段：
 *
 *     阶段 1  check_subtree()     四条不变式的校验器
 *     阶段 2  fix_after_insert()  情形 3：叔叔是黑的，当前节点是外侧孩子
 *     阶段 3  fix_after_insert()  情形 2：叔叔是黑的，当前节点是内侧孩子
 *     阶段 4  fix_after_insert()  情形 1：叔叔是红的（双红往上推）
 *
 * 这个文件同时是声明与实现：类模板的成员函数要写在使用点可见的地方。
 * 旋转、搜索树插入、打印、三个测试工具都已经给出，不需要改。
 */
#ifndef RB_TREE_HPP
#define RB_TREE_HPP

#include <cstddef>
#include <ostream>
#include <string>

namespace dsr {

template <class K, class V>
class RBTree {
public:
    enum class Color : unsigned char { Red, Black };

    struct Node {
        K key{};
        V value{};
        Color color = Color::Red;       /* 新节点一定是红的 */
        Node *parent = nullptr;
        Node *left = nullptr;
        Node *right = nullptr;
    };

    RBTree(void) = default;
    ~RBTree(void) { destroy(root_); }

    /* 本模板不做拷贝：树的所有权在节点指针上 */
    RBTree(const RBTree &) = delete;
    RBTree &operator=(const RBTree &) = delete;

    std::size_t size(void) const { return size_; }
    bool empty(void) const { return size_ == 0; }

    /* 已给出：按搜索树往下找 */
    const V *find(const K &key) const {
        const Node *n = find_node(key);
        return n == nullptr ? nullptr : &n->value;
    }

    /* 已给出：搜索树插入 + 修复。修复是阶段 2 到阶段 4 的内容 */
    void insert(const K &key, const V &value) {
        Node *fresh = insert_raw(key, value);
        fix_after_insert(fresh);
    }

    /* 已给出：跑一遍校验器。返回 nullptr 表示四条不变式都成立，
     * 否则返回一句说明，指出**是哪一条**被违反了。
     * 根是不是黑的在这里查（它要先拿到根），其余三条在 check_subtree 里查。 */
    const char *check(void) const {
        black_height_ = 0;
        if (root_ == nullptr) {
            return nullptr;
        }
        if (root_->color != Color::Black) {
            return "the root is not black";
        }
        return check_subtree(root_, black_height_);
    }

    /* 已给出：最近一次 check() 算出来的黑高度 */
    int black_height(void) const { return black_height_; }

    /* 已给出：把树打印出来，颜色一起打。每一行标出它是左孩子还是右孩子 */
    void print(std::ostream &os) const { print_sub(root_, os, "", "", true); }

    /* ------------------------------------------------------------------
     * 下面三个是测试工具，已给出，练习不需要改。
     * 验收程序用它们手搭出违反不变式的树，检验校验器是否真的能报出哪一条被违反。
     * ------------------------------------------------------------------ */

    /* 只做搜索树插入，不做任何修复；新节点染红，返回它 */
    Node *insert_raw(const K &key, const V &value) {
        Node *fresh = new Node();
        fresh->key = key;
        fresh->value = value;
        fresh->color = Color::Red;
        ++size_;
        if (root_ == nullptr) {
            root_ = fresh;
            return fresh;
        }
        Node *cur = root_;
        for (;;) {
            if (key < cur->key) {
                if (cur->left == nullptr) {
                    cur->left = fresh;
                    fresh->parent = cur;
                    break;
                }
                cur = cur->left;
            } else {
                if (cur->right == nullptr) {
                    cur->right = fresh;
                    fresh->parent = cur;
                    break;
                }
                cur = cur->right;
            }
        }
        return fresh;
    }

    /* 把某个键所在的节点染色；找不到返回 false */
    bool repaint(const K &key, Color color) {
        Node *n = const_cast<Node *>(find_node(key));
        if (n == nullptr) {
            return false;
        }
        n->color = color;
        return true;
    }

    /* 把某个键改成另一个键（用来破坏搜索树顺序）；找不到返回 false */
    bool set_key(const K &from, const K &to) {
        Node *n = const_cast<Node *>(find_node(from));
        if (n == nullptr) {
            return false;
        }
        n->key = to;
        return true;
    }

private:
    /* 阶段 1-1：校验四条不变式，并算出这棵子树的黑高度 */
    const char *check_subtree(const Node *p, int &height) const;

    /* 阶段 2、3、4：插入修复 */
    void fix_after_insert(Node *node);

    /* 已给出：旋转。中序顺序不变，只改三个指针 */
    void rotate_left(Node *x) {
        Node *y = x->right;
        x->right = y->left;                     /* ① y 的左子树过继给 x */
        if (y->left != nullptr) {
            y->left->parent = x;
        }
        y->parent = x->parent;                  /* ② y 顶替 x 的位置 */
        if (x->parent == nullptr) {
            root_ = y;
        } else if (x == x->parent->left) {
            x->parent->left = y;
        } else {
            x->parent->right = y;
        }
        y->left = x;                            /* ③ x 变成 y 的左孩子 */
        x->parent = y;
    }

    void rotate_right(Node *x) {
        Node *y = x->left;
        x->left = y->right;
        if (y->right != nullptr) {
            y->right->parent = x;
        }
        y->parent = x->parent;
        if (x->parent == nullptr) {
            root_ = y;
        } else if (x == x->parent->right) {
            x->parent->right = y;
        } else {
            x->parent->left = y;
        }
        y->right = x;
        x->parent = y;
    }

    const Node *find_node(const K &key) const {
        const Node *cur = root_;
        while (cur != nullptr) {
            if (key < cur->key) {
                cur = cur->left;
            } else if (cur->key < key) {
                cur = cur->right;
            } else {
                return cur;
            }
        }
        return nullptr;
    }

    void print_sub(const Node *p, std::ostream &os, const std::string &prefix,
                   const char *side, bool last) const {
        os << prefix << side << (p->color == Color::Black ? "B:" : "R:") << p->key << "\n";
        const std::string next =
            prefix + (side[0] == '\0' ? std::string() : (last ? "    " : "|   "));
        if (p->left != nullptr) {
            print_sub(p->left, os, next, "|-- L ", p->right == nullptr);
        }
        if (p->right != nullptr) {
            print_sub(p->right, os, next, "\\-- R ", true);
        }
    }

    static void destroy(Node *p) {
        if (p == nullptr) {
            return;
        }
        destroy(p->left);
        destroy(p->right);
        delete p;
    }

    Node *root_ = nullptr;
    std::size_t size_ = 0;
    mutable int black_height_ = 0;
};

/* ==================================================================
 * 阶段 1-1：不变式校验器
 * ================================================================== */

/* TODO（阶段 1-1）：校验四条不变式，并把「从 p 出发到它所有叶子的路径上黑节点个数」
 * 通过 height 返回。
 *
 * 四条不变式：
 *
 * | 不变式 | 违反了会怎样 |
 * |---|---|
 * | 每个节点是红或黑 | 这是类型本身保证的，不用查 |
 * | 根是黑的 | 后续修复失去落脚点（这条在 check() 里查，见下） |
 * | 红节点的孩子必须是黑的 | 同一路径上不能连着两个红 |
 * | 从任一节点到它所有叶子的路径上，黑节点个数相同 | 各条路径的长度被锁在一个范围内 |
 *
 * 另外还要查一条不属于红黑性质、但树必须满足的：**搜索树顺序**
 * （左孩子的键小于自己、右孩子的键大于自己）。
 *
 * 提示：
 *   1. 空节点算一个黑：它的高度是 1。这样「黑节点个数相同」在叶子上也有意义；
 *   2. 先查当前节点（红节点的孩子是不是红的、左右孩子的键顺序对不对），
 *      再递归左右子树，最后比较两边返回的高度：不相等就报错；
 *   3. 自己的高度 = 孩子的高度 + （自己是黑节点时加一）；
 *   4. 报错用返回一句说明的办法（本模板的 check() 不抛异常、不退出），
 *      说明里要能看出**是哪一条**被违反——验收程序会检查这四句话各不相同；
 *   5. 根是不是黑的已经在 check() 里查过了，这里不必重复。
 *
 * 判据（见《配置步骤.md》阶段 1）：验收程序手工搭一棵合法的红黑树，
 *   check() 要返回 nullptr、黑高度是 3；然后把它分别破坏四次，四行输出分别是
 *   「the root is not black」「a red node has a red child」
 *   「two paths have different black heights」「the search-tree order is broken」。
 *   占位实现永远说「没问题」，四行都会是 ok。 */
template <class K, class V>
const char *RBTree<K, V>::check_subtree(const Node *p, int &height) const
{
    (void)p;
    height = 1;         /* 占位实现：永远说「没问题」，高度永远算 1 */
    return nullptr;
}

/* ==================================================================
 * 阶段 2、3、4：插入修复
 * ================================================================== */

/* 三个情形看的是**父亲与叔叔**（祖父的另一个孩子）。三种情形的分工：
 *
 * | 情形 | 叔叔的颜色 | 在修什么 | 修完还要往上吗 |
 * |---|---|---|---|
 * | 1 | 红 | 把「双红」往上推给祖父 | **要**，把祖父当成新的当前节点 |
 * | 2 | 黑，当前节点是内侧孩子 | 先把折线拉直，转成情形 3 | 不用，落进情形 3 |
 * | 3 | 黑，当前节点是外侧孩子 | 局部重排，这一棵子树就修好了 | 不用，修复结束 |
 *
 * 循环条件已经写好：只要「当前节点不是根」并且「父亲是红的」就继续修。
 * 祖父一定存在（父亲是红节点，而根是黑的），因此 parent->parent 可以直接取。
 * 三段占位各自带一个 TODO，按阶段 2、3、4 依次替换。
 *
 * 判据（见《配置步骤.md》阶段 2、3、4）：
 *   阶段 2 之后，「依次插入 30、20、10」这棵树的 check() 变成 ok；
 *   阶段 3 之后，「依次插入 30、10、20」也变成 ok；
 *   阶段 4 之后，「依次插入 20、10、30、5」以及 64 个键的两种顺序都变成 ok。 */
template <class K, class V>
void RBTree<K, V>::fix_after_insert(Node *node)
{
    while (node != root_ && node->parent->color == Color::Red) {
        Node *parent = node->parent;
        Node *grand = parent->parent;
        const bool parent_is_left = (parent == grand->left);
        Node *uncle = parent_is_left ? grand->right : grand->left;

        if (uncle != nullptr && uncle->color == Color::Red) {
            /* ---- 情形 1：叔叔是红的（双红往上推） ---- */
            /* TODO（阶段 4-1）：父亲与叔叔染黑、祖父染红，
             * 然后把祖父当成新的「当前节点」继续往上修（不要在这里结束这一轮）。
             * 提示：循环条件会重新判断新的当前节点是不是根，一路推到根时自然收尾。 */
            (void)parent;
            (void)grand;
            (void)uncle;
            return;     /* 占位实现：直接收工，双红留在树里 */
        }

        if (parent_is_left ? (node == parent->right) : (node == parent->left)) {
            /* ---- 情形 2：叔叔是黑的，当前节点是内侧孩子（先把折线拉直） ---- */
            /* TODO（阶段 3-1）：对**父亲**做一次旋转，把折线拉直，让它落进情形 3；
             * 旋转之后父亲与祖父都变了，要重新取一遍再往下走（不要在这里结束）。
             * 提示：旋转的方向由 parent_is_left 决定；旋转完把 node 换成旋转前的父亲，
             * 这样情形 3 看到的就是「外侧孩子」了。 */
            (void)parent;
            (void)grand;
            return;     /* 占位实现：直接收工，双红留在树里 */
        }

        /* ---- 情形 3：叔叔是黑的，当前节点是外侧孩子（局部重排，修复结束） ---- */
        /* TODO（阶段 2-1）：父亲染黑、祖父染红，然后对**祖父**做一次旋转。
         * 这一棵子树的高度与黑高度都回到原样，因此修完就可以结束（用 break）。
         * 提示：旋转方向由 parent_is_left 决定；本阶段做完之后，
         * 「依次插入 30、20、10」这棵树应当完全合法。 */
        (void)parent;
        (void)grand;
        return;         /* 占位实现：直接收工，双红留在树里 */
    }

    root_->color = Color::Black;    /* 根永远染黑：情形 1 可能一路推到根 */
}

} /* namespace dsr */

#endif /* RB_TREE_HPP */
