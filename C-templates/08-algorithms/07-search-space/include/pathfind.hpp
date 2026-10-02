/* pathfind.hpp —— 练习模板 07 的核心接口（C++）
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
 * 搜索这一件事拆成 4 个阶段，每个阶段的实现写在 src/pathfind.cpp 里：
 *
 *     阶段 1  dfs        显式栈的深搜（已给出，它给出的路径不是最短的）
 *     阶段 2  bfs        广搜的层推进
 *     阶段 3  dijkstra   Dijkstra 的松弛
 *     阶段 4  astar      A* 的估价与开放集选择
 *
 * 地图是一张写死的网格：'#' 是墙、'.' 是通路、'S' 是起点、'G' 是终点，
 * 四邻接（上下左右）。同一张网格另有一份写死的代价图，
 * 每一格进入的代价是 1 到 9 的固定数字——广搜看不见它，Dijkstra 与 A* 按它走。
 *
 * 本模板只碰「在状态空间里找路」这一件事：图是用网格隐式给的，
 * 没有邻接表、没有并查集、没有拓扑排序——那些属于 08-graph-structure。
 */
#ifndef PATHFIND_HPP
#define PATHFIND_HPP

#include <cstddef>
#include <iosfwd>
#include <queue>
#include <utility>
#include <vector>

namespace pf {

/* 一格的位置：行号与列号，都从 0 起算 */
struct Pos {
    int r = 0;
    int c = 0;
};

/* ------------------------------------------------------------------
 * 已给出：那张写死的地图与它的代价图。
 *
 * 地图与代价都在 src/pathfind.cpp 里以字符串字面量写死，重跑逐位相同。
 * 格子既可以按行列取，也可以按行优先的下标取（index_of / pos_of），
 * 下标空间包含墙，因此下标总数是 rows() * cols()。
 * ------------------------------------------------------------------ */

int rows();                         /* 地图有几行 */
int cols();                         /* 地图有几列 */
char cell(int r, int c);            /* '#' 墙、'.' 通路、'S' 起点、'G' 终点 */
int cost(int r, int c);             /* 进入这一格要付的代价 1..9；墙是 0 */
bool walkable(int r, int c);        /* 在界内而且不是墙 */
Pos start();
Pos goal();
int index_of(Pos p);                /* 行优先下标；越界返回 -1 */
Pos pos_of(int index);
long long path_cost(const std::vector<Pos> &path);  /* 路径总代价，起点那一格不算 */

/* 按「上、右、下、左」的固定次序取四个邻居，返回写进 out 的个数。
 * 次序写死是有意的：展开顺序要重跑逐位相同，就不能让邻接次序随实现变。 */
int neighbors_of(int index, int out[4]);

/* ------------------------------------------------------------------
 * 已给出：打印工具。
 *
 * 输出全部是 ASCII：路径用 '*' 覆盖在网格上，
 * 展开顺序打印成一串坐标，各点距离打印成一个整数网格。
 * ------------------------------------------------------------------ */

void print_map(std::ostream &os);
void print_cost_map(std::ostream &os);
void print_map_with_path(std::ostream &os, const std::vector<Pos> &path);
void print_coords(std::ostream &os, const std::vector<Pos> &v, std::size_t per_line);
void print_dist_grid(std::ostream &os, const std::vector<long long> &dist);
void print_levels(std::ostream &os, const std::vector<long long> &levels);

/* ------------------------------------------------------------------
 * 三种搜索共用的结果。
 *
 * order        展开顺序（出队或出堆，坐标序列）——本模板最硬的判据
 * path         从起点到终点的路径，含两端；走不到时是空的
 * steps        路径步数（path 的长度减一）
 * cost         路径总代价（不含起点那一格）
 * level_sizes  阶段 2：广搜逐层入队的节点数
 * dist         阶段 3、4：各格的最短距离，按行优先下标；-1 是没到过
 * relax_ok     阶段 3：松弛成功的次数（新距离更小，更新并入队）
 * relax_drop   阶段 3：抛弃的次数（新距离不小于记录，丢弃）
 * pops         从优先队列里弹出的总次数（含已经过时的旧条目）
 * ------------------------------------------------------------------ */
struct Result {
    std::vector<Pos> order;
    std::vector<Pos> path;
    long long steps = 0;
    long long cost = 0;
    std::vector<long long> level_sizes;
    std::vector<long long> dist;
    long long relax_ok = 0;
    long long relax_drop = 0;
    long long pops = 0;
};

/* ---------- 阶段 1：显式栈的深搜（已给出） ---------- */

/* 用 std::vector 当栈：压栈时标记已访问。它找到的路径不是最短的，
 * 这正是后面三个阶段要解决的问题。 */
Result dfs();

/* ---------- 阶段 2：广搜的层推进 ---------- */

/* 队列、访问标记与前驱数组都已给出，留空的是「一层推完怎么进到下一层」。 */
Result bfs();

/* ---------- 阶段 3：Dijkstra 的松弛 ---------- */

/* 小根堆、距离数组与前驱数组都已给出，留空的是松弛那一步。 */
Result dijkstra();

/* ---------- 阶段 4：A* 的估价与开放集选择 ---------- */

/* 已给出：曼哈顿距离。四邻接下每走一步至少付 1 点代价，
 * 因此它永远不高估剩下的代价，是可采纳估价。 */
long long manhattan(Pos a, Pos b);

/* 已给出：开放集里的一项。f = g + h 是估计的总代价，g 是已经付出的实际代价，
 * cell 是行优先下标。比较器要按 f 排序，而 std::priority_queue 默认是大根堆，
 * 因此 GreaterF 把「大于」的意思翻过来。 */
struct Entry {
    long long f = 0;
    long long g = 0;
    int cell = 0;
};

struct GreaterF {
    bool operator()(const Entry &x, const Entry &y) const
    {
        if (x.f != y.f) {
            return x.f > y.f;       /* f 小的先出堆 */
        }
        if (x.g != y.g) {
            return x.g < y.g;       /* f 相同时 g 大的先出堆：离终点更近 */
        }
        return x.cell > y.cell;     /* 再相同就按下标，次序写死才好复现 */
    }
};

/* 已给出：按 use_h 决定这一项的 h 取曼哈顿距离还是取 0 */
Entry make_entry(int index, long long g, bool use_h);

/* 开放集怎么选、选出来怎么处理都留空。use_h 为 false 时估价恒为 0，
 * A* 就退化成 Dijkstra——这也是阶段 4 的一条判据。 */
Result astar(bool use_h);

} /* namespace pf */

#endif /* PATHFIND_HPP */
