/* graphalgo.cpp —— 练习模板 08 的实现（C++）
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
 * 本模板的 4 处 TODO 全在这个文件里，按阶段编号：
 *
 *     阶段 1-1   kahn_toposort       取出一个点之后，邻居的入度怎么改、什么时候入队
 *     阶段 2-1   tarjan_dfs          low 的两条更新规则
 *     阶段 3-1   find_cut_vertices   割点的判定（桥的判定已给出）
 *     阶段 4-1   kruskal             取出一条边之后，判断、加不加、什么时候收工
 *
 * 每个 TODO 上面写明「要做什么」，并给出「判据」——那些数都由
 * src/main_cli.cpp 打印，逐个对得上才算做完。
 *
 * 图的数据写死在本文件里，重跑逐位相同。图的存法与并查集都属于
 * 09-高阶数据结构 板块，本模板只使用它们。
 */
#include "graphalgo.hpp"

#include <algorithm>

namespace galgo {

/* ==================================================================
 * 已给出：写死在源码里的图
 * ================================================================== */

AdjList sample_digraph()
{
    /* 9 个点、12 条边。两个环：0→1→2→0 与 5→6→5。
     * 点 7 与点 8 从 0 出发走不到，因此 Tarjan 那一步会有第二趟 DFS。
     * 6→4 是一条指向「已经收完的分量」的边，是 low 更新里最容易写错的一处。 */
    AdjList g(9);
    g[0] = {1, 2, 3, 5};
    g[1] = {2};
    g[2] = {1, 3};
    g[3] = {4};
    /* g[4] 没有出边 */
    g[5] = {6};
    g[6] = {4, 5};
    g[7] = {8};
    /* g[8] 没有出边 */
    return g;
}

AdjList sample_digraph_acyclic()
{
    AdjList g = sample_digraph();

    /* 去掉两条回边：2→1 与 6→5。剩下的图没有环，拓扑排序有解。 */
    std::vector<int> &row2 = g[2];
    row2.erase(std::remove(row2.begin(), row2.end(), 1), row2.end());

    std::vector<int> &row6 = g[6];
    row6.erase(std::remove(row6.begin(), row6.end(), 5), row6.end());

    return g;
}

int sample_undirected_n()
{
    return 8;
}

std::vector<WeightedEdge> sample_undirected_edges()
{
    /* 8 个点、11 条边。两块稠密处（0-1-2-3 与 4-5-6）由桥接起来，
     * 7 是挂在 6 上的一片叶子。权两两不同，因此最小生成树唯一。 */
    std::vector<WeightedEdge> e;
    e.push_back({0, 1, 1});
    e.push_back({1, 2, 2});
    e.push_back({0, 2, 3});
    e.push_back({3, 4, 4});
    e.push_back({6, 7, 5});
    e.push_back({4, 5, 6});
    e.push_back({5, 6, 7});
    e.push_back({2, 3, 8});
    e.push_back({0, 3, 9});
    e.push_back({4, 6, 10});
    e.push_back({1, 3, 11});
    return e;
}

AdjList to_unweighted(int n, const std::vector<WeightedEdge> &edges)
{
    AdjList g(static_cast<std::size_t>(n));
    for (const WeightedEdge &e : edges) {
        g[static_cast<std::size_t>(e.u)].push_back(e.v);
        g[static_cast<std::size_t>(e.v)].push_back(e.u);
    }
    for (std::vector<int> &row : g) {
        std::sort(row.begin(), row.end());
    }
    return g;
}

long long directed_edge_count(const AdjList &g)
{
    long long total = 0;
    for (const std::vector<int> &row : g) {
        total += static_cast<long long>(row.size());
    }
    return total;
}

/* ==================================================================
 * 阶段 1：拓扑排序的入度更新
 * ================================================================== */

TopoResult kahn_toposort(const AdjList &g)
{
    const int n = static_cast<int>(g.size());
    TopoResult r;

    /* 已给出：入度数组 —— 每个点被多少条边指着 */
    std::vector<int> indeg(static_cast<std::size_t>(n), 0);
    for (int u = 0; u < n; ++u) {
        for (int v : g[static_cast<std::size_t>(u)]) {
            ++indeg[static_cast<std::size_t>(v)];
        }
    }

    /* 已给出：入度为 0 的点先进队列。按编号从小到大进，保证重跑次序相同。
     * 队列是先进先出，用 head 下标往前推，不真的从中间删除。 */
    std::vector<int> queue;
    std::size_t head = 0;
    for (int v = 0; v < n; ++v) {
        if (indeg[static_cast<std::size_t>(v)] == 0) {
            queue.push_back(v);
            r.enqueued.push_back(v);
        }
    }

    /* TODO（阶段 1-1）：
     * 队列里还有人，就取出队首的那一个，按取出的先后写进 r.order。
     * 取出之后逐条看它的出边：这条边指向的那个点，入度要减一
     * ——排在前面的一端已经把它消化掉了，每减一次让 r.decrements 加一。
     * 入度减到 0 的点，说明它的前驱全部排完了，这时候才轮到它进队列，
     * 同时按进队列的先后写进 r.enqueued。入度还没到 0 的点不要提前放进去。
     * 队列空了就收工，剩下的收尾由下面的已给出部分处理。
     * 判据（见《配置步骤.md》阶段 1）：
     *   无环图上 order 是 0 7 1 5 8 2 6 3 4，长度 9，enqueued 与它相同，
     *   decrements 是 10（正好等于边数），还有入度的点数是 0，has cycle 是 no；
     *   有环图上 order 是空的、has cycle 是 yes、enqueued 只有 0 7 8、
     *   decrements 是 5，还有入度的点数是 6。 */

    /* 占位实现：一个点也没有取出来。上面那处 TODO 写完之后整段删掉。 */
    (void)head;

    /* 已给出：结果存放 —— 出队顺序就是拓扑序列；排完的点数不够，
     * 说明剩下的点互相等着，图里有环。 */
    if (static_cast<int>(r.order.size()) != n) {
        r.order.clear();
        r.has_cycle = true;
    }
    r.indeg_left = indeg;
    return r;
}

/* ==================================================================
 * 阶段 2：Tarjan 的 low 更新
 * ================================================================== */

TarjanState make_tarjan_state(const AdjList &g)
{
    TarjanState st;
    st.g = &g;
    const std::size_t n = g.size();
    st.dfn.assign(n, 0);
    st.low.assign(n, 0);
    st.in_stack.assign(n, 0);
    return st;
}

void tarjan_dfs(int u, TarjanState &st)
{
    const AdjList &g = *st.g;
    const std::size_t ui = static_cast<std::size_t>(u);

    /* 已给出：分配 DFS 序号，dfn 与 low 先记成同一个数 */
    ++st.timer;
    st.dfn[ui] = st.timer;
    st.low[ui] = st.timer;

    /* 已给出：进栈，并打上「还在栈里」的标记 */
    st.stack.push_back(u);
    st.in_stack[ui] = 1;

    /* TODO（阶段 2-1）：low 的两条更新规则，就在下面这个循环的两处留空里。
     * 一条管「子树回来了」，一条管「边指向一个还在栈里的点」。
     * 两条都是把当前这个点的 low 往更早的序号上拉，拉不拉得动要看对面那一点。
     * 判据（见《配置步骤.md》阶段 2）：
     *   分量个数 7，成员依次是 4 / 3 / 1 2 / 5 6 / 0 / 8 / 7；
     *   dfn 是 1 2 3 4 5 6 7 8 9，low 是 1 2 2 4 5 6 6 8 9；
     *   scc matches reachability 那一行要判成 yes。 */
    for (int v : g[ui]) {
        const std::size_t vi = static_cast<std::size_t>(v);

        if (st.dfn[vi] == 0) {
            /* 已给出：树边 —— 先递归下去，回来再看这一棵子树 */
            tarjan_dfs(v, st);

            /* 留空 A（阶段 2-1）：
             * 子树回来了。子树里如果有一条路能绕回 u 的祖先，
             * 那么从 u 出发也能顺着这条路绕上去，u 的 low 因此要跟着子树走。
             * 占位实现：本处不写，low 停在 dfn 上。 */
        } else if (st.in_stack[vi] != 0) {
            /* 留空 B（阶段 2-1）：
             * v 访问过，而且还在栈里 —— 它比 u 更早被发现，u 有一条路绕回它。
             * 占位实现：本处不写，low 停在 dfn 上。 */
        }
        /* 已给出：v 访问过但已经不在栈里，说明它属于另一个已经收完的分量，
         * 这条路绕不过去，low 不动。 */
    }

    /* 已给出：弹栈与分量收集 —— low 与 dfn 相等时，u 就是本分量的根，
     * 从栈顶一路弹到 u 为止，弹出来的这些点同属一个分量。 */
    if (st.low[ui] == st.dfn[ui]) {
        std::vector<int> comp;
        for (;;) {
            const int w = st.stack.back();
            st.stack.pop_back();
            st.in_stack[static_cast<std::size_t>(w)] = 0;
            comp.push_back(w);
            if (w == u) {
                break;
            }
        }
        std::sort(comp.begin(), comp.end());
        st.comps.push_back(comp);
    }
}

SccResult tarjan_scc(const AdjList &g)
{
    TarjanState st = make_tarjan_state(g);
    const int n = static_cast<int>(g.size());

    for (int u = 0; u < n; ++u) {
        if (st.dfn[static_cast<std::size_t>(u)] == 0) {
            tarjan_dfs(u, st);
        }
    }

    SccResult r;
    r.comps = st.comps;
    r.dfn = st.dfn;
    r.low = st.low;
    return r;
}

/* ==================================================================
 * 阶段 3：割点与桥
 * ================================================================== */

namespace {

/* 已给出：割点与桥的 DFS 状态 */
struct CutDfs {
    const AdjList *g = nullptr;
    std::vector<int> dfn;
    std::vector<int> low;
    std::vector<int> cut;                       /* 找到的割点，同一个点可能进多次 */
    std::vector<std::pair<int, int>> bridges;
    int timer = 0;
};

void cut_dfs(int u, int parent, CutDfs &st)
{
    const AdjList &g = *st.g;
    const std::size_t ui = static_cast<std::size_t>(u);

    /* 已给出：分配 DFS 序号 */
    ++st.timer;
    st.dfn[ui] = st.timer;
    st.low[ui] = st.timer;

    int children = 0;   /* 已给出：u 在 DFS 树上有几个孩子 */

    for (int v : g[ui]) {
        const std::size_t vi = static_cast<std::size_t>(v);

        if (v == parent) {
            continue;                       /* 已给出：不走回父亲的那条边 */
        }

        if (st.dfn[vi] != 0) {
            /* 已给出：v 访问过 —— 一条回边，u 能绕到更早的点 */
            if (st.dfn[vi] < st.low[ui]) {
                st.low[ui] = st.dfn[vi];
            }
            continue;
        }

        /* 已给出：树边 —— 递归下去 */
        ++children;
        cut_dfs(v, u, st);

        /* 已给出：桥的判定收集 —— 子树里没有别的路能回到 u 或 u 的祖先，
         * 这条边就是必经之路，删掉它，子树整块掉出去。
         * 收进结果时按 (小, 大) 存，与暴力验证的写法一致。 */
        if (st.low[vi] > st.dfn[ui]) {
            const int a = u < v ? u : v;
            const int b = u < v ? v : u;
            st.bridges.push_back(std::make_pair(a, b));
        }

        /* TODO（阶段 3-1）：
         * 割点的判定。u 是不是割点，要看 v 这棵子树：子树里有没有别的路
         * 能绕回 u 自己或 u 的祖先。绕得回去，删掉 u 之后子树还连在上面；
         * 绕不回去，删掉 u 就会把子树整块带走，u 就是一个割点。
         * 桥的判定就在上面，两者的差别只在一个等号上——想清楚是哪一个方向，
         * 并且想清楚「u 是这一趟 DFS 的起点」时要另外按什么来判：
         * 起点没有祖先，上面那条路走不通，它只能靠孩子的个数说话。
         * 判出来是割点就把它写进 st.cut，重复写进去没关系，收尾时会去重。
         * 判据（见《配置步骤.md》阶段 3）：割点是 3 4 6，
         *   与逐个删点试出来的集合一致；桥是 (3,4) 与 (6,7)，
         *   与逐条删边试出来的集合一致。 */

        /* 占位实现：下面这一行只是让 children 不算未使用，
         * 上面那处 TODO 写完之后整段删掉。 */
        (void)children;

        /* 已给出：子树回来了，u 的 low 往子树那里靠拢 */
        if (st.low[vi] < st.low[ui]) {
            st.low[ui] = st.low[vi];
        }
    }
}

} /* namespace */

CutResult find_cut_vertices(const AdjList &g)
{
    CutResult r;
    CutDfs st;
    st.g = &g;
    const std::size_t n = g.size();
    st.dfn.assign(n, 0);
    st.low.assign(n, 0);

    /* 已给出：对每个还没访问到的点各起一趟 DFS，parent 传 -1 */
    for (int u = 0; u < static_cast<int>(n); ++u) {
        if (st.dfn[static_cast<std::size_t>(u)] == 0) {
            cut_dfs(u, -1, st);
        }
    }

    /* 已给出：收尾 —— 割点去重后升序，桥按 (小, 大) 的字典序升序 */
    std::sort(st.cut.begin(), st.cut.end());
    st.cut.erase(std::unique(st.cut.begin(), st.cut.end()), st.cut.end());
    std::sort(st.bridges.begin(), st.bridges.end());

    r.cut_vertices = st.cut;
    r.bridges = st.bridges;
    r.dfn = st.dfn;
    r.low = st.low;
    return r;
}

/* ==================================================================
 * 阶段 4：Kruskal 的并查集接入
 * ================================================================== */

DisjointSet::DisjointSet(int n)
    : parent_(static_cast<std::size_t>(n)),
      size_(static_cast<std::size_t>(n), 1),
      count_(n)
{
    for (int i = 0; i < n; ++i) {
        parent_[static_cast<std::size_t>(i)] = i;
    }
}

int DisjointSet::find(int x)
{
    /* 已给出：先一路往上找到根，再把路上遇到的点全挂到根上（路径压缩） */
    int root = x;
    while (parent_[static_cast<std::size_t>(root)] != root) {
        root = parent_[static_cast<std::size_t>(root)];
    }
    while (parent_[static_cast<std::size_t>(x)] != root) {
        const int next = parent_[static_cast<std::size_t>(x)];
        parent_[static_cast<std::size_t>(x)] = root;
        x = next;
    }
    return root;
}

void DisjointSet::unite(int a, int b)
{
    /* 已给出：按大小合并 —— 小的那棵树挂到大的下面，树高压得住 */
    int ra = find(a);
    int rb = find(b);
    if (ra == rb) {
        return;
    }
    if (size_[static_cast<std::size_t>(ra)] < size_[static_cast<std::size_t>(rb)]) {
        std::swap(ra, rb);
    }
    parent_[static_cast<std::size_t>(rb)] = ra;
    size_[static_cast<std::size_t>(ra)] += size_[static_cast<std::size_t>(rb)];
    --count_;
}

int DisjointSet::size_of(int x)
{
    return size_[static_cast<std::size_t>(find(x))];
}

MstResult kruskal(int n, std::vector<WeightedEdge> edges)
{
    MstResult r;

    /* 已给出：按权从小到大排。本模板的权两两不同，因此这个次序唯一，
     * 重跑得到的边集与加入顺序逐位相同。 */
    std::sort(edges.begin(), edges.end(),
              [](const WeightedEdge &a, const WeightedEdge &b) { return a.w < b.w; });

    /* 已给出：并查集 —— find 带路径压缩，unite 按大小合并 */
    DisjointSet dsu(n);

    /* 已给出：从排好序的边表里一条一条取出来 */
    for (const WeightedEdge &e : edges) {
        ++r.considered;     /* 已给出：取出一条就记一条 */

        /* TODO（阶段 4-1）：
         * 这条边的两端，现在是不是已经连在同一棵树上？
         * 已经连通，加上它就会成环，放弃它，并让 r.skipped 加一；
         * 还没连通，它就是最小生成树的一条边：按加入顺序写进 r.chosen，
         * 把它的权累加进 r.total_weight，再把两端合成一棵树。
         * 另外要盯住树的规模：一个 n 个点的连通图，最小生成树正好 n-1 条边，
         * 凑够这个数就不必再往下看了，剩下的边连取都不用取
         * ——「取出了几条」与「跳过了几条」都算进判据，取多了就对不上。
         * 判据（见《配置步骤.md》阶段 4）：取出 8 条、选中 7 条、跳过 1 条，
         *   剩下 3 条根本没取；总权 33；收工时只剩 1 个集合；
         *   选中的边按加入顺序是 0-1(1)、1-2(2)、3-4(4)、6-7(5)、4-5(6)、
         *   5-6(7)、2-3(8)。 */

        /* 占位实现：一条也不选。上面那处 TODO 写完之后整段删掉。 */
        (void)e;
    }

    /* 已给出：收工时还剩几个集合 */
    r.sets_left = dsu.set_count();
    return r;
}

/* ==================================================================
 * 已给出的工具：暴力验证与分量验证
 * ================================================================== */

std::vector<int> brute_force_cut_vertices(const AdjList &g)
{
    /* 逐个删点：删掉 ban 之后从编号最小的点出发能走到几个点，
     * 走不到 n-1 个，说明 ban 把图拆开了。 */
    const int n = static_cast<int>(g.size());
    std::vector<int> out;

    for (int ban = 0; ban < n; ++ban) {
        int start = -1;
        for (int v = 0; v < n; ++v) {
            if (v != ban) {
                start = v;
                break;
            }
        }
        if (start < 0) {
            continue;
        }

        std::vector<char> seen(static_cast<std::size_t>(n), 0);
        std::vector<int> stack;
        stack.push_back(start);
        seen[static_cast<std::size_t>(start)] = 1;
        int reached = 0;
        while (!stack.empty()) {
            const int u = stack.back();
            stack.pop_back();
            ++reached;
            for (int v : g[static_cast<std::size_t>(u)]) {
                if (v == ban || seen[static_cast<std::size_t>(v)] != 0) {
                    continue;
                }
                seen[static_cast<std::size_t>(v)] = 1;
                stack.push_back(v);
            }
        }

        if (reached != n - 1) {
            out.push_back(ban);
        }
    }
    return out;
}

namespace {

/* 已给出：删掉 a-b 这条边之后，全图还连不连通 */
bool connected_without_edge(const AdjList &g, int a, int b)
{
    const std::size_t n = g.size();
    std::vector<char> seen(n, 0);
    std::vector<int> stack;
    stack.push_back(0);
    seen[0] = 1;
    std::size_t reached = 0;

    while (!stack.empty()) {
        const int u = stack.back();
        stack.pop_back();
        ++reached;
        for (int v : g[static_cast<std::size_t>(u)]) {
            if (seen[static_cast<std::size_t>(v)] != 0) {
                continue;
            }
            if ((u == a && v == b) || (u == b && v == a)) {
                continue;
            }
            seen[static_cast<std::size_t>(v)] = 1;
            stack.push_back(v);
        }
    }
    return reached == n;
}

} /* namespace */

std::vector<std::pair<int, int>> brute_force_bridges(const AdjList &g)
{
    /* 逐条删边：邻接表里每条无向边出现两次，只看 u 小于 v 的那一次。 */
    const int n = static_cast<int>(g.size());
    std::vector<std::pair<int, int>> out;

    for (int u = 0; u < n; ++u) {
        for (int v : g[static_cast<std::size_t>(u)]) {
            if (u >= v) {
                continue;
            }
            if (!connected_without_edge(g, u, v)) {
                out.push_back(std::make_pair(u, v));
            }
        }
    }
    std::sort(out.begin(), out.end());
    return out;
}

bool scc_matches_reachability(const AdjList &g, const std::vector<std::vector<int>> &comps)
{
    /* 已给出：按定义验一遍分量。
     * 先算出「谁能走到谁」，再检查三件事：
     * 每个点恰好属于一个分量、同一个分量里两两可达、不同分量之间不互相可达。 */
    const std::size_t n = g.size();
    std::vector<std::vector<char>> reach(n, std::vector<char>(n, 0));

    for (std::size_t s = 0; s < n; ++s) {
        std::vector<int> stack;
        stack.push_back(static_cast<int>(s));
        reach[s][s] = 1;
        while (!stack.empty()) {
            const std::size_t u = static_cast<std::size_t>(stack.back());
            stack.pop_back();
            for (int v : g[u]) {
                const std::size_t vi = static_cast<std::size_t>(v);
                if (reach[s][vi] == 0) {
                    reach[s][vi] = 1;
                    stack.push_back(v);
                }
            }
        }
    }

    std::vector<int> owner(n, -1);
    for (std::size_t c = 0; c < comps.size(); ++c) {
        for (int v : comps[c]) {
            const std::size_t vi = static_cast<std::size_t>(v);
            if (vi >= n || owner[vi] != -1) {
                return false;               /* 越界，或者一个点被算进了两个分量 */
            }
            owner[vi] = static_cast<int>(c);
        }
    }
    for (std::size_t v = 0; v < n; ++v) {
        if (owner[v] == -1) {
            return false;                   /* 有点不属于任何分量 */
        }
    }

    for (std::size_t a = 0; a < n; ++a) {
        for (std::size_t b = 0; b < n; ++b) {
            const bool both_ways = reach[a][b] != 0 && reach[b][a] != 0;
            if (both_ways != (owner[a] == owner[b])) {
                return false;               /* 互相可达 与 同分量 必须完全一致 */
            }
        }
    }
    return true;
}

} /* namespace galgo */
