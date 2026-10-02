/* main_cli.cpp —— 练习模板 08 的命令行验收程序（C++）
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
 * 这个文件**不需要改**：它按 4 个阶段调用 galgo 里的函数，把结果打印成
 * 《配置步骤.md》里的验收输出。你的实现写在 src/graphalgo.cpp 里。
 *
 * 构建与运行（在模板目录下）：
 *     cmake --preset mingw-gdb
 *     cmake --build --preset mingw-gdb
 *     build\mingw\bin\app_cli.exe
 *
 * 输出全部写成 ASCII：换一台代码页不是 65001 的机器也照样能读。
 */
#include <cstddef>
#include <iomanip>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

#include "graphalgo.hpp"

namespace {

const int kLabelWidth = 32;

void line(const std::string &label, long long value)
{
    std::cout << std::left << std::setw(kLabelWidth) << label << ": " << value << "\n";
}

void line(const std::string &label, const std::string &text)
{
    std::cout << std::left << std::setw(kLabelWidth) << label << ": " << text << "\n";
}

std::string yes_no(bool v)
{
    return v ? "yes" : "no";
}

std::string numbers(const std::vector<int> &v)
{
    if (v.empty()) {
        return "(none)";
    }
    std::string s;
    for (std::size_t i = 0; i < v.size(); ++i) {
        if (i != 0) {
            s += ' ';
        }
        s += std::to_string(v[i]);
    }
    return s;
}

std::string pairs(const std::vector<std::pair<int, int>> &v)
{
    if (v.empty()) {
        return "(none)";
    }
    std::string s;
    for (std::size_t i = 0; i < v.size(); ++i) {
        if (i != 0) {
            s += ' ';
        }
        s += "(" + std::to_string(v[i].first) + "," + std::to_string(v[i].second) + ")";
    }
    return s;
}

std::string vertices_with_indegree(const std::vector<int> &indeg)
{
    std::vector<int> left;
    for (std::size_t i = 0; i < indeg.size(); ++i) {
        if (indeg[i] != 0) {
            left.push_back(static_cast<int>(i));
        }
    }
    return numbers(left);
}

long long count_nonzero(const std::vector<int> &indeg)
{
    long long c = 0;
    for (int x : indeg) {
        if (x != 0) {
            ++c;
        }
    }
    return c;
}

/* ------------------------------------------------------------ 阶段 1 */

void print_topo(const std::string &title, const galgo::AdjList &g)
{
    const galgo::TopoResult t = galgo::kahn_toposort(g);

    std::cout << "-- " << title << " --\n";
    line("topological order", numbers(t.order));
    line("order length", static_cast<long long>(t.order.size()));
    line("enqueue order", numbers(t.enqueued));
    line("indegree decrements", t.decrements);
    line("vertices with indegree left", count_nonzero(t.indeg_left));
    line("those vertices", vertices_with_indegree(t.indeg_left));
    line("has cycle", yes_no(t.has_cycle));
}

void stage1()
{
    std::cout << "=== Stage 1: Kahn topological sort ===\n";

    const galgo::AdjList dag = galgo::sample_digraph_acyclic();
    const galgo::AdjList cyc = galgo::sample_digraph();

    line("acyclic graph vertices", static_cast<long long>(dag.size()));
    line("acyclic graph edges", galgo::directed_edge_count(dag));
    line("cyclic graph vertices", static_cast<long long>(cyc.size()));
    line("cyclic graph edges", galgo::directed_edge_count(cyc));

    print_topo("acyclic graph", dag);
    print_topo("cyclic graph", cyc);
}

/* ------------------------------------------------------------ 阶段 2 */

void stage2()
{
    std::cout << "\n=== Stage 2: Tarjan strong components ===\n";

    const galgo::AdjList g = galgo::sample_digraph();
    const galgo::SccResult r = galgo::tarjan_scc(g);

    line("vertices", static_cast<long long>(g.size()));
    line("edges", galgo::directed_edge_count(g));
    line("component count", static_cast<long long>(r.comps.size()));

    long long total = 0;
    long long largest = 0;
    long long singles = 0;
    for (const std::vector<int> &comp : r.comps) {
        const long long size = static_cast<long long>(comp.size());
        total += size;
        if (size > largest) {
            largest = size;
        }
        if (size == 1) {
            ++singles;
        }
    }
    line("sum of component sizes", total);
    line("largest component size", largest);
    line("single-vertex components", singles);

    for (std::size_t i = 0; i < r.comps.size(); ++i) {
        const std::string label = "component " + std::to_string(i + 1) +
                                  " (size " + std::to_string(r.comps[i].size()) + ")";
        line(label, numbers(r.comps[i]));
    }

    line("dfn", numbers(r.dfn));
    line("low", numbers(r.low));
    line("scc matches reachability", yes_no(galgo::scc_matches_reachability(g, r.comps)));
}

/* ------------------------------------------------------------ 阶段 3 */

void stage3()
{
    std::cout << "\n=== Stage 3: cut vertices and bridges ===\n";

    const int n = galgo::sample_undirected_n();
    const std::vector<galgo::WeightedEdge> edges = galgo::sample_undirected_edges();
    const galgo::AdjList g = galgo::to_unweighted(n, edges);

    line("vertices", n);
    line("edges", static_cast<long long>(edges.size()));

    const galgo::CutResult r = galgo::find_cut_vertices(g);
    line("cut vertices", numbers(r.cut_vertices));
    line("cut vertex count", static_cast<long long>(r.cut_vertices.size()));
    line("bridges", pairs(r.bridges));
    line("bridge count", static_cast<long long>(r.bridges.size()));
    line("dfn", numbers(r.dfn));
    line("low", numbers(r.low));

    const std::vector<int> bc = galgo::brute_force_cut_vertices(g);
    const std::vector<std::pair<int, int>> bb = galgo::brute_force_bridges(g);
    line("cut vertices by brute force", numbers(bc));
    line("bridges by brute force", pairs(bb));
    line("cut vertices match", yes_no(r.cut_vertices == bc));
    line("bridges match", yes_no(r.bridges == bb));
}

/* ------------------------------------------------------------ 阶段 4 */

void stage4()
{
    std::cout << "\n=== Stage 4: Kruskal and the disjoint set ===\n";

    const int n = galgo::sample_undirected_n();
    const std::vector<galgo::WeightedEdge> edges = galgo::sample_undirected_edges();

    line("vertices", n);
    line("edges in the table", static_cast<long long>(edges.size()));

    const galgo::MstResult r = galgo::kruskal(n, edges);

    line("edges considered", r.considered);
    line("edges never considered", static_cast<long long>(edges.size()) - r.considered);
    line("edges chosen", static_cast<long long>(r.chosen.size()));
    line("edges skipped", r.skipped);
    line("chosen equals vertices - 1", yes_no(static_cast<long long>(r.chosen.size()) == n - 1));
    line("total weight", r.total_weight);
    line("sets left", r.sets_left);

    std::cout << "chosen edges, in the order they were added:\n";
    for (const galgo::WeightedEdge &e : r.chosen) {
        std::cout << "  " << e.u << "-" << e.v << " w=" << e.w << "\n";
    }
}

} /* namespace */

int main()
{
    stage1();
    stage2();
    stage3();
    stage4();
    return 0;
}
