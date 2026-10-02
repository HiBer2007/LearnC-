/* main_cli.cpp —— 练习模板 07 的命令行验收程序（C++）
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
 * 这个文件**不需要改**：它按 4 个阶段调用 pf 里的函数，把结果打印成
 * 《配置步骤.md》里的验收输出。你的实现写在 src/pathfind.cpp 里。
 *
 * 构建与运行（在模板目录下）：
 *     cmake --preset mingw-gdb
 *     cmake --build --preset mingw-gdb
 *     build\mingw\bin\app_cli.exe
 *
 * 输出全部写成 ASCII：换一台代码页不是 65001 的机器也照样能读。
 * 地图与代价都写死在 src/pathfind.cpp 里，没有随机数、没有文件输入，
 * 因此同一份源码重跑逐位相同。
 */
#include <cstddef>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

#include "pathfind.hpp"

namespace {

/* 每条判据一行：标签左对齐占 32 列，值与标签之间是「冒号加空格」 */
void line(const char *label, long long value)
{
    std::cout << std::left << std::setw(32) << label << ": " << value << "\n";
}

void line(const char *label, const std::string &text)
{
    std::cout << std::left << std::setw(32) << label << ": " << text << "\n";
}

std::string yes_no(bool v)
{
    return v ? "yes" : "no";
}

long long walkable_cells()
{
    long long n = 0;
    for (int r = 0; r < pf::rows(); ++r) {
        for (int c = 0; c < pf::cols(); ++c) {
            if (pf::walkable(r, c)) {
                ++n;
            }
        }
    }
    return n;
}

bool same_path(const std::vector<pf::Pos> &a, const std::vector<pf::Pos> &b)
{
    if (a.size() != b.size()) {
        return false;
    }
    for (std::size_t i = 0; i < a.size(); ++i) {
        if (a[i].r != b[i].r || a[i].c != b[i].c) {
            return false;
        }
    }
    return true;
}

/* 展开顺序 / 弹出顺序都是坐标序列，8 个一行 */
const std::size_t kPerLine = 8;

void order_block(const char *label, const std::vector<pf::Pos> &v)
{
    std::cout << label << " (" << v.size() << "):\n";
    pf::print_coords(std::cout, v, kPerLine);
}

void path_block(const char *label, const pf::Result &r)
{
    std::cout << label << " (" << r.steps << " steps):\n";
    pf::print_coords(std::cout, r.path, kPerLine);
}

} /* namespace */

int main()
{
    /* ---------------------------------------------------------- 阶段 1 */
    std::cout << "=== Stage 1: depth-first search, an explicit stack ===\n";
    {
        line("map", std::string(std::to_string(pf::cols()) + " cols x " +
                                std::to_string(pf::rows()) + " rows"));
        line("walkable cells", walkable_cells());
        std::cout << "map:\n";
        pf::print_map(std::cout);

        const pf::Result d = pf::dfs();
        line("dfs expanded", static_cast<long long>(d.order.size()));
        line("dfs path steps", d.steps);
        line("dfs path cost", d.cost);
        order_block("dfs order (r,c)", d.order);
        path_block("dfs path (r,c)", d);
        std::cout << "dfs path on map:\n";
        pf::print_map_with_path(std::cout, d.path);
    }

    /* ---------------------------------------------------------- 阶段 2 */
    std::cout << "\n=== Stage 2: breadth-first search, one level at a time ===\n";
    {
        const pf::Result b = pf::bfs();
        line("bfs expanded", static_cast<long long>(b.order.size()));
        line("bfs levels", static_cast<long long>(b.level_sizes.size()));

        long long entered = 0;
        for (std::size_t i = 0; i < b.level_sizes.size(); ++i) {
            entered += b.level_sizes[i];
        }
        line("bfs level total", entered);
        std::cout << std::left << std::setw(32) << "bfs level sizes" << ": ";
        pf::print_levels(std::cout, b.level_sizes);

        line("bfs path steps", b.steps);
        line("bfs path cost", b.cost);
        order_block("bfs order (r,c)", b.order);
        path_block("bfs path (r,c)", b);
        std::cout << "bfs path on map:\n";
        pf::print_map_with_path(std::cout, b.path);
    }

    /* ---------------------------------------------------------- 阶段 3 */
    std::cout << "\n=== Stage 3: Dijkstra, the relaxation step ===\n";
    {
        std::cout << "cost map ('#' wall, 1..9 cost of entering the cell):\n";
        pf::print_cost_map(std::cout);

        const pf::Result b = pf::bfs();
        const pf::Result j = pf::dijkstra();
        line("dijkstra expanded", static_cast<long long>(j.order.size()));
        line("dijkstra pops from open", j.pops);
        line("dijkstra relax ok", j.relax_ok);
        line("dijkstra relax dropped", j.relax_drop);
        line("dijkstra path steps", j.steps);
        line("dijkstra path cost", j.cost);
        order_block("dijkstra pop order (r,c)", j.order);

        std::cout << "dijkstra dist grid (## wall, .. not reached):\n";
        pf::print_dist_grid(std::cout, j.dist);

        std::cout << "dijkstra path on map:\n";
        pf::print_map_with_path(std::cout, j.path);

        std::cout << "bfs on the same cost map:\n";
        line("  bfs path steps", b.steps);
        line("  bfs path cost", b.cost);
        line("  dijkstra path steps", j.steps);
        line("  dijkstra path cost", j.cost);
        line("  dijkstra is cheaper by", b.cost - j.cost);
    }

    /* ---------------------------------------------------------- 阶段 4 */
    std::cout << "\n=== Stage 4: A*, the estimate and the open set ===\n";
    {
        const pf::Result j = pf::dijkstra();
        const pf::Result a = pf::astar(true);
        const pf::Result z = pf::astar(false);

        line("astar(h=1) expanded", static_cast<long long>(a.order.size()));
        line("astar(h=1) pops from open", a.pops);
        line("astar(h=1) path steps", a.steps);
        line("astar(h=1) path cost", a.cost);
        line("dijkstra expanded", static_cast<long long>(j.order.size()));
        line("dijkstra path cost", j.cost);
        line("astar saves expansions", static_cast<long long>(j.order.size() - a.order.size()));
        line("astar(h=1) path same as dijkstra", yes_no(same_path(a.path, j.path)));
        order_block("astar(h=1) order (r,c)", a.order);

        std::cout << "estimate turned off, A* must fall back to Dijkstra:\n";
        line("  astar(h=0) expanded", static_cast<long long>(z.order.size()));
        line("  astar(h=0) pops from open", z.pops);
        line("  astar(h=0) path steps", z.steps);
        line("  astar(h=0) path cost", z.cost);
        line("  astar(h=0) vs dijkstra, order", yes_no(same_path(z.order, j.order)));
        line("  astar(h=0) vs dijkstra, cost", yes_no(z.cost == j.cost));
    }

    return 0;
}
