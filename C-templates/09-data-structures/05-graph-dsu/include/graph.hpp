/* graph.hpp —— 练习模板 05 的三种图存法（C++）
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
 * 三种存法（邻接表、位图矩阵、边表）实现**同一个**遍历接口：
 *
 *     template <class F> void for_each_neighbor(int v, F f) const;
 *
 * 接口设计的目标只有一个：**让算法不必知道底下是哪种存法**。
 * include/traverse.hpp 里的广度优先与深度优先只调用这个接口，
 * 因此换一种存法，算法一个字都不用改。三处 TODO 就是这个接口的三份实现。
 *
 * 题目、验收标准与自查方法见同目录《配置步骤.md》。
 */
#ifndef GRAPH_HPP
#define GRAPH_HPP

#include <cstddef>
#include <cstdint>
#include <vector>

namespace dsg {

/* ------------------------------------------------------------------
 * 存法一：邻接表。每个点一个数组，只存它真正的邻居。
 * ------------------------------------------------------------------ */
class AdjList {
public:
    explicit AdjList(int n) : adj_(static_cast<std::size_t>(n)) {}

    int node_count(void) const { return static_cast<int>(adj_.size()); }

    /* 已给出：无向图，两个方向各存一次 */
    void add_edge(int a, int b) {
        adj_[static_cast<std::size_t>(a)].push_back(b);
        adj_[static_cast<std::size_t>(b)].push_back(a);
    }

    /* TODO（阶段 1-1）：对 v 的每个邻居调用一次 f。
     * 判据：邻居列表与边集合一致，并且 traverse.hpp 里的 bfs_distance 能跑出结果。 */
    template <class F>
    void for_each_neighbor(int v, F f) const {
        (void)v;
        (void)f;
        /* 占位实现：一个邻居都不报 */
    }

private:
    std::vector<std::vector<int>> adj_;
};

/* ------------------------------------------------------------------
 * 存法二：位图矩阵。一格一位，n×n 位。
 * 查一条边是 O(1)，但取一个点的邻居要扫整行。
 * ------------------------------------------------------------------ */
class BitMatrix {
public:
    explicit BitMatrix(int n)
        : n_(n), bits_(static_cast<std::size_t>(n) * static_cast<std::size_t>(n) / 64U + 1U, 0U) {}

    int node_count(void) const { return n_; }

    /* 已给出：无向图，两个位置都要置位 */
    void add_edge(int a, int b) {
        set(a, b);
        set(b, a);
    }

    /* 已给出：查一条边。除以 64 与取余 64 就是移位与掩码 */
    bool get(int a, int b) const {
        const std::size_t i = index(a, b);
        return ((bits_[i >> 6] >> (i & 63U)) & 1ULL) != 0ULL;
    }

    /* TODO（阶段 2-1）：对 v 的每个邻居调用一次 f。
     * 判据：与邻接表给出同样的邻居集合（顺序可以不同），
     * 并且 bfs_distance 得到的距离数组与邻接表完全一致。 */
    template <class F>
    void for_each_neighbor(int v, F f) const {
        (void)v;
        (void)f;
        /* 占位实现：一个邻居都不报 */
    }

private:
    std::size_t index(int a, int b) const {
        return static_cast<std::size_t>(a) * static_cast<std::size_t>(n_) +
               static_cast<std::size_t>(b);
    }

    void set(int a, int b) {
        const std::size_t i = index(a, b);
        bits_[i >> 6] |= (1ULL << (i & 63U));
    }

    int n_;
    std::vector<std::uint64_t> bits_;
};

/* ------------------------------------------------------------------
 * 存法三：边表。只有一串 (u, v)，不建索引。
 * 加边最便宜，查边与取邻居都要扫全表。
 * ------------------------------------------------------------------ */
class EdgeList {
public:
    struct Edge {
        int from;
        int to;
    };

    explicit EdgeList(int n) : n_(n) {}

    int node_count(void) const { return n_; }

    /* 已给出：无向图只存一份，取邻居时两个方向都要看 */
    void add_edge(int a, int b) {
        edges_.push_back(Edge{a, b});
    }

    std::size_t edge_count(void) const { return edges_.size(); }

    /* TODO（阶段 2-2）：对 v 的每个邻居调用一次 f。
     * 提示：无向边只存了一份，因此一条边可能以 v 为 from，也可能以 v 为 to。
     * 判据：与前两种存法给出同样的邻居集合。 */
    template <class F>
    void for_each_neighbor(int v, F f) const {
        (void)v;
        (void)f;
        /* 占位实现：一个邻居都不报 */
    }

private:
    int n_;
    std::vector<Edge> edges_;
};

} /* namespace dsg */

#endif /* GRAPH_HPP */
