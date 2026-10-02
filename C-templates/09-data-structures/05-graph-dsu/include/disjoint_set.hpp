/* disjoint_set.hpp —— 练习模板 05 的并查集接口（C++）
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
 * 并查集只有二十行，两处优化各只有几行：
 *
 *     阶段 3  find()    路径压缩：走过的路压平，下次一步就到
 *     阶段 4  unite()   按大小合并：把小的树挂到大的下面，树别长高
 *
 * 两处优化的实现在 src/disjoint_set.cpp 里；这个头文件里的计数器（步数、树高、
 * 树的棵数）都已经给出，练习不需要改。
 *
 * 「步数」的口径：find 每往上走一级算一步（走一步用的是 step_to）。
 * 压缩那一段不记步数——测的是「查找要走多远」，不是「改了几个指针」。
 */
#ifndef DISJOINT_SET_HPP
#define DISJOINT_SET_HPP

#include <vector>

namespace dsg {

class DisjointSet {
public:
    /* n 个点，每个点自成一集合：根记自己，大小为 1 */
    explicit DisjointSet(int n);

    /* 阶段 3-1：求 x 所在集合的根 */
    int find(int x);

    /* 阶段 4-1：合并 a 与 b 所在的两个集合；本来就在一起时返回 false */
    bool unite(int a, int b);

    /* 已给出：是不是同一个集合 */
    bool same(int a, int b) { return find(a) == find(b); }

    /* 已给出：先求根，再读根上记的大小。
     * 注意非根节点上的大小是陈旧值，不能直接拿来当「这个集合有多大」。 */
    int size_of(int x);

    int node_count(void) const { return n_; }

    /* 已给出：最深的那棵树有多高（根记 0 层） */
    int max_height(void) const;

    /* 已给出：还剩几棵树。全部合并之后应当是 1 */
    int distinct_roots(void) const;

    /* 已给出：find 走了多少步；reset 之后重新计 */
    long steps(void) const { return steps_; }
    void reset_steps(void) { steps_ = 0; }

    /* 测试工具（已给出）：把 child 所在树的根直接挂到 root 所在树的根下面。
     * 验收程序用它手工搭出很高的树，检验路径压缩有没有生效。 */
    void link_raw(int child, int root);

private:
    /* 已给出：往上走一级，返回 x 的父亲，顺便记一步。find 的循环里请用它 */
    int step_to(int x) const;

    /* 已给出：找根，但不记步数（计数器自己用） */
    int root_of_unchecked(int x) const;

    std::vector<int> parent_;       /* parent_[x] == x 表示 x 是根 */
    std::vector<int> size_;         /* 只在根上有意义：这个集合有几个元素 */
    int n_ = 0;
    mutable long steps_ = 0;
};

} /* namespace dsg */

#endif /* DISJOINT_SET_HPP */
