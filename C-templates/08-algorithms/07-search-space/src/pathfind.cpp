/* pathfind.cpp —— 练习模板 07 的实现（C++）
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
 * 本模板的 3 处 TODO 全在这个文件里，按阶段编号：
 *
 *     阶段 2-1   bfs        一层推完怎么进到下一层
 *     阶段 3-1   dijkstra   松弛那一步
 *     阶段 4-1   astar      开放集怎么选、选出来怎么处理
 *
 * 阶段 1 的 dfs 是已经给出的：它给出路径，但那条路径不是最短的。
 *
 * 每个 TODO 上面写明「要做什么」，下面的「判据」一行给出填完之后
 * 应当看到的数——那些数都由 src/main_cli.cpp 打印，逐个对得上才算做完。
 */
#include "pathfind.hpp"

#include <algorithm>
#include <deque>
#include <functional>
#include <ostream>
#include <string>
#include <utility>

namespace pf {

namespace {

/* ==================================================================
 * 写死的地图与代价图
 * ================================================================== */

const int kRows = 9;
const int kCols = 15;
const int kCells = kRows * kCols;

/* '#' 墙、'.' 通路、'S' 起点、'G' 终点 */
const char *const kMap[kRows] = {
    "###############",
    "#S............#",
    "##....#.#.....#",
    "#.............#",
    "#.#.#.#.#.###.#",
    "#...#.........#",
    "#...##.#..#...#",
    "#.........#..G#",
    "###############",
};

/* 进入每一格的代价，1..9 的固定数字；墙写成 '#' */
const char *const kCostMap[kRows] = {
    "###############",
    "#1117961711111#",
    "##1166#1#11111#",
    "#1761111111111#",
    "#1#8#1#1#1###1#",
    "#166#769111111#",
    "#111##7#11#111#",
    "#111678611#111#",
    "###############",
};

const int kDr[4] = {-1, 0, 1, 0};   /* 上、右、下、左——次序写死 */
const int kDc[4] = {0, 1, 0, -1};

/* 从 prev 里把 s 到 t 的路径倒着串出来；走不到 t 时返回空 */
std::vector<Pos> rebuild_path(const std::vector<int> &prev, int s, int t)
{
    std::vector<Pos> path;
    int u = t;
    while (u != s) {
        if (u < 0) {
            return std::vector<Pos>();
        }
        path.push_back(pos_of(u));
        u = prev[u];
    }
    path.push_back(pos_of(s));
    std::reverse(path.begin(), path.end());
    return path;
}

/* 搜索结束后三个阶段都要做的收尾：串路径、数步数、算代价 */
void finish(Result &res, const std::vector<int> &prev, int s, int t)
{
    res.path = rebuild_path(prev, s, t);
    res.steps = res.path.empty() ? 0 : static_cast<long long>(res.path.size()) - 1;
    res.cost = path_cost(res.path);
}

} /* anonymous namespace */

/* ==================================================================
 * 已给出：地图与它的取用
 * ================================================================== */

int rows()
{
    return kRows;
}

int cols()
{
    return kCols;
}

char cell(int r, int c)
{
    if (r < 0 || r >= kRows || c < 0 || c >= kCols) {
        return '#';
    }
    return kMap[r][c];
}

int cost(int r, int c)
{
    if (r < 0 || r >= kRows || c < 0 || c >= kCols) {
        return 0;
    }
    const char ch = kCostMap[r][c];
    if (ch < '1' || ch > '9') {
        return 0;
    }
    return ch - '0';
}

bool walkable(int r, int c)
{
    return r >= 0 && r < kRows && c >= 0 && c < kCols && kMap[r][c] != '#';
}

Pos start()
{
    for (int r = 0; r < kRows; ++r) {
        for (int c = 0; c < kCols; ++c) {
            if (kMap[r][c] == 'S') {
                Pos p;
                p.r = r;
                p.c = c;
                return p;
            }
        }
    }
    return Pos();
}

Pos goal()
{
    for (int r = 0; r < kRows; ++r) {
        for (int c = 0; c < kCols; ++c) {
            if (kMap[r][c] == 'G') {
                Pos p;
                p.r = r;
                p.c = c;
                return p;
            }
        }
    }
    return Pos();
}

int index_of(Pos p)
{
    if (!walkable(p.r, p.c)) {
        return -1;
    }
    return p.r * kCols + p.c;
}

Pos pos_of(int index)
{
    Pos p;
    if (index < 0 || index >= kCells) {
        p.r = -1;
        p.c = -1;
        return p;
    }
    p.r = index / kCols;
    p.c = index % kCols;
    return p;
}

long long path_cost(const std::vector<Pos> &path)
{
    long long sum = 0;
    for (std::size_t i = 1; i < path.size(); ++i) {
        sum += cost(path[i].r, path[i].c);
    }
    return sum;
}

int neighbors_of(int index, int out[4])
{
    const Pos p = pos_of(index);
    int n = 0;
    for (int i = 0; i < 4; ++i) {
        const int r = p.r + kDr[i];
        const int c = p.c + kDc[i];
        if (walkable(r, c)) {
            out[n] = r * kCols + c;
            ++n;
        }
    }
    return n;
}

/* ==================================================================
 * 已给出：打印工具
 * ================================================================== */

void print_map(std::ostream &os)
{
    for (int r = 0; r < kRows; ++r) {
        os << "    " << kMap[r] << "\n";
    }
}

void print_cost_map(std::ostream &os)
{
    for (int r = 0; r < kRows; ++r) {
        os << "    " << kCostMap[r] << "\n";
    }
}

void print_map_with_path(std::ostream &os, const std::vector<Pos> &path)
{
    std::vector<char> on_path(kCells, 0);
    for (std::size_t i = 0; i < path.size(); ++i) {
        const int idx = index_of(path[i]);
        if (idx >= 0) {
            on_path[idx] = 1;
        }
    }

    for (int r = 0; r < kRows; ++r) {
        os << "    ";
        for (int c = 0; c < kCols; ++c) {
            const char ch = kMap[r][c];
            if (ch == '.' && on_path[r * kCols + c] != 0) {
                os << '*';
            } else {
                os << ch;
            }
        }
        os << "\n";
    }
}

void print_coords(std::ostream &os, const std::vector<Pos> &v, std::size_t per_line)
{
    if (v.empty()) {
        os << "    (empty)\n";
        return;
    }
    if (per_line == 0) {
        per_line = 1;
    }
    for (std::size_t i = 0; i < v.size(); ++i) {
        os << ((i % per_line == 0) ? "    " : " ");
        os << v[i].r << "," << v[i].c;
        if (i % per_line == per_line - 1 || i + 1 == v.size()) {
            os << "\n";
        }
    }
}

void print_dist_grid(std::ostream &os, const std::vector<long long> &dist)
{
    /* 这里手工补空格，不用 std::setw：调用方把流设成了左对齐，
     * 用 setw 会把数字左对齐，网格就对不齐了。 */
    for (int r = 0; r < kRows; ++r) {
        os << "   ";
        for (int c = 0; c < kCols; ++c) {
            const int i = r * kCols + c;
            if (kMap[r][c] == '#') {
                os << "  ##";
            } else if (i < static_cast<int>(dist.size()) && dist[i] >= 0) {
                const std::string s = std::to_string(dist[i]);
                os << std::string(s.size() < 4 ? 4 - s.size() : 0, ' ') << s;
            } else {
                os << "  ..";
            }
        }
        os << "\n";
    }
}

void print_levels(std::ostream &os, const std::vector<long long> &levels)
{
    for (std::size_t i = 0; i < levels.size(); ++i) {
        os << (i == 0 ? "" : " ") << levels[i];
    }
    os << "\n";
}

/* ==================================================================
 * 阶段 1：显式栈的深搜（已给出）
 * ================================================================== */

Result dfs()
{
    Result res;

    std::vector<char> seen(kCells, 0);          /* 访问标记 */
    std::vector<int> prev(kCells, -1);          /* 前驱 */
    std::vector<int> stack;                     /* std::vector 当栈：后进先出 */

    const int s = index_of(start());
    const int t = index_of(goal());

    seen[s] = 1;
    stack.push_back(s);

    while (!stack.empty()) {
        const int u = stack.back();
        stack.pop_back();

        res.order.push_back(pos_of(u));
        if (u == t) {
            break;
        }

        int nb[4];
        const int n = neighbors_of(u, nb);
        for (int i = 0; i < n; ++i) {
            const int v = nb[i];
            if (seen[v] == 0) {
                seen[v] = 1;
                prev[v] = u;
                stack.push_back(v);     /* 压进去的次序与执行次序相反 */
            }
        }
    }

    finish(res, prev, s, t);
    return res;
}

/* ==================================================================
 * 阶段 2：广搜的层推进
 * ================================================================== */

Result bfs()
{
    Result res;

    /* 已给出：队列、访问标记、前驱数组 */
    std::deque<int> q;
    std::vector<char> seen(kCells, 0);
    std::vector<int> prev(kCells, -1);

    /* 已给出：逐层推进用的两个计数器 */
    long long level_left = 0;       /* 当前这一层还剩几个没有出队 */
    long long level_next = 0;       /* 下一层已经入队了几个 */

    const int s = index_of(start());
    const int t = index_of(goal());

    seen[s] = 1;
    q.push_back(s);

    /* 已给出：第 0 层就是起点自己 */
    level_left = 1;
    res.level_sizes.push_back(1);

    while (!q.empty()) {
        const int u = q.front();
        q.pop_front();
        --level_left;               /* 已给出：当前这一层又出去了一个 */

        res.order.push_back(pos_of(u));
        if (u == t) {
            break;
        }

        int nb[4];
        const int n = neighbors_of(u, nb);
        for (int i = 0; i < n; ++i) {
            const int v = nb[i];
            if (seen[v] == 0) {
                seen[v] = 1;
                prev[v] = u;
                q.push_back(v);
                ++level_next;       /* 已给出：下一层又进来了一个 */
            }
        }

        /* TODO（阶段 2-1，一层推完怎么进到下一层）：
         * 一个节点出队之后，怎么知道这一层还剩没剩？
         * 层与层之间的界画在哪里，「下一层已经入队了几个」才数得准——
         * 是在扩展一个节点之前数，还是等它的邻居都进完队之后数？
         * res.level_sizes 要的是每一层入队的节点数，第 0 层那一个已经给好了。
         * 判据（见《配置步骤.md》阶段 2）：
         *       bfs expanded 是 75；
         *       bfs level sizes 是 1 1 2 3 4 5 4 7 6 7 6 6 6 5 3 3 3 2 1（19 层，加起来 75）；
         *       bfs path steps 是 18、bfs path cost 是 43。 */

        /* 占位实现：下面两行整段删掉（它们只是让骨架在 -Wall -Wextra 下零警告） */
        (void)level_left;
        (void)level_next;
    }

    finish(res, prev, s, t);
    return res;
}

/* ==================================================================
 * 阶段 3：Dijkstra 的松弛
 * ================================================================== */

Result dijkstra()
{
    Result res;

    /* 已给出：距离数组（-1 表示还没到过）与前驱数组 */
    std::vector<long long> dist(kCells, -1);
    std::vector<int> prev(kCells, -1);

    /* 已给出：小根堆。std::priority_queue 默认是大根堆，
     * 用 std::greater 把次序翻过来，堆顶就是距离最小的那一项。
     * 每一项是「距离，行优先下标」——并列时按下标定次序，重跑才逐位相同。 */
    typedef std::pair<long long, int> Item;
    std::priority_queue<Item, std::vector<Item>, std::greater<Item> > open;

    const int s = index_of(start());
    const int t = index_of(goal());

    dist[s] = 0;
    open.push(Item(0, s));
    res.dist.assign(kCells, -1);

    while (!open.empty()) {
        const Item top = open.top();
        open.pop();
        ++res.pops;                 /* 已给出：弹出一次（含下面那些过时的旧条目） */

        const long long d = top.first;
        const int u = top.second;
        if (d != dist[u]) {
            continue;               /* 已给出：过时的旧条目，丢掉 */
        }
        res.order.push_back(pos_of(u));     /* 已给出：这一格真的被展开了 */
        if (u == t) {
            break;
        }

        int nb[4];
        const int n = neighbors_of(u, nb);
        for (int i = 0; i < n; ++i) {
            const int v = nb[i];

            /* TODO（阶段 3-1，松弛）：
             * 站在 u 这一格上，把四个邻居各算一遍「绕经这里走过去」的总代价，
             * 与距离数组里给这个邻居记着的数比一比——这一比就是松弛的全部。
             * 两种结果各对应一个计数器：更小的那一支更新记录、让它重新参与选择，
             * 记 res.relax_ok；另一支什么都不改，记 res.relax_drop。
             * 每检查一个邻居，两个计数器里必然有一个加一。
             * 还有一个要自己想清楚：距离数组里「还没到过」的那一格记的是什么，
             * 它算哪一支？不把它安顿好，除了起点谁也进不了堆。
             * 判据（见《配置步骤.md》阶段 3）：
             *       dijkstra relax ok 是 72、dijkstra relax dropped 是 126；
             *       dijkstra path steps 是 18、path cost 是 23，
             *       而步数相同的广搜路径代价是 43——广搜给的确实不是最小代价；
             *       dijkstra expanded 是 70，pops 也是 70。 */

            /* 占位实现：下面这一行整段删掉 */
            (void)v;
        }
    }

    res.dist = dist;
    finish(res, prev, s, t);
    return res;
}

/* ==================================================================
 * 阶段 4：A* 的估价与开放集选择
 * ================================================================== */

long long manhattan(Pos a, Pos b)
{
    const long long dr = a.r > b.r ? a.r - b.r : b.r - a.r;
    const long long dc = a.c > b.c ? a.c - b.c : b.c - a.c;
    return dr + dc;
}

Entry make_entry(int index, long long g, bool use_h)
{
    Entry e;
    e.g = g;
    e.cell = index;
    e.f = g + (use_h ? manhattan(pos_of(index), goal()) : 0);
    return e;
}

Result astar(bool use_h)
{
    Result res;

    /* 已给出：从起点到这一格的实际代价（-1 表示还没到过）与前驱数组 */
    std::vector<long long> g(kCells, -1);
    std::vector<int> prev(kCells, -1);

    /* 已给出：开放集。比较器 GreaterF 让 f 最小的那一项待在堆顶 */
    std::priority_queue<Entry, std::vector<Entry>, GreaterF> open;

    const int s = index_of(start());
    const int t = index_of(goal());

    g[s] = 0;
    open.push(make_entry(s, 0, use_h));
    res.dist.assign(kCells, -1);

    /* TODO（阶段 4-1，开放集怎么选下一个、选出来怎么处理）：
     * 开放集就是那个优先队列，堆顶那一项就是「按 f 算最值得先看」的那一个。
     * 取出来之后先问一句：什么情况下它已经不作数了——堆里带着的 g
     * 与 g 数组里给这一格记着的数对不上，说明它是什么时候留下的旧账？
     * 真正要扩展它时，四个邻居该怎么跟着变，才与阶段 3 的松弛是同一件事？
     * 终点什么时候可以收工，为什么这时不必再往下走？
     * use_h 由调用方给：为 false 时估价恒为 0，A* 就该退化成 Dijkstra。
     * 判据（见《配置步骤.md》阶段 4）：
     *       astar(h=1) expanded 是 20，明显少于 dijkstra 的 70；
     *       astar(h=1) path steps 是 18、path cost 是 23，与 dijkstra 逐项相同；
     *       astar(h=0) 的 expanded 是 70，弹出顺序与 Dijkstra 逐项相同。 */

    /* 已给出：把 g 抄进 res.dist，再从 prev 里把路径串出来 */
    res.dist = g;
    finish(res, prev, s, t);
    return res;
}

} /* namespace pf */
