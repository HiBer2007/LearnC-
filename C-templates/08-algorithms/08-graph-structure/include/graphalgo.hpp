/* graphalgo.hpp —— 练习模板 08 的核心接口（C++）
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
 * 图上的结构性问题拆成 4 个阶段，每个阶段的实现都写在 src/graphalgo.cpp 里：
 *
 *     阶段 1  kahn_toposort      拓扑排序的入度更新
 *     阶段 2  tarjan_dfs         Tarjan 的 low 更新
 *     阶段 3  find_cut_vertices  割点与桥
 *     阶段 4  kruskal            Kruskal 的并查集接入
 *
 * 图只用一种存法：邻接表 std::vector<std::vector<int>>，图的数据写死在
 * src/graphalgo.cpp 里，不读文件、不用随机数，重跑逐位相同。
 * 图的存法本身属于 09-高阶数据结构 板块（见《09-高阶数据结构/
 * A-10-图：关系怎么存.md》第 3 节），本模板只使用它，不讲解它。
 *
 * 并查集 DisjointSet 由本骨架完整给出（同属 09 板块，见《09-高阶数据结构/
 * A-12-不相交集合：只要连通性.md》第 3 节），阶段 4 只做「把它接进 Kruskal」。
 */
#ifndef GRAPHALGO_HPP
#define GRAPHALGO_HPP

#include <cstddef>
#include <utility>
#include <vector>

namespace galgo {

/* 邻接表：g[u] 里是 u 的所有出边终点（有向图）或邻居（无向图），按编号升序 */
using AdjList = std::vector<std::vector<int>>;

/* 无向带权图的一条边；约定 u 小于 v，w 是权 */
struct WeightedEdge {
    int u = 0;
    int v = 0;
    int w = 0;
};

/* ------------------------------------------------------------------
 * 已给出：写死在源码里的图
 * ------------------------------------------------------------------ */

/* 有向图：9 个点、12 条边，含两个环 —— 0→1→2→0 与 5→6→5 */
AdjList sample_digraph();

/* 同一张有向图，去掉 2→1 与 6→5 两条回边之后没有环：9 个点、10 条边 */
AdjList sample_digraph_acyclic();

/* 无向带权图：8 个点、11 条边 */
int sample_undirected_n();
std::vector<WeightedEdge> sample_undirected_edges();

/* 已给出：把带权边表转成不带权的邻接表（每条边两个方向都加，行内升序） */
AdjList to_unweighted(int n, const std::vector<WeightedEdge> &edges);

/* 已给出：有向图的边数（邻接表里每一项都是一条出边） */
long long directed_edge_count(const AdjList &g);

/* ---------- 阶段 1：拓扑排序的入度更新 ---------- */

struct TopoResult {
    std::vector<int> order;         /* 出队顺序；判出有环时被清空 */
    std::vector<int> enqueued;      /* 进过队列的顺序 */
    std::vector<int> indeg_left;    /* 收工时每个点还剩多少入度 */
    long long decrements = 0;       /* 入度被减一的次数 */
    bool has_cycle = false;
};

/* 卡恩拓扑排序。入度数组的初始化、队列、结果的存放都已经给出，
 * 留空的是「取出一个点之后，怎么改它邻居的入度、什么时候把邻居入队」。 */
TopoResult kahn_toposort(const AdjList &g);

/* ---------- 阶段 2：Tarjan 的 low 更新 ---------- */

/* 已给出：一趟 Tarjan 遍历要用的全部状态 */
struct TarjanState {
    const AdjList *g = nullptr;
    std::vector<int> dfn;                   /* DFS 序号，从 1 开始，0 表示还没访问 */
    std::vector<int> low;                   /* 能绕回去的最早序号 */
    std::vector<int> stack;                 /* 当栈用的 vector */
    std::vector<char> in_stack;             /* 是否还在栈里 */
    std::vector<std::vector<int>> comps;    /* 收集到的分量，按弹出顺序 */
    int timer = 0;
};

struct SccResult {
    std::vector<std::vector<int>> comps;    /* 每个分量内部按编号升序 */
    std::vector<int> dfn;
    std::vector<int> low;
};

/* 已给出：按图的大小把状态准备好（dfn 与 low 全 0，栈为空） */
TarjanState make_tarjan_state(const AdjList &g);

/* 从 u 出发的一趟 DFS。dfn／low 的分配、进栈、弹栈与分量收集都已经给出，
 * 留空的是 low 的两条更新规则。 */
void tarjan_dfs(int u, TarjanState &st);

/* 已给出：对每个还没访问到的点各起一趟 DFS，返回全图的分量 */
SccResult tarjan_scc(const AdjList &g);

/* 已给出：按定义验证分量 —— 每个分量内部两两可达，不同分量之间不互相可达 */
bool scc_matches_reachability(const AdjList &g, const std::vector<std::vector<int>> &comps);

/* ---------- 阶段 3：割点与桥 ---------- */

struct CutResult {
    std::vector<int> cut_vertices;              /* 升序，无重复 */
    std::vector<std::pair<int, int>> bridges;   /* 每项 (小, 大)，字典序升序 */
    std::vector<int> dfn;
    std::vector<int> low;
};

/* DFS 骨架、dfn／low 的分配、回边的处理与「桥」的判定收集都已经给出，
 * 留空的是「割点」的判定。 */
CutResult find_cut_vertices(const AdjList &g);

/* 已给出：暴力验证 —— 逐个删点，看剩下的图还连不连通 */
std::vector<int> brute_force_cut_vertices(const AdjList &g);

/* 已给出：暴力验证 —— 逐条删边，看剩下的图还连不连通 */
std::vector<std::pair<int, int>> brute_force_bridges(const AdjList &g);

/* ---------- 阶段 4：Kruskal 的并查集接入 ---------- */

/* 已给出：并查集 —— find 带路径压缩，unite 按大小合并 */
class DisjointSet {
public:
    explicit DisjointSet(int n);

    int find(int x);            /* 找根，顺路把路径上的点都挂到根上 */
    void unite(int a, int b);   /* 把两棵树合成一棵，小的挂到大的下面 */
    int set_count() const { return count_; }
    int size_of(int x);         /* 所在集合的大小 */

private:
    std::vector<int> parent_;   /* 根指向自己 */
    std::vector<int> size_;     /* 只有根那一项有意义 */
    int count_ = 0;
};

struct MstResult {
    std::vector<WeightedEdge> chosen;   /* 选中的边，按加入顺序 */
    long long total_weight = 0;
    long long considered = 0;           /* 从排好序的边表里取出了几条 */
    long long skipped = 0;              /* 因为两端已经连通而放弃的条数 */
    int sets_left = 0;                  /* 收工时还剩几个集合 */
};

/* Kruskal。边表、按权排序之后的遍历骨架、完整的并查集都已经给出，
 * 留空的是「取出下一条边之后怎么判断、加不加、什么时候收工」。 */
MstResult kruskal(int n, std::vector<WeightedEdge> edges);

} /* namespace galgo */

#endif /* GRAPHALGO_HPP */
