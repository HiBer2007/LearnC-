/* main_cli.cpp —— 练习模板 05 的命令行验收程序（C++）
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
 * 这个文件**不需要改**：它按 4 个阶段调用 dsg 里的三个图存法与并查集，
 * 把结果打印成《配置步骤.md》里的验收输出。
 *
 * 图那一半的判据不是「三种存法互相相等」（都是空的也相等），而是
 * **与边表本身对出来的答案**：验收程序自己从边表算出邻居集合与距离数组，
 * 再拿三种存法的结果去比。
 *
 * 构建与运行（在模板目录下）：
 *     cmake --preset mingw-gdb
 *     cmake --build --preset mingw-gdb
 *     build\mingw\bin\app_cli.exe
 *
 * 输出全部写成 ASCII：换一台代码页不是 65001 的机器也照样能读。
 */
#include <algorithm>
#include <cstddef>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

#include "disjoint_set.hpp"
#include "graph.hpp"
#include "traverse.hpp"

namespace {

void line(const char *label, const std::string &text)
{
    std::cout << std::left << std::setw(32) << label << ": " << text << "\n";
}

void line(const char *label, long value)
{
    std::cout << std::left << std::setw(32) << label << ": " << value << "\n";
}

/* 固定的一张无向图：8 个点、8 条边，另有孤点 7 */
const int kNodes = 8;
const int kEdges[8][2] = {
    {0, 1}, {0, 2}, {1, 3}, {2, 3}, {3, 4}, {4, 5}, {5, 6}, {4, 6},
};

template <class Graph>
void build_graph(Graph &g)
{
    for (int i = 0; i < 8; ++i) {
        g.add_edge(kEdges[i][0], kEdges[i][1]);
    }
}

/* ---------------------------------------------------------- 判据那一侧
 * 下面两个函数只看边表，与三种存法的实现无关，因此可以当作标准答案。 */

std::vector<int> table_neighbours(int v)
{
    std::vector<int> out;
    for (int i = 0; i < 8; ++i) {
        if (kEdges[i][0] == v) {
            out.push_back(kEdges[i][1]);
        } else if (kEdges[i][1] == v) {
            out.push_back(kEdges[i][0]);
        }
    }
    std::sort(out.begin(), out.end());
    return out;
}

std::vector<int> table_bfs(int start)
{
    std::vector<int> distance(static_cast<std::size_t>(kNodes), -1);
    std::vector<int> queue;
    distance[static_cast<std::size_t>(start)] = 0;
    queue.push_back(start);
    for (std::size_t head = 0; head < queue.size(); ++head) {
        const int v = queue[head];
        const std::vector<int> neighbours = table_neighbours(v);
        for (std::size_t i = 0; i < neighbours.size(); ++i) {
            const int u = neighbours[i];
            if (distance[static_cast<std::size_t>(u)] < 0) {
                distance[static_cast<std::size_t>(u)] = distance[static_cast<std::size_t>(v)] + 1;
                queue.push_back(u);
            }
        }
    }
    return distance;
}

/* ---------------------------------------------------------- 存法那一侧 */

/* 把邻居收集起来排序：三种存法给出的顺序不同，比集合才有意义 */
template <class Graph>
std::vector<int> neighbours_of(const Graph &g, int v)
{
    std::vector<int> out;
    g.for_each_neighbor(v, [&out](int u) { out.push_back(u); });
    std::sort(out.begin(), out.end());
    return out;
}

std::string format(const std::vector<int> &values)
{
    std::string text = "[";
    for (std::size_t i = 0; i < values.size(); ++i) {
        if (i != 0) {
            text += " ";
        }
        text += std::to_string(values[i]);
    }
    text += "]";
    return text;
}

/* 邻居集合与距离数组都对得上边表，才算这一种存法实现好了 */
template <class Graph>
bool matches_table(const Graph &g)
{
    for (int v = 0; v < kNodes; ++v) {
        if (neighbours_of(g, v) != table_neighbours(v)) {
            return false;
        }
    }
    return dsg::bfs_distance(g, 0) == table_bfs(0);
}

template <class Graph>
void report(const Graph &g)
{
    line("neighbors of 3", format(neighbours_of(g, 3)));
    line("neighbors of 4", format(neighbours_of(g, 4)));
    line("bfs distances from 0", format(dsg::bfs_distance(g, 0)));
    line("dfs order from 0", format(dsg::dfs_order(g, 0)));
}

const int kDsuNodes = 2000;

} /* namespace */

int main()
{
    /* ---------------------------------------------------------- 阶段 1 */
    std::cout << "=== Stage 1: adjacency list ===\n";
    {
        std::cout << "from the edge table (the judge)\n";
        line("neighbors of 3", format(table_neighbours(3)));
        line("neighbors of 4", format(table_neighbours(4)));
        line("bfs distances from 0", format(table_bfs(0)));

        dsg::AdjList list(kNodes);
        build_graph(list);
        std::cout << "adjacency list\n";
        report(list);
        line("adjacency list matches table", matches_table(list) ? "yes" : "no");

        dsg::BitMatrix matrix(kNodes);
        build_graph(matrix);
        dsg::EdgeList edges(kNodes);
        build_graph(edges);
        line("bitmap matrix matches table", matches_table(matrix) ? "yes" : "no");
        line("edge list matches table", matches_table(edges) ? "yes" : "no");
    }

    /* ---------------------------------------------------------- 阶段 2 */
    std::cout << "\n=== Stage 2: bitmap matrix and edge list ===\n";
    {
        dsg::BitMatrix matrix(kNodes);
        build_graph(matrix);
        dsg::EdgeList edges(kNodes);
        build_graph(edges);
        dsg::AdjList list(kNodes);
        build_graph(list);

        std::cout << "bitmap matrix\n";
        report(matrix);
        std::cout << "edge list\n";
        report(edges);

        line("bitmap matrix matches table", matches_table(matrix) ? "yes" : "no");
        line("edge list matches table", matches_table(edges) ? "yes" : "no");
        line("edges in the graph", static_cast<long>(edges.edge_count()));
        line("adjacency list slots", static_cast<long>(edges.edge_count()) * 2);
        line("bitmap matrix words", static_cast<long>(kNodes) * kNodes / 64 + 1);
    }

    /* ---------------------------------------------------------- 阶段 3 */
    std::cout << "\n=== Stage 3: path compression in find ===\n";
    {
        dsg::DisjointSet set(kDsuNodes);
        for (int i = 1; i < kDsuNodes; ++i) {
            set.link_raw(i - 1, i);         /* 手工搭一条 0 → 1 → … → 1999 的长链 */
        }
        line("nodes in the chain", static_cast<long>(kDsuNodes));
        line("max height before find", static_cast<long>(set.max_height()));

        set.reset_steps();
        const int first = set.find(0);
        const long first_steps = set.steps();

        set.reset_steps();
        const int second = set.find(0);
        const long second_steps = set.steps();

        line("find(0), first pass", first_steps);
        line("find(0), second pass", second_steps);
        line("root of every node", first == second ? std::to_string(first) : std::string("differs"));
        line("distinct roots", static_cast<long>(set.distinct_roots()));
    }

    /* ---------------------------------------------------------- 阶段 4 */
    std::cout << "\n=== Stage 4: union by size ===\n";
    {
        dsg::DisjointSet set(kDsuNodes);
        set.reset_steps();
        for (int i = 1; i < kDsuNodes; ++i) {
            set.unite(i, i - 1);            /* 总是把新点并到老树上 */
        }
        line("unions", static_cast<long>(kDsuNodes) - 1);
        line("steps while uniting", set.steps());
        line("max height", static_cast<long>(set.max_height()));
        line("distinct roots", static_cast<long>(set.distinct_roots()));
        line("same(0, 1999)", set.same(0, kDsuNodes - 1) ? "yes" : "no");
        line("size of the set", static_cast<long>(set.size_of(0)));

        set.reset_steps();
        for (int i = 0; i < kDsuNodes; ++i) {
            set.find(i);
        }
        line("steps for 2000 finds", set.steps());
    }

    return 0;
}
