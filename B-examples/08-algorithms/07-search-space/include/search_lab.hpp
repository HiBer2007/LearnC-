/**
 * search_lab.hpp —— 搜索：在状态空间里找路
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
 * 报告里出现的每个数字都由这里的计数产出，重跑逐位相同：
 *
 *   expanded      从容器里取出、且没有作废的节点数，也就是真正展开的节点数
 *   edge_checks   展开时看过的可通行邻居数，等于已展开节点的度数之和
 *   pushes        入队次数；DFS 与 BFS 每个节点最多入队一次，优先队列可以有重复条目
 *   pops          出队次数，含作废条目；恒等式 pushes == pops + pending
 *   stale_pops    出队时发现条目已作废而跳过的次数
 *   pending       算法停下时容器里还剩几条
 *   path_steps    路径上的边数；path_cost 是沿途格子代价之和，起点不计
 *
 * 地图写死在源码里（default_map），不读任何文件。四种代价：草地 1、泥地 2、
 * 沼地 5、岩壁不可通行。同一张图上跑六种走法：深搜、广搜、一致代价的 Dijkstra、
 * 启发恒为 0 的 A*、启发取曼哈顿距离的 A*、只看启发的贪心。
 *
 * 一切都是确定的：容器都是 std::vector 或基于 std::vector 的优先队列，
 * 没有哈希表；邻居按固定顺序枚举；优先队列里键相同的条目按入队序号先来先服务。
 */
#ifndef SEARCH_LAB_HPP
#define SEARCH_LAB_HPP

#include <cstddef>
#include <string>
#include <vector>

namespace slab {

/* ================= 地图 ================= */

/** 岩壁的代价。地图上不可通行的格子都记成它 */
constexpr int kWall = -1;

/** 状态空间：一张字符网格解析出来的代价表。
    行从上到下编号，列从左到右编号；S 是起点，G 是终点。 */
struct Map {
    int rows = 0;
    int cols = 0;
    std::vector<int> cost;    /**< rows × cols，kWall 表示岩壁 */
    std::vector<char> glyph;  /**< 屏幕上显示的那个字符 */
    int start_row = 0;
    int start_col = 0;
    int goal_row = 0;
    int goal_col = 0;

    int index(int row, int col) const { return row * cols + col; }
    bool inside(int row, int col) const;
    bool passable(int row, int col) const;
    int cost_at(int row, int col) const;
    char glyph_at(int row, int col) const;

    std::size_t cell_count() const;
    std::size_t passable_count() const;
    std::size_t count_glyph(char ch) const;
};

/** 本示例用的那张图，函数内静态对象，只构造一次 */
const Map &default_map();

/** 曼哈顿距离。每走一步至少付 1，因此它不会高估剩下的代价 */
int manhattan(const Map &map, int row, int col);

/* ================= 六种走法 ================= */

enum class Algo {
    Dfs,          /**< 深度优先：vector 当栈，后进先出 */
    Bfs,          /**< 广度优先：vector 当队列，先进先出 */
    Dijkstra,     /**< 一致代价搜索：优先队列，键是 g */
    AStarZeroH,   /**< A*：优先队列，键是 g + 0 */
    AStar,        /**< A*：优先队列，键是 g + 曼哈顿距离 */
    Greedy,       /**< 贪心最佳优先：优先队列，键只取 h */
};

constexpr int kAlgoCount = 6;

/** 走法的名字（UTF-8） */
const char *algo_name(Algo algo);

/** 这种走法用的容器与排序键（UTF-8），一行 */
const char *algo_note(Algo algo);

/** 一次搜索跑出来的结构量。每个数字都是数出来的，一个都不是估的 */
struct SearchResult {
    Algo algo = Algo::Bfs;
    bool found = false;
    long long path_cost = 0;
    std::vector<int> path;              /**< 格子下标，从起点到终点 */
    std::size_t expanded = 0;
    std::size_t edge_checks = 0;
    std::size_t pushes = 0;
    std::size_t pops = 0;
    std::size_t stale_pops = 0;
    std::size_t pending = 0;
    std::vector<unsigned char> expanded_flags;   /**< 每个格子是否被展开过 */

    std::size_t path_steps() const { return path.empty() ? 0 : path.size() - 1; }
};

/** 在 map 上按 algo 跑一次完整搜索。六种走法共用同一套邻居枚举顺序与同一套计数 */
SearchResult run_search(const Map &map, Algo algo);

/* ================= 独立核对 ================= */

/** 与优先队列无关的另一套算法：从 (from_row, from_col) 出发，反复对全图做松弛
    直到不动，得到每个格子的最少代价。它不参与搜索，只用来自我对照——
    两条独立的路子给出同一个代价，才敢写进报告 */
long long dist_by_relaxation(const Map &map, int from_row, int from_col,
                             std::vector<long long> *dist);

/** 起点到终点的最少代价，dist 里是起点到每个格子的最少代价 */
long long best_cost_by_relaxation(const Map &map, std::vector<long long> *dist);

/** 最少代价路径的条数。按代价从小到大推一遍，用在报告与自测里 */
std::size_t count_best_paths(const Map &map, const std::vector<long long> &dist);

/* ================= 打印 ================= */

/** 把地图画成字符网格，带行列号 */
std::string render_map(const Map &map);

/** 把地图画一遍，路径经过的格子换成 mark（起点与终点保持原样） */
std::string render_path_overlay(const Map &map, const std::vector<int> &path, char mark);

/** 路径的节点序列 "(4,1)→(4,2)→…"，每行最多 8 个节点，续行末尾留一个箭头 */
std::string format_path(const Map &map, const std::vector<int> &path);

/* ================= 报告与自测 ================= */

/** 自测结果。不打印，交给界面决定怎么显示 */
struct CheckResult {
    std::size_t total = 0;
    std::size_t passed = 0;
    std::size_t failed = 0;
    std::vector<std::string> lines;   /**< 每项一行，例如 "[通过] 3. ……" */

    bool all_passed() const { return failed == 0; }
    std::string summary() const;      /**< "25 项中 25 项通过，全部通过" */
};

/** 项目输出：地图、六种走法的结构量、最优性对照、路径序列、两条路径的落图、计数口径。
    返回多行 UTF-8 文本，末尾带一个换行；由界面层决定怎么显示 */
std::string build_report();

/** 逐项核对地图、启发、六种走法的结构量与恒等式 */
CheckResult run_self_tests();

}   /* namespace slab */

#endif /* SEARCH_LAB_HPP */
