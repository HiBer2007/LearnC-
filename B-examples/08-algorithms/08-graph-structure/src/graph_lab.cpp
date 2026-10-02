/**
 * graph_lab.cpp —— 图上的结构性问题：拓扑排序、强连通分量、割点与桥、最小生成树
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

#include "graph_lab.hpp"

#include <algorithm>
#include <functional>
#include <queue>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace glab {

/* ================= 图 ================= */

namespace {

/** 把 v 插进升序的列表里；已经有了就不动。
    加边的先后因此不影响邻接表，遍历顺序只由顶点编号决定 */
void insert_sorted(std::vector<int> *list, int v)
{
    const std::vector<int>::iterator pos = std::lower_bound(list->begin(), list->end(), v);
    if (pos == list->end() || *pos != v) {
        list->insert(pos, v);
    }
}

}   /* namespace */

Graph::Graph(int vertex_count, bool directed)
    : vertex_count_(vertex_count),
      directed_(directed),
      adjacency_(static_cast<std::size_t>(vertex_count))
{
}

void Graph::add_edge(int u, int v, int weight)
{
    if (u == v) {
        return;                       /* 自环不进教学数据 */
    }
    int a = u;
    int b = v;
    if (!directed_ && a > b) {
        std::swap(a, b);              /* 无向图里统一记成 u < v */
    }
    for (const Edge &e : edges_) {
        if (e.u == a && e.v == b) {
            return;                   /* 重边不进教学数据 */
        }
    }
    edges_.push_back(Edge{a, b, weight});
    insert_sorted(&adjacency_[static_cast<std::size_t>(a)], b);
    if (!directed_) {
        insert_sorted(&adjacency_[static_cast<std::size_t>(b)], a);
    }
}

std::vector<int> Graph::adjacency_matrix() const
{
    const std::size_t n = static_cast<std::size_t>(vertex_count_);
    std::vector<int> matrix(n * n, 0);
    for (const Edge &e : edges_) {
        matrix[static_cast<std::size_t>(e.u) * n + static_cast<std::size_t>(e.v)] = e.weight;
        if (!directed_) {
            matrix[static_cast<std::size_t>(e.v) * n + static_cast<std::size_t>(e.u)] = e.weight;
        }
    }
    return matrix;
}

int weight_of(const Graph &g, int u, int v)
{
    for (const Edge &e : g.edges()) {
        if ((e.u == u && e.v == v) || (!g.directed() && e.u == v && e.v == u)) {
            return e.weight;
        }
    }
    return 0;
}

Graph make_undirected_graph()
{
    /* 两个三角形用一条边串起来，两端各挂一条链：
         0-1-2 是一个三角形，3-4-5 是另一个，中间靠 1-3 相连，
         顶点 8 挂在 1 上，顶点 6、7 挂在 5 下面成一条链 */
    Graph g(9, false);
    g.add_edge(0, 1, 4);
    g.add_edge(0, 2, 3);
    g.add_edge(1, 2, 1);
    g.add_edge(1, 3, 2);
    g.add_edge(1, 8, 9);
    g.add_edge(3, 4, 5);
    g.add_edge(3, 5, 7);
    g.add_edge(4, 5, 6);
    g.add_edge(5, 6, 8);
    g.add_edge(6, 7, 10);
    return g;
}

Graph make_dag()
{
    /* 无环版：两个源点 0 与 4，汇到 7、8，再分给 3 与 5，最后汇到 6 */
    Graph g(9, true);
    g.add_edge(0, 7);
    g.add_edge(1, 8);
    g.add_edge(2, 8);
    g.add_edge(3, 6);
    g.add_edge(4, 7);
    g.add_edge(5, 6);
    g.add_edge(7, 1);
    g.add_edge(7, 2);
    g.add_edge(8, 3);
    g.add_edge(8, 5);
    return g;
}

Graph make_cyclic_digraph()
{
    Graph g = make_dag();
    g.add_edge(7, 0);                 /* 回边一：把 0 与 7 圈在一起 */
    g.add_edge(8, 7);                 /* 回边二：把 1、2、7、8 圈在一起 */
    g.add_edge(6, 5);                 /* 回边三：把 5 与 6 圈在一起 */
    return g;
}

/* ================= 连通块 ================= */

namespace {

const Edge kNoEdge{-1, -1, 0};

bool is_banned(const Edge &banned, int a, int b)
{
    return (banned.u == a && banned.v == b) || (banned.u == b && banned.v == a);
}

/** 从一个顶点出发走一遍。removed 是要跳过的顶点（-1 表示不跳过），
    banned 是要跳过的边。走过哪些顶点记在 seen 里，顺序记在 out 里 */
void walk(const Graph &g, int start, int removed, const Edge &banned,
          std::vector<char> *seen, std::vector<int> *out)
{
    std::vector<int> stack;
    stack.push_back(start);
    (*seen)[static_cast<std::size_t>(start)] = 1;
    while (!stack.empty()) {
        const int v = stack.back();
        stack.pop_back();
        if (out != nullptr) {
            out->push_back(v);
        }
        for (const int u : g.neighbors(v)) {
            if (u == removed || is_banned(banned, v, u)) {
                continue;
            }
            if (!(*seen)[static_cast<std::size_t>(u)]) {
                (*seen)[static_cast<std::size_t>(u)] = 1;
                stack.push_back(u);
            }
        }
    }
}

std::vector<std::size_t> sizes_with(const Graph &g, int removed, const Edge &banned)
{
    std::vector<std::size_t> sizes;
    std::vector<char> seen(static_cast<std::size_t>(g.vertex_count()), 0);
    for (int v = 0; v < g.vertex_count(); ++v) {
        if (v == removed || seen[static_cast<std::size_t>(v)]) {
            continue;
        }
        std::vector<int> block;
        walk(g, v, removed, banned, &seen, &block);
        sizes.push_back(block.size());
    }
    std::sort(sizes.begin(), sizes.end());
    return sizes;
}

}   /* namespace */

std::vector<std::size_t> component_sizes(const Graph &g)
{
    return sizes_with(g, -1, kNoEdge);
}

std::vector<std::size_t> component_sizes_without_vertex(const Graph &g, int removed)
{
    return sizes_with(g, removed, kNoEdge);
}

std::vector<std::size_t> component_sizes_without_edge(const Graph &g, const Edge &removed)
{
    return sizes_with(g, -1, removed);
}

/* ================= 拓扑排序 ================= */

TopoRun topo_sort_kahn(const Graph &g)
{
    TopoRun run;
    const int n = g.vertex_count();
    std::vector<int> in_degree(static_cast<std::size_t>(n), 0);
    for (const Edge &e : g.edges()) {
        ++in_degree[static_cast<std::size_t>(e.v)];
    }

    /* 用最小堆代替普通队列：多个入度为 0 的顶点同时可选时，
       每次取编号最小的那个，出队顺序只由入度表决定，重跑逐位相同 */
    std::priority_queue<int, std::vector<int>, std::greater<int>> ready;
    for (int v = 0; v < n; ++v) {
        if (in_degree[static_cast<std::size_t>(v)] == 0) {
            ready.push(v);
            ++run.enqueues;
        }
    }

    while (!ready.empty()) {
        const int v = ready.top();
        ready.pop();
        ++run.dequeues;
        run.order.push_back(v);
        for (const int u : g.neighbors(v)) {
            ++run.edge_checks;
            if (--in_degree[static_cast<std::size_t>(u)] == 0) {
                ready.push(u);
                ++run.enqueues;
            }
        }
    }

    run.acyclic = (run.order.size() == static_cast<std::size_t>(n));
    for (int v = 0; v < n; ++v) {
        if (in_degree[static_cast<std::size_t>(v)] > 0) {
            run.remaining.push_back(v);
            run.remaining_in_degree.push_back(in_degree[static_cast<std::size_t>(v)]);
        }
    }
    return run;
}

TopoRun topo_sort_dfs_postorder(const Graph &g)
{
    TopoRun run;
    const int n = g.vertex_count();
    std::vector<char> visited(static_cast<std::size_t>(n), 0);
    std::vector<std::size_t> next_index(static_cast<std::size_t>(n), 0);
    std::vector<int> stack;
    std::vector<int> post_order;

    for (int start = 0; start < n; ++start) {
        if (visited[static_cast<std::size_t>(start)]) {
            continue;
        }
        stack.push_back(start);
        visited[static_cast<std::size_t>(start)] = 1;
        ++run.enqueues;
        while (!stack.empty()) {
            const int v = stack.back();
            const std::vector<int> &out = g.neighbors(v);
            if (next_index[static_cast<std::size_t>(v)] < out.size()) {
                const int u = out[next_index[static_cast<std::size_t>(v)]++];
                ++run.edge_checks;
                if (!visited[static_cast<std::size_t>(u)]) {
                    visited[static_cast<std::size_t>(u)] = 1;
                    stack.push_back(u);
                    ++run.enqueues;
                }
            } else {
                post_order.push_back(v);
                stack.pop_back();
                ++run.dequeues;
            }
        }
    }

    run.order.assign(post_order.rbegin(), post_order.rend());
    run.acyclic = is_topological_order(g, run.order);
    return run;
}

bool is_topological_order(const Graph &g, const std::vector<int> &order)
{
    const int n = g.vertex_count();
    std::vector<int> position(static_cast<std::size_t>(n), -1);
    for (std::size_t i = 0; i < order.size(); ++i) {
        const int v = order[i];
        if (v < 0 || v >= n) {
            return false;
        }
        position[static_cast<std::size_t>(v)] = static_cast<int>(i);
    }
    for (int v = 0; v < n; ++v) {
        if (position[static_cast<std::size_t>(v)] < 0) {
            return false;                 /* 有顶点没排进来 */
        }
    }
    for (const Edge &e : g.edges()) {
        if (position[static_cast<std::size_t>(e.u)] >= position[static_cast<std::size_t>(e.v)]) {
            return false;                 /* 边的起点排到了终点后面 */
        }
    }
    return true;
}

/* ================= 强连通分量 ================= */

namespace {

struct TarjanState {
    const Graph *g = nullptr;
    SccRun *run = nullptr;
    std::vector<int> dfn;
    std::vector<int> low;
    std::vector<int> stack;
    std::vector<char> on_stack;
    int timer = 0;
};

void tarjan_dfs(int u, TarjanState *st)
{
    SccRun &run = *st->run;
    ++st->timer;
    st->dfn[static_cast<std::size_t>(u)] = st->timer;
    st->low[static_cast<std::size_t>(u)] = st->timer;
    ++run.visits;

    st->stack.push_back(u);
    st->on_stack[static_cast<std::size_t>(u)] = 1;
    ++run.pushes;

    for (const int v : st->g->neighbors(u)) {
        ++run.edge_checks;
        if (st->dfn[static_cast<std::size_t>(v)] == 0) {
            tarjan_dfs(v, st);
            if (st->low[static_cast<std::size_t>(v)] < st->low[static_cast<std::size_t>(u)]) {
                st->low[static_cast<std::size_t>(u)] = st->low[static_cast<std::size_t>(v)];
                ++run.low_updates;
            }
        } else if (st->on_stack[static_cast<std::size_t>(v)]) {
            /* 指回栈里的顶点才算数：指向已经弹出的分量说明那条路是死路 */
            if (st->dfn[static_cast<std::size_t>(v)] < st->low[static_cast<std::size_t>(u)]) {
                st->low[static_cast<std::size_t>(u)] = st->dfn[static_cast<std::size_t>(v)];
                ++run.low_updates;
            }
        }
    }

    if (st->low[static_cast<std::size_t>(u)] == st->dfn[static_cast<std::size_t>(u)]) {
        std::vector<int> component;
        for (;;) {
            const int w = st->stack.back();
            st->stack.pop_back();
            st->on_stack[static_cast<std::size_t>(w)] = 0;
            ++run.pops;
            component.push_back(w);
            if (w == u) {
                break;
            }
        }
        std::sort(component.begin(), component.end());
        run.components.push_back(component);
    }
}

}   /* namespace */

SccRun tarjan_scc(const Graph &g)
{
    SccRun run;
    const std::size_t n = static_cast<std::size_t>(g.vertex_count());

    TarjanState st;
    st.g = &g;
    st.run = &run;
    st.dfn.assign(n, 0);
    st.low.assign(n, 0);
    st.on_stack.assign(n, 0);

    for (int v = 0; v < g.vertex_count(); ++v) {
        if (st.dfn[static_cast<std::size_t>(v)] == 0) {
            tarjan_dfs(v, &st);
        }
    }

    run.component_of.assign(n, -1);
    for (std::size_t i = 0; i < run.components.size(); ++i) {
        for (const int v : run.components[i]) {
            run.component_of[static_cast<std::size_t>(v)] = static_cast<int>(i);
        }
    }
    return run;
}

/* ================= 割点与桥 ================= */

namespace {

struct ConnState {
    const Graph *g = nullptr;
    ConnectivityRun *run = nullptr;
    std::vector<char> is_cut;
    int timer = 0;
};

void conn_dfs(int u, ConnState *st)
{
    ConnectivityRun &run = *st->run;
    ++st->timer;
    run.dfn[static_cast<std::size_t>(u)] = st->timer;
    run.low[static_cast<std::size_t>(u)] = st->timer;
    ++run.visits;
    run.dfs_order.push_back(u);

    for (const int v : st->g->neighbors(u)) {
        ++run.edge_checks;
        if (run.dfn[static_cast<std::size_t>(v)] == 0) {
            run.parent[static_cast<std::size_t>(v)] = u;
            ++run.dfs_children[static_cast<std::size_t>(u)];
            run.tree_edges.push_back(Edge{u, v, weight_of(*st->g, u, v)});
            conn_dfs(v, st);

            if (run.low[static_cast<std::size_t>(v)] < run.low[static_cast<std::size_t>(u)]) {
                run.low[static_cast<std::size_t>(u)] = run.low[static_cast<std::size_t>(v)];
                ++run.low_updates;
            }
            if (run.low[static_cast<std::size_t>(v)] > run.dfn[static_cast<std::size_t>(u)]) {
                run.bridges.push_back(Edge{std::min(u, v), std::max(u, v),
                                           weight_of(*st->g, u, v)});
            }
            /* 非根顶点：只要有一个孩子的 low 不低于自己，去掉它就断开了 */
            if (run.parent[static_cast<std::size_t>(u)] != -1 &&
                run.low[static_cast<std::size_t>(v)] >= run.dfn[static_cast<std::size_t>(u)]) {
                st->is_cut[static_cast<std::size_t>(u)] = 1;
            }
        } else if (v != run.parent[static_cast<std::size_t>(u)]) {
            /* 无向图里指回已访问顶点，只可能是连到祖先的回边；用 dfn 而不是 low，
               指到后代的边只会让值变大，取 min 之后自然不起作用 */
            if (run.dfn[static_cast<std::size_t>(v)] < run.low[static_cast<std::size_t>(u)]) {
                run.low[static_cast<std::size_t>(u)] = run.dfn[static_cast<std::size_t>(v)];
                ++run.low_updates;
            }
        }
    }
}

}   /* namespace */

ConnectivityRun analyze_connectivity(const Graph &g)
{
    ConnectivityRun run;
    const std::size_t n = static_cast<std::size_t>(g.vertex_count());
    run.dfn.assign(n, 0);
    run.low.assign(n, 0);
    run.parent.assign(n, -1);
    run.dfs_children.assign(n, 0);

    ConnState st;
    st.g = &g;
    st.run = &run;
    st.is_cut.assign(n, 0);

    for (int root = 0; root < g.vertex_count(); ++root) {
        if (run.dfn[static_cast<std::size_t>(root)] != 0) {
            continue;
        }
        conn_dfs(root, &st);
        /* 根是割点当且仅当它在 DFS 树上有两棵以上的子树 */
        if (run.dfs_children[static_cast<std::size_t>(root)] >= 2) {
            st.is_cut[static_cast<std::size_t>(root)] = 1;
        }
    }

    for (int v = 0; v < g.vertex_count(); ++v) {
        if (st.is_cut[static_cast<std::size_t>(v)]) {
            run.cut_vertices.push_back(v);
        }
    }
    /* 桥是按 DFS 的回溯顺序发现的，报告里要按 (u, v) 升序看，这里统一排一遍 */
    std::sort(run.bridges.begin(), run.bridges.end(), [](const Edge &lhs, const Edge &rhs) {
        if (lhs.u != rhs.u) {
            return lhs.u < rhs.u;
        }
        return lhs.v < rhs.v;
    });
    return run;
}

std::vector<int> low_by_brute_force(const Graph &g, const ConnectivityRun &run)
{
    const int n = g.vertex_count();
    std::vector<std::vector<int>> children(static_cast<std::size_t>(n));
    for (int v = 0; v < n; ++v) {
        const int p = run.parent[static_cast<std::size_t>(v)];
        if (p != -1) {
            children[static_cast<std::size_t>(p)].push_back(v);
        }
    }

    std::vector<int> result(static_cast<std::size_t>(n), 0);
    for (int v = 0; v < n; ++v) {
        /* 先把 v 的子树收出来 */
        std::vector<char> in_subtree(static_cast<std::size_t>(n), 0);
        std::vector<int> stack;
        stack.push_back(v);
        in_subtree[static_cast<std::size_t>(v)] = 1;
        while (!stack.empty()) {
            const int x = stack.back();
            stack.pop_back();
            for (const int c : children[static_cast<std::size_t>(x)]) {
                in_subtree[static_cast<std::size_t>(c)] = 1;
                stack.push_back(c);
            }
        }

        int best = run.dfn[static_cast<std::size_t>(v)];
        for (const Edge &e : g.edges()) {
            /* 树边不算回边：两端互为父子的那一条要排掉 */
            if (run.parent[static_cast<std::size_t>(e.u)] == e.v ||
                run.parent[static_cast<std::size_t>(e.v)] == e.u) {
                continue;
            }
            const bool u_in = in_subtree[static_cast<std::size_t>(e.u)] != 0;
            const bool v_in = in_subtree[static_cast<std::size_t>(e.v)] != 0;
            if (u_in == v_in) {
                continue;                 /* 两端都在子树里，或者都在外面 */
            }
            const int outside = u_in ? e.v : e.u;
            if (run.dfn[static_cast<std::size_t>(outside)] < best) {
                best = run.dfn[static_cast<std::size_t>(outside)];
            }
        }
        result[static_cast<std::size_t>(v)] = best;
    }
    return result;
}

std::vector<Edge> bridges_by_brute_force(const Graph &g)
{
    std::vector<Edge> found;
    const std::size_t base = component_sizes(g).size();
    for (const Edge &e : g.edges()) {
        if (component_sizes_without_edge(g, e).size() > base) {
            found.push_back(e);
        }
    }
    return found;
}

std::vector<int> cut_vertices_by_brute_force(const Graph &g)
{
    std::vector<int> found;
    const std::size_t base = component_sizes(g).size();
    for (int v = 0; v < g.vertex_count(); ++v) {
        if (component_sizes_without_vertex(g, v).size() > base) {
            found.push_back(v);
        }
    }
    return found;
}

/* ================= 并查集与最小生成树 ================= */

DisjointSet::DisjointSet(int count)
    : parent_(static_cast<std::size_t>(count)),
      size_(static_cast<std::size_t>(count), 1)
{
    for (int i = 0; i < count; ++i) {
        parent_[static_cast<std::size_t>(i)] = i;
    }
}

int DisjointSet::find(int x)
{
    ++find_calls_;
    int root = x;
    while (parent_[static_cast<std::size_t>(root)] != root) {
        ++find_hops_;
        root = parent_[static_cast<std::size_t>(root)];
    }
    /* 路径压缩：把这条链上的顶点都挂到根上，下次再来就一步到位 */
    while (parent_[static_cast<std::size_t>(x)] != root) {
        const int next = parent_[static_cast<std::size_t>(x)];
        parent_[static_cast<std::size_t>(x)] = root;
        x = next;
    }
    return root;
}

bool DisjointSet::unite(int a, int b)
{
    int ra = find(a);
    int rb = find(b);
    if (ra == rb) {
        return false;
    }
    if (size_[static_cast<std::size_t>(ra)] < size_[static_cast<std::size_t>(rb)]) {
        std::swap(ra, rb);               /* 小树挂到大树上 */
    }
    parent_[static_cast<std::size_t>(rb)] = ra;
    size_[static_cast<std::size_t>(ra)] += size_[static_cast<std::size_t>(rb)];
    ++unions_;
    return true;
}

namespace {

/** 自己写的插入排序，顺便数比较次数。
    不用 std::sort：它的比较次数由实现决定，同一份数据换一个标准库就是另一个数，
    那样的数字不进报告 */
void insertion_sort_by_weight(std::vector<Edge> *edges, std::size_t *comparisons)
{
    for (std::size_t i = 1; i < edges->size(); ++i) {
        const Edge key = (*edges)[i];
        std::size_t j = i;
        while (j > 0) {
            ++*comparisons;
            if ((*edges)[j - 1].weight <= key.weight) {
                break;
            }
            (*edges)[j] = (*edges)[j - 1];
            --j;
        }
        (*edges)[j] = key;
    }
}

}   /* namespace */

KruskalRun kruskal_mst(const Graph &g)
{
    KruskalRun run;
    run.sorted_edges = g.edges();
    insertion_sort_by_weight(&run.sorted_edges, &run.sort_comparisons);

    DisjointSet dsu(g.vertex_count());
    const std::size_t need = static_cast<std::size_t>(g.vertex_count() - 1);

    for (const Edge &e : run.sorted_edges) {
        ++run.edge_checks;
        if (dsu.unite(e.u, e.v)) {
            run.chosen.push_back(e);
            run.total_weight += e.weight;
            if (run.chosen.size() == need) {
                break;                    /* 已经有 n - 1 条边，剩下的不必再看 */
            }
        } else {
            ++run.rejected;
        }
    }

    run.find_calls = dsu.find_calls();
    run.find_hops = dsu.find_hops();
    run.unions = dsu.unions();
    run.spanning = (run.chosen.size() == need);
    return run;
}

namespace {

/** Prim 堆里的一条候选边：权值、边的另一头、这一头 */
struct PrimItem {
    int weight = 0;
    int vertex = 0;
    int from = 0;
};

/** 小顶堆：先比权值，再比顶点编号，最后比来路，比法全定死，弹出顺序才唯一 */
struct PrimGreater {
    bool operator()(const PrimItem &lhs, const PrimItem &rhs) const
    {
        if (lhs.weight != rhs.weight) {
            return lhs.weight > rhs.weight;
        }
        if (lhs.vertex != rhs.vertex) {
            return lhs.vertex > rhs.vertex;
        }
        return lhs.from > rhs.from;
    }
};

}   /* namespace */

PrimRun prim_mst(const Graph &g, int start)
{
    PrimRun run;
    run.start = start;
    const int n = g.vertex_count();
    std::vector<char> in_tree(static_cast<std::size_t>(n), 0);
    std::priority_queue<PrimItem, std::vector<PrimItem>, PrimGreater> heap;

    heap.push(PrimItem{0, start, start});
    ++run.pushes;

    int added = 0;
    while (!heap.empty() && added < n) {
        const PrimItem top = heap.top();
        heap.pop();
        ++run.pops;

        if (in_tree[static_cast<std::size_t>(top.vertex)]) {
            ++run.stale_pops;             /* 惰性删除：顶点早就进树了 */
            continue;
        }
        in_tree[static_cast<std::size_t>(top.vertex)] = 1;
        ++added;
        if (top.vertex != start) {
            run.chosen.push_back(Edge{std::min(top.from, top.vertex),
                                      std::max(top.from, top.vertex), top.weight});
            run.total_weight += top.weight;
        }

        for (const int u : g.neighbors(top.vertex)) {
            ++run.edge_checks;
            if (!in_tree[static_cast<std::size_t>(u)]) {
                heap.push(PrimItem{weight_of(g, top.vertex, u), u, top.vertex});
                ++run.pushes;
            }
        }
    }

    run.heap_left = heap.size();
    run.spanning = (added == n);
    return run;
}

/* ================= 报告用的排版工具 ================= */

namespace {

/** 表格按显示宽度对齐：CJK 与全角标点算 2 列，其余算 1 列 */
bool is_wide_codepoint(unsigned int cp)
{
    return (cp >= 0x1100 && cp <= 0x115F) || cp == 0x2329 || cp == 0x232A ||
           (cp >= 0x2E80 && cp <= 0xA4CF && cp != 0x303F) ||
           (cp >= 0xAC00 && cp <= 0xD7A3) || (cp >= 0xF900 && cp <= 0xFAFF) ||
           (cp >= 0xFE30 && cp <= 0xFE6F) || (cp >= 0xFF00 && cp <= 0xFF60) ||
           (cp >= 0xFFE0 && cp <= 0xFFE6) || (cp >= 0x20000 && cp <= 0x3FFFD);
}

std::size_t display_width(const std::string &text)
{
    std::size_t width = 0;
    for (std::size_t i = 0; i < text.size();) {
        const unsigned char lead = static_cast<unsigned char>(text[i]);
        std::size_t length = 1;
        unsigned int cp = lead;
        if (lead >= 0xF0) {
            length = 4;
            cp = lead & 0x07u;
        } else if (lead >= 0xE0) {
            length = 3;
            cp = lead & 0x0Fu;
        } else if (lead >= 0xC0) {
            length = 2;
            cp = lead & 0x1Fu;
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
    const std::size_t shown = display_width(text);
    if (shown >= width) {
        return text;
    }
    return text + std::string(width - shown, ' ');
}

/** 只用于纯 ASCII 的对齐（数字、点号），因此长度就是显示宽度 */
std::string pad_left_ascii(const std::string &text, std::size_t width)
{
    if (text.size() >= width) {
        return text;
    }
    return std::string(width - text.size(), ' ') + text;
}

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

std::string brace_ints(const std::vector<int> &values)
{
    std::ostringstream os;
    os << '{';
    for (std::size_t i = 0; i < values.size(); ++i) {
        if (i != 0) {
            os << ", ";
        }
        os << values[i];
    }
    os << '}';
    return os.str();
}

std::string join_sizes(const std::vector<std::size_t> &values)
{
    std::ostringstream os;
    for (std::size_t i = 0; i < values.size(); ++i) {
        if (i != 0) {
            os << u8"、";
        }
        os << values[i];
    }
    return os.str();
}

/** "(1,3) (1,8) (5,6)" */
std::string edges_text(const std::vector<Edge> &edges)
{
    std::ostringstream os;
    for (std::size_t i = 0; i < edges.size(); ++i) {
        if (i != 0) {
            os << ' ';
        }
        os << '(' << edges[i].u << ',' << edges[i].v << ')';
    }
    return os.str();
}

/** "(1,2) 1、(1,3) 2" */
std::string weighted_edges_text(const std::vector<Edge> &edges)
{
    std::ostringstream os;
    for (std::size_t i = 0; i < edges.size(); ++i) {
        if (i != 0) {
            os << u8"、";
        }
        os << '(' << edges[i].u << ',' << edges[i].v << u8") " << edges[i].weight;
    }
    return os.str();
}

/** "0 -> 7、1 -> 8" */
std::string arcs_text(const std::vector<Edge> &edges)
{
    std::ostringstream os;
    for (std::size_t i = 0; i < edges.size(); ++i) {
        if (i != 0) {
            os << u8"、";
        }
        os << edges[i].u << " -> " << edges[i].v;
    }
    return os.str();
}

std::string yes_no(bool value)
{
    return value ? u8"是" : u8"否";
}

/** 邻接矩阵画成网格：每格固定宽，行号与列号都是顶点编号 */
std::string matrix_grid(const Graph &g, bool with_weight)
{
    const int n = g.vertex_count();
    const std::vector<int> matrix = g.adjacency_matrix();
    const std::size_t width = with_weight ? 3 : 2;

    std::ostringstream os;
    os << std::string(width, ' ');
    for (int c = 0; c < n; ++c) {
        os << pad_left_ascii(std::to_string(c), width);
    }
    os << "\n";
    for (int r = 0; r < n; ++r) {
        os << pad_left_ascii(std::to_string(r), width);
        for (int c = 0; c < n; ++c) {
            const int value = matrix[static_cast<std::size_t>(r) * static_cast<std::size_t>(n) +
                                    static_cast<std::size_t>(c)];
            if (value == 0) {
                os << pad_left_ascii(".", width);
            } else if (with_weight) {
                os << pad_left_ascii(std::to_string(value), width);
            } else {
                os << pad_left_ascii("1", width);
            }
        }
        os << "\n";
    }
    return os.str();
}

/** 顶点在序列里的位置，1 起；不在序列里是 0 */
std::vector<int> positions_of(const std::vector<int> &order, int vertex_count)
{
    std::vector<int> position(static_cast<std::size_t>(vertex_count), 0);
    for (std::size_t i = 0; i < order.size(); ++i) {
        position[static_cast<std::size_t>(order[i])] = static_cast<int>(i) + 1;
    }
    return position;
}

std::string number_cells(const std::vector<int> &values)
{
    std::ostringstream os;
    for (const int value : values) {
        os << pad_left_ascii(std::to_string(value), 3);
    }
    return os.str();
}

/** 从 from 出发能不能走到 to */
bool reaches(const Graph &g, int from, int to)
{
    if (from == to) {
        return true;
    }
    std::vector<char> seen(static_cast<std::size_t>(g.vertex_count()), 0);
    std::vector<int> stack;
    stack.push_back(from);
    seen[static_cast<std::size_t>(from)] = 1;
    while (!stack.empty()) {
        const int v = stack.back();
        stack.pop_back();
        for (const int u : g.neighbors(v)) {
            if (u == to) {
                return true;
            }
            if (!seen[static_cast<std::size_t>(u)]) {
                seen[static_cast<std::size_t>(u)] = 1;
                stack.push_back(u);
            }
        }
    }
    return false;
}

/* ================= 报告 ================= */

void append_graphs(std::ostringstream &os)
{
    const Graph g = make_undirected_graph();
    const Graph dag = make_dag();
    const Graph cyc = make_cyclic_digraph();

    os << u8"一、写死在代码里的两张图\n";
    os << "\n";
    os << u8"  无向图 G：" << g.vertex_count() << u8" 个顶点、" << g.edges().size()
       << u8" 条边、连通、带权。下面的网格就是它的邻接矩阵，格子里是权值，点号表示没有边。\n";
    os << matrix_grid(g, true);
    os << u8"  边表（按 (u, v) 升序）：" << weighted_edges_text(g.edges()) << "\n";
    os << u8"  这张图由两个三角形（0-1-2 与 3-4-5）用边 1-3 串起来，\n";
    os << u8"  顶点 8 挂在 1 上，顶点 6、7 挂在 5 下面连成一条链。\n";
    os << "\n";
    os << u8"  有向图 D 的无环版：" << dag.vertex_count() << u8" 个顶点、" << dag.edges().size()
       << u8" 条边。网格里 1 表示有一条从行指向列的边。\n";
    os << matrix_grid(dag, false);
    os << u8"  边表（按 u 升序）：" << arcs_text(dag.edges()) << "\n";
    os << u8"  有环版：在无环版上加三条回边 7 -> 0、8 -> 7、6 -> 5，边数从 "
       << dag.edges().size() << u8" 涨到 " << cyc.edges().size() << u8"。\n";
}

void append_topological(std::ostringstream &os)
{
    const Graph dag = make_dag();
    const TopoRun kahn = topo_sort_kahn(dag);
    const TopoRun post = topo_sort_dfs_postorder(dag);
    const int n = dag.vertex_count();

    os << u8"二、拓扑排序：Kahn 与 DFS 后序\n";
    os << u8"  Kahn：先数一遍入度，入度为 0 的进队列；出队一个，就把它出边终点的入度减一。\n";
    os << u8"  队列用最小堆，同时有几个可选的顶点时取编号最小的，出队顺序只由入度表决定。\n";
    os << u8"    入队 " << kahn.enqueues << u8" 次、出队 " << kahn.dequeues << u8" 次、检查出边 "
       << kahn.edge_checks << u8" 条\n";
    os << u8"    出队顺序：" << join_ints(kahn.order) << "\n";
    os << u8"  DFS 后序：按编号从小到大挑起点做深度优先，回溯时把顶点接到序列尾部，最后整体取逆。\n";
    os << u8"    入栈 " << post.enqueues << u8" 次、出栈 " << post.dequeues << u8" 次、检查出边 "
       << post.edge_checks << u8" 条\n";
    os << u8"    取逆之后：" << join_ints(post.order) << "\n";
    os << u8"  两条序列都是合法拓扑序：" << yes_no(is_topological_order(dag, kahn.order)) << u8"、"
       << yes_no(is_topological_order(dag, post.order)) << "\n";
    os << u8"  同一个顶点在两条序列里的位置：\n";
    std::vector<int> vertices;
    for (int v = 0; v < n; ++v) {
        vertices.push_back(v);
    }
    os << "  " << pad_right(u8"顶点", 21) << number_cells(vertices) << "\n";
    os << "  " << pad_right(u8"Kahn 里的位置", 21) << number_cells(positions_of(kahn.order, n)) << "\n";
    os << "  " << pad_right(u8"DFS 后序里的位置", 21) << number_cells(positions_of(post.order, n)) << "\n";
    os << u8"  两条序列不一样，因为拓扑序本来就不唯一：Kahn 让能走的顶点尽早出队，\n";
    os << u8"  DFS 后序要先把一条链走到底才回头，顶点 4 的位置就差了 1。\n";
    os << u8"  出队次数等于顶点数（" << kahn.dequeues << u8" = " << n
       << u8"），说明每个顶点都排上了位置。\n";
    os << "\n";
}

void append_cycle(std::ostringstream &os)
{
    const Graph dag = make_dag();
    const Graph cyc = make_cyclic_digraph();
    const TopoRun kahn = topo_sort_kahn(cyc);
    const SccRun scc = tarjan_scc(cyc);

    std::vector<int> on_cycle;
    for (const std::vector<int> &component : scc.components) {
        if (component.size() >= 2) {
            for (const int v : component) {
                on_cycle.push_back(v);
            }
        }
    }
    std::sort(on_cycle.begin(), on_cycle.end());

    os << u8"三、有环版：Kahn 怎样发现环\n";
    os << u8"  有环版在无环版上加三条回边 7 -> 0、8 -> 7、6 -> 5，边数从 "
       << dag.edges().size() << u8" 涨到 " << cyc.edges().size() << u8"。\n";
    os << u8"  一开始只有顶点 4 的入度是 0：入队 " << kahn.enqueues << u8" 次、出队 "
       << kahn.dequeues << u8" 次之后队列就空了。\n";
    os << u8"  出队次数 " << kahn.dequeues << u8" 小于顶点数 " << cyc.vertex_count()
       << u8"：环上的顶点互相撑着，入度永远减不到 0，Kahn 就是这样发现环的。\n";
    os << u8"  入度表里还剩 " << kahn.remaining.size() << u8" 个顶点：\n";
    os << "  " << pad_right(u8"顶点", 8) << number_cells(kahn.remaining) << "\n";
    os << "  " << pad_right(u8"剩余入度", 8) << number_cells(kahn.remaining_in_degree) << "\n";
    os << u8"  这 " << kahn.remaining.size() << u8" 个顶点里，真正在环上的只有 "
       << on_cycle.size() << u8" 个：" << join_ints(on_cycle) << u8"。\n";
    os << u8"  顶点 3 不在环上，它只是排在环的下游，被环挡住了入度。\n";
    os << u8"  要分清「在环上」与「被环堵住」，得看第四段的强连通分量。\n";
    os << "\n";
}

void append_scc(std::ostringstream &os)
{
    const Graph cyc = make_cyclic_digraph();
    const SccRun run = tarjan_scc(cyc);

    os << u8"四、强连通分量：Tarjan\n";
    os << u8"  一次 DFS。dfn 记访问的先后，low 记这个顶点和它的后代能回到的最小 dfn；\n";
    os << u8"  另有一个栈，顶点第一次访问时进栈；回溯时若 low 与 dfn 相等，\n";
    os << u8"  栈里从它往上的那些顶点就合成一个分量。\n";
    os << u8"    访问 " << run.visits << u8" 次、检查出边 " << run.edge_checks << u8" 条、low 改小 "
       << run.low_updates << u8" 次、入栈 " << run.pushes << u8" 次、出栈 " << run.pops << u8" 次\n";
    os << u8"  分量个数 " << run.components.size() << u8"，按弹出的先后列出：\n";
    for (std::size_t i = 0; i < run.components.size(); ++i) {
        os << u8"    分量 " << (i + 1) << u8"：" << brace_ints(run.components[i]) << u8"，大小 "
           << run.components[i].size() << "\n";
    }

    /* 凝结图：分量缩成一个点之后剩下的边，去重后按字典序排出来 */
    std::vector<std::pair<int, int>> arcs;
    for (const Edge &e : cyc.edges()) {
        const int a = run.component_of[static_cast<std::size_t>(e.u)];
        const int b = run.component_of[static_cast<std::size_t>(e.v)];
        if (a == b) {
            continue;
        }
        const std::pair<int, int> arc(a, b);
        if (std::find(arcs.begin(), arcs.end(), arc) == arcs.end()) {
            arcs.push_back(arc);
        }
    }
    std::sort(arcs.begin(), arcs.end());
    os << u8"  凝结图（把每个分量缩成一个点）有 " << arcs.size() << u8" 条边：";
    for (std::size_t i = 0; i < arcs.size(); ++i) {
        if (i != 0) {
            os << u8"、";
        }
        os << brace_ints(run.components[static_cast<std::size_t>(arcs[i].first)]) << " -> "
           << brace_ints(run.components[static_cast<std::size_t>(arcs[i].second)]);
    }
    os << "\n";
    os << u8"  分量弹出顺序就是凝结图的逆拓扑序：每条分量间的边都从后弹出的指向先弹出的。\n";
    os << u8"  在环上的顶点正是大小超过 1 的分量里的那些；剩下的大小为 1 的分量就是不在环上的单点。\n";
    os << "\n";
}

void append_connectivity(std::ostringstream &os)
{
    const Graph g = make_undirected_graph();
    const ConnectivityRun run = analyze_connectivity(g);
    const int n = g.vertex_count();
    const std::size_t base = component_sizes(g).size();

    os << u8"五、割点与桥：DFS 树上的 dfn 与 low\n";
    os << u8"  dfn 是访问的先后（从 1 开始），low 是这个顶点和它的后代能碰到的最小 dfn。\n";
    os << u8"  树边 (u,v)（u 是父）是桥，当且仅当 low[v] > dfn[u]；\n";
    os << u8"  非根顶点 u 是割点，当且仅当它有孩子 v 满足 low[v] >= dfn[u]；\n";
    os << u8"  根是割点，当且仅当它在 DFS 树上有两棵以上的子树。\n";
    os << u8"  一次 DFS：访问 " << run.visits << u8" 次、检查邻接条目 " << run.edge_checks
       << u8" 条（等于 2 × 边数）、low 改小 " << run.low_updates << u8" 次。\n";
    os << "  " << pad_right(u8"顶点", 6) << pad_right("dfn", 6) << pad_right("low", 6)
       << pad_right(u8"父", 5) << pad_right(u8"树上的孩子", 13) << u8"是否割点\n";
    for (int v = 0; v < n; ++v) {
        const bool is_cut = std::find(run.cut_vertices.begin(), run.cut_vertices.end(), v) !=
                            run.cut_vertices.end();
        const std::string parent_text =
            (run.parent[static_cast<std::size_t>(v)] == -1)
                ? std::string("-")
                : std::to_string(run.parent[static_cast<std::size_t>(v)]);
        os << "  " << pad_right(std::to_string(v), 6)
           << pad_right(std::to_string(run.dfn[static_cast<std::size_t>(v)]), 6)
           << pad_right(std::to_string(run.low[static_cast<std::size_t>(v)]), 6)
           << pad_right(parent_text, 5)
           << pad_right(std::to_string(run.dfs_children[static_cast<std::size_t>(v)]), 13)
           << yes_no(is_cut) << "\n";
    }
    os << u8"  DFS 树（父 -> 子）：" << arcs_text(run.tree_edges) << "\n";
    os << u8"  割点集合 " << brace_ints(run.cut_vertices) << u8"，共 " << run.cut_vertices.size()
       << u8" 个；桥集合 " << edges_text(run.bridges) << u8"，共 " << run.bridges.size() << u8" 条。\n";
    os << u8"  去掉每个割点之后还剩几个连通块（与它相连的边一并去掉，孤立顶点各算一块）：\n";
    for (const int v : run.cut_vertices) {
        const std::vector<std::size_t> sizes = component_sizes_without_vertex(g, v);
        os << u8"    去掉顶点 " << v << u8"：" << base << " -> " << sizes.size()
           << u8" 块，各块大小 " << join_sizes(sizes) << "\n";
    }
    os << u8"    非割点 ";
    {
        bool first = true;
        for (int v = 0; v < n; ++v) {
            if (std::find(run.cut_vertices.begin(), run.cut_vertices.end(), v) !=
                run.cut_vertices.end()) {
                continue;
            }
            if (!first) {
                os << u8"、";
            }
            os << v;
            first = false;
        }
    }
    os << u8" 去掉之后仍然 " << base << u8" 块\n";
    os << u8"  每条桥单独去掉，连通块数都从 " << base << " -> " << (base + 1) << u8"：";
    for (std::size_t i = 0; i < run.bridges.size(); ++i) {
        if (i != 0) {
            os << u8"、";
        }
        const Edge &e = run.bridges[i];
        os << '(' << e.u << ',' << e.v << ") " << component_sizes_without_edge(g, e).size();
        os << u8" 块";
    }
    os << "\n";
    os << "\n";
}

void append_mst(std::ostringstream &os)
{
    const Graph g = make_undirected_graph();
    const KruskalRun kruskal = kruskal_mst(g);
    const PrimRun prim = prim_mst(g, 0);

    os << u8"六、最小生成树：Kruskal 与 Prim\n";
    os << u8"  Kruskal：把边按权值升序排好，逐条看，两端还不在同一棵树里就要。\n";
    os << u8"    排序比较 " << kruskal.sort_comparisons
       << u8" 次（自己写的插入排序；std::sort 的比较次数由实现决定，不进报告）\n";
    os << u8"    看边 " << kruskal.edge_checks << u8" 条、find " << kruskal.find_calls
       << u8" 次、union " << kruskal.unions << u8" 次，因为成环跳过 " << kruskal.rejected << u8" 条\n";
    os << u8"    find 沿父指针一共走了 " << kruskal.find_hops
       << u8" 步：按大小合并加上路径压缩之后，树一直很浅\n";
    os << u8"    选中的边（按权值升序）：" << weighted_edges_text(kruskal.chosen) << "\n";
    os << u8"    累计权值：";
    {
        int sum = 0;
        for (std::size_t i = 0; i < kruskal.chosen.size(); ++i) {
            sum += kruskal.chosen[i].weight;
            if (i != 0) {
                os << u8"、";
            }
            os << sum;
        }
    }
    os << u8"；总权值 " << kruskal.total_weight << u8"，" << kruskal.chosen.size()
       << u8" 条边，正好是顶点数减一。\n";
    os << u8"  Prim：从顶点 " << prim.start << u8" 起步，每次拿一条连到树外的权值最小的边。\n";
    os << u8"    push " << prim.pushes << u8" 次、pop " << prim.pops << u8" 次，其中 "
       << prim.stale_pops << u8" 次弹出的顶点早就进树了（惰性删除），堆里还剩 " << prim.heap_left
       << u8" 条。\n";
    os << u8"    边检查 " << prim.edge_checks << u8" 次，等于 2 × 边数：每个顶点进树时扫一遍它的邻接表。\n";
    os << u8"    选中的边（按顶点进树的先后）：" << edges_text(prim.chosen) << "\n";
    os << u8"    总权值 " << prim.total_weight << u8"。\n";
    os << u8"  两版的总权值相同：" << yes_no(kruskal.total_weight == prim.total_weight)
       << u8"；权值两两不同时最小生成树唯一，所以两边选中的边也逐条相同。\n";
}

}   /* namespace */

std::string build_report()
{
    std::ostringstream os;
    append_graphs(os);
    append_topological(os);
    append_cycle(os);
    append_scc(os);
    append_connectivity(os);
    append_mst(os);
    return os.str();
}

/* ================= 自测 ================= */

namespace {

class Checks {
public:
    void expect(bool ok, const std::string &what)
    {
        ++total_;
        if (ok) {
            ++passed_;
            lines_.push_back(u8"[通过] " + std::to_string(total_) + ". " + what);
        } else {
            ++failed_;
            lines_.push_back(u8"[失败] " + std::to_string(total_) + ". " + what);
        }
    }

    void expect_ints(const std::vector<int> &got, const std::vector<int> &want,
                     const std::string &what)
    {
        if (got == want) {
            expect(true, what);
        } else {
            expect(false, what + u8"（得到 " + join_ints(got) + u8"，应为 " +
                              join_ints(want) + u8"）");
        }
    }

    /** 通过时只打一行结论，不符时把两边的实际值都附上 */
    void expect_detail(bool ok, const std::string &what, const std::string &detail)
    {
        expect(ok, ok ? what : what + detail);
    }

    CheckResult finish() const
    {
        CheckResult result;
        result.total = total_;
        result.passed = passed_;
        result.failed = failed_;
        result.lines = lines_;
        return result;
    }

private:
    std::size_t total_ = 0;
    std::size_t passed_ = 0;
    std::size_t failed_ = 0;
    std::vector<std::string> lines_;
};

/** 同一条边在两张图里是不是同一条：只比两个端点 */
bool same_edge_set(std::vector<Edge> lhs, std::vector<Edge> rhs)
{
    const auto less = [](const Edge &a, const Edge &b) {
        if (a.u != b.u) {
            return a.u < b.u;
        }
        return a.v < b.v;
    };
    std::sort(lhs.begin(), lhs.end(), less);
    std::sort(rhs.begin(), rhs.end(), less);
    return lhs == rhs;
}

}   /* namespace */

std::string CheckResult::summary() const
{
    std::ostringstream os;
    os << total << u8" 项中 " << passed << u8" 项通过";
    if (failed == 0) {
        os << u8"，全部通过";
    } else {
        os << u8"，" << failed << u8" 项不符";
    }
    return os.str();
}

CheckResult run_self_tests()
{
    Checks checks;

    const Graph g = make_undirected_graph();
    const Graph dag = make_dag();
    const Graph cyc = make_cyclic_digraph();
    const int n = g.vertex_count();
    /* 有向图是另一张图，顶点数要单独取：拿 n 去索引分量表，
       只要两张图的顶点数不一样就会越界 */
    const int cyc_n = cyc.vertex_count();
    const ConnectivityRun conn = analyze_connectivity(g);

    /* 1—3 图本身 */
    checks.expect(g.vertex_count() == 9 && g.edges().size() == 10 && !g.directed() &&
                      component_sizes(g).size() == 1,
                  u8"无向图 G：9 个顶点、10 条边、无向、1 个连通块");

    bool ascending = true;
    bool symmetric = true;
    for (int v = 0; v < n; ++v) {
        const std::vector<int> &nb = g.neighbors(v);
        for (std::size_t i = 1; i < nb.size(); ++i) {
            if (nb[i - 1] >= nb[i]) {
                ascending = false;
            }
        }
        for (const int u : nb) {
            const std::vector<int> &back = g.neighbors(u);
            if (!std::binary_search(back.begin(), back.end(), v)) {
                symmetric = false;
            }
        }
    }
    checks.expect(ascending && symmetric,
                  u8"邻接表都是升序，而且一条边在两个端点的表里都出现");

    const std::vector<int> matrix = g.adjacency_matrix();
    int nonzero = 0;
    bool matrix_symmetric = true;
    for (int r = 0; r < n; ++r) {
        for (int c = 0; c < n; ++c) {
            const std::size_t at = static_cast<std::size_t>(r) * static_cast<std::size_t>(n) +
                                   static_cast<std::size_t>(c);
            const std::size_t mirror =
                static_cast<std::size_t>(c) * static_cast<std::size_t>(n) +
                static_cast<std::size_t>(r);
            if (matrix[at] != 0) {
                ++nonzero;
            }
            if (matrix[at] != matrix[mirror]) {
                matrix_symmetric = false;
            }
        }
    }
    checks.expect(nonzero == 20 && matrix_symmetric,
                  u8"邻接矩阵对称，非零格子 20 个，等于 2 × 边数");

    /* 4 加边顺序无关 */
    Graph flipped(g.vertex_count(), false);
    for (std::size_t i = g.edges().size(); i > 0; --i) {
        const Edge &e = g.edges()[i - 1];
        flipped.add_edge(e.v, e.u, e.weight);   /* 倒着加，端点也颠倒着传 */
    }
    const ConnectivityRun flipped_run = analyze_connectivity(flipped);
    checks.expect(flipped_run.dfn == conn.dfn && flipped_run.low == conn.low &&
                      flipped_run.bridges == conn.bridges &&
                      flipped_run.cut_vertices == conn.cut_vertices,
                  u8"把 10 条边倒过来加、端点颠倒着传，dfn、low、桥、割点全都不变");

    /* 5—6 DFS 树 */
    std::vector<int> dfn_sorted = conn.dfn;
    std::sort(dfn_sorted.begin(), dfn_sorted.end());
    bool permutation = (dfn_sorted.size() == static_cast<std::size_t>(n));
    for (int i = 0; i < n && permutation; ++i) {
        if (dfn_sorted[static_cast<std::size_t>(i)] != i + 1) {
            permutation = false;
        }
    }
    checks.expect(permutation && conn.visits == static_cast<std::size_t>(n),
                  u8"dfn 是 1 到 9 的一个排列，访问次数 9 次");

    bool tree_ok = (conn.tree_edges.size() == static_cast<std::size_t>(n - 1));
    std::size_t child_total = 0;
    for (int v = 0; v < n; ++v) {
        child_total += static_cast<std::size_t>(conn.dfs_children[static_cast<std::size_t>(v)]);
        const int p = conn.parent[static_cast<std::size_t>(v)];
        if (p == -1) {
            continue;
        }
        const std::vector<int> &nb = g.neighbors(v);
        if (!std::binary_search(nb.begin(), nb.end(), p)) {
            tree_ok = false;
        }
    }
    checks.expect(tree_ok && child_total == static_cast<std::size_t>(n - 1),
                  u8"DFS 树有 8 条树边，每个顶点的父都是它的邻接顶点");

    /* 7 low 按定义重算 */
    checks.expect(low_by_brute_force(g, conn) == conn.low,
                  u8"low 按定义（子树里连出去的非树边）重算一遍，9 个值逐位相同");

    /* 8—9 割点与桥，按定义核对 */
    const std::vector<int> brute_cuts = cut_vertices_by_brute_force(g);
    checks.expect_detail(conn.cut_vertices == brute_cuts &&
                             conn.cut_vertices == std::vector<int>{1, 3, 5, 6},
                         u8"割点 {1, 3, 5, 6}，与逐个删顶点试出来的完全一致",
                         u8"（一次 DFS 得到 " + join_ints(conn.cut_vertices) +
                             u8"，逐个删顶点得到 " + join_ints(brute_cuts) + u8"）");

    const std::vector<Edge> want_bridges = {
        Edge{1, 3, weight_of(g, 1, 3)},
        Edge{1, 8, weight_of(g, 1, 8)},
        Edge{5, 6, weight_of(g, 5, 6)},
        Edge{6, 7, weight_of(g, 6, 7)},
    };
    const std::vector<Edge> brute_bridges = bridges_by_brute_force(g);
    checks.expect_detail(conn.bridges == want_bridges && same_edge_set(brute_bridges, want_bridges),
                         u8"桥 (1,3)、(1,8)、(5,6)、(6,7)，与逐条删边试出来的完全一致",
                         u8"（一次 DFS 得到 " + edges_text(conn.bridges) +
                             u8"，逐条删边得到 " + edges_text(brute_bridges) + u8"）");

    /* 10—12 去掉割点、去掉桥之后的连通块 */
    std::vector<std::size_t> block_counts;
    bool sizes_sum_ok = true;
    for (const int v : conn.cut_vertices) {
        const std::vector<std::size_t> sizes = component_sizes_without_vertex(g, v);
        block_counts.push_back(sizes.size());
        std::size_t sum = 0;
        for (const std::size_t size : sizes) {
            sum += size;
        }
        if (sum != static_cast<std::size_t>(n - 1)) {
            sizes_sum_ok = false;
        }
    }
    checks.expect(block_counts == std::vector<std::size_t>{3, 2, 2, 2} && sizes_sum_ok,
                  u8"去掉 4 个割点之后分别分成 3、2、2、2 块，各块大小之和都是 8");

    bool non_cut_ok = true;
    for (int v = 0; v < n; ++v) {
        if (std::find(conn.cut_vertices.begin(), conn.cut_vertices.end(), v) !=
            conn.cut_vertices.end()) {
            continue;
        }
        if (component_sizes_without_vertex(g, v).size() != 1) {
            non_cut_ok = false;
        }
    }
    checks.expect(non_cut_ok, u8"5 个非割点（0、2、4、7、8）去掉之后仍然只有 1 个连通块");

    bool bridge_blocks_ok = true;
    for (const Edge &e : conn.bridges) {
        if (component_sizes_without_edge(g, e).size() != 2) {
            bridge_blocks_ok = false;
        }
    }
    checks.expect(bridge_blocks_ok, u8"每条桥单独去掉，连通块数都从 1 变成 2");

    /* 13—16 拓扑排序 */
    const TopoRun kahn = topo_sort_kahn(dag);
    checks.expect(kahn.acyclic && kahn.order.size() == static_cast<std::size_t>(n) &&
                      kahn.dequeues == static_cast<std::size_t>(n) &&
                      kahn.enqueues == static_cast<std::size_t>(n) &&
                      is_topological_order(dag, kahn.order),
                  u8"Kahn 在无环版上：入队 9 次、出队 9 次，排出的序列是合法拓扑序");
    checks.expect_ints(kahn.order, std::vector<int>{0, 4, 7, 1, 2, 8, 3, 5, 6},
                       u8"Kahn 用最小堆时的出队顺序是 0 4 7 1 2 8 3 5 6");

    const TopoRun post = topo_sort_dfs_postorder(dag);
    checks.expect(post.enqueues == static_cast<std::size_t>(n) &&
                      post.dequeues == static_cast<std::size_t>(n) &&
                      is_topological_order(dag, post.order),
                  u8"DFS 后序取逆：入栈 9 次、出栈 9 次，也是合法拓扑序");
    checks.expect_ints(post.order, std::vector<int>{4, 0, 7, 2, 1, 8, 5, 3, 6},
                       u8"DFS 后序取逆的顺序是 4 0 7 2 1 8 5 3 6，与 Kahn 的不同");

    /* 17—18 有环版：Kahn 与环 */
    const TopoRun bad = topo_sort_kahn(cyc);
    bool remaining_indegree_ok = true;
    for (const int degree : bad.remaining_in_degree) {
        if (degree == 0) {
            remaining_indegree_ok = false;
        }
    }
    checks.expect(!bad.acyclic && bad.enqueues == 1 && bad.dequeues == 1 &&
                      bad.remaining.size() == 8 && remaining_indegree_ok,
                  u8"有环版：Kahn 只入队、出队各 1 次就空了，还剩 8 个顶点的入度不为 0");

    const SccRun scc = tarjan_scc(cyc);
    std::vector<int> on_cycle;
    for (const std::vector<int> &component : scc.components) {
        if (component.size() >= 2) {
            for (const int v : component) {
                on_cycle.push_back(v);
            }
        }
    }
    std::sort(on_cycle.begin(), on_cycle.end());
    checks.expect_ints(on_cycle, std::vector<int>{0, 1, 2, 5, 6, 7, 8},
                       u8"大小超过 1 的分量合起来是 7 个顶点：0、1、2、5、6、7、8");

    std::vector<int> blocked;
    for (int v = 0; v < cyc_n; ++v) {
        for (const int u : on_cycle) {
            if (reaches(cyc, u, v)) {
                blocked.push_back(v);
                break;
            }
        }
    }
    checks.expect_ints(blocked, bad.remaining,
                       u8"Kahn 剩下的 8 个顶点正好是「在环上」加上「从环上可达」，顶点 3 属于后者");

    /* 19—22 强连通分量 */
    checks.expect(scc.components.size() == 4 && scc.visits == 9 && scc.edge_checks == 13 &&
                      scc.pushes == 9 && scc.pops == 9,
                  u8"Tarjan：4 个分量，访问 9 次、检查出边 13 条、入栈 9 次、出栈 9 次");
    const std::vector<std::vector<int>> want_components = {
        {5, 6}, {3}, {0, 1, 2, 7, 8}, {4}};
    checks.expect(scc.components == want_components,
                  u8"按弹出先后：{5,6}、{3}、{0,1,2,7,8}、{4}");

    bool mutual_ok = true;
    for (int a = 0; a < cyc_n; ++a) {
        for (int b = 0; b < cyc_n; ++b) {
            const bool same = (scc.component_of[static_cast<std::size_t>(a)] ==
                               scc.component_of[static_cast<std::size_t>(b)]);
            const bool both_ways = reaches(cyc, a, b) && reaches(cyc, b, a);
            if (same != both_ways) {
                mutual_ok = false;
            }
        }
    }
    checks.expect(mutual_ok,
                  u8"两个顶点在同一个分量里，当且仅当它们互相可达（9 × 9 对全查一遍）");

    bool reverse_topo_ok = true;
    std::vector<std::pair<int, int>> arcs;
    for (const Edge &e : cyc.edges()) {
        const int a = scc.component_of[static_cast<std::size_t>(e.u)];
        const int b = scc.component_of[static_cast<std::size_t>(e.v)];
        if (a == b) {
            continue;
        }
        if (b >= a) {
            reverse_topo_ok = false;      /* 分量编号就是弹出先后，边只能指向更早弹出的 */
        }
        const std::pair<int, int> arc(a, b);
        if (std::find(arcs.begin(), arcs.end(), arc) == arcs.end()) {
            arcs.push_back(arc);
        }
    }
    checks.expect(reverse_topo_ok && arcs.size() == 4,
                  u8"分量弹出顺序是凝结图的逆拓扑序，凝结图有 4 条边");

    /* 23—26 最小生成树 */
    const KruskalRun kruskal = kruskal_mst(g);
    checks.expect(kruskal.spanning && kruskal.chosen.size() == 8 && kruskal.total_weight == 44,
                  u8"Kruskal：8 条边、总权值 44，是一棵生成树");
    checks.expect(kruskal.edge_checks == 10 && kruskal.find_calls == 20 &&
                      kruskal.unions == 8 && kruskal.rejected == 2,
                  u8"Kruskal：看边 10 条、find 20 次等于 2 × 边检查、union 8 次、成环跳过 2 条");

    bool sorted_ok = (kruskal.sorted_edges.size() == 10);
    for (std::size_t i = 1; i < kruskal.sorted_edges.size(); ++i) {
        if (kruskal.sorted_edges[i - 1].weight > kruskal.sorted_edges[i].weight) {
            sorted_ok = false;
        }
    }
    checks.expect(sorted_ok && kruskal.sort_comparisons == 17,
                  u8"插入排序把 10 条边排成升序，比较权值 17 次");

    const PrimRun prim = prim_mst(g, 0);
    checks.expect(prim.spanning && prim.chosen.size() == 8 && prim.total_weight == 44 &&
                      prim.edge_checks == 20,
                  u8"Prim 从顶点 0 起步：8 条边、总权值 44、边检查 20 次等于 2 × 边数");
    checks.expect(prim.pushes == prim.pops + prim.heap_left && prim.stale_pops == 2,
                  u8"Prim：push 11 次、pop 11 次、堆里一条不剩，其中 2 次是过期弹出");

    bool prim_same = same_edge_set(prim.chosen, kruskal.chosen);
    bool all_starts_ok = true;
    for (int s = 0; s < n; ++s) {
        if (prim_mst(g, s).total_weight != kruskal.total_weight) {
            all_starts_ok = false;
        }
    }
    checks.expect(prim_same,
                  u8"Prim 选出的边与 Kruskal 逐条相同（权值两两不同，最小生成树唯一）");
    checks.expect(all_starts_ok, u8"Prim 从 9 个顶点分别起步，总权值都是 44");

    /* 27 并查集本身 */
    DisjointSet dsu(9);
    const bool self_union = dsu.unite(1, 1);
    const bool first = dsu.unite(1, 2);
    const bool second = dsu.unite(2, 3);
    const bool again = dsu.unite(3, 1);
    checks.expect(!self_union && first && second && !again &&
                      dsu.find(1) == dsu.find(3) && dsu.unions() == 2,
                  u8"并查集：自合并返回假，同一对顶点重复合并只有第一次成功");

    return checks.finish();
}

}   /* namespace glab */
