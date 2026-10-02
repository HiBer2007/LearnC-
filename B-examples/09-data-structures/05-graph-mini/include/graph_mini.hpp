/**
 * graph_mini.hpp —— 图的三种存法，与同一套回调式遍历接口
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
 * 三种存法各自实现同一组成员函数：
 *
 *   AdjList    邻接表：每个点一个数组，只存它真正的邻居
 *   AdjMatrix  邻接矩阵：一位一条边，查边是常数时间，取邻居要扫一整行
 *   EdgeList   边表：只存一串 (u, v)，加边最便宜，查边与取邻居都要扫全表
 *
 * 算法只依赖 for_each_neighbor 这一个接口，因此换存法不用改算法一个字。
 * 顶点编号一律是 0 到 n-1，邻居一律按编号升序回调——顺序定死，
 * 三种存法给出的遍历序列才可能逐位相同。
 *
 * 面向人的文字一律是 u8"" 字面量，因此本头文件里的 std::string 承载 UTF-8 字节。
 */
#ifndef GRAPH_MINI_HPP
#define GRAPH_MINI_HPP

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace gmini {

/* ================= 计数 ================= */

/** 全局计数，按「先 reset、再做一件事、读增量」的方式用 */
struct GraphStats {
    std::size_t neighbor_visits = 0;   /**< 回调被调用的次数（一个点一次算一次） */
    std::size_t probes = 0;            /**< 查边或取邻居时检查过的位置数 */

    void reset();
};

GraphStats &stats();

/* ================= 邻接表 ================= */

class AdjList {
public:
    explicit AdjList(int n) : adj_(static_cast<std::size_t>(n)) {}

    static const char *name() { return u8"邻接表"; }

    int vertex_count() const { return static_cast<int>(adj_.size()); }

    std::size_t edge_count() const { return edges_; }

    void add_undirected(int a, int b)
    {
        adj_[static_cast<std::size_t>(a)].push_back(b);     /* 两个方向各存一次 */
        adj_[static_cast<std::size_t>(b)].push_back(a);
        ++edges_;
    }

    void add_directed(int from, int to)
    {
        adj_[static_cast<std::size_t>(from)].push_back(to); /* 只存出边 */
        ++edges_;
    }

    /** 查边：在 from 的邻居里线性找。查一次最多比较 degree(from) 次 */
    bool has_edge(int u, int v) const
    {
        for (int neighbor : adj_[static_cast<std::size_t>(u)]) {
            ++stats().probes;
            if (neighbor == v) {
                return true;
            }
        }
        return false;
    }

    template <class F>
    void for_each_neighbor(int v, F f) const
    {
        for (int neighbor : adj_[static_cast<std::size_t>(v)]) {
            ++stats().probes;
            ++stats().neighbor_visits;
            f(neighbor);
        }
    }

    std::size_t degree(int v) const { return adj_[static_cast<std::size_t>(v)].size(); }

    /** 每个点一个 vector 头部，加上各内层数组的容量 */
    std::size_t memory_bytes() const
    {
        std::size_t bytes = adj_.size() * sizeof(std::vector<int>);
        for (const std::vector<int> &row : adj_) {
            bytes += row.capacity() * sizeof(int);
        }
        return bytes;
    }

    /** 让每个内层数组的容量正好等于度数，报告里的字节数才只由图决定 */
    void shrink_rows()
    {
        for (std::vector<int> &row : adj_) {
            row.shrink_to_fit();
        }
    }

private:
    std::vector<std::vector<int>> adj_;
    std::size_t edges_ = 0;
};

/* ================= 邻接矩阵（位图） ================= */

class AdjMatrix {
public:
    explicit AdjMatrix(int n)
        : n_(n), bits_(static_cast<std::size_t>(n) * static_cast<std::size_t>(n) / 64u + 1u, 0)
    {
    }

    static const char *name() { return u8"邻接矩阵（位图）"; }

    int vertex_count() const { return n_; }

    std::size_t edge_count() const { return edges_; }

    void add_undirected(int a, int b)
    {
        set(a, b);
        set(b, a);
        ++edges_;
    }

    void add_directed(int from, int to)
    {
        set(from, to);
        ++edges_;
    }

    /** 查边：算一个下标再取一位，与密度无关 */
    bool has_edge(int u, int v) const
    {
        ++stats().probes;
        return get(u, v);
    }

    /** 取邻居：矩阵没有邻居列表，只能扫一整行 */
    template <class F>
    void for_each_neighbor(int v, F f) const
    {
        for (int u = 0; u < n_; ++u) {
            ++stats().probes;
            if (get(v, u)) {
                ++stats().neighbor_visits;
                f(u);
            }
        }
    }

    std::size_t degree(int v) const
    {
        std::size_t count = 0;
        for (int u = 0; u < n_; ++u) {
            if (get(v, u)) {
                ++count;
            }
        }
        return count;
    }

    std::size_t memory_bytes() const { return bits_.size() * sizeof(std::uint64_t); }

private:
    void set(int a, int b)
    {
        const std::size_t index = static_cast<std::size_t>(a) * static_cast<std::size_t>(n_)
                                  + static_cast<std::size_t>(b);
        bits_[index >> 6] |= 1ULL << (index & 63u);     /* 除以 64 与取余 64 就是移位与掩码 */
    }

    bool get(int a, int b) const
    {
        const std::size_t index = static_cast<std::size_t>(a) * static_cast<std::size_t>(n_)
                                  + static_cast<std::size_t>(b);
        return (bits_[index >> 6] >> (index & 63u)) & 1ULL;
    }

    int n_ = 0;
    std::vector<std::uint64_t> bits_;
    std::size_t edges_ = 0;
};

/* ================= 边表 ================= */

class EdgeList {
public:
    struct Edge {
        int from = 0;
        int to = 0;
    };

    explicit EdgeList(int n) : n_(n) {}

    static const char *name() { return u8"边表"; }

    int vertex_count() const { return n_; }

    std::size_t edge_count() const { return edge_total_; }

    void add_undirected(int a, int b)
    {
        edges_.push_back(Edge{a, b});       /* 两个方向各一条 */
        edges_.push_back(Edge{b, a});
        ++edge_total_;
    }

    void add_directed(int from, int to)
    {
        edges_.push_back(Edge{from, to});
        ++edge_total_;
    }

    /** 查边：没有索引，只能扫全表 */
    bool has_edge(int u, int v) const
    {
        for (const Edge &edge : edges_) {
            ++stats().probes;
            if (edge.from == u && edge.to == v) {
                return true;
            }
        }
        return false;
    }

    template <class F>
    void for_each_neighbor(int v, F f) const
    {
        for (const Edge &edge : edges_) {
            ++stats().probes;
            if (edge.from == v) {
                ++stats().neighbor_visits;
                f(edge.to);
            }
        }
    }

    std::size_t degree(int v) const
    {
        std::size_t count = 0;
        for (const Edge &edge : edges_) {
            if (edge.from == v) {
                ++count;
            }
        }
        return count;
    }

    std::size_t memory_bytes() const { return edges_.size() * sizeof(Edge); }

    /** 让边数组的容量正好等于条数，报告里的字节数才只由图决定 */
    void shrink_rows() { edges_.shrink_to_fit(); }

private:
    int n_ = 0;
    std::vector<Edge> edges_;
    std::size_t edge_total_ = 0;
};

/* ================= 只依赖接口的算法 ================= */

/** 广度优先：队列 + 访问标记，返回访问顺序 */
template <class G>
std::vector<int> bfs_order(const G &graph, int source)
{
    const std::size_t n = static_cast<std::size_t>(graph.vertex_count());
    std::vector<char> seen(n, 0);
    std::vector<int> order;
    std::vector<int> queue;
    seen[static_cast<std::size_t>(source)] = 1;
    queue.push_back(source);
    for (std::size_t head = 0; head < queue.size(); ++head) {
        const int v = queue[head];
        order.push_back(v);
        graph.for_each_neighbor(v, [&](int u) {
            if (!seen[static_cast<std::size_t>(u)]) {
                seen[static_cast<std::size_t>(u)] = 1;
                queue.push_back(u);
            }
        });
    }
    return order;
}

/** 深度优先：显式栈，不用递归——链状的图会把递归深度顶到点数 */
template <class G>
std::vector<int> dfs_order(const G &graph, int source)
{
    const std::size_t n = static_cast<std::size_t>(graph.vertex_count());
    std::vector<char> seen(n, 0);
    std::vector<int> order;
    std::vector<int> stack;
    stack.push_back(source);
    while (!stack.empty()) {
        const int v = stack.back();
        stack.pop_back();
        if (seen[static_cast<std::size_t>(v)]) {
            continue;
        }
        seen[static_cast<std::size_t>(v)] = 1;
        order.push_back(v);
        std::vector<int> neighbors;
        graph.for_each_neighbor(v, [&](int u) { neighbors.push_back(u); });
        /* 倒着压栈，弹出时就是编号小的先走，序列与存法无关 */
        for (std::size_t i = neighbors.size(); i > 0; --i) {
            if (!seen[static_cast<std::size_t>(neighbors[i - 1])]) {
                stack.push_back(neighbors[i - 1]);
            }
        }
    }
    return order;
}

/** 连通块编号：用块里最小的顶点编号当标号，因此三种存法结果必然一致 */
template <class G>
std::vector<int> component_label(const G &graph)
{
    const int n = graph.vertex_count();
    std::vector<int> label(static_cast<std::size_t>(n), -1);
    std::vector<int> stack;
    for (int start = 0; start < n; ++start) {
        if (label[static_cast<std::size_t>(start)] >= 0) {
            continue;
        }
        label[static_cast<std::size_t>(start)] = start;
        stack.push_back(start);
        while (!stack.empty()) {
            const int v = stack.back();
            stack.pop_back();
            graph.for_each_neighbor(v, [&](int u) {
                if (label[static_cast<std::size_t>(u)] < 0) {
                    label[static_cast<std::size_t>(u)] = start;
                    stack.push_back(u);
                }
            });
        }
    }
    return label;
}

/** 把所有点的邻居都走一遍，数一数回调了多少次。有向图等于边数，无向图等于两倍边数 */
template <class G>
std::size_t count_all_neighbor_visits(const G &graph)
{
    std::size_t total = 0;
    for (int v = 0; v < graph.vertex_count(); ++v) {
        graph.for_each_neighbor(v, [&](int) { ++total; });
    }
    return total;
}

/** 拓扑排序（Kahn）。有环时返回空表 */
template <class G>
std::vector<int> topological_order(const G &graph)
{
    const int n = graph.vertex_count();
    std::vector<int> indegree(static_cast<std::size_t>(n), 0);
    for (int v = 0; v < n; ++v) {
        graph.for_each_neighbor(v, [&](int u) { ++indegree[static_cast<std::size_t>(u)]; });
    }
    std::vector<int> queue;
    for (int v = 0; v < n; ++v) {
        if (indegree[static_cast<std::size_t>(v)] == 0) {
            queue.push_back(v);
        }
    }
    std::vector<int> order;
    for (std::size_t head = 0; head < queue.size(); ++head) {
        const int v = queue[head];
        order.push_back(v);
        graph.for_each_neighbor(v, [&](int u) {
            --indegree[static_cast<std::size_t>(u)];
            if (indegree[static_cast<std::size_t>(u)] == 0) {
                queue.push_back(u);
            }
        });
    }
    if (order.size() != static_cast<std::size_t>(n)) {
        return std::vector<int>();      /* 还有入度不为 0 的点：有环 */
    }
    return order;
}

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

/** 项目输出：三种存法、换存法不改算法、有向图、操作次数对照四段。
    返回多行 UTF-8 文本，末尾带一个换行；由界面层决定怎么显示 */
std::string build_report();

/** 逐项核对三种存法的边、邻居、遍历序列与算法结果 */
CheckResult run_self_tests();

}   /* namespace gmini */

#endif /* GRAPH_MINI_HPP */
