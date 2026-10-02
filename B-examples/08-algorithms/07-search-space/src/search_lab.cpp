/**
 * search_lab.cpp —— 搜索：在状态空间里找路
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

#include "search_lab.hpp"

#include <algorithm>
#include <iomanip>
#include <queue>
#include <sstream>
#include <string>
#include <vector>

namespace slab {

/* ================= 地图 ================= */

bool Map::inside(int row, int col) const
{
    return row >= 0 && row < rows && col >= 0 && col < cols;
}

bool Map::passable(int row, int col) const
{
    return inside(row, col) &&
           cost[static_cast<std::size_t>(index(row, col))] != kWall;
}

int Map::cost_at(int row, int col) const
{
    return cost[static_cast<std::size_t>(index(row, col))];
}

char Map::glyph_at(int row, int col) const
{
    return glyph[static_cast<std::size_t>(index(row, col))];
}

std::size_t Map::cell_count() const
{
    return static_cast<std::size_t>(rows) * static_cast<std::size_t>(cols);
}

std::size_t Map::passable_count() const
{
    std::size_t count = 0;
    for (const int value : cost) {
        if (value != kWall) {
            ++count;
        }
    }
    return count;
}

std::size_t Map::count_glyph(char ch) const
{
    std::size_t count = 0;
    for (const char value : glyph) {
        if (value == ch) {
            ++count;
        }
    }
    return count;
}

namespace {

/** 整张图写死在这里。改动它，报告里的每个数字都会跟着变。
 *
 *  图例   . 草地，走一格付 1        , 泥地，走一格付 2
 *         % 沼地，走一格付 5        # 岩壁，不可通行
 *         S 起点                    G 终点
 *
 *           0  1  2  3  4  5  6  7  8  9 10 11 12 13 14
 *      0 |  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .
 *      1 |  .  .  .  .  ,  ,  ,  ,  ,  ,  ,  .  .  .  .
 *      2 |  .  .  .  #  #  #  #  #  #  #  #  #  #  .  .
 *      3 |  .  .  .  %  %  %  %  %  %  %  %  %  %  .  .
 *      4 |  .  S  .  %  %  %  %  %  %  %  %  %  %  G  .
 *      5 |  .  .  .  %  %  %  %  %  %  %  %  %  %  .  .
 *      6 |  .  .  %  %  %  %  %  %  %  %  %  %  %  .  .
 *      7 |  .  .  %  %  %  %  %  %  %  %  %  %  %  .  .
 *      8 |  .  .  %  %  %  %  %  %  %  %  %  %  %  .  .
 *
 *  这张图上同时有三样东西：
 *    1. 起点与终点在同一行，中间那条直路只有 12 步，但穿 10 格沼地；
 *    2. 第 2 行是一道岩壁，把上面一层草脊隔成一个只能从左右两端上去的平台，
 *       想从上面绕，就得先走到边上再翻上去；
 *    3. 左下与下方是一大片沼地，往下走越走越贵。
 */
const char *const kMapText[] = {
    "...............",
    "....,,,,,,,....",
    "...##########..",
    "...%%%%%%%%%%..",
    ".S.%%%%%%%%%%G.",
    "...%%%%%%%%%%..",
    "..%%%%%%%%%%%..",
    "..%%%%%%%%%%%..",
    "..%%%%%%%%%%%..",
};

constexpr int kMapRows = 9;
constexpr int kMapCols = 15;

/** 字符到代价。没列出来的字符一律当岩壁，地图上写错一个字不会静默变成通路 */
int cost_of_glyph(char ch)
{
    switch (ch) {
    case '.':
        return 1;
    case ',':
        return 2;
    case '%':
        return 5;
    case 'S':
    case 'G':
        return 1;
    default:
        return kWall;
    }
}

}   /* namespace */

const Map &default_map()
{
    static const Map map = [] {
        Map built;
        built.rows = kMapRows;
        built.cols = kMapCols;
        built.cost.assign(static_cast<std::size_t>(kMapRows * kMapCols), kWall);
        built.glyph.assign(static_cast<std::size_t>(kMapRows * kMapCols), '#');
        for (int row = 0; row < kMapRows; ++row) {
            const std::string line(kMapText[row]);
            for (int col = 0; col < kMapCols && col < static_cast<int>(line.size()); ++col) {
                const char ch = line[static_cast<std::size_t>(col)];
                const std::size_t at = static_cast<std::size_t>(built.index(row, col));
                built.glyph[at] = ch;
                built.cost[at] = cost_of_glyph(ch);
                if (ch == 'S') {
                    built.start_row = row;
                    built.start_col = col;
                } else if (ch == 'G') {
                    built.goal_row = row;
                    built.goal_col = col;
                }
            }
        }
        return built;
    }();
    return map;
}

int manhattan(const Map &map, int row, int col)
{
    const int dr = row - map.goal_row;
    const int dc = col - map.goal_col;
    return (dr < 0 ? -dr : dr) + (dc < 0 ? -dc : dc);
}

/* ================= 六种走法 ================= */

const char *algo_name(Algo algo)
{
    switch (algo) {
    case Algo::Dfs:
        return u8"DFS";
    case Algo::Bfs:
        return u8"BFS";
    case Algo::Dijkstra:
        return u8"Dijkstra";
    case Algo::AStarZeroH:
        return u8"A*（h 恒为 0）";
    case Algo::AStar:
        return u8"A*（h 曼哈顿）";
    case Algo::Greedy:
        return u8"贪心（只看 h）";
    }
    return u8"?";
}

const char *algo_note(Algo algo)
{
    switch (algo) {
    case Algo::Dfs:
        return u8"栈，后进先出";
    case Algo::Bfs:
        return u8"队列，先进先出";
    case Algo::Dijkstra:
        return u8"优先队列，键 = g";
    case Algo::AStarZeroH:
        return u8"优先队列，键 = g + 0";
    case Algo::AStar:
        return u8"优先队列，键 = g + h";
    case Algo::Greedy:
        return u8"优先队列，键 = h";
    }
    return u8"?";
}

namespace {

constexpr long long kInf = 1LL << 40;

/** 优先队列里的一条。键是排序用的，g 只在判断作废时用 */
struct Entry {
    long long key = 0;
    long long g = 0;
    std::size_t seq = 0;   /**< 入队序号 */
    int node = 0;
};

/** 小顶堆：键小的先出；键相同时入队序号小的先出，也就是先来先服务。
    这一条把「同代价元素的次序」定死了，重跑逐位相同 */
struct EntryGreater {
    bool operator()(const Entry &lhs, const Entry &rhs) const
    {
        if (lhs.key != rhs.key) {
            return lhs.key > rhs.key;
        }
        return lhs.seq > rhs.seq;
    }
};

/** 邻居枚举顺序：上、右、下、左。六种走法用的是同一个顺序。
    DFS 依次把它们压栈，因此最后压进去的「左」最先出栈；
    BFS 依次入队，因此出队次序就是上、右、下、左 */
const int kDr[4] = {-1, 0, 1, 0};
const int kDc[4] = {0, 1, 0, -1};

}   /* namespace */

SearchResult run_search(const Map &map, Algo algo)
{
    SearchResult result;
    result.algo = algo;

    const std::size_t cells = map.cell_count();
    result.expanded_flags.assign(cells, 0);

    std::vector<long long> dist(cells, kInf);
    std::vector<int> prev(cells, -1);
    std::vector<unsigned char> closed(cells, 0);

    const int start = map.index(map.start_row, map.start_col);
    const int goal = map.index(map.goal_row, map.goal_col);
    dist[static_cast<std::size_t>(start)] = 0;

    const bool uses_heap = (algo == Algo::Dijkstra || algo == Algo::AStarZeroH ||
                            algo == Algo::AStar || algo == Algo::Greedy);

    if (!uses_heap) {
        /* DFS 与 BFS 都不需要优先队列：一个整数数组当容器就行。
           DFS 从尾部取，BFS 从头取，两者共用同一段循环 */
        std::vector<int> container;
        std::size_t head = 0;                 /* 只给 BFS 用：队头下标 */
        std::vector<unsigned char> discovered(cells, 0);

        container.push_back(start);
        ++result.pushes;
        discovered[static_cast<std::size_t>(start)] = 1;

        while (!container.empty()) {
            int node = 0;
            if (algo == Algo::Dfs) {
                node = container.back();
                container.pop_back();
            } else {
                node = container[head];
                ++head;
            }
            ++result.pops;

            const std::size_t at = static_cast<std::size_t>(node);
            result.expanded_flags[at] = 1;
            ++result.expanded;

            const int row = node / map.cols;
            const int col = node % map.cols;
            for (int k = 0; k < 4; ++k) {
                const int nr = row + kDr[k];
                const int nc = col + kDc[k];
                if (!map.passable(nr, nc)) {
                    continue;
                }
                ++result.edge_checks;         /* 看过的可通行邻居 */
                const std::size_t next = static_cast<std::size_t>(map.index(nr, nc));
                if (discovered[next] != 0) {
                    continue;
                }
                discovered[next] = 1;         /* 一入队就记上，每个格子最多入队一次 */
                dist[next] = dist[at] + map.cost_at(nr, nc);
                prev[next] = node;
                container.push_back(static_cast<int>(next));
                ++result.pushes;
            }

            if (node == goal) {
                break;                        /* 取出终点就停下，六种走法同一条规矩 */
            }
        }
        result.pending = (algo == Algo::Dfs) ? container.size() : container.size() - head;
    } else {
        std::priority_queue<Entry, std::vector<Entry>, EntryGreater> queue;
        std::size_t seq = 0;

        /* 排序键：Dijkstra 与「h 恒为 0 的 A*」都是 g，两者因此完全同路；
           启发非零的 A* 是 g + h；贪心只看 h，一条路走到黑 */
        const auto key_of = [&map, algo](int node, long long g) -> long long {
            const int row = node / map.cols;
            const int col = node % map.cols;
            const int h = manhattan(map, row, col);
            switch (algo) {
            case Algo::Dijkstra:
            case Algo::AStarZeroH:
                return g;
            case Algo::AStar:
                return g + h;
            default:
                return h;
            }
        };

        queue.push(Entry{key_of(start, 0), 0, seq++, start});
        ++result.pushes;

        while (!queue.empty()) {
            const Entry top = queue.top();
            queue.pop();
            ++result.pops;

            const std::size_t at = static_cast<std::size_t>(top.node);
            if (closed[at] != 0 || top.g > dist[at]) {
                ++result.stale_pops;          /* 这个格子已经展开过，或者这条 g 不是最小的 */
                continue;
            }
            closed[at] = 1;
            result.expanded_flags[at] = 1;
            ++result.expanded;

            const int row = top.node / map.cols;
            const int col = top.node % map.cols;
            for (int k = 0; k < 4; ++k) {
                const int nr = row + kDr[k];
                const int nc = col + kDc[k];
                if (!map.passable(nr, nc)) {
                    continue;
                }
                ++result.edge_checks;
                const std::size_t next = static_cast<std::size_t>(map.index(nr, nc));
                if (closed[next] != 0) {
                    continue;                 /* 展开过的格子不再改 */
                }
                const long long candidate = dist[at] + map.cost_at(nr, nc);
                if (candidate < dist[next]) {
                    dist[next] = candidate;
                    prev[next] = top.node;
                    queue.push(Entry{key_of(static_cast<int>(next), candidate), candidate,
                                     seq++, static_cast<int>(next)});
                    ++result.pushes;
                }
            }

            if (top.node == goal) {
                break;
            }
        }
        result.pending = queue.size();
    }

    if (result.expanded_flags[static_cast<std::size_t>(goal)] != 0) {
        result.found = true;
        for (int node = goal; node != -1; node = prev[static_cast<std::size_t>(node)]) {
            result.path.push_back(node);
            if (node == start) {
                break;
            }
        }
        std::reverse(result.path.begin(), result.path.end());

        long long cost = 0;
        for (std::size_t i = 1; i < result.path.size(); ++i) {
            const int node = result.path[i];
            cost += map.cost_at(node / map.cols, node % map.cols);
        }
        result.path_cost = cost;
    }
    return result;
}

/* ================= 独立核对 ================= */

long long dist_by_relaxation(const Map &map, int from_row, int from_col,
                             std::vector<long long> *dist)
{
    const std::size_t cells = map.cell_count();
    std::vector<long long> best(cells, kInf);
    const int start = map.index(from_row, from_col);
    best[static_cast<std::size_t>(start)] = 0;

    /* 反复扫全图直到一轮下来没有任何改动。没有优先队列，没有启发，
       与 run_search 是两条完全独立的路子 */
    bool changed = true;
    std::size_t rounds = 0;
    while (changed && rounds <= cells) {
        changed = false;
        ++rounds;
        for (int row = 0; row < map.rows; ++row) {
            for (int col = 0; col < map.cols; ++col) {
                if (!map.passable(row, col)) {
                    continue;
                }
                const std::size_t at = static_cast<std::size_t>(map.index(row, col));
                if (best[at] == kInf) {
                    continue;
                }
                for (int k = 0; k < 4; ++k) {
                    const int nr = row + kDr[k];
                    const int nc = col + kDc[k];
                    if (!map.passable(nr, nc)) {
                        continue;
                    }
                    const std::size_t next = static_cast<std::size_t>(map.index(nr, nc));
                    const long long candidate = best[at] + map.cost_at(nr, nc);
                    if (candidate < best[next]) {
                        best[next] = candidate;
                        changed = true;
                    }
                }
            }
        }
    }

    if (dist != nullptr) {
        *dist = best;
    }
    return best[static_cast<std::size_t>(map.index(from_row, from_col))];
}

long long best_cost_by_relaxation(const Map &map, std::vector<long long> *dist)
{
    std::vector<long long> best;
    dist_by_relaxation(map, map.start_row, map.start_col, &best);
    if (dist != nullptr) {
        *dist = best;
    }
    return best[static_cast<std::size_t>(map.index(map.goal_row, map.goal_col))];
}

std::size_t count_best_paths(const Map &map, const std::vector<long long> &dist)
{
    const std::size_t cells = map.cell_count();
    std::vector<int> order;
    for (std::size_t i = 0; i < cells; ++i) {
        if (dist[i] != kInf) {
            order.push_back(static_cast<int>(i));
        }
    }
    /* 按代价从小到大，同代价按下标：次序是写死的，数出来的条数就唯一 */
    std::sort(order.begin(), order.end(), [&dist](int lhs, int rhs) {
        if (dist[static_cast<std::size_t>(lhs)] != dist[static_cast<std::size_t>(rhs)]) {
            return dist[static_cast<std::size_t>(lhs)] < dist[static_cast<std::size_t>(rhs)];
        }
        return lhs < rhs;
    });

    std::vector<unsigned long long> ways(cells, 0);
    const int start = map.index(map.start_row, map.start_col);
    const int goal = map.index(map.goal_row, map.goal_col);
    ways[static_cast<std::size_t>(start)] = 1;

    for (const int node : order) {
        const std::size_t at = static_cast<std::size_t>(node);
        if (ways[at] == 0) {
            continue;
        }
        const int row = node / map.cols;
        const int col = node % map.cols;
        for (int k = 0; k < 4; ++k) {
            const int nr = row + kDr[k];
            const int nc = col + kDc[k];
            if (!map.passable(nr, nc)) {
                continue;
            }
            const std::size_t next = static_cast<std::size_t>(map.index(nr, nc));
            if (dist[at] + map.cost_at(nr, nc) == dist[next]) {
                ways[next] += ways[at];
            }
        }
    }
    return static_cast<std::size_t>(ways[static_cast<std::size_t>(goal)]);
}

/* ================= 打印 ================= */

namespace {

std::string grid_text(const Map &map, const std::vector<char> &cells)
{
    std::ostringstream os;
    os << "     ";
    for (int col = 0; col < map.cols; ++col) {
        os << std::setw(3) << col;
    }
    os << "\n";
    for (int row = 0; row < map.rows; ++row) {
        os << std::setw(2) << row << " |";
        for (int col = 0; col < map.cols; ++col) {
            os << "  " << cells[static_cast<std::size_t>(map.index(row, col))];
        }
        os << "\n";
    }
    return os.str();
}

}   /* namespace */

std::string render_map(const Map &map)
{
    return grid_text(map, map.glyph);
}

std::string render_path_overlay(const Map &map, const std::vector<int> &path, char mark)
{
    std::vector<char> cells = map.glyph;
    for (const int node : path) {
        if (node == map.index(map.start_row, map.start_col) ||
            node == map.index(map.goal_row, map.goal_col)) {
            continue;
        }
        cells[static_cast<std::size_t>(node)] = mark;
    }
    return grid_text(map, cells);
}

std::string format_path(const Map &map, const std::vector<int> &path)
{
    std::ostringstream os;
    const std::size_t per_line = 8;
    for (std::size_t i = 0; i < path.size(); ++i) {
        if (i % per_line == 0) {
            os << (i == 0 ? "    " : "      ");
        }
        os << "(" << path[i] / map.cols << "," << path[i] % map.cols << ")";
        if (i + 1 < path.size()) {
            os << u8"→";
        }
        if (i % per_line == per_line - 1 && i + 1 < path.size()) {
            os << "\n";
        }
    }
    return os.str();
}

/* ================= 报告 ================= */

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

std::string num(std::size_t value)
{
    return std::to_string(value);
}

/** 一条路径上有几格沼地 */
std::size_t marsh_on(const Map &map, const std::vector<int> &path)
{
    std::size_t count = 0;
    for (const int node : path) {
        if (map.glyph_at(node / map.cols, node % map.cols) == '%') {
            ++count;
        }
    }
    return count;
}

void append_map(std::ostringstream &os, const Map &map, const SearchResult *runs)
{
    os << u8"一、状态空间：一张写死在源码里的地图\n";
    os << u8"  图例  . 草地，走一格付 1     , 泥地，走一格付 2\n";
    os << u8"        % 沼地，走一格付 5     # 岩壁，不可通行\n";
    os << u8"        S 起点                 G 终点\n";
    os << render_map(map);
    os << u8"  共 " << map.cell_count() << u8" 格：草地 " << map.count_glyph('.')
       << u8"、泥地 " << map.count_glyph(',') << u8"、沼地 " << map.count_glyph('%')
       << u8"、岩壁 " << map.count_glyph('#') << u8"；可通行 " << map.passable_count()
       << u8" 格\n";
    os << u8"  起点 (" << map.start_row << "," << map.start_col << u8")，终点 ("
       << map.goal_row << "," << map.goal_col << u8")，两点之间横平竖直的步数下界是 "
       << manhattan(map, map.start_row, map.start_col) << u8" 步\n";
    os << u8"  贴着第 " << map.start_row << u8" 行直着走正好就是这个步数，BFS 找到的就是它："
       << runs[1].path_steps() << u8" 步、代价 " << runs[1].path_cost << u8"，其中 "
       << marsh_on(map, runs[1].path) << u8" 格是沼地\n";
    os << u8"  第 2 行是一道岩壁，把上面两行隔成一块只能从左右两端上去的草脊：\n";
    os << u8"  想从上面绕，就得先走到边上再翻上去；左下与下方是大片沼地，越往下越贵\n";
    os << "\n";
}

void append_table(std::ostringstream &os, const Map &map, const SearchResult *runs)
{
    os << u8"二、同一张图上的六种走法（结构量，重跑逐位相同）\n";
    os << "  " << pad_right(u8"走法", 16) << pad_right(u8"容器与排序键", 28)
       << pad_right(u8"展开", 6) << pad_right(u8"边检查", 8) << pad_right("push", 6)
       << pad_right("pop", 6) << pad_right(u8"作废", 6) << pad_right(u8"在队", 6)
       << pad_right(u8"步数", 6) << u8"代价\n";
    for (int i = 0; i < kAlgoCount; ++i) {
        const SearchResult &run = runs[i];
        os << "  " << pad_right(algo_name(run.algo), 16) << pad_right(algo_note(run.algo), 28)
           << pad_right(num(run.expanded), 6) << pad_right(num(run.edge_checks), 8)
           << pad_right(num(run.pushes), 6) << pad_right(num(run.pops), 6)
           << pad_right(num(run.stale_pops), 6) << pad_right(num(run.pending), 6)
           << pad_right(run.found ? num(run.path_steps()) : "-", 6)
           << (run.found ? std::to_string(run.path_cost) : std::string("-")) << "\n";
    }
    os << u8"  展开 = 从容器里取出且没作废的节点数；边检查 = 展开时看过的可通行邻居数\n";
    os << u8"  作废 = 取出时该格子已经展开过，或者这条条目的 g 已经不是最小的\n";
    os << u8"  在队 = 停下来时容器里还剩几条，恒等式 push = pop + 在队\n";
    os << u8"  优先队列里键相同的条目按入队序号先来先服务，DFS 的邻居按上右下左依次压栈，\n";
    os << u8"  因此「同代价谁先出」是写死的，不是碰运气；这张图共 "
       << map.passable_count() << u8" 个可通行格子\n";
    os << u8"  这张图上六种走法都没弹出过作废条目：重复条目还躺在队列里，算法就弹出终点\n";
    os << u8"  停下了。push 比展开多出来的那些就是它们：Dijkstra 多 "
       << (runs[2].pushes - runs[2].expanded) << u8" 条，贪心多 "
       << (runs[5].pushes - runs[5].expanded) << u8" 条；\n";
    os << u8"  贪心的键只取 h，同一个格子改小了 g 也看不出来，于是被反复入队\n";
    os << "\n";
}

void append_optimality(std::ostringstream &os, const Map &map, const SearchResult *runs,
                       long long best_cost, std::size_t best_paths)
{
    const SearchResult &dfs = runs[0];
    const SearchResult &bfs = runs[1];
    const SearchResult &dij = runs[2];
    const SearchResult &astar0 = runs[3];
    const SearchResult &astar = runs[4];
    const SearchResult &greedy = runs[5];

    os << u8"三、最优性对照\n";
    os << u8"  最少代价 " << best_cost << u8"：由全图反复松弛独立算出，与优先队列无关\n";
    os << u8"  代价 " << best_cost << u8" 的最优路径共 " << best_paths << u8" 条\n";
    os << "  " << pad_right(algo_name(dfs.algo), 16)
       << u8"代价 " << dfs.path_cost << u8"，比最优多 " << (dfs.path_cost - best_cost)
       << u8"：先沿左边界下到底，沿第 8 行走到右端，\n";
    os << u8"                  又折回第 6 行向左走到第 3 列，最后从第 4 行穿过去；"
       << dfs.path_steps() << u8" 步里有 " << marsh_on(map, dfs.path) << u8" 格沼地\n";
    os << "  " << pad_right(algo_name(bfs.algo), 16) << u8"代价 " << bfs.path_cost << u8"，比最优多 "
       << (bfs.path_cost - best_cost) << u8"：它只数步数，不看一格多贵\n";
    os << "  " << pad_right(algo_name(dij.algo), 16) << u8"代价 " << dij.path_cost
       << u8"，等于最优；展开 " << dij.expanded << u8" 个节点\n";
    os << "  " << pad_right(algo_name(astar0.algo), 16) << u8"代价 " << astar0.path_cost
       << u8"，等于最优；展开 " << astar0.expanded << u8" 个节点，与 Dijkstra 逐位相同\n";
    os << "  " << pad_right(algo_name(astar.algo), 16) << u8"代价 " << astar.path_cost
       << u8"，等于最优；展开 " << astar.expanded << u8" 个节点，比 Dijkstra 少 "
       << (dij.expanded - astar.expanded) << u8" 个\n";
    os << "  " << pad_right(algo_name(greedy.algo), 16) << u8"代价 " << greedy.path_cost
       << u8"，比最优多 " << (greedy.path_cost - best_cost) << u8"：一路贴着终点穿沼地\n";
    os << u8"  DFS 的 " << dfs.path_cost << u8" 大于 BFS 的 " << bfs.path_cost
       << u8"：同一张图、同一个起点终点，深搜找到的不一定最短，\n";
    os << u8"  它只保证「找到」，不保证「找好」；这次它比广搜多付了 "
       << (dfs.path_cost - bfs.path_cost) << u8" 的代价\n";
    os << u8"  贪心的 " << greedy.path_cost << u8" 大于 A* 的 " << astar.path_cost
       << u8"：只看 h 会被那条「离终点最近但很贵」的直路骗过去，\n";
    os << u8"  A* 把已经付掉的 g 也算进排序键，于是先爬上草脊再横穿，从右端下来\n";
    os << u8"  BFS 的 " << bfs.path_steps() << u8" 步是全场最少的步数，代价却是 "
       << bfs.path_cost << u8"；\n";
    os << u8"  Dijkstra 与 A* 走 " << dij.path_steps() << u8" 步、代价 " << dij.path_cost
       << u8"：步数最少与代价最小不是一回事\n";
    os << u8"  启发非零且可采纳时，A* 仍然拿到最优代价，展开的节点却从 "
       << dij.expanded << u8" 降到 " << astar.expanded << u8"\n";
    os << u8"  h 恒为 0 时排序键就是 g，A* 的展开数、边检查数、push、pop、路径\n";
    os << u8"  与 Dijkstra 逐位相同：这时 A* 就是 Dijkstra，启发没有帮上任何忙\n";
    os << "\n";
}

void append_paths(std::ostringstream &os, const Map &map, const SearchResult *runs)
{
    os << u8"四、每种走法找到的路径（节点序列，格式是 (行,列)）\n";
    for (int i = 0; i < kAlgoCount; ++i) {
        const SearchResult &run = runs[i];
        os << "  " << pad_right(algo_name(run.algo), 16) << run.path_steps() << u8" 步，代价 "
           << run.path_cost << "\n";
        os << format_path(map, run.path) << "\n";
    }
    const bool same = (runs[2].path == runs[3].path);
    os << u8"  A*（h 恒为 0）与 Dijkstra 的节点序列逐位相同："
       << (same ? u8"是" : u8"否") << "\n";
    os << u8"  A*（h 曼哈顿）与 Dijkstra 的节点序列逐位相同："
       << (runs[2].path == runs[4].path ? u8"是" : u8"否") << "\n";
    os << u8"  两者代价相同，但排序键不同：这张图上代价 " << runs[4].path_cost
       << u8" 的路不止一条，\n";
    os << u8"  谁先被取出来由排序键与入队序号共同决定\n";
    os << "\n";
}

void append_overlays(std::ostringstream &os, const Map &map, const SearchResult *runs)
{
    os << u8"五、把两条路径画回地图上\n";
    os << u8"A*（h 曼哈顿）的 " << runs[4].path_steps() << u8" 步，代价 " << runs[4].path_cost
       << u8"，* 是它走过的格子\n";
    os << render_path_overlay(map, runs[4].path, '*');
    os << u8"贪心（只看 h）的 " << runs[5].path_steps() << u8" 步，代价 " << runs[5].path_cost
       << u8"，+ 是它走过的格子\n";
    os << render_path_overlay(map, runs[5].path, '+');
    os << u8"  贪心一路向右：每往右一格，离终点的曼哈顿距离就少一；它看不见脚下那 "
       << marsh_on(map, runs[5].path) << u8" 格沼地要付 5 倍价钱\n";
    os << u8"  A* 先向上爬到草脊，从第 0 行横穿，再从右端下来：多走 "
       << (runs[4].path_steps() - runs[5].path_steps()) << u8" 步，少付 "
       << (runs[5].path_cost - runs[4].path_cost) << u8" 的代价\n";
    os << "\n";
}

void append_accounting(std::ostringstream &os, long long best_cost, std::size_t best_paths)
{
    os << u8"六、这些数字是怎么来的\n";
    os << u8"  代价按「进入一格付该格的代价」算，起点不计；岩壁永远进不去\n";
    os << u8"  最优代价有两条独立的路子互相核对：优先队列的 Dijkstra 与全图反复松弛，\n";
    os << u8"  两者都得到 " << best_cost << u8"；A* 与它相同，说明可采纳的启发没有破坏最优性\n";
    os << u8"  最优路径的条数用「按代价从小到大推一遍」数出来，共 " << best_paths << u8" 条\n";
    os << u8"  六种走法共用同一套邻居枚举顺序与同一套计数代码，差别只在容器的取出规则与排序键\n";
    os << u8"  报告里没有地址、没有时钟、没有随机数：换一台机器重跑，这一页逐位相同\n";
}

}   /* namespace */

std::string build_report()
{
    SearchResult runs[kAlgoCount];
    for (int i = 0; i < kAlgoCount; ++i) {
        runs[i] = run_search(default_map(), static_cast<Algo>(i));
    }

    std::vector<long long> dist;
    const long long best_cost = best_cost_by_relaxation(default_map(), &dist);
    const std::size_t best_paths = count_best_paths(default_map(), dist);

    std::ostringstream os;
    append_map(os, default_map(), runs);
    append_table(os, default_map(), runs);
    append_optimality(os, default_map(), runs, best_cost, best_paths);
    append_paths(os, default_map(), runs);
    append_overlays(os, default_map(), runs);
    append_accounting(os, best_cost, best_paths);
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

    void expect_eq(long long got, long long want, const std::string &what)
    {
        if (got == want) {
            expect(true, what);
        } else {
            expect(false, what + u8"（得到 " + std::to_string(got) + u8"，应为 " +
                              std::to_string(want) + u8"）");
        }
    }

    void expect_size(std::size_t got, std::size_t want, const std::string &what)
    {
        if (got == want) {
            expect(true, what);
        } else {
            expect(false, what + u8"（得到 " + std::to_string(got) + u8"，应为 " +
                              std::to_string(want) + u8"）");
        }
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

/** 路径是不是一条合法的行走：首尾对、每步都相邻、都踩在可通行格子上、不重复 */
bool path_is_legal(const Map &map, const SearchResult &run)
{
    if (!run.found || run.path.size() < 2) {
        return false;
    }
    if (run.path.front() != map.index(map.start_row, map.start_col) ||
        run.path.back() != map.index(map.goal_row, map.goal_col)) {
        return false;
    }
    std::vector<unsigned char> seen(map.cell_count(), 0);
    for (std::size_t i = 0; i < run.path.size(); ++i) {
        const int node = run.path[i];
        if (!map.passable(node / map.cols, node % map.cols)) {
            return false;
        }
        if (seen[static_cast<std::size_t>(node)] != 0) {
            return false;
        }
        seen[static_cast<std::size_t>(node)] = 1;
        if (i == 0) {
            continue;
        }
        const int step = (node > run.path[i - 1]) ? (node - run.path[i - 1])
                                                  : (run.path[i - 1] - node);
        if (step != 1 && step != map.cols) {
            return false;
        }
    }
    return true;
}

/** 路径代价的独立算法：逐格累加，与 run_search 里那段分开写 */
long long cost_of_path(const Map &map, const std::vector<int> &path)
{
    long long total = 0;
    for (std::size_t i = 1; i < path.size(); ++i) {
        total += map.cost_at(path[i] / map.cols, path[i] % map.cols);
    }
    return total;
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
    const Map &map = default_map();

    /* 1—3 地图本身 */
    checks.expect(map.rows == 9 && map.cols == 15 && map.start_row == 4 &&
                      map.start_col == 1 && map.goal_row == 4 && map.goal_col == 13,
                  u8"地图是 9 行 15 列，起点 (4,1)，终点 (4,13)");
    checks.expect(map.cell_count() == 135 && map.passable_count() == 125 &&
                      map.count_glyph('#') == 10,
                  u8"共 135 格，其中岩壁 10 格、可通行 125 格");
    checks.expect(map.count_glyph('.') == 53 && map.count_glyph(',') == 7 &&
                      map.count_glyph('%') == 63 && map.count_glyph('S') == 1 &&
                      map.count_glyph('G') == 1,
                  u8"草地 53、泥地 7、沼地 63、起点 1、终点 1");

    /* 4—7 独立核对：松弛从终点出发算一遍，再逐格逐边检查启发。
       可采纳要拿「本格到终点的真实最少代价」比，因此松弛的方向不能反 */
    std::vector<long long> best;
    const long long best_cost = best_cost_by_relaxation(map, &best);
    std::vector<long long> to_goal;
    dist_by_relaxation(map, map.goal_row, map.goal_col, &to_goal);
    checks.expect_eq(to_goal[static_cast<std::size_t>(map.index(map.start_row, map.start_col))],
                     best_cost, u8"从终点出发松弛算出的起点代价，与从起点出发算出的最优代价相同");
    bool admissible = true;
    for (int row = 0; row < map.rows; ++row) {
        for (int col = 0; col < map.cols; ++col) {
            if (!map.passable(row, col)) {
                continue;
            }
            const std::size_t at = static_cast<std::size_t>(map.index(row, col));
            if (to_goal[at] == kInf) {
                continue;
            }
            if (manhattan(map, row, col) > to_goal[at]) {
                admissible = false;
            }
        }
    }
    checks.expect(admissible, u8"曼哈顿启发在整张图上都不高估：每格的 h 都不超过它到终点的真实最少代价");

    bool consistent = true;
    for (int row = 0; row < map.rows; ++row) {
        for (int col = 0; col < map.cols; ++col) {
            if (!map.passable(row, col)) {
                continue;
            }
            for (int k = 0; k < 4; ++k) {
                const int nr = row + kDr[k];
                const int nc = col + kDc[k];
                if (!map.passable(nr, nc)) {
                    continue;
                }
                if (manhattan(map, row, col) > map.cost_at(nr, nc) + manhattan(map, nr, nc)) {
                    consistent = false;
                }
            }
        }
    }
    checks.expect(consistent, u8"曼哈顿启发是一致的：每条边上 h(本格) <= 进入代价 + h(邻格)");

    /* 7—8 最优代价与最优路径的条数 */
    checks.expect_eq(best_cost, 20, u8"全图反复松弛算出的最少代价是 20");
    const std::size_t best_paths = count_best_paths(map, best);
    checks.expect(best_paths >= 1, u8"最少代价路径至少有 1 条（数出来 " +
                                        std::to_string(best_paths) + u8" 条）");

    /* 9—15 六种走法逐个核对 */
    SearchResult runs[kAlgoCount];
    for (int i = 0; i < kAlgoCount; ++i) {
        runs[i] = run_search(map, static_cast<Algo>(i));
    }

    bool all_found = true;
    bool all_legal = true;
    bool all_cost_match = true;
    bool all_balanced = true;
    bool all_flags_match = true;
    bool all_edges_match = true;
    bool all_stale_rule = true;
    for (int i = 0; i < kAlgoCount; ++i) {
        const SearchResult &run = runs[i];
        if (!run.found) {
            all_found = false;
            continue;
        }
        if (!path_is_legal(map, run)) {
            all_legal = false;
        }
        if (cost_of_path(map, run.path) != run.path_cost) {
            all_cost_match = false;
        }
        if (run.pushes != run.pops + run.pending) {
            all_balanced = false;
        }
        std::size_t flagged = 0;
        for (const unsigned char flag : run.expanded_flags) {
            if (flag != 0) {
                ++flagged;
            }
        }
        if (flagged != run.expanded) {
            all_flags_match = false;
        }
        std::size_t degrees = 0;
        for (std::size_t at = 0; at < run.expanded_flags.size(); ++at) {
            if (run.expanded_flags[at] == 0) {
                continue;
            }
            const int node = static_cast<int>(at);
            const int row = node / map.cols;
            const int col = node % map.cols;
            for (int k = 0; k < 4; ++k) {
                if (map.passable(row + kDr[k], col + kDc[k])) {
                    ++degrees;
                }
            }
        }
        if (degrees != run.edge_checks) {
            all_edges_match = false;
        }
        if (run.expanded + run.stale_pops != run.pops) {
            all_stale_rule = false;
        }
        if (run.path_cost < best_cost) {
            all_cost_match = false;
        }
    }
    checks.expect(all_found, u8"六种走法都找到了终点");
    checks.expect(all_legal, u8"六条路径都合法：首尾对、每步横竖相邻、不踩岩壁、不重复经过同一格");
    checks.expect(all_cost_match,
                  u8"六条路径的代价与逐格累加的结果一致，且都不小于最优代价 20");
    checks.expect(all_balanced, u8"六种走法都满足 push = pop + 在队");
    checks.expect(all_flags_match, u8"展开数等于展开标记的个数");
    checks.expect(all_edges_match, u8"边检查数等于已展开格子的可通行邻居数之和");
    checks.expect(all_stale_rule, u8"展开数 + 作废弹出数 = pop 次数");

    /* 16 DFS 与 BFS 没有作废条目，取出一条就是展开一个 */
    checks.expect(runs[0].stale_pops == 0 && runs[1].stale_pops == 0 &&
                      runs[0].expanded == runs[0].pops && runs[1].expanded == runs[1].pops,
                  u8"DFS 与 BFS 每条入队记录都会走到展开：作废 0 条，展开数等于 pop 次数");

    /* 17—18 最优性的交叉核对 */
    checks.expect(runs[2].found && runs[2].path_cost == best_cost &&
                      runs[4].found && runs[4].path_cost == best_cost,
                  u8"Dijkstra 与 A* 的路径代价都等于全图松弛算出的最优代价 20");
    checks.expect(runs[3].path == runs[2].path && runs[3].expanded == runs[2].expanded &&
                      runs[3].edge_checks == runs[2].edge_checks &&
                      runs[3].pushes == runs[2].pushes && runs[3].pops == runs[2].pops,
                  u8"h 恒为 0 的 A* 与 Dijkstra 逐位相同：路径、展开、边检查、push、pop");

    /* 19—20 一致性启发带来的包含关系 */
    bool subset = true;
    for (std::size_t at = 0; at < runs[4].expanded_flags.size(); ++at) {
        if (runs[4].expanded_flags[at] != 0 && runs[2].expanded_flags[at] == 0) {
            subset = false;
        }
    }
    checks.expect(subset, u8"启发一致时，A* 展开的每个格子 Dijkstra 也展开过");
    checks.expect(runs[4].expanded < runs[2].expanded,
                  u8"启发非零且可采纳时 A* 展开的节点更少（" +
                      std::to_string(runs[4].expanded) + u8" < " +
                      std::to_string(runs[2].expanded) + u8"）");

    /* 21—24 四个必须成立的对照 */
    checks.expect(runs[0].path_cost > runs[1].path_cost,
                  u8"DFS 的路径代价大于 BFS 的路径代价（" +
                      std::to_string(runs[0].path_cost) + u8" > " +
                      std::to_string(runs[1].path_cost) + u8"）");
    checks.expect(runs[5].path_cost > runs[4].path_cost,
                  u8"贪心的路径代价大于 A* 的路径代价（" +
                      std::to_string(runs[5].path_cost) + u8" > " +
                      std::to_string(runs[4].path_cost) + u8"）");
    bool bfs_fewest = true;
    for (int i = 0; i < kAlgoCount; ++i) {
        if (runs[i].path_steps() < runs[1].path_steps()) {
            bfs_fewest = false;
        }
    }
    checks.expect(bfs_fewest, u8"BFS 的路径步数是六种走法里最少的");
    checks.expect(runs[1].path_steps() < runs[2].path_steps() &&
                      runs[1].path_cost > runs[2].path_cost,
                  u8"BFS 步数最少但代价不是最小，Dijkstra 反过来：步数最优与代价最优是两件事");

    /* 25 可复现：再跑一遍逐位相同 */
    bool repeatable = true;
    for (int i = 0; i < kAlgoCount; ++i) {
        const SearchResult again = run_search(map, static_cast<Algo>(i));
        if (again.path != runs[i].path || again.expanded != runs[i].expanded ||
            again.edge_checks != runs[i].edge_checks || again.pushes != runs[i].pushes ||
            again.pops != runs[i].pops || again.stale_pops != runs[i].stale_pops ||
            again.path_cost != runs[i].path_cost) {
            repeatable = false;
        }
    }
    checks.expect(repeatable, u8"六种走法各跑两遍，路径与全部结构量逐位相同");

    return checks.finish();
}

}   /* namespace slab */
