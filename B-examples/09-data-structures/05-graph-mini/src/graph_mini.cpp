/**
 * graph_mini.cpp —— 三种存法的项目输出与自测
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
 * 这里没有任何界面代码，也不打印任何东西。
 * 报告里的数字全是计数：回调次数、检查过的位置数、度数之和、内存字节数。
 * 内存字节数是按结构算出来的（容器头部加上数组容量），不是称出来的。
 * 计时一个都不做。
 */
#include "graph_mini.hpp"

#include <cstdint>
#include <iomanip>
#include <set>
#include <sstream>
#include <string>
#include <utility>

namespace gmini {

void GraphStats::reset()
{
    *this = GraphStats();
}

GraphStats &stats()
{
    static GraphStats instance;
    return instance;
}

std::string CheckResult::summary() const
{
    std::ostringstream os;
    os << total << u8" 项中 " << passed << u8" 项通过";
    if (failed == 0) {
        os << u8"，全部通过";
    } else {
        os << u8"，" << failed << u8" 项失败";
    }
    return os.str();
}

/* ================= 报告用的小工具 ================= */

namespace {

bool is_wide_codepoint(unsigned int cp)
{
    return (cp >= 0x1100u && cp <= 0x115Fu) || (cp >= 0x2E80u && cp <= 0x303Eu)
           || (cp >= 0x3041u && cp <= 0x33FFu) || (cp >= 0x3400u && cp <= 0x4DBFu)
           || (cp >= 0x4E00u && cp <= 0x9FFFu) || (cp >= 0xA000u && cp <= 0xA4CFu)
           || (cp >= 0xAC00u && cp <= 0xD7A3u) || (cp >= 0xF900u && cp <= 0xFAFFu)
           || (cp >= 0xFE30u && cp <= 0xFE6Fu) || (cp >= 0xFF00u && cp <= 0xFF60u)
           || (cp >= 0xFFE0u && cp <= 0xFFE6u);
}

std::size_t display_width(const std::string &text)
{
    std::size_t width = 0;
    for (std::size_t i = 0; i < text.size();) {
        const unsigned char lead = static_cast<unsigned char>(text[i]);
        unsigned int cp = lead;
        std::size_t length = 1;
        if (lead >= 0xF0u) {
            cp = lead & 0x07u;
            length = 4;
        } else if (lead >= 0xE0u) {
            cp = lead & 0x0Fu;
            length = 3;
        } else if (lead >= 0xC0u) {
            cp = lead & 0x1Fu;
            length = 2;
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
    const std::size_t used = display_width(text);
    return used >= width ? text : text + std::string(width - used, ' ');
}

std::string pad_left_text(const std::string &text, std::size_t width)
{
    const std::size_t used = display_width(text);
    return used >= width ? text : std::string(width - used, ' ') + text;
}

std::string pad_left(std::size_t value, std::size_t width)
{
    return pad_left_text(std::to_string(value), width);
}

std::string one_decimal(std::size_t part, std::size_t whole)
{
    std::ostringstream os;
    os << std::fixed << std::setprecision(1)
       << (static_cast<double>(part) / static_cast<double>(whole));
    return os.str();
}

/** 固定种子的线性同余发生器：报告里的图因此可以逐位复现 */
class Lcg {
public:
    explicit Lcg(std::uint64_t seed) : state_(seed) {}

    std::uint32_t next()
    {
        state_ = state_ * 6364136223846793005ull + 1442695040888963407ull;
        return static_cast<std::uint32_t>(state_ >> 33);
    }

private:
    std::uint64_t state_;
};

using EdgePair = std::pair<int, int>;

/** 造一张稀疏无向图：去重、去掉自环、按 (小, 大) 排好序 */
std::vector<EdgePair> make_edges(int n, std::size_t count, std::uint64_t seed)
{
    Lcg lcg(seed);
    std::set<EdgePair> unique;
    std::size_t guard = 0;
    while (unique.size() < count && guard < count * 200) {
        ++guard;
        const int a = static_cast<int>(lcg.next() % static_cast<std::uint32_t>(n));
        const int b = static_cast<int>(lcg.next() % static_cast<std::uint32_t>(n));
        if (a == b) {
            continue;
        }
        unique.insert(a < b ? EdgePair{a, b} : EdgePair{b, a});
    }
    return std::vector<EdgePair>(unique.begin(), unique.end());
}

template <class G>
void fill_undirected(G &graph, const std::vector<EdgePair> &edges)
{
    for (const EdgePair &edge : edges) {
        graph.add_undirected(edge.first, edge.second);
    }
}

template <class G>
std::string neighbors_dump(const G &graph)
{
    std::ostringstream os;
    for (int v = 0; v < graph.vertex_count(); ++v) {
        os << v << u8" 号：";
        bool first = true;
        graph.for_each_neighbor(v, [&](int u) {
            if (!first) {
                os << ',';
            }
            os << u;
            first = false;
        });
        if (first) {
            os << u8"（没有邻居）";
        }
        if (v + 1 < graph.vertex_count()) {
            os << u8"；";
        }
    }
    return os.str();
}

template <class G>
std::size_t degree_sum(const G &graph)
{
    std::size_t total = 0;
    for (int v = 0; v < graph.vertex_count(); ++v) {
        total += graph.degree(v);
    }
    return total;
}

std::string join_ints(const std::vector<int> &values)
{
    std::ostringstream os;
    for (std::size_t i = 0; i < values.size(); ++i) {
        if (i != 0) {
            os << ' ';
        }
        os << values[i];
    }
    return os.str();
}

/* 自测的小工具：把每一项的结果记下来，不打印 */
class Checker {
public:
    void check(bool ok, const std::string &what, const std::string &detail = std::string())
    {
        ++result_.total;
        std::ostringstream line;
        if (ok) {
            ++result_.passed;
            line << u8"[通过] " << result_.total << ". " << what;
        } else {
            ++result_.failed;
            line << u8"[失败] " << result_.total << ". " << what;
            if (!detail.empty()) {
                line << u8"（" << detail << u8"）";
            }
        }
        result_.lines.push_back(line.str());
    }

    CheckResult take() { return result_; }

private:
    CheckResult result_;
};

const int kDemoVertices = 12;
const std::size_t kDemoEdges = 18;
const int kSparseVertices = 200;
const std::size_t kSparseEdges = 300;

/** 三种存法各自装同一张无向图 */
struct GraphTriple {
    AdjList list{kDemoVertices};
    AdjMatrix matrix{kDemoVertices};
    EdgeList edges{kDemoVertices};

    void fill(const std::vector<EdgePair> &source)
    {
        fill_undirected(list, source);
        fill_undirected(matrix, source);
        fill_undirected(edges, source);
        list.shrink_rows();
        edges.shrink_rows();
    }
};

void append_storage_section(std::ostringstream &os)
{
    const std::vector<EdgePair> source = make_edges(kDemoVertices, kDemoEdges, 20261002u);
    GraphTriple triple;
    triple.fill(source);

    os << u8"同一张图，三种存法（" << kDemoVertices << u8" 个点、" << kDemoEdges << u8" 条无向边）\n";
    os << "  " << pad_right(u8"存法", 20) << pad_left_text(u8"顶点数", 8) << "  "
       << pad_left_text(u8"边数", 6) << "  " << pad_left_text(u8"度数之和", 10) << "  "
       << pad_left_text(u8"内存字节", 10) << "\n";
    os << "  " << pad_right(AdjList::name(), 20) << pad_left(static_cast<std::size_t>(triple.list.vertex_count()), 8)
       << "  " << pad_left(triple.list.edge_count(), 6) << "  "
       << pad_left(degree_sum(triple.list), 10) << "  " << pad_left(triple.list.memory_bytes(), 10)
       << "\n";
    os << "  " << pad_right(AdjMatrix::name(), 20)
       << pad_left(static_cast<std::size_t>(triple.matrix.vertex_count()), 8) << "  "
       << pad_left(triple.matrix.edge_count(), 6) << "  " << pad_left(degree_sum(triple.matrix), 10)
       << "  " << pad_left(triple.matrix.memory_bytes(), 10) << "\n";
    os << "  " << pad_right(EdgeList::name(), 20) << pad_left(static_cast<std::size_t>(triple.edges.vertex_count()), 8)
       << "  " << pad_left(triple.edges.edge_count(), 6) << "  "
       << pad_left(degree_sum(triple.edges), 10) << "  " << pad_left(triple.edges.memory_bytes(), 10)
       << "\n";
    os << u8"  度数之和等于 2 倍边数，三种存法都一样；内存字节数是按结构算的\n";
    os << u8"  邻接表 = 每个点一个 vector 头部 + 各内层数组的容量；矩阵 = 位图字节数；边表 = 条数 × 8\n";

    os << u8"\n各点的邻居（三种存法逐位相同）\n";
    os << u8"  邻接表：" << neighbors_dump(triple.list) << "\n";
    os << u8"  矩阵：  " << neighbors_dump(triple.matrix) << "\n";
    os << u8"  边表：  " << neighbors_dump(triple.edges) << "\n";
}

void append_same_algorithm_section(std::ostringstream &os)
{
    const std::vector<EdgePair> source = make_edges(kDemoVertices, kDemoEdges, 20261002u);
    GraphTriple triple;
    triple.fill(source);

    os << u8"\n换存法不改算法：同一份模板实例化三次\n";

    const std::vector<int> bfs_list = bfs_order(triple.list, 0);
    const std::vector<int> bfs_matrix = bfs_order(triple.matrix, 0);
    const std::vector<int> bfs_edges = bfs_order(triple.edges, 0);
    os << u8"  BFS：  邻接表 " << join_ints(bfs_list) << "\n";
    os << u8"         矩阵   " << join_ints(bfs_matrix) << "\n";
    os << u8"         边表   " << join_ints(bfs_edges) << "\n";
    os << u8"         三者逐位相同：" << ((bfs_list == bfs_matrix && bfs_list == bfs_edges) ? u8"是" : u8"否")
       << "\n";

    const std::vector<int> dfs_list = dfs_order(triple.list, 0);
    const std::vector<int> dfs_matrix = dfs_order(triple.matrix, 0);
    const std::vector<int> dfs_edges = dfs_order(triple.edges, 0);
    os << u8"  DFS：  邻接表 " << join_ints(dfs_list) << "\n";
    os << u8"         矩阵   " << join_ints(dfs_matrix) << "\n";
    os << u8"         边表   " << join_ints(dfs_edges) << "\n";
    os << u8"         三者逐位相同：" << ((dfs_list == dfs_matrix && dfs_list == dfs_edges) ? u8"是" : u8"否")
       << "\n";

    const std::vector<int> label_list = component_label(triple.list);
    const std::vector<int> label_matrix = component_label(triple.matrix);
    const std::vector<int> label_edges = component_label(triple.edges);
    std::size_t components = 0;
    for (std::size_t i = 0; i < label_list.size(); ++i) {
        if (static_cast<std::size_t>(label_list[i]) == i) {
            ++components;       /* 标号等于自己下标的那些就是各块的第一个点 */
        }
    }
    os << u8"  连通块标号：邻接表 " << join_ints(label_list) << "\n";
    os << u8"              矩阵   " << join_ints(label_matrix) << "\n";
    os << u8"              边表   " << join_ints(label_edges) << "\n";
    os << u8"              连通块 " << components << u8" 个，三者逐位相同："
       << ((label_list == label_matrix && label_list == label_edges) ? u8"是" : u8"否") << "\n";

    const std::size_t visits_list = count_all_neighbor_visits(triple.list);
    const std::size_t visits_matrix = count_all_neighbor_visits(triple.matrix);
    const std::size_t visits_edges = count_all_neighbor_visits(triple.edges);
    os << u8"  把所有点的邻居走一遍：邻接表 " << visits_list << u8" 次、矩阵 " << visits_matrix
       << u8" 次、边表 " << visits_edges << u8" 次，都等于 2 倍边数 = " << (2 * kDemoEdges) << "\n";
    os << u8"  三个算法的源码里没有出现过任何一种存法的类型名\n";

    /* 有向无环图上的拓扑排序 */
    const int dag_vertices = 8;
    AdjList dag_list(dag_vertices);
    AdjMatrix dag_matrix(dag_vertices);
    EdgeList dag_edges(dag_vertices);
    const EdgePair dag[] = { {0, 1}, {0, 2}, {1, 3}, {2, 3}, {3, 4},
                             {1, 5}, {5, 6}, {4, 6}, {2, 6}, {6, 7} };
    for (const EdgePair &edge : dag) {
        dag_list.add_directed(edge.first, edge.second);
        dag_matrix.add_directed(edge.first, edge.second);
        dag_edges.add_directed(edge.first, edge.second);
    }
    const std::vector<int> topo_list = topological_order(dag_list);
    const std::vector<int> topo_matrix = topological_order(dag_matrix);
    const std::vector<int> topo_edges = topological_order(dag_edges);
    os << u8"\n  拓扑排序（8 个点、10 条有向边）：" << join_ints(topo_list) << "\n";
    os << u8"    三种存法逐位相同："
       << ((topo_list == topo_matrix && topo_list == topo_edges) ? u8"是" : u8"否")
       << u8"；长度 " << topo_list.size() << u8" 等于点数\n";
}

void append_directed_section(std::ostringstream &os)
{
    const std::vector<EdgePair> source = make_edges(kDemoVertices, kDemoEdges, 20261002u);

    AdjList undirected_list(kDemoVertices);
    AdjList directed_list(kDemoVertices);
    EdgeList undirected_edges(kDemoVertices);
    EdgeList directed_edges(kDemoVertices);
    for (const EdgePair &edge : source) {
        undirected_list.add_undirected(edge.first, edge.second);
        directed_list.add_directed(edge.first, edge.second);
        undirected_edges.add_undirected(edge.first, edge.second);
        directed_edges.add_directed(edge.first, edge.second);
    }
    undirected_list.shrink_rows();
    directed_list.shrink_rows();
    undirected_edges.shrink_rows();
    directed_edges.shrink_rows();

    os << u8"\n有向与无向：同一条边集，存的东西不一样\n";
    os << u8"  邻接表：无向存 " << undirected_list.edge_count() << u8" 条边、"
       << undirected_list.memory_bytes() << u8" 字节、邻居访问 "
       << count_all_neighbor_visits(undirected_list) << u8" 次；有向存 "
       << directed_list.edge_count() << u8" 条边、" << directed_list.memory_bytes()
       << u8" 字节、邻居访问 " << count_all_neighbor_visits(directed_list) << u8" 次\n";
    os << u8"  边表：  无向 " << undirected_edges.memory_bytes() << u8" 字节（每条边两个方向）；有向 "
       << directed_edges.memory_bytes() << u8" 字节（只存一侧）\n";
    os << u8"  点少的时候，邻接表的字节数被每个点的 vector 头部（"
       << sizeof(std::vector<int>) << u8" 字节）占去大半，省下的一半边只体现在访问次数上\n";

    const EdgePair &probe = source[0];
    os << u8"  取第一条边 (" << probe.first << u8", " << probe.second << u8") 试两个方向：\n";
    os << u8"    无向图：has_edge(" << probe.first << u8", " << probe.second << u8") = "
       << (undirected_list.has_edge(probe.first, probe.second) ? u8"真" : u8"假") << u8"，has_edge("
       << probe.second << u8", " << probe.first << u8") = "
       << (undirected_list.has_edge(probe.second, probe.first) ? u8"真" : u8"假") << u8"\n";
    os << u8"    有向图：has_edge(" << probe.first << u8", " << probe.second << u8") = "
       << (directed_list.has_edge(probe.first, probe.second) ? u8"真" : u8"假") << u8"，has_edge("
       << probe.second << u8", " << probe.first << u8") = "
       << (directed_list.has_edge(probe.second, probe.first) ? u8"真" : u8"假") << u8"\n";
}

void append_operation_section(std::ostringstream &os)
{
    const std::vector<EdgePair> source = make_edges(kSparseVertices, kSparseEdges, 7u);
    AdjList list(kSparseVertices);
    AdjMatrix matrix(kSparseVertices);
    EdgeList edges(kSparseVertices);
    fill_undirected(list, source);
    fill_undirected(matrix, source);
    fill_undirected(edges, source);

    os << u8"\n查边与取邻居的操作次数（" << kSparseVertices << u8" 个点、" << kSparseEdges
       << u8" 条无向边，全是计数不是计时）\n";

    stats().reset();
    for (const EdgePair &edge : source) {
        list.has_edge(edge.first, edge.second);
    }
    const std::size_t list_lookup = stats().probes;

    stats().reset();
    for (const EdgePair &edge : source) {
        matrix.has_edge(edge.first, edge.second);
    }
    const std::size_t matrix_lookup = stats().probes;

    stats().reset();
    for (const EdgePair &edge : source) {
        edges.has_edge(edge.first, edge.second);
    }
    const std::size_t edge_lookup = stats().probes;

    os << u8"  查这 " << kSparseEdges << u8" 条边各一次，检查过的位置数：\n";
    os << "    " << pad_right(u8"邻接表", 20) << pad_left(list_lookup, 10) << u8"（每条边平均 "
       << pad_left_text(one_decimal(list_lookup, source.size()), 4) << u8" 个）\n";
    os << "    " << pad_right(u8"邻接矩阵（位图）", 20) << pad_left(matrix_lookup, 10)
       << u8"（每条边 1 个：算下标、取一位）\n";
    os << "    " << pad_right(u8"边表", 20) << pad_left(edge_lookup, 10) << u8"（每条边都要扫全表）\n";

    stats().reset();
    count_all_neighbor_visits(list);
    const std::size_t list_scan = stats().probes;

    stats().reset();
    count_all_neighbor_visits(matrix);
    const std::size_t matrix_scan = stats().probes;

    stats().reset();
    count_all_neighbor_visits(edges);
    const std::size_t edge_scan = stats().probes;

    os << u8"  取所有点的邻居（" << kSparseVertices << u8" 个点各一次），检查过的位置数：\n";
    os << "    " << pad_right(u8"邻接表", 20) << pad_left(list_scan, 10) << u8"（等于 2 倍边数 = "
       << (2 * kSparseEdges) << u8"）\n";
    os << "    " << pad_right(u8"邻接矩阵（位图）", 20) << pad_left(matrix_scan, 10) << u8"（等于 点数² = "
       << (static_cast<std::size_t>(kSparseVertices) * kSparseVertices) << u8"）\n";
    os << "    " << pad_right(u8"边表", 20) << pad_left(edge_scan, 10) << u8"（等于 点数 × 2 倍边数 = "
       << (static_cast<std::size_t>(kSparseVertices) * 2 * kSparseEdges) << u8"）\n";
    os << u8"  稠密小图查边用矩阵，稀疏图取邻居用邻接表——这一行就是选择的依据\n";
}

}   /* namespace */

std::string build_report()
{
    std::ostringstream os;
    append_storage_section(os);
    append_same_algorithm_section(os);
    append_directed_section(os);
    append_operation_section(os);
    return os.str();
}

/* ================= 自测 ================= */

CheckResult run_self_tests()
{
    Checker c;
    const std::vector<EdgePair> source = make_edges(kDemoVertices, kDemoEdges, 20261002u);
    GraphTriple triple;
    triple.fill(source);

    /* 1. 三种存法的 has_edge 对全部点对一致 */
    {
        std::size_t mismatches = 0;
        for (int u = 0; u < kDemoVertices; ++u) {
            for (int v = 0; v < kDemoVertices; ++v) {
                const bool a = triple.list.has_edge(u, v);
                const bool b = triple.matrix.has_edge(u, v);
                const bool d = triple.edges.has_edge(u, v);
                if (a != b || a != d) {
                    ++mismatches;
                }
            }
        }
        c.check(mismatches == 0, u8"三种存法的 has_edge 对全部 144 个点对逐位一致");
    }

    /* 2. 三种存法每个点的邻居（升序）一致 */
    {
        bool same = true;
        for (int v = 0; v < kDemoVertices && same; ++v) {
            std::vector<int> a;
            std::vector<int> b;
            std::vector<int> d;
            triple.list.for_each_neighbor(v, [&](int u) { a.push_back(u); });
            triple.matrix.for_each_neighbor(v, [&](int u) { b.push_back(u); });
            triple.edges.for_each_neighbor(v, [&](int u) { d.push_back(u); });
            same = a == b && a == d;
        }
        c.check(same, u8"三种存法每个点的邻居序列逐位一致（都是升序）");
    }

    /* 3. 三种存法的边数一致 */
    {
        c.check(triple.list.edge_count() == kDemoEdges && triple.matrix.edge_count() == kDemoEdges
                    && triple.edges.edge_count() == kDemoEdges,
                u8"三种存法数出来的边数都等于 " + std::to_string(kDemoEdges),
                std::to_string(triple.list.edge_count()) + u8" / "
                    + std::to_string(triple.matrix.edge_count()) + u8" / "
                    + std::to_string(triple.edges.edge_count()));
    }

    /* 4. 无向图对称 */
    {
        std::size_t asymmetric = 0;
        for (int u = 0; u < kDemoVertices; ++u) {
            for (int v = 0; v < kDemoVertices; ++v) {
                if (triple.list.has_edge(u, v) != triple.list.has_edge(v, u)) {
                    ++asymmetric;
                }
            }
        }
        c.check(asymmetric == 0, u8"无向图两个方向都存：has_edge(a,b) 与 has_edge(b,a) 一致");
    }

    /* 5. 有向图不对称 */
    {
        AdjList directed(kDemoVertices);
        for (const EdgePair &edge : source) {
            directed.add_directed(edge.first, edge.second);
        }
        std::size_t asymmetric = 0;
        for (const EdgePair &edge : source) {
            if (directed.has_edge(edge.first, edge.second) && !directed.has_edge(edge.second, edge.first)) {
                ++asymmetric;
            }
        }
        c.check(asymmetric > 0, u8"有向图只存一侧：至少有一条边的反向查不到",
                std::to_string(asymmetric) + u8" 条");
    }

    /* 6. 位图矩阵的定位：跨 uint64 边界的那一格也正确 */
    {
        AdjMatrix matrix(10);
        matrix.add_undirected(6, 4);        /* 下标 64，正好是第二位图的第 0 位 */
        const bool ok = matrix.has_edge(6, 4) && matrix.has_edge(4, 6) && !matrix.has_edge(6, 5)
                        && !matrix.has_edge(5, 4) && !matrix.has_edge(6, 3);
        c.check(ok, u8"位图矩阵跨 uint64 边界的那一格定位正确，相邻位没被污染");
    }

    /* 7. 空图：什么边都没有 */
    {
        AdjList empty_list(5);
        AdjMatrix empty_matrix(5);
        EdgeList empty_edges(5);
        const std::vector<int> bfs = bfs_order(empty_list, 3);
        c.check(empty_list.edge_count() == 0 && empty_matrix.edge_count() == 0
                    && empty_edges.edge_count() == 0 && bfs.size() == 1 && bfs[0] == 3,
                u8"空图：没有边，从 3 号出发只访问到 3 号自己");
    }

    /* 8. 单点图 */
    {
        AdjList single(1);
        const std::vector<int> bfs = bfs_order(single, 0);
        const std::vector<int> label = component_label(single);
        c.check(bfs.size() == 1 && label.size() == 1 && label[0] == 0,
                u8"单点图：BFS 只有它自己，连通块标号是 0");
    }

    /* 9. 不存在的边返回假 */
    {
        std::size_t misses = 0;
        for (int v = 0; v < kDemoVertices; ++v) {
            if (!triple.list.has_edge(v, v) && !triple.matrix.has_edge(v, v) && !triple.edges.has_edge(v, v)) {
                ++misses;
            }
        }
        c.check(misses == kDemoVertices, u8"没有自环：每个点查自己都返回假");
    }

    /* 10. BFS 与 DFS 在三种存法上一致 */
    {
        const bool bfs_same = bfs_order(triple.list, 0) == bfs_order(triple.matrix, 0)
                              && bfs_order(triple.list, 0) == bfs_order(triple.edges, 0);
        const bool dfs_same = dfs_order(triple.list, 0) == dfs_order(triple.matrix, 0)
                              && dfs_order(triple.list, 0) == dfs_order(triple.edges, 0);
        c.check(bfs_same && dfs_same, u8"BFS 与 DFS 序列在三种存法上逐位相同");
    }

    /* 11. DFS 序列长度等于可达点数 */
    {
        const std::vector<int> order = dfs_order(triple.list, 0);
        std::size_t reachable = 0;
        for (int v = 0; v < kDemoVertices; ++v) {
            if (triple.list.degree(v) > 0 || v == 0) {
                ++reachable;
            }
        }
        c.check(!order.empty() && order.size() <= reachable,
                u8"DFS 只访问可达的点", std::to_string(order.size()) + u8" 个");
    }

    /* 12. 连通块标号：三种存法一致，标号是块里最小的点 */
    {
        const std::vector<int> a = component_label(triple.list);
        const std::vector<int> b = component_label(triple.matrix);
        const std::vector<int> d = component_label(triple.edges);
        bool minimal = true;
        for (int v = 0; v < kDemoVertices; ++v) {
            if (a[static_cast<std::size_t>(v)] > v) {
                minimal = false;
            }
            if (triple.list.has_edge(v, 1) && a[static_cast<std::size_t>(v)] != a[1]) {
                minimal = false;        /* 相邻的点标号必须相同 */
            }
        }
        c.check(a == b && a == d && minimal, u8"连通块标号：三种存法一致，标号取块里最小的点");
    }

    /* 13. 孤立点自成一个连通块 */
    {
        AdjList graph(4);
        graph.add_undirected(0, 1);
        const std::vector<int> label = component_label(graph);
        c.check(label[0] == 0 && label[1] == 0 && label[2] == 2 && label[3] == 3,
                u8"2 号与 3 号是孤立点，各自成一个连通块");
    }

    /* 14. 拓扑排序：每个点的前驱都排在它前面 */
    {
        AdjList dag(6);
        const EdgePair edges[] = { {0, 1}, {0, 2}, {1, 3}, {2, 3}, {3, 4}, {1, 5} };
        for (const EdgePair &edge : edges) {
            dag.add_directed(edge.first, edge.second);
        }
        const std::vector<int> order = topological_order(dag);
        std::vector<std::size_t> position(6, 0);
        for (std::size_t i = 0; i < order.size(); ++i) {
            position[static_cast<std::size_t>(order[i])] = i;
        }
        bool ok = order.size() == 6;
        for (const EdgePair &edge : edges) {
            ok = ok && position[static_cast<std::size_t>(edge.first)]
                           < position[static_cast<std::size_t>(edge.second)];
        }
        c.check(ok, u8"拓扑排序：长度等于点数，每条边的起点都排在终点前面", join_ints(order));
    }

    /* 15. 有环的有向图返回空表 */
    {
        AdjList cycle(3);
        cycle.add_directed(0, 1);
        cycle.add_directed(1, 2);
        cycle.add_directed(2, 0);
        c.check(topological_order(cycle).empty(), u8"有环的有向图：拓扑排序返回空表");
    }

    /* 16. 邻居访问次数在三种存法上相同，且等于 2 倍边数 */
    {
        const std::size_t a = count_all_neighbor_visits(triple.list);
        const std::size_t b = count_all_neighbor_visits(triple.matrix);
        const std::size_t d = count_all_neighbor_visits(triple.edges);
        c.check(a == b && a == d && a == 2 * kDemoEdges,
                u8"三种存法的邻居访问次数相同，都等于 2 倍边数",
                std::to_string(a) + u8" 次");
    }

    /* 17. 度数之和等于 2 倍边数 */
    {
        c.check(degree_sum(triple.list) == 2 * kDemoEdges
                    && degree_sum(triple.matrix) == 2 * kDemoEdges
                    && degree_sum(triple.edges) == 2 * kDemoEdges,
                u8"度数之和等于 2 倍边数：" + std::to_string(2 * kDemoEdges),
                std::to_string(degree_sum(triple.list)));
    }

    /* 18. 矩阵的字节数就是位图字节数 */
    {
        AdjMatrix matrix(100);
        const std::size_t expected = (100u * 100u / 64u + 1u) * sizeof(std::uint64_t);
        c.check(matrix.memory_bytes() == expected,
                u8"邻接矩阵的字节数等于位图字节数，与边数无关",
                std::to_string(matrix.memory_bytes()) + u8" 字节");
    }

    /* 19. 邻接表在有向图上省下约一半的边 */
    {
        AdjList undirected(kDemoVertices);
        AdjList directed(kDemoVertices);
        for (const EdgePair &edge : source) {
            undirected.add_undirected(edge.first, edge.second);
            directed.add_directed(edge.first, edge.second);
        }
        c.check(directed.memory_bytes() < undirected.memory_bytes()
                    && count_all_neighbor_visits(directed) == kDemoEdges
                    && count_all_neighbor_visits(undirected) == 2 * kDemoEdges,
                u8"有向只存一侧：字节更少、邻居访问次数减半",
                std::to_string(directed.memory_bytes()) + u8" 对 "
                    + std::to_string(undirected.memory_bytes()));
    }

    return c.take();
}

}   /* namespace gmini */
