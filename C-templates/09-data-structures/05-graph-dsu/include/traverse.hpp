/* traverse.hpp —— 练习模板 05 的遍历算法（C++，已给出）
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
 * 这两个函数是这张卷子的「判据」：它们只调用 for_each_neighbor，
 * 不知道底下是邻接表、矩阵还是边表。因此只要三份接口实现都对，
 * 三种存法跑出来的距离数组必须完全一样。
 *
 * 这个文件不需要改。
 */
#ifndef TRAVERSE_HPP
#define TRAVERSE_HPP

#include <cstddef>
#include <vector>

namespace dsg {

/* 广度优先：返回每个点到 start 的距离，-1 表示到不了。
 * 用队列（这里就是一个从头读到尾的数组）逐层推进。 */
template <class Graph>
std::vector<int> bfs_distance(const Graph &g, int start)
{
    std::vector<int> distance(static_cast<std::size_t>(g.node_count()), -1);
    if (start < 0 || start >= g.node_count()) {
        return distance;
    }
    std::vector<int> queue;
    distance[static_cast<std::size_t>(start)] = 0;
    queue.push_back(start);

    for (std::size_t head = 0; head < queue.size(); ++head) {
        const int v = queue[head];
        g.for_each_neighbor(v, [&](int u) {
            if (distance[static_cast<std::size_t>(u)] < 0) {
                distance[static_cast<std::size_t>(u)] = distance[static_cast<std::size_t>(v)] + 1;
                queue.push_back(u);
            }
        });
    }
    return distance;
}

/* 深度优先：**显式栈**，不用递归。
 * 链状的图（十万个点连成一条线）会让递归深度等于点数，
 * 本机默认栈大约 1 MB，十万层递归足够把它用光。 */
template <class Graph>
std::vector<int> dfs_order(const Graph &g, int start)
{
    std::vector<int> order;
    std::vector<char> visited(static_cast<std::size_t>(g.node_count()), 0);
    if (start < 0 || start >= g.node_count()) {
        return order;
    }
    std::vector<int> stack;
    stack.push_back(start);
    visited[static_cast<std::size_t>(start)] = 1;

    while (!stack.empty()) {
        const int v = stack.back();
        stack.pop_back();
        order.push_back(v);
        g.for_each_neighbor(v, [&](int u) {
            if (visited[static_cast<std::size_t>(u)] == 0) {
                visited[static_cast<std::size_t>(u)] = 1;
                stack.push_back(u);
            }
        });
    }
    return order;
}

} /* namespace dsg */

#endif /* TRAVERSE_HPP */
