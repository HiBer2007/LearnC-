/**
 * rb_tree_mini.cpp —— 红黑树的实现、项目输出与自测
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
 * 这里没有任何界面代码，也不打印任何东西。
 * 报告里的数字全是计数器：旋转次数、染色次数、比较次数、修复循环转过的圈数，
 * 以及校验器跑过多少次、报出过多少次失败。计时一个都不做。
 */
#include "rb_tree_mini.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <set>
#include <sstream>
#include <string>
#include <utility>

namespace rbmini {

void RbStats::reset()
{
    *this = RbStats();
}

RbStats &stats()
{
    static RbStats instance;
    return instance;
}

const char *color_name(Color color)
{
    return color == Color::Red ? u8"红" : u8"黑";
}

/* ================= 红黑树 ================= */

RbTree::RbTree()
{
    nil_ = new Node();
    nil_->color = Color::Black;
    nil_->parent = nil_;
    nil_->left = nil_;
    nil_->right = nil_;
    root_ = nil_;
}

RbTree::~RbTree()
{
    destroy_subtree(nil_, root_);
    delete nil_;
}

void RbTree::destroy_subtree(Node *nil, Node *node)
{
    if (node == nil) {
        return;
    }
    destroy_subtree(nil, node->left);
    destroy_subtree(nil, node->right);
    delete node;
}

void RbTree::set_color(Node *node, Color color)
{
    if (node != nil_ && node->color != color) {
        node->color = color;
        ++stats().recolors;
    }
}

RbTree::Node *RbTree::minimum(Node *subtree) const
{
    while (subtree->left != nil_) {
        subtree = subtree->left;
    }
    return subtree;
}

RbTree::Node *RbTree::maximum(Node *subtree) const
{
    while (subtree->right != nil_) {
        subtree = subtree->right;
    }
    return subtree;
}

RbTree::Node *RbTree::find_node(int key) const
{
    Node *current = root_;
    while (current != nil_) {
        ++stats().comparisons;
        if (key == current->key) {
            return current;
        }
        current = key < current->key ? current->left : current->right;
    }
    return nil_;
}

bool RbTree::contains(int key) const
{
    return find_node(key) != nil_;
}

int RbTree::height() const
{
    /* 层数：空树 0，只有根 1 */
    std::vector<std::pair<Node *, int>> stack;
    if (root_ == nil_) {
        return 0;
    }
    stack.emplace_back(root_, 1);
    int best = 0;
    while (!stack.empty()) {
        const std::pair<Node *, int> top = stack.back();
        stack.pop_back();
        best = std::max(best, top.second);
        if (top.first->left != nil_) {
            stack.emplace_back(top.first->left, top.second + 1);
        }
        if (top.first->right != nil_) {
            stack.emplace_back(top.first->right, top.second + 1);
        }
    }
    return best;
}

int RbTree::black_height() const
{
    int count = 1;      /* 空叶子算一个黑 */
    Node *current = root_;
    while (current != nil_) {
        if (current->color == Color::Black) {
            ++count;
        }
        current = current->left;
    }
    return count;
}

/* ---------------- 旋转：只改三个指针，中序完全不变 ---------------- */

void RbTree::rotate_left(Node *x)
{
    Node *y = x->right;
    x->right = y->left;                     /* ① 把 y 的左子树过继给 x */
    if (y->left != nil_) {
        y->left->parent = x;
    }
    y->parent = x->parent;                  /* ② y 顶替 x 的位置 */
    if (x->parent == nil_) {
        root_ = y;
    } else if (x == x->parent->left) {
        x->parent->left = y;
    } else {
        x->parent->right = y;
    }
    y->left = x;                            /* ③ x 变成 y 的左孩子 */
    x->parent = y;
    ++stats().rotations;
}

void RbTree::rotate_right(Node *x)
{
    Node *y = x->left;
    x->left = y->right;
    if (y->right != nil_) {
        y->right->parent = x;
    }
    y->parent = x->parent;
    if (x->parent == nil_) {
        root_ = y;
    } else if (x == x->parent->right) {
        x->parent->right = y;
    } else {
        x->parent->left = y;
    }
    y->right = x;
    x->parent = y;
    ++stats().rotations;
}

/* ---------------- 插入 ---------------- */

bool RbTree::insert(int key)
{
    Node *parent = nil_;
    Node *current = root_;
    while (current != nil_) {
        parent = current;
        ++stats().comparisons;
        if (key == current->key) {
            return false;                   /* 键已经存在 */
        }
        current = key < current->key ? current->left : current->right;
    }

    Node *fresh = new Node();
    fresh->key = key;
    fresh->color = Color::Red;              /* 新节点一定染红：染黑会破坏黑高度 */
    fresh->parent = parent;
    fresh->left = nil_;
    fresh->right = nil_;

    if (parent == nil_) {
        root_ = fresh;
    } else if (key < parent->key) {
        parent->left = fresh;
    } else {
        parent->right = fresh;
    }
    ++size_;
    insert_fixup(fresh);
    return true;
}

void RbTree::insert_fixup(Node *z)
{
    /* 插入只可能破坏「红节点的孩子必须是黑的」这一条：新节点是红的，父亲可能也是红的 */
    while (z->parent->color == Color::Red) {
        ++stats().insert_fix_steps;
        Node *grand = z->parent->parent;
        if (z->parent == grand->left) {
            Node *uncle = grand->right;
            if (uncle->color == Color::Red) {
                /* 情形 1：叔叔是红的。父叔染黑、祖父染红，双红往上推一层 */
                set_color(z->parent, Color::Black);
                set_color(uncle, Color::Black);
                set_color(grand, Color::Red);
                z = grand;
            } else {
                if (z == z->parent->right) {
                    /* 情形 2：内侧孩子。先对父亲左旋，把折线拉直，转成情形 3 */
                    z = z->parent;
                    rotate_left(z);
                }
                /* 情形 3：外侧孩子。祖父染红、父亲染黑，对祖父右旋，这一层修好 */
                set_color(z->parent, Color::Black);
                set_color(z->parent->parent, Color::Red);
                rotate_right(z->parent->parent);
            }
        } else {
            Node *uncle = grand->left;
            if (uncle->color == Color::Red) {
                set_color(z->parent, Color::Black);
                set_color(uncle, Color::Black);
                set_color(grand, Color::Red);
                z = grand;
            } else {
                if (z == z->parent->left) {
                    z = z->parent;
                    rotate_right(z);
                }
                set_color(z->parent, Color::Black);
                set_color(z->parent->parent, Color::Red);
                rotate_left(z->parent->parent);
            }
        }
    }
    set_color(root_, Color::Black);         /* 根永远染黑：所有路径的黑节点数一起加一 */
}

/* ---------------- 删除 ---------------- */

void RbTree::transplant(Node *u, Node *v)
{
    if (u->parent == nil_) {
        root_ = v;
    } else if (u == u->parent->left) {
        u->parent->left = v;
    } else {
        u->parent->right = v;
    }
    v->parent = u->parent;
}

bool RbTree::erase(int key)
{
    Node *z = find_node(key);
    if (z == nil_) {
        return false;
    }

    Node *y = z;
    Color y_color = y->color;
    Node *x = nil_;

    if (z->left == nil_) {
        x = z->right;
        transplant(z, z->right);
    } else if (z->right == nil_) {
        x = z->left;
        transplant(z, z->left);
    } else {
        /* 两个孩子：先和后继交换位置，转成「删一个最多只有一个孩子的节点」 */
        y = minimum(z->right);
        y_color = y->color;
        x = y->right;
        if (y->parent == z) {
            x->parent = y;
        } else {
            transplant(y, y->right);
            y->right = z->right;
            y->right->parent = y;
        }
        transplant(z, y);
        y->left = z->left;
        y->left->parent = y;
        y->color = z->color;
    }

    delete z;
    --size_;
    if (y_color == Color::Black) {
        erase_fixup(x);     /* 少了一个黑，从 x 这一侧往上修 */
    }
    return true;
}

void RbTree::erase_fixup(Node *x)
{
    while (x != root_ && x->color == Color::Black) {
        ++stats().erase_fix_steps;
        if (x == x->parent->left) {
            Node *w = x->parent->right;             /* 兄弟 */
            if (w->color == Color::Red) {
                /* 情形 1：兄弟是红的。兄弟染黑、父亲染红，对父亲左旋，转成后三种之一 */
                set_color(w, Color::Black);
                set_color(x->parent, Color::Red);
                rotate_left(x->parent);
                w = x->parent->right;
            }
            if (w->left->color == Color::Black && w->right->color == Color::Black) {
                /* 情形 2：兄弟是黑的而且两个孩子都黑。兄弟染红，缺一个黑往上推给父亲 */
                set_color(w, Color::Red);
                x = x->parent;
            } else {
                if (w->right->color == Color::Black) {
                    /* 情形 3：远侄子黑、近侄子红。近侄子染黑、兄弟染红，对兄弟右旋 */
                    set_color(w->left, Color::Black);
                    set_color(w, Color::Red);
                    rotate_right(w);
                    w = x->parent->right;
                }
                /* 情形 4：远侄子是红的。兄弟继承父亲的颜色、父亲与远侄子染黑，对父亲左旋 */
                set_color(w, x->parent->color);
                set_color(x->parent, Color::Black);
                set_color(w->right, Color::Black);
                rotate_left(x->parent);
                x = root_;                              /* 修复结束 */
            }
        } else {
            Node *w = x->parent->left;
            if (w->color == Color::Red) {
                set_color(w, Color::Black);
                set_color(x->parent, Color::Red);
                rotate_right(x->parent);
                w = x->parent->left;
            }
            if (w->right->color == Color::Black && w->left->color == Color::Black) {
                set_color(w, Color::Red);
                x = x->parent;
            } else {
                if (w->left->color == Color::Black) {
                    set_color(w->right, Color::Black);
                    set_color(w, Color::Red);
                    rotate_left(w);
                    w = x->parent->left;
                }
                set_color(w, x->parent->color);
                set_color(x->parent, Color::Black);
                set_color(w->left, Color::Black);
                rotate_right(x->parent);
                x = root_;
            }
        }
    }
    x->color = Color::Black;
}

/* ---------------- 校验器 ---------------- */

int RbTree::walk_check(const Node *nil, const Node *node, CheckReport &report, int depth,
                       bool &height_known)
{
    if (node == nil) {
        return 1;                       /* 空叶子算一个黑 */
    }

    ++report.nodes;
    if (node->color == Color::Red) {
        ++report.red_nodes;
        if (node->left->color == Color::Red || node->right->color == Color::Red) {
            if (report.ok) {
                report.ok = false;
                report.first_problem = u8"红节点的孩子是红的";
            }
        }
    } else {
        ++report.black_nodes;
    }

    if (node->left != nil) {
        if (!(node->left->key < node->key)) {
            report.ordered = false;
            if (report.ok) {
                report.ok = false;
                report.first_problem = u8"搜索树性质被破坏（左孩子不小于自己）";
            }
        }
        if (node->left->parent != node) {
            if (report.ok) {
                report.ok = false;
                report.first_problem = u8"父指针不一致（左孩子不认这个父亲）";
            }
        }
    }
    if (node->right != nil) {
        if (!(node->key < node->right->key)) {
            report.ordered = false;
            if (report.ok) {
                report.ok = false;
                report.first_problem = u8"搜索树性质被破坏（右孩子不大于自己）";
            }
        }
        if (node->right->parent != node) {
            if (report.ok) {
                report.ok = false;
                report.first_problem = u8"父指针不一致（右孩子不认这个父亲）";
            }
        }
    }

    if (depth > report.height || !height_known) {
        report.height = depth;
        height_known = true;
    }

    const int left_black = walk_check(nil, node->left, report, depth + 1, height_known);
    const int right_black = walk_check(nil, node->right, report, depth + 1, height_known);
    if (left_black != right_black) {
        if (report.ok) {
            report.ok = false;
            report.first_problem = u8"两条路径的黑节点数不同";
        }
    }
    return left_black + (node->color == Color::Black ? 1 : 0);
}

RbTree::CheckReport RbTree::check() const
{
    ++stats().checks;
    CheckReport report;
    if (root_ != nil_ && root_->color != Color::Black) {
        report.ok = false;
        report.first_problem = u8"根不是黑色";
    }
    if (root_ == nil_) {
        report.nodes = 0;
        report.height = 0;
        report.black_height = 1;
    } else {
        bool height_known = false;
        report.black_height = walk_check(nil_, root_, report, 1, height_known);
        if (root_->parent != nil_) {
            report.ok = false;
            if (report.first_problem.empty()) {
                report.first_problem = u8"根的父亲不是空哨兵";
            }
        }
    }
    if (report.nodes != size_) {
        report.ok = false;
        if (report.first_problem.empty()) {
            report.first_problem = u8"走一遍数出来的节点数与 size() 不一致";
        }
    }
    if (!report.ordered) {
        report.ok = false;
    }
    if (!report.ok) {
        ++stats().failures;
    }
    return report;
}

/* ---------------- 打印与遍历 ---------------- */

namespace {

struct Cell {
    std::size_t rank = 0;
    std::string label;
};

void collect_levels(const detail::RbNode *nil, const detail::RbNode *node, int depth,
                    std::size_t &rank, std::vector<std::vector<Cell>> &levels)
{
    if (node == nil) {
        return;
    }
    collect_levels(nil, node->left, depth + 1, rank, levels);
    if (levels.size() <= static_cast<std::size_t>(depth)) {
        levels.resize(static_cast<std::size_t>(depth) + 1);
    }
    std::string label;
    label += (node->color == Color::Red ? 'R' : 'B');
    label += ':';
    label += std::to_string(node->key);
    levels[static_cast<std::size_t>(depth)].push_back(Cell{rank, label});
    ++rank;
    collect_levels(nil, node->right, depth + 1, rank, levels);
}

void collect_inorder(const detail::RbNode *nil, const detail::RbNode *node, std::vector<int> &out)
{
    if (node == nil) {
        return;
    }
    collect_inorder(nil, node->left, out);
    out.push_back(node->key);
    collect_inorder(nil, node->right, out);
}

}   /* namespace */

std::string RbTree::to_levels() const
{
    std::ostringstream os;
    if (root_ == nil_) {
        os << u8"（空树）\n";
        return os.str();
    }
    std::vector<std::vector<Cell>> levels;
    std::size_t rank = 0;
    collect_levels(nil_, root_, 0, rank, levels);

    const std::size_t width = 7;            /* 每个键占的列数，够放下 B:1000 */
    const std::size_t total = rank * width;
    for (std::size_t depth = 0; depth < levels.size(); ++depth) {
        std::string line(total, ' ');
        for (const Cell &cell : levels[depth]) {
            const std::size_t start = cell.rank * width + (width - cell.label.size()) / 2;
            for (std::size_t i = 0; i < cell.label.size() && start + i < line.size(); ++i) {
                line[start + i] = cell.label[i];
            }
        }
        while (!line.empty() && line.back() == ' ') {
            line.pop_back();
        }
        os << u8"  " << line << "\n";
    }
    return os.str();
}

std::vector<int> RbTree::inorder() const
{
    std::vector<int> out;
    collect_inorder(nil_, root_, out);
    return out;
}

/* ---------------- 迭代器 ---------------- */

RbTree::Iterator &RbTree::Iterator::operator++()
{
    if (node_ == nil_) {
        return *this;                       /* end() 再往后还是 end() */
    }
    ++stats().successor_steps;
    if (node_->right != nil_) {
        node_ = node_->right;               /* 第一步：有右子树，找右子树里最左的 */
        while (node_->left != nil_) {
            node_ = node_->left;
            ++stats().successor_steps;
        }
        return *this;
    }
    detail::RbNode *parent = node_->parent; /* 第二步：往上走，直到「我是我父亲的左孩子」 */
    while (parent != nil_ && node_ == parent->right) {
        node_ = parent;
        parent = parent->parent;
        ++stats().successor_steps;
    }
    node_ = parent;                         /* 第三步：那个父亲就是下一个；走到头就是 end() */
    return *this;
}

RbTree::Iterator &RbTree::Iterator::operator--()
{
    if (node_ == nil_) {
        /* --end() 是最大的那个键 */
        node_ = const_cast<detail::RbNode *>(root_);
        while (node_ != nil_ && node_->right != nil_) {
            node_ = node_->right;
            ++stats().successor_steps;
        }
        return *this;
    }
    if (node_->left != nil_) {
        node_ = node_->left;
        while (node_->right != nil_) {
            node_ = node_->right;
            ++stats().successor_steps;
        }
        return *this;
    }
    detail::RbNode *parent = node_->parent;
    while (parent != nil_ && node_ == parent->left) {
        node_ = parent;
        parent = parent->parent;
        ++stats().successor_steps;
    }
    node_ = parent;
    return *this;
}

/* ================= 朴素搜索树 ================= */

PlainBst::~PlainBst()
{
    destroy_subtree(root_);
}

void PlainBst::destroy_subtree(Node *node)
{
    if (node == nullptr) {
        return;
    }
    destroy_subtree(node->left);
    destroy_subtree(node->right);
    delete node;
}

int PlainBst::height_of(const Node *node)
{
    if (node == nullptr) {
        return 0;
    }
    return 1 + std::max(height_of(node->left), height_of(node->right));
}

int PlainBst::height() const
{
    return height_of(root_);
}

bool PlainBst::insert(int key)
{
    Node **link = &root_;
    while (*link != nullptr) {
        ++stats().comparisons;
        if (key == (*link)->key) {
            return false;
        }
        link = key < (*link)->key ? &(*link)->left : &(*link)->right;
    }
    *link = new Node();
    (*link)->key = key;
    ++size_;
    return true;
}

bool PlainBst::contains(int key) const
{
    const Node *current = root_;
    while (current != nullptr) {
        ++stats().comparisons;
        if (key == current->key) {
            return true;
        }
        current = key < current->key ? current->left : current->right;
    }
    return false;
}

/* ================= 报告用的小工具 ================= */

namespace {

bool is_wide_codepoint(unsigned int cp)
{
    return (cp >= 0x1100u && cp <= 0x115Fu) || (cp >= 0x2E80u && cp <= 0x303Eu)
           || (cp >= 0x3041u && cp <= 0x33FFu) || (cp >= 0x3400u && cp <= 0x4DBFu)
           || (cp >= 0x4E00u && cp <= 0x9FFFu) || (cp >= 0xA000u && cp <= 0xA4CFu)
           || (cp >= 0xAC00u && cp <= 0xD7A3u) || (cp >= 0xF900u && cp <= 0xFAFFu)
           || (cp >= 0xFE30u && cp <= 0xFE6Fu) || (cp >= 0xFF00u && cp <= 0xFF60u)
           || (cp >= 0xFFE0u && cp <= 0xFFE6u);
}

std::size_t display_width(const std::string &text)
{
    std::size_t width = 0;
    for (std::size_t i = 0; i < text.size();) {
        const unsigned char lead = static_cast<unsigned char>(text[i]);
        unsigned int cp = lead;
        std::size_t length = 1;
        if (lead >= 0xF0u) {
            cp = lead & 0x07u;
            length = 4;
        } else if (lead >= 0xE0u) {
            cp = lead & 0x0Fu;
            length = 3;
        } else if (lead >= 0xC0u) {
            cp = lead & 0x1Fu;
            length = 2;
        }
        for (std::size_t k = 1; k < length && i + k < text.size(); ++k) {
            cp = (cp << 6) | (static_cast<unsigned char>(text[i + k]) & 0x3Fu);
        }
        width += is_wide_codepoint(cp) ? 2 : 1;
        i += length;
    }
    return width;
}

std::string pad_right(const std::string &text, std::size_t width)
{
    const std::size_t used = display_width(text);
    return used >= width ? text : text + std::string(width - used, ' ');
}

std::string pad_left_text(const std::string &text, std::size_t width)
{
    const std::size_t used = display_width(text);
    return used >= width ? text : std::string(width - used, ' ') + text;
}

std::string pad_left(std::size_t value, std::size_t width)
{
    return pad_left_text(std::to_string(value), width);
}

/** 固定种子的线性同余发生器：报告里的随机序列因此可以逐位复现 */
class Lcg {
public:
    explicit Lcg(std::uint64_t seed) : state_(seed) {}

    std::uint32_t next()
    {
        state_ = state_ * 6364136223846793005ull + 1442695040888963407ull;
        return static_cast<std::uint32_t>(state_ >> 33);
    }

private:
    std::uint64_t state_;
};

std::string join_ints(const std::vector<int> &values)
{
    std::ostringstream os;
    for (std::size_t i = 0; i < values.size(); ++i) {
        if (i != 0) {
            os << ' ';
        }
        os << values[i];
    }
    return os.str();
}

/** 查全部 count 个键，返回最坏那一次的比较次数：这就是「最深那个键要走多少步」 */
template <class Tree>
std::size_t worst_lookup(const Tree &tree, std::size_t count)
{
    std::size_t worst = 0;
    for (std::size_t i = 1; i <= count; ++i) {
        stats().reset();
        tree.contains(static_cast<int>(i));
        if (stats().comparisons > worst) {
            worst = stats().comparisons;
        }
    }
    return worst;
}

/* 自测的小工具：把每一项的结果记下来，不打印 */
class Checker {
public:
    void check(bool ok, const std::string &what, const std::string &detail = std::string())
    {
        ++result_.total;
        std::ostringstream line;
        if (ok) {
            ++result_.passed;
            line << u8"[通过] " << result_.total << ". " << what;
        } else {
            ++result_.failed;
            line << u8"[失败] " << result_.total << ". " << what;
            if (!detail.empty()) {
                line << u8"（" << detail << u8"）";
            }
        }
        result_.lines.push_back(line.str());
    }

    CheckResult take() { return result_; }

private:
    CheckResult result_;
};

/* 报告第一、三段用的固定键序 */
const int kDemoKeys[] = { 30, 10, 50, 5, 20, 40, 60, 15, 25, 45, 55 };
const std::size_t kDemoKeyCount = sizeof(kDemoKeys) / sizeof(kDemoKeys[0]);

void append_insert_section(std::ostringstream &os)
{
    os << u8"红黑树：插入与不变式校验（" << kDemoKeyCount << u8" 个键，按固定顺序）\n";
    os << u8"  第几个  键    节点数  树高  黑高度  旋转次数  染色次数  校验\n";

    stats().reset();
    RbTree tree;
    for (std::size_t i = 0; i < kDemoKeyCount; ++i) {
        tree.insert(kDemoKeys[i]);
        const RbTree::CheckReport report = tree.check();
        os << "  " << pad_left(i + 1, 6) << "  " << pad_left(static_cast<std::size_t>(kDemoKeys[i]), 4)
           << "  " << pad_left(report.nodes, 6) << "  " << pad_left(static_cast<std::size_t>(report.height), 4)
           << "  " << pad_left(static_cast<std::size_t>(report.black_height), 6) << "  "
           << pad_left(stats().rotations, 8) << "  " << pad_left(stats().recolors, 8) << "  "
           << (report.ok ? u8"通过" : u8"失败：" + report.first_problem) << "\n";
    }
    os << u8"  合计：旋转 " << stats().rotations << u8" 次、染色 " << stats().recolors
       << u8" 次、键比较 " << stats().comparisons << u8" 次、校验 " << stats().checks
       << u8" 次、失败 " << stats().failures << u8" 次\n";
    const RbTree::CheckReport final_report = tree.check();
    os << u8"  红节点 " << final_report.red_nodes << u8" 个、黑节点 " << final_report.black_nodes
       << u8" 个（空叶子不算在内）\n";
}

void append_ascending_section(std::ostringstream &os)
{
    const int last = 8;
    os << u8"\n换一个顺序：1 到 " << last << u8" 依次插入，这一次旋转上场了\n";
    os << u8"  第几个  键  节点数  树高  黑高度  旋转次数  染色次数  校验\n";

    stats().reset();
    RbTree tree;
    for (int key = 1; key <= last; ++key) {
        tree.insert(key);
        const RbTree::CheckReport report = tree.check();
        os << "  " << pad_left(static_cast<std::size_t>(key), 6) << "  "
           << pad_left(static_cast<std::size_t>(key), 2) << "  " << pad_left(report.nodes, 6)
           << "  " << pad_left(static_cast<std::size_t>(report.height), 4) << "  "
           << pad_left(static_cast<std::size_t>(report.black_height), 6) << "  "
           << pad_left(stats().rotations, 8) << "  " << pad_left(stats().recolors, 8) << "  "
           << (report.ok ? u8"通过" : u8"失败：" + report.first_problem) << "\n";
    }
    os << u8"  一共旋转 " << stats().rotations << u8" 次、染色 " << stats().recolors
       << u8" 次——有序插入是插入修复最忙的时候\n";
    os << tree.to_levels();
}

void append_tree_section(std::ostringstream &os, const RbTree &tree, const std::string &title){
    os << u8"\n" << title << u8"（B=黑 R=红，位置按中序名次排）\n";
    os << tree.to_levels();
}

void append_erase_section(std::ostringstream &os)
{
    os << u8"\n红黑树：删除与不变式校验（从上面那棵树里删）\n";
    os << u8"  删掉  节点数  树高  黑高度  本轮旋转  校验\n";

    RbTree copy;
    for (std::size_t i = 0; i < kDemoKeyCount; ++i) {
        copy.insert(kDemoKeys[i]);
    }
    const int removals[] = { 5, 30, 60, 50, 15 };
    for (int key : removals) {
        const std::size_t rotations_before = stats().rotations;
        const bool removed = copy.erase(key);
        const RbTree::CheckReport report = copy.check();
        os << "  " << pad_left(static_cast<std::size_t>(key), 4) << "  "
           << pad_left(report.nodes, 6) << "  "
           << pad_left(static_cast<std::size_t>(report.height), 4) << "  "
           << pad_left(static_cast<std::size_t>(report.black_height), 6) << "  "
           << pad_left(stats().rotations - rotations_before, 8) << "  "
           << (report.ok ? u8"通过" : u8"失败：" + report.first_problem) << "\n";
        if (!removed) {
            os << u8"  键 " << key << u8" 不在树里\n";
        }
    }
}

void append_random_section(std::ostringstream &os)
{
    const std::size_t operations = 2000;
    Lcg lcg(20261002u);

    RbTree tree;
    std::set<int> oracle;
    std::size_t inserted = 0;
    std::size_t erased = 0;
    std::size_t mismatches = 0;
    std::size_t max_height = 0;
    std::size_t failures = 0;

    stats().reset();
    for (std::size_t i = 0; i < operations; ++i) {
        const int key = static_cast<int>(lcg.next() % 1000u) * 3;
        if (lcg.next() % 2u == 0u) {
            const bool ok = tree.insert(key);
            const bool oracle_ok = oracle.insert(key).second;
            if (ok != oracle_ok) {
                ++mismatches;
            }
            if (ok) {
                ++inserted;
            }
        } else {
            const bool ok = tree.erase(key);
            const bool oracle_ok = oracle.erase(key) != 0;
            if (ok != oracle_ok) {
                ++mismatches;
            }
            if (ok) {
                ++erased;
            }
        }
        const RbTree::CheckReport report = tree.check();
        if (!report.ok) {
            ++failures;
        }
        if (static_cast<std::size_t>(report.height) > max_height) {
            max_height = static_cast<std::size_t>(report.height);
        }
    }

    os << u8"\n随机插删：固定种子，2000 次操作，每一步都跑一遍校验器\n";
    os << u8"  插入成功 " << inserted << u8" 次、删除成功 " << erased << u8" 次，最终 " << tree.size()
       << u8" 个键\n";
    os << u8"  与 std::set 逐次比对：不一致 " << mismatches << u8" 次\n";
    os << u8"  校验 2000 次，不变量被破坏 " << failures << u8" 次；过程中最大树高 " << max_height
       << u8"，最终树高 " << tree.height() << u8"，黑高度 " << tree.black_height() << u8"\n";
    os << u8"  旋转 " << stats().rotations << u8" 次、染色 " << stats().recolors << u8" 次\n";
}

void append_degenerate_section(std::ostringstream &os)
{
    const std::size_t count = 1000;

    os << u8"\n退化对照：同样的 " << count << u8" 个键，两种插入顺序\n";
    os << u8"  插入顺序          朴素搜索树树高  红黑树树高  朴素树最深比较  红黑树最深比较\n";

    /* 有序插入 */
    {
        PlainBst plain;
        RbTree rb;
        stats().reset();
        for (std::size_t i = 1; i <= count; ++i) {
            plain.insert(static_cast<int>(i));
        }
        const std::size_t plain_comparisons = stats().comparisons;
        stats().reset();
        for (std::size_t i = 1; i <= count; ++i) {
            rb.insert(static_cast<int>(i));
        }
        const std::size_t rb_insert_comparisons = stats().comparisons;
        const std::size_t rb_rotations = stats().rotations;

        const std::size_t plain_deepest = worst_lookup(plain, count);
        const std::size_t rb_deepest = worst_lookup(rb, count);

        os << "  " << pad_right(u8"1 2 3 … 1000（有序）", 20)
           << pad_left(static_cast<std::size_t>(plain.height()), 12)
           << pad_left(static_cast<std::size_t>(rb.height()), 12) << "  "
           << pad_left(plain_deepest, 12) << "  " << pad_left(rb_deepest, 12) << "\n";
        os << u8"    插入阶段比较次数：朴素 " << plain_comparisons << u8"、红黑树 "
           << rb_insert_comparisons << u8"；红黑树旋转 " << rb_rotations << u8" 次\n";
    }

    /* 打乱插入：同一个排列喂给两棵树 */
    {
        Lcg lcg(20261002u);
        std::vector<int> order(count);
        for (std::size_t i = 0; i < count; ++i) {
            order[i] = static_cast<int>(i) + 1;
        }
        for (std::size_t i = count; i > 1; --i) {
            const std::size_t j = lcg.next() % i;
            std::swap(order[i - 1], order[j]);
        }

        PlainBst plain;
        RbTree rb;
        stats().reset();
        for (int key : order) {
            plain.insert(key);
        }
        const std::size_t plain_comparisons = stats().comparisons;
        stats().reset();
        for (int key : order) {
            rb.insert(key);
        }
        const std::size_t rb_insert_comparisons = stats().comparisons;
        const std::size_t rb_rotations = stats().rotations;

        const std::size_t plain_deepest = worst_lookup(plain, count);
        const std::size_t rb_deepest = worst_lookup(rb, count);

        os << "  " << pad_right(u8"固定种子打乱", 20)
           << pad_left(static_cast<std::size_t>(plain.height()), 12)
           << pad_left(static_cast<std::size_t>(rb.height()), 12) << "  "
           << pad_left(plain_deepest, 12) << "  " << pad_left(rb_deepest, 12) << "\n";
        os << u8"    插入阶段比较次数：朴素 " << plain_comparisons << u8"、红黑树 "
           << rb_insert_comparisons << u8"；红黑树旋转 " << rb_rotations << u8" 次\n";
    }
    os << u8"  「最深比较」= 查全部 1000 个键时最坏那一次的比较次数，也就是最深的键要走多少步\n";
    os << u8"  红黑树的高度上界是 2·log2(n+1)，十万个键时约 34 层；朴素树没有上界\n";
}

void append_iterator_section(std::ostringstream &os, const RbTree &tree)
{
    os << u8"\n中序迭代器：++ 与 --（有父指针，因此都是常数时间）\n";

    stats().reset();
    std::vector<int> values;
    for (RbTree::Iterator it = tree.begin(); it != tree.end(); ++it) {
        values.push_back(*it);
    }
    const std::size_t walk_steps = stats().successor_steps;
    bool ordered = true;
    for (std::size_t i = 1; i < values.size(); ++i) {
        if (!(values[i - 1] < values[i])) {
            ordered = false;
        }
    }

    std::vector<int> reversed;
    for (RbTree::Iterator it = tree.end(); it != tree.begin();) {
        --it;
        reversed.push_back(*it);
    }

    os << u8"  中序序列：" << join_ints(values) << "\n";
    os << u8"  严格递增：" << (ordered ? u8"是" : u8"否") << u8"；从头走到尾 "
       << values.size() << u8" 步，++ 内部走过的步数 " << walk_steps << u8"\n";
    os << u8"  从 end() 起步反复 --：" << join_ints(reversed) << "\n";
    os << u8"  --end() 是最大的键：" << *(--tree.end()) << u8"；begin() 是最小的键：" << *tree.begin()
       << u8"\n";
}

}   /* namespace */

std::string build_report()
{
    std::ostringstream os;

    append_insert_section(os);
    append_ascending_section(os);

    RbTree tree;
    for (std::size_t i = 0; i < kDemoKeyCount; ++i) {
        tree.insert(kDemoKeys[i]);
    }
    append_tree_section(os, tree, u8"按层打印：插入完的形状");
    append_erase_section(os);

    RbTree remaining;
    for (std::size_t i = 0; i < kDemoKeyCount; ++i) {
        remaining.insert(kDemoKeys[i]);
    }
    const int removals[] = { 5, 30, 60, 50, 15 };
    for (int key : removals) {
        remaining.erase(key);
    }
    append_tree_section(os, remaining, u8"按层打印：删除之后的形状");
    append_iterator_section(os, remaining);
    append_random_section(os);
    append_degenerate_section(os);
    return os.str();
}

/* ================= 自测 ================= */

std::string CheckResult::summary() const
{
    std::ostringstream os;
    os << total << u8" 项中 " << passed << u8" 项通过";
    if (failed == 0) {
        os << u8"，全部通过";
    } else {
        os << u8"，" << failed << u8" 项失败";
    }
    return os.str();
}

CheckResult run_self_tests()
{
    Checker c;

    /* 1. 空树 */
    {
        RbTree tree;
        const RbTree::CheckReport report = tree.check();
        c.check(tree.size() == 0 && tree.height() == 0 && tree.begin() == tree.end() && report.ok
                    && report.black_height == 1,
                u8"空树：size 0、树高 0、begin == end、校验通过",
                std::to_string(report.black_height));
    }

    /* 2. 插入一个键之后根是黑的 */
    {
        RbTree tree;
        tree.insert(42);
        const RbTree::CheckReport report = tree.check();
        c.check(report.ok && report.nodes == 1 && report.black_nodes == 1 && report.red_nodes == 0
                    && report.height == 1,
                u8"插入一个键：它是根、是黑的、树高 1");
    }

    /* 3. 重复键 */
    {
        RbTree tree;
        const bool first = tree.insert(7);
        const bool second = tree.insert(7);
        c.check(first && !second && tree.size() == 1, u8"重复插入返回 false，size 不变");
    }

    /* 4. 11 个键插入之后不变式全部成立 */
    {
        RbTree tree;
        for (std::size_t i = 0; i < kDemoKeyCount; ++i) {
            tree.insert(kDemoKeys[i]);
        }
        const RbTree::CheckReport report = tree.check();
        const std::vector<int> order = tree.inorder();
        bool sorted = order.size() == kDemoKeyCount;
        for (std::size_t i = 1; i < order.size(); ++i) {
            sorted = sorted && order[i - 1] < order[i];
        }
        c.check(report.ok && sorted && report.nodes == kDemoKeyCount,
                u8"插入 11 个键：四条不变式全成立、中序严格递增",
                std::to_string(report.nodes) + u8" 个节点、树高 "
                    + std::to_string(report.height));
    }

    /* 5. 红节点的孩子都是黑的（由校验器覆盖，这里单独数一遍） */
    {
        RbTree tree;
        for (std::size_t i = 0; i < kDemoKeyCount; ++i) {
            tree.insert(kDemoKeys[i]);
        }
        const RbTree::CheckReport report = tree.check();
        c.check(report.red_nodes + report.black_nodes == kDemoKeyCount && report.red_nodes > 0,
                u8"红节点与黑节点加起来等于节点总数（空叶子不算）",
                std::to_string(report.red_nodes) + u8" 红 + " + std::to_string(report.black_nodes)
                    + u8" 黑");
    }

    /* 6. 按层打印里有所有键与颜色字母 */
    {
        RbTree tree;
        for (std::size_t i = 0; i < kDemoKeyCount; ++i) {
            tree.insert(kDemoKeys[i]);
        }
        const std::string picture = tree.to_levels();
        bool has_all = true;
        for (std::size_t i = 0; i < kDemoKeyCount; ++i) {
            has_all = has_all && picture.find(std::to_string(kDemoKeys[i])) != std::string::npos;
        }
        c.check(has_all && picture.find("B:") != std::string::npos
                    && picture.find("R:") != std::string::npos,
                u8"按层打印里每个键都在，且颜色一起打了出来（B: 与 R:）");
    }

    /* 7. 删除不存在的键 */
    {
        RbTree tree;
        tree.insert(1);
        c.check(!tree.erase(2) && tree.size() == 1, u8"删除不存在的键返回 false，size 不变");
    }

    /* 8. 删掉所有键之后树回到空 */
    {
        RbTree tree;
        for (std::size_t i = 0; i < kDemoKeyCount; ++i) {
            tree.insert(kDemoKeys[i]);
        }
        bool all_removed = true;
        for (std::size_t i = 0; i < kDemoKeyCount; ++i) {
            all_removed = all_removed && tree.erase(kDemoKeys[i]);
        }
        const RbTree::CheckReport report = tree.check();
        c.check(all_removed && tree.size() == 0 && tree.height() == 0 && tree.begin() == tree.end()
                    && report.ok,
                u8"把 11 个键逐个删掉：树回到空、校验仍然通过");
    }

    /* 9. 删除两个孩子都有的节点 */
    {
        RbTree tree;
        for (std::size_t i = 0; i < kDemoKeyCount; ++i) {
            tree.insert(kDemoKeys[i]);
        }
        tree.erase(30);     /* 根，左右都有孩子 */
        const RbTree::CheckReport report = tree.check();
        const std::vector<int> order = tree.inorder();
        bool sorted = true;
        for (std::size_t i = 1; i < order.size(); ++i) {
            sorted = sorted && order[i - 1] < order[i];
        }
        c.check(report.ok && sorted && tree.size() == kDemoKeyCount - 1 && !tree.contains(30),
                u8"删掉有两个孩子的根：不变式成立、中序仍然严格递增");
    }

    /* 10. 每一步都校验：500 次插入 */
    {
        RbTree tree;
        Lcg lcg(7u);
        std::size_t failures = 0;
        for (std::size_t i = 0; i < 500; ++i) {
            tree.insert(static_cast<int>(lcg.next() % 5000u));
            if (!tree.check().ok) {
                ++failures;
            }
        }
        c.check(failures == 0 && tree.size() > 0,
                u8"随机插入 500 个键，每插一步校验一次：失败 0 次",
                std::to_string(tree.size()) + u8" 个键");
    }

    /* 11. 随机插删 1000 次之后与 std::set 一致 */
    {
        RbTree tree;
        std::set<int> oracle;
        Lcg lcg(99u);
        std::size_t mismatches = 0;
        for (std::size_t i = 0; i < 1000; ++i) {
            const int key = static_cast<int>(lcg.next() % 400u);
            if (lcg.next() % 2u == 0u) {
                if (tree.insert(key) != oracle.insert(key).second) {
                    ++mismatches;
                }
            } else {
                if (tree.erase(key) != (oracle.erase(key) != 0)) {
                    ++mismatches;
                }
            }
        }
        bool same = tree.size() == oracle.size();
        for (int key = 0; key < 400 && same; ++key) {
            same = tree.contains(key) == (oracle.count(key) != 0);
        }
        c.check(mismatches == 0 && same && tree.check().ok,
                u8"随机插删 1000 次：每一步都与 std::set 一致，最终集合相同",
                std::to_string(tree.size()) + u8" 个键");
    }

    /* 12. contains 对不在树里的键返回 false */
    {
        RbTree tree;
        for (std::size_t i = 0; i < kDemoKeyCount; ++i) {
            tree.insert(kDemoKeys[i]);
        }
        std::size_t misses = 0;
        for (int key = 1; key <= 200; ++key) {
            if (key % 5 != 0 && !tree.contains(key)) {
                ++misses;
            }
        }
        c.check(misses == 160, u8"不在树里的键全部返回 false", std::to_string(misses) + u8" 个");
    }

    /* 13. 黑高度与手算一致 */
    {
        RbTree tree;
        for (std::size_t i = 0; i < kDemoKeyCount; ++i) {
            tree.insert(kDemoKeys[i]);
        }
        const RbTree::CheckReport report = tree.check();
        c.check(report.black_height == tree.black_height(),
                u8"校验器算出的黑高度与 black_height() 一致",
                std::to_string(report.black_height));
    }

    /* 14. 迭代器 ++ 得到严格递增序列 */
    {
        RbTree tree;
        for (std::size_t i = 0; i < kDemoKeyCount; ++i) {
            tree.insert(kDemoKeys[i]);
        }
        std::vector<int> walked;
        for (RbTree::Iterator it = tree.begin(); it != tree.end(); ++it) {
            walked.push_back(*it);
        }
        const std::vector<int> direct = tree.inorder();
        c.check(walked == direct, u8"迭代器 ++ 走出来的序列与中序一致");
    }

    /* 15. --end() 是最大的键 */
    {
        RbTree tree;
        for (std::size_t i = 0; i < kDemoKeyCount; ++i) {
            tree.insert(kDemoKeys[i]);
        }
        RbTree::Iterator last = tree.end();
        --last;
        c.check(*last == 60 && *tree.begin() == 5, u8"--end() 是最大的键、begin() 是最小的键",
                std::to_string(*last) + u8" / " + std::to_string(*tree.begin()));
    }

    /* 16. 从 end() 反复 -- 得到递减序列 */
    {
        RbTree tree;
        for (std::size_t i = 0; i < kDemoKeyCount; ++i) {
            tree.insert(kDemoKeys[i]);
        }
        std::vector<int> reversed;
        for (RbTree::Iterator it = tree.end(); it != tree.begin();) {
            --it;
            reversed.push_back(*it);
        }
        bool decreasing = reversed.size() == kDemoKeyCount;
        for (std::size_t i = 1; i < reversed.size(); ++i) {
            decreasing = decreasing && reversed[i] < reversed[i - 1];
        }
        c.check(decreasing, u8"从 end() 反复 -- 得到严格递减序列");
    }

    /* 17. 有序插入 1000 个键：红黑树高有上界，朴素树退化成链 */
    {
        const std::size_t count = 1000;
        PlainBst plain;
        RbTree rb;
        for (std::size_t i = 1; i <= count; ++i) {
            plain.insert(static_cast<int>(i));
            rb.insert(static_cast<int>(i));
        }
        const std::size_t bound = 2 * static_cast<std::size_t>(
                                          std::log2(static_cast<double>(count) + 1.0)) + 2;
        c.check(plain.height() == static_cast<int>(count)
                    && static_cast<std::size_t>(rb.height()) <= bound && rb.check().ok,
                u8"有序插入 1000 个键：朴素树高 1000，红黑树高不超过 2·log2(n+1)+2",
                u8"朴素 " + std::to_string(plain.height()) + u8"、红黑树 "
                    + std::to_string(rb.height()) + u8"（上界 " + std::to_string(bound) + u8"）");
    }

    /* 18. 打乱插入同一个排列：两棵树装的东西一样 */
    {
        Lcg lcg(5u);
        std::vector<int> order(300);
        for (std::size_t i = 0; i < order.size(); ++i) {
            order[i] = static_cast<int>(i) + 1;
        }
        for (std::size_t i = order.size(); i > 1; --i) {
            std::swap(order[i - 1], order[lcg.next() % i]);
        }
        RbTree tree;
        for (int key : order) {
            tree.insert(key);
        }
        std::size_t hits = 0;
        for (int key = 1; key <= 300; ++key) {
            if (tree.contains(key)) {
                ++hits;
            }
        }
        c.check(hits == 300 && tree.check().ok && tree.size() == 300,
                u8"打乱顺序插入 300 个键：全部找得到、不变式成立");
    }

    /* 19. 旋转不改变中序：插入过程中的中序始终有序 */
    {
        RbTree tree;
        Lcg lcg(11u);
        bool always_sorted = true;
        for (std::size_t i = 0; i < 300; ++i) {
            tree.insert(static_cast<int>(lcg.next() % 3000u));
            const std::vector<int> order = tree.inorder();
            for (std::size_t k = 1; k < order.size(); ++k) {
                if (!(order[k - 1] < order[k])) {
                    always_sorted = false;
                }
            }
        }
        c.check(always_sorted, u8"插入过程中每一步的中序都严格递增（旋转不改中序）");
    }

    /* 20. 删除过程中每一步都校验 */
    {
        RbTree tree;
        std::vector<int> keys;
        Lcg lcg(13u);
        for (std::size_t i = 0; i < 200; ++i) {
            const int key = static_cast<int>(lcg.next() % 2000u);
            if (tree.insert(key)) {
                keys.push_back(key);
            }
        }
        std::size_t failures = 0;
        for (int key : keys) {
            tree.erase(key);
            if (!tree.check().ok) {
                ++failures;
            }
        }
        c.check(failures == 0 && tree.size() == 0,
                u8"把插入的键再逐个删掉，每删一步校验一次：失败 0 次",
                std::to_string(keys.size()) + u8" 个键");
    }

    return c.take();
}

}   /* namespace rbmini */
