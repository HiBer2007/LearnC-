/**
 * graph_lab.hpp —— 图上的结构性问题：拓扑排序、强连通分量、割点与桥、最小生成树
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
 * 报告里的每一个数字都由这里的计数器数出来，重跑逐位相同：
 *
 *   visits             访问次数：第一次走进一个顶点算一次
 *   edge_checks        边检查次数：看一眼邻接表里的一条记录算一次
 *   low_updates        low 值被改小的次数（无向图的割点与桥、有向图的强连通分量各一套）
 *   enqueues/dequeues  队列的入队与出队次数；树的遍历里是入栈与出栈次数
 *   find_calls         并查集 find 被调用的次数
 *   find_hops          并查集 find 沿父指针走过的总步数
 *   sort_comparisons   自己写的插入排序里比较权值的次数
 *   pushes/pops        优先队列的进与出（Prim），其中弹出时顶点已进树的算过期弹出
 *
 * 三张图都写死在 make_* 里，不读任何外部文件。邻接表按顶点编号升序排好，
 * 遍历顺序只由编号决定、与加边的先后无关：dfn、low、拓扑序才能重跑逐位相同。
 *
 * 无向图 G 用来讲割点、桥与最小生成树；有向图 D 的无环版用来讲拓扑排序，
 * 有环版（在无环版上加三条回边）用来讲 Kahn 怎样发现环与强连通分量。
 *
 * 递归实现的只有两处：割点与桥的 DFS、Tarjan 的强连通分量。两处的图都只有
 * 9 个顶点，深度不成问题；换成几十万个顶点的图要改成显式栈，见 README 的「已知问题」。
 */
#ifndef GRAPH_LAB_HPP
#define GRAPH_LAB_HPP

#include <cstddef>
#include <string>
#include <vector>

namespace glab {

/* ================= 图 ================= */

/** 一条边。无向图里统一记成 u < v；有向图里方向是 u -> v */
struct Edge {
    int u = 0;
    int v = 0;
    int weight = 1;
};

inline bool operator==(const Edge &lhs, const Edge &rhs)
{
    return lhs.u == rhs.u && lhs.v == rhs.v && lhs.weight == rhs.weight;
}

inline bool operator!=(const Edge &lhs, const Edge &rhs)
{
    return !(lhs == rhs);
}

/** 邻接表加边表。顶点编号 0 到 n - 1。
    无向图里一条边会进两个顶点的邻接表，边表里只留一条。
    重边与自环会被忽略：教学数据里没有这两种情况。 */
class Graph {
public:
    Graph(int vertex_count, bool directed);

    void add_edge(int u, int v, int weight = 1);

    int vertex_count() const { return vertex_count_; }
    bool directed() const { return directed_; }
    const std::vector<Edge> &edges() const { return edges_; }
    const std::vector<int> &neighbors(int v) const
    {
        return adjacency_[static_cast<std::size_t>(v)];
    }
    std::size_t degree(int v) const { return neighbors(v).size(); }

    /** 邻接矩阵，行主序：第 u 行第 v 列是 matrix[u * n + v]。0 表示没有边 */
    std::vector<int> adjacency_matrix() const;

private:
    int vertex_count_;
    bool directed_;
    std::vector<Edge> edges_;
    std::vector<std::vector<int>> adjacency_;
};

/** 顶点 u 与 v 之间那条边的权值；没有这条边时返回 0 */
int weight_of(const Graph &g, int u, int v);

/** 无向图 G：9 个顶点、10 条边、连通、带权 */
Graph make_undirected_graph();

/** 有向图 D 的无环版：9 个顶点、10 条边 */
Graph make_dag();

/** 有向图 D 的有环版：无环版加三条回边 7 -> 0、8 -> 7、6 -> 5 */
Graph make_cyclic_digraph();

/* ================= 连通块 ================= */

/** 整张图的连通块大小，升序 */
std::vector<std::size_t> component_sizes(const Graph &g);

/** 删掉一个顶点（连同它的边）之后，剩下的图分成几块，每块多大，升序 */
std::vector<std::size_t> component_sizes_without_vertex(const Graph &g, int removed);

/** 删掉一条边之后，连通块大小，升序。用来按定义核对桥 */
std::vector<std::size_t> component_sizes_without_edge(const Graph &g, const Edge &removed);

/* ================= 拓扑排序 ================= */

struct TopoRun {
    std::vector<int> order;               /**< 排出来的序列 */
    std::size_t enqueues = 0;             /**< 入队 / 入栈次数 */
    std::size_t dequeues = 0;             /**< 出队 / 出栈次数 */
    std::size_t edge_checks = 0;          /**< 检查过的出边条数 */
    bool acyclic = false;                 /**< Kahn：出队次数等于顶点数才算排完 */
    std::vector<int> remaining;           /**< Kahn 没能排上位置的顶点，升序 */
    std::vector<int> remaining_in_degree; /**< 与 remaining 一一对应的剩余入度 */
};

/** Kahn：数一遍入度，入度为 0 的进最小堆，出队一个就把它出边终点的入度减一 */
TopoRun topo_sort_kahn(const Graph &g);

/** DFS 后序：按编号挑起点做深度优先，回溯时接在序列尾部，最后整体取逆 */
TopoRun topo_sort_dfs_postorder(const Graph &g);

/** 这个序列是不是拓扑序：要求每条边 (u, v) 都满足 u 排在 v 前面 */
bool is_topological_order(const Graph &g, const std::vector<int> &order);

/* ================= 强连通分量 ================= */

struct SccRun {
    std::vector<std::vector<int>> components; /**< 每个分量的顶点，升序；分量按弹出的先后排 */
    std::vector<int> component_of;            /**< 顶点属于第几个分量 */
    std::size_t visits = 0;                   /**< 访问次数 */
    std::size_t edge_checks = 0;              /**< 检查过的出边条数 */
    std::size_t low_updates = 0;              /**< low 值被改小的次数 */
    std::size_t pushes = 0;                   /**< Tarjan 栈的入栈次数 */
    std::size_t pops = 0;                     /**< Tarjan 栈的出栈次数 */
};

/** Tarjan：一次 DFS，dfn 记访问的先后，low 记能回到的最小 dfn，另有一个栈。
    low 与 dfn 相等的顶点退栈时，栈里从它往上的顶点合成一个分量 */
SccRun tarjan_scc(const Graph &g);

/* ================= 割点与桥 ================= */

struct ConnectivityRun {
    std::vector<int> dfn;           /**< 访问的先后，从 1 开始；0 表示没访问到 */
    std::vector<int> low;           /**< 这个顶点和它的后代能碰到的最小 dfn */
    std::vector<int> parent;        /**< DFS 树上的父；-1 表示根 */
    std::vector<int> dfs_children;  /**< DFS 树上的孩子数 */
    std::vector<int> dfs_order;     /**< 顶点被访问的先后 */
    std::vector<Edge> tree_edges;   /**< DFS 树的树边，u 是父、v 是子 */
    std::vector<int> cut_vertices;  /**< 割点，升序 */
    std::vector<Edge> bridges;      /**< 桥，按 (u, v) 升序 */
    std::size_t visits = 0;
    std::size_t edge_checks = 0;
    std::size_t low_updates = 0;
};

ConnectivityRun analyze_connectivity(const Graph &g);

/** 按定义重算一遍 low：取「v 的子树里的顶点连出去的非树边」另一端 dfn 的最小值。
    树边不算回边，所以要排掉两端互为父子的边。用来核对一次 DFS 里算出来的 low */
std::vector<int> low_by_brute_force(const Graph &g, const ConnectivityRun &run);

/** 按定义找桥：逐条删掉再看连通块数有没有变多 */
std::vector<Edge> bridges_by_brute_force(const Graph &g);

/** 按定义找割点：逐个删掉再看连通块数有没有变多 */
std::vector<int> cut_vertices_by_brute_force(const Graph &g);

/* ================= 最小生成树 ================= */

/** 并查集：按大小合并，查找时路径压缩。两个计数都记在里面 */
class DisjointSet {
public:
    explicit DisjointSet(int count);

    /** 找根，顺手把走过的链挂到根上 */
    int find(int x);

    /** 两个顶点不在同一棵树里就合并，返回是否真的合并了 */
    bool unite(int a, int b);

    std::size_t find_calls() const { return find_calls_; }
    std::size_t find_hops() const { return find_hops_; }
    std::size_t unions() const { return unions_; }

private:
    std::vector<int> parent_;
    std::vector<int> size_;
    std::size_t find_calls_ = 0;
    std::size_t find_hops_ = 0;
    std::size_t unions_ = 0;
};

struct KruskalRun {
    std::vector<Edge> chosen;           /**< 选中的边，按权值升序 */
    std::vector<Edge> sorted_edges;     /**< 排好序的全部边 */
    int total_weight = 0;
    std::size_t edge_checks = 0;        /**< 看过的边条数 */
    std::size_t sort_comparisons = 0;   /**< 插入排序里比较权值的次数 */
    std::size_t find_calls = 0;
    std::size_t find_hops = 0;
    std::size_t unions = 0;
    std::size_t rejected = 0;           /**< 因为两端已经连通而跳过的边 */
    bool spanning = false;              /**< 选出的边是不是一棵生成树 */
};

KruskalRun kruskal_mst(const Graph &g);

struct PrimRun {
    std::vector<Edge> chosen;        /**< 选中的边，按顶点进树的先后 */
    int total_weight = 0;
    int start = 0;
    std::size_t pushes = 0;          /**< 优先队列入队次数 */
    std::size_t pops = 0;            /**< 优先队列出队次数 */
    std::size_t stale_pops = 0;      /**< 弹出的顶点已经进树，这一条作废 */
    std::size_t heap_left = 0;       /**< 收工时堆里还剩几条 */
    std::size_t edge_checks = 0;     /**< 顶点进树时扫过的邻接条目数 */
    bool spanning = false;
};

PrimRun prim_mst(const Graph &g, int start);

/* ================= 报告与自测 ================= */

/** 自测结果。不打印，交给界面决定怎么显示 */
struct CheckResult {
    std::size_t total = 0;
    std::size_t passed = 0;
    std::size_t failed = 0;
    std::vector<std::string> lines; /**< 每项一行，例如 "[通过] 3. ……" */

    bool all_passed() const { return failed == 0; }
    std::string summary() const;    /**< "24 项中 24 项通过，全部通过" */
};

/** 项目输出：两张图、拓扑排序、环的发现、强连通分量、割点与桥、最小生成树六段。
    返回多行 UTF-8 文本，末尾带一个换行；由界面层决定怎么显示 */
std::string build_report();

/** 逐项核对图的结构、两套拓扑序、强连通分量、割点与桥、两版最小生成树 */
CheckResult run_self_tests();

}   /* namespace glab */

#endif /* GRAPH_LAB_HPP */
