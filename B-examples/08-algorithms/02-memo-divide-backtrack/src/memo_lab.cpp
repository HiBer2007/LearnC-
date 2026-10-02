/**
 * memo_lab.cpp —— 记忆化、分治与回溯
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

#include "memo_lab.hpp"

#include <algorithm>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

namespace mdlab {

/* ================= 第一部分：同一题的三种写法 ================= */

namespace {

long long grid_naive_impl(int m, int n, GridRun *run)
{
    ++run->calls;
    if (m == 0 || n == 0) {
        return 1;                     /* 基线：只剩一条直线可走 */
    }
    return grid_naive_impl(m - 1, n, run) + grid_naive_impl(m, n - 1, run);
}

/** 表里存 -1 表示这个子问题还没算过；算过的子问题第二次进来直接取。 */
long long grid_memo_impl(int m, int n, int width, std::vector<long long> *table, GridRun *run)
{
    ++run->calls;
    const std::size_t index =
        static_cast<std::size_t>(m) * static_cast<std::size_t>(width) +
        static_cast<std::size_t>(n);
    long long &slot = (*table)[index];
    if (slot >= 0) {
        ++run->hits;
        return slot;
    }
    ++run->computes;
    if (m == 0 || n == 0) {
        slot = 1;
        return slot;
    }
    slot = grid_memo_impl(m - 1, n, width, table, run) + grid_memo_impl(m, n - 1, width, table, run);
    return slot;
}

std::size_t grid_index(int i, int j, int width)
{
    return static_cast<std::size_t>(i) * static_cast<std::size_t>(width) +
           static_cast<std::size_t>(j);
}

long long lcs_naive_impl(const std::string &a, const std::string &b, std::size_t i,
                         std::size_t j, LcsRun *run)
{
    ++run->calls;
    if (i == a.size() || j == b.size()) {
        return 0;                     /* 基线：有一边已经走完 */
    }
    ++run->comparisons;
    if (a[i] == b[j]) {
        return 1 + lcs_naive_impl(a, b, i + 1, j + 1, run);
    }
    const long long skip_a = lcs_naive_impl(a, b, i + 1, j, run);
    const long long skip_b = lcs_naive_impl(a, b, i, j + 1, run);
    return skip_a > skip_b ? skip_a : skip_b;
}

long long lcs_memo_impl(const std::string &a, const std::string &b, std::size_t i, std::size_t j,
                        std::size_t width, std::vector<long long> *table, LcsRun *run)
{
    ++run->calls;
    long long &slot = (*table)[i * width + j];
    if (slot >= 0) {
        ++run->hits;
        return slot;
    }
    ++run->computes;
    if (i == a.size() || j == b.size()) {
        slot = 0;
        return slot;
    }
    ++run->comparisons;
    if (a[i] == b[j]) {
        slot = 1 + lcs_memo_impl(a, b, i + 1, j + 1, width, table, run);
        return slot;
    }
    const long long skip_a = lcs_memo_impl(a, b, i + 1, j, width, table, run);
    const long long skip_b = lcs_memo_impl(a, b, i, j + 1, width, table, run);
    slot = skip_a > skip_b ? skip_a : skip_b;
    return slot;
}

}   /* namespace */

GridRun grid_naive(int m, int n)
{
    GridRun run;
    if (m < 0) {
        m = 0;
    }
    if (n < 0) {
        n = 0;
    }
    run.value = grid_naive_impl(m, n, &run);
    return run;
}

GridRun grid_memo(int m, int n)
{
    GridRun run;
    if (m < 0) {
        m = 0;
    }
    if (n < 0) {
        n = 0;
    }
    const int width = n + 1;
    run.cells = static_cast<std::size_t>(m + 1) * static_cast<std::size_t>(width);
    std::vector<long long> table(run.cells, -1);
    run.value = grid_memo_impl(m, n, width, &table, &run);
    return run;
}

GridRun grid_bottom_up(int m, int n)
{
    GridRun run;
    if (m < 0) {
        m = 0;
    }
    if (n < 0) {
        n = 0;
    }
    const int width = n + 1;
    run.cells = static_cast<std::size_t>(m + 1) * static_cast<std::size_t>(width);
    std::vector<long long> table(run.cells, 1);   /* 第一行与第一列全是 1 */
    for (int i = 1; i <= m; ++i) {
        for (int j = 1; j <= n; ++j) {
            table[grid_index(i, j, width)] =
                table[grid_index(i - 1, j, width)] + table[grid_index(i, j - 1, width)];
            ++run.additions;
        }
    }
    run.value = table[grid_index(m, n, width)];
    return run;
}

long long binomial(int n, int k)
{
    if (n < 0 || k < 0 || k > n) {
        return 0;
    }
    if (k > n - k) {
        k = n - k;
    }
    long long result = 1;
    for (int i = 1; i <= k; ++i) {
        result = result * (n - k + i) / i;   /* 每一步都能整除，不丢精度 */
    }
    return result;
}

LcsRun lcs_naive(const std::string &a, const std::string &b)
{
    LcsRun run;
    run.value = lcs_naive_impl(a, b, 0, 0, &run);
    return run;
}

LcsRun lcs_memo(const std::string &a, const std::string &b)
{
    LcsRun run;
    const std::size_t width = b.size() + 1;
    run.cells = (a.size() + 1) * (b.size() + 1);
    std::vector<long long> table(run.cells, -1);
    run.value = lcs_memo_impl(a, b, 0, 0, width, &table, &run);
    return run;
}

LcsRun lcs_bottom_up(const std::string &a, const std::string &b)
{
    LcsRun run;
    const std::size_t la = a.size();
    const std::size_t lb = b.size();
    const std::size_t width = lb + 1;
    run.cells = (la + 1) * (lb + 1);
    std::vector<long long> table(run.cells, 0);
    /* 从右下角倒着填：每个格子只依赖它右边、下边与右下的格子 */
    for (std::size_t i = la; i-- > 0;) {
        for (std::size_t j = lb; j-- > 0;) {
            ++run.comparisons;
            if (a[i] == b[j]) {
                table[i * width + j] = table[(i + 1) * width + (j + 1)] + 1;
            } else {
                const long long up = table[(i + 1) * width + j];
                const long long left = table[i * width + (j + 1)];
                table[i * width + j] = up > left ? up : left;
            }
        }
    }
    run.value = table[0];
    return run;
}

/* ================= 第二部分：分治的合并代价 ================= */

namespace {

/** 把三段已经各自有序的区间归并到 buf，再整体拷回 a。
 *
 *  三段的区间是 [b1, e1)、[b2, e2)、[b3, e3)，二路归并时第三段为空。
 *  每输出一个元素，要在各段当前的候选里挑最小：段数越多，一次挑选要比较的次数越多，
 *  这就是三分「层数少了、每层却更贵」的来路。
 *
 *  逆序对在挑选的同一处数：从后面的段里取出一个元素时，
 *  前面各段还没轮到的元素全都比它大，个数就是剩下的长度。 */
void merge_three(std::vector<int> &a, std::vector<int> &buf, std::size_t b1, std::size_t e1,
                 std::size_t b2, std::size_t e2, std::size_t b3, std::size_t e3,
                 std::size_t out_begin, SortStats *st)
{
    std::size_t i1 = b1;
    std::size_t i2 = b2;
    std::size_t i3 = b3;
    std::size_t out = out_begin;

    while (i1 < e1 || i2 < e2 || i3 < e3) {
        std::size_t best = 0;
        int seg = 0;
        if (i1 < e1) {
            best = i1;
            seg = 1;
        }
        if (i2 < e2) {
            if (seg == 0) {
                best = i2;
                seg = 2;
            } else {
                ++st->comparisons;
                if (a[i2] < a[best]) {
                    best = i2;
                    seg = 2;
                }
            }
        }
        if (i3 < e3) {
            if (seg == 0) {
                best = i3;
                seg = 3;
            } else {
                ++st->comparisons;
                if (a[i3] < a[best]) {
                    best = i3;
                    seg = 3;
                }
            }
        }
        if (seg >= 2) {
            st->inversions += static_cast<long long>(e1 - i1);
        }
        if (seg == 3) {
            st->inversions += static_cast<long long>(e2 - i2);
        }
        buf[out] = a[best];
        ++out;
        ++st->moves;
        if (seg == 1) {
            ++i1;
        } else if (seg == 2) {
            ++i2;
        } else {
            ++i3;
        }
    }
    for (std::size_t k = out_begin; k < out_begin + (e1 - b1) + (e2 - b2) + (e3 - b3); ++k) {
        a[k] = buf[k];
        ++st->moves;
    }
    ++st->merges;
}

void sort_rec(std::vector<int> &a, std::vector<int> &buf, std::size_t begin, std::size_t end,
              int parts, SortStats *st, std::size_t depth)
{
    if (depth > st->levels) {
        st->levels = depth;
    }
    const std::size_t len = end - begin;
    if (len < 2) {
        return;
    }
    if (parts == 2) {
        const std::size_t mid = begin + len / 2;
        sort_rec(a, buf, begin, mid, parts, st, depth + 1);
        sort_rec(a, buf, mid, end, parts, st, depth + 1);
        merge_three(a, buf, begin, mid, mid, end, end, end, begin, st);
        return;
    }
    /* 三分：切成 [begin, cut1)、[cut1, cut2)、[cut2, end) 三段。
       长度 2 时 cut1 与 begin 重合，第一段为空，归并照样能走完。 */
    const std::size_t cut1 = begin + len / 3;
    const std::size_t cut2 = begin + (2 * len) / 3;
    sort_rec(a, buf, begin, cut1, parts, st, depth + 1);
    sort_rec(a, buf, cut1, cut2, parts, st, depth + 1);
    sort_rec(a, buf, cut2, end, parts, st, depth + 1);
    merge_three(a, buf, begin, cut1, cut1, cut2, cut2, end, begin, st);
}

}   /* namespace */

SortStats merge_sort_counted(const std::vector<int> &data, SplitWay way)
{
    SortStats st;
    st.sorted = data;
    std::vector<int> buf(data.size());
    sort_rec(st.sorted, buf, 0, st.sorted.size(), way == SplitWay::Two ? 2 : 3, &st, 0);
    return st;
}

long long inversions_brute(const std::vector<int> &data)
{
    long long count = 0;
    for (std::size_t i = 0; i < data.size(); ++i) {
        for (std::size_t j = i + 1; j < data.size(); ++j) {
            if (data[i] > data[j]) {
                ++count;
            }
        }
    }
    return count;
}

std::vector<int> make_sequence(std::size_t length, unsigned int seed)
{
    std::vector<int> out;
    out.reserve(length);
    unsigned int state = seed;
    for (std::size_t i = 0; i < length; ++i) {
        state = state * 1103515245u + 12345u;   /* 无符号回绕是定义好的 */
        out.push_back(static_cast<int>((state >> 8) & 0xFFFFu));
    }
    return out;
}

/* ================= 第三部分：回溯的选择与撤销 ================= */

namespace {

struct QueenBoard {
    std::vector<char> column;      /* 这一列有没有皇后 */
    std::vector<char> diag_down;   /* 主对角线：row + col */
    std::vector<char> diag_up;     /* 副对角线：row - col + n - 1 */
};

void queens_dfs(int n, int row, QueenPrune prune, QueenBoard *board, QueenRun *run, int depth)
{
    ++run->nodes;
    if (static_cast<std::size_t>(depth) > run->peak_depth) {
        run->peak_depth = static_cast<std::size_t>(depth);
    }
    if (row == n) {
        ++run->solutions;              /* 每一行都放好了 */
        return;
    }
    for (int col = 0; col < n; ++col) {
        ++run->candidates;
        bool allowed = true;
        if (prune != QueenPrune::None) {
            allowed = (board->column[static_cast<std::size_t>(col)] == 0);
        }
        if (allowed && prune == QueenPrune::Full) {
            const std::size_t down = static_cast<std::size_t>(row + col);
            const std::size_t up = static_cast<std::size_t>(row - col + n - 1);
            allowed = (board->diag_down[down] == 0) && (board->diag_up[up] == 0);
        }
        if (!allowed) {
            ++run->rejected;           /* 这个候选被剪掉 */
            continue;
        }

        /* 选择：把皇后放上去，标好它占住的列与两条对角线 */
        const std::size_t down = static_cast<std::size_t>(row + col);
        const std::size_t up = static_cast<std::size_t>(row - col + n - 1);
        board->column[static_cast<std::size_t>(col)] = 1;
        board->diag_down[down] = 1;
        board->diag_up[up] = 1;
        ++run->placed;

        queens_dfs(n, row + 1, prune, board, run, depth + 1);

        /* 撤销：原样拿回来，棋盘交还给上一层 */
        board->column[static_cast<std::size_t>(col)] = 0;
        board->diag_down[down] = 0;
        board->diag_up[up] = 0;
        ++run->undone;
    }
}

int queen_n_signature(int n)
{
    return n < 0 ? 0 : n;
}

}   /* namespace */

QueenRun queens(int n, QueenPrune prune)
{
    QueenRun run;
    n = queen_n_signature(n);
    if (n == 0) {
        ++run.solutions;               /* 空棋盘算一种摆法：什么都不放 */
        ++run.nodes;
        return run;
    }
    QueenBoard board;
    board.column.assign(static_cast<std::size_t>(n), 0);
    board.diag_down.assign(static_cast<std::size_t>(2 * n - 1), 0);
    board.diag_up.assign(static_cast<std::size_t>(2 * n - 1), 0);
    queens_dfs(n, 0, prune, &board, &run, 0);
    return run;
}

unsigned long long permutation_tree_nodes(int n)
{
    if (n < 0) {
        return 0;
    }
    unsigned long long total = 1;      /* k = 0：空棋盘一个节点 */
    unsigned long long term = 1;       /* term = n!/(n-k)! */
    for (int k = 1; k <= n; ++k) {
        term *= static_cast<unsigned long long>(n - k + 1);
        total += term;
    }
    return total;
}

unsigned long long free_tree_nodes(int n)
{
    if (n < 0) {
        return 0;
    }
    unsigned long long total = 0;
    unsigned long long term = 1;       /* term = n^k */
    for (int k = 0; k <= n; ++k) {
        total += term;
        term *= static_cast<unsigned long long>(n);
    }
    return total;
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

/** 有符号、无符号、各种宽度的整数都从这里转成十进制文本 */
template <typename T>
std::string num(T value)
{
    return std::to_string(value);
}

std::string one_decimal(double value)
{
    std::ostringstream os;
    os << std::fixed << std::setprecision(1) << value;
    return os.str();
}

void append_three_ways(std::ostringstream &os)
{
    os << u8"一、同一题的三种写法：网格路径数\n";
    os << u8"  递推式  paths(m, n) = (m == 0 或 n == 0) ? 1 : paths(m - 1, n) + paths(m, n - 1)\n";
    os << "  " << pad_right(u8"规模", 10) << pad_right(u8"答案", 12) << pad_right(u8"朴素递归节点数", 16)
       << pad_right(u8"记忆化实算次数", 16) << pad_right(u8"记忆化调用次数", 16)
       << pad_right(u8"命中缓存", 10) << pad_right(u8"表规模", 8) << u8"递推加法次数\n";

    const int sizes[][2] = {{4, 4}, {6, 6}, {8, 8}, {10, 10}, {12, 12}};
    GridRun big_naive;
    GridRun big_memo;
    GridRun big_loop;
    for (const auto &size : sizes) {
        const GridRun naive = grid_naive(size[0], size[1]);
        const GridRun memo = grid_memo(size[0], size[1]);
        const GridRun loop = grid_bottom_up(size[0], size[1]);
        os << "  " << pad_right(num(size[0]) + "x" + num(size[1]), 10) << pad_right(num(naive.value), 12)
           << pad_right(num(naive.calls), 16) << pad_right(num(memo.computes), 16)
           << pad_right(num(memo.calls), 16) << pad_right(num(memo.hits), 10)
           << pad_right(num(memo.cells), 8) << num(loop.additions) << "\n";
        if (size[0] == 12) {
            big_naive = naive;
            big_memo = memo;
            big_loop = loop;
        }
    }
    os << u8"  朴素递归的节点数 = 2 × 答案 − 1：每个内部节点恰好两个分支，叶子就是命中基线的那些调用\n";
    os << u8"  记忆化的实算次数就是不同子问题的个数：同一个子问题只算第一次，之后都从表里取\n";
    os << u8"  12x12：朴素递归 " << big_naive.calls << u8" 次调用，记忆化只算 " << big_memo.computes
       << u8" 个子问题，相差 " << one_decimal(static_cast<double>(big_naive.calls) /
                                                 static_cast<double>(big_memo.computes))
       << u8" 倍\n";
    os << u8"  同一道题的三种写法（12x12）：\n";
    os << "  " << pad_right(u8"写法", 20) << pad_right(u8"函数调用次数", 16) << pad_right(u8"表内加法次数", 16)
       << u8"表的格子数\n";
    os << "  " << pad_right(u8"朴素递归", 20) << pad_right(num(big_naive.calls), 16)
       << pad_right(u8"0", 16) << u8"0\n";
    os << "  " << pad_right(u8"记忆化（自顶向下）", 20) << pad_right(num(big_memo.calls), 16)
       << pad_right(num(big_loop.additions), 16) << num(big_memo.cells) << "\n";
    os << "  " << pad_right(u8"自底向上（递推）", 20) << pad_right(u8"0", 16)
       << pad_right(num(big_loop.additions), 16) << num(big_loop.cells) << "\n";
    os << u8"  朴素递归那一行的「表的格子数」是 0，它不存中间结果\n";
    os << u8"  自底向上一行的调用次数是 0，它没有函数调用\n";
    os << u8"  表里有一个格子从没被问过：两个下标同时为 0 的那个，走到边界就返回了\n";
    os << "\n";

    os << u8"  第二个例子：最长公共子序列，同一个模式换一道题\n";
    os << u8"  递推式  lcs(i, j) = a[i] == b[j] ? 1 + lcs(i + 1, j + 1) : max(lcs(i + 1, j), lcs(i, j + 1))\n";
    const std::string pair_a[] = {"ABCBDAB", "AGGTAB", "ACGTACGTACGT"};
    const std::string pair_b[] = {"BDCABA", "GXTXAYB", "TGCATGCATGCA"};
    os << "  " << pad_right(u8"两个串", 32) << pad_right(u8"答案", 8) << pad_right(u8"朴素递归调用次数", 18)
       << pad_right(u8"记忆化实算次数", 16) << pad_right(u8"记忆化调用次数", 16)
       << pad_right(u8"命中缓存", 10) << u8"递推比较次数\n";
    for (int k = 0; k < 3; ++k) {
        const std::string &a = pair_a[k];
        const std::string &b = pair_b[k];
        const LcsRun naive = lcs_naive(a, b);
        const LcsRun memo = lcs_memo(a, b);
        const LcsRun loop = lcs_bottom_up(a, b);
        os << "  " << pad_right(a + " / " + b, 32) << pad_right(num(naive.value), 8)
           << pad_right(num(naive.calls), 18) << pad_right(num(memo.computes), 16)
           << pad_right(num(memo.calls), 16) << pad_right(num(memo.hits), 10)
           << num(loop.comparisons) << "\n";
    }
    os << u8"  三种写法给出的答案逐位相同；记忆化的实算次数与串的内容有关，遇到相同字符时这一步斜着走\n";
    os << u8"  实算次数不超过「表规模 − 1」：斜着走会跳过一批子问题，跳过的一次都不用算\n";
    os << u8"  递推版从右下角倒着填，比较次数 = 两个串长之积，每个格子恰好比一次\n";
}

void append_merge_cost(std::ostringstream &os)
{
    os << u8"二、分治的合并代价：归并排序与逆序对（二分与三分对照）\n";
    os << u8"  合并时每一段各出一个候选，挑最小的那一个；段数越多，一次挑选要比较的次数越多\n";
    os << "  " << pad_right(u8"规模", 8) << pad_right(u8"分法", 8) << pad_right(u8"递归层数", 10)
       << pad_right(u8"合并次数", 10) << pad_right(u8"合并比较次数", 14) << pad_right(u8"合并搬移次数", 14)
       << u8"逆序对数\n";

    const std::size_t sizes[] = {256, 1024, 4096};
    SortStats first_two;
    SortStats last_two;
    SortStats last_three;
    for (std::size_t i = 0; i < 3; ++i) {
        const std::size_t n = sizes[i];
        const std::vector<int> data = make_sequence(n, 20261002u);
        const SortStats two = merge_sort_counted(data, SplitWay::Two);
        const SortStats three = merge_sort_counted(data, SplitWay::Three);
        os << "  " << pad_right(num(n), 8) << pad_right(u8"二分", 8) << pad_right(num(two.levels), 10)
           << pad_right(num(two.merges), 10) << pad_right(num(two.comparisons), 14)
           << pad_right(num(two.moves), 14) << num(two.inversions) << "\n";
        os << "  " << pad_right(num(n), 8) << pad_right(u8"三分", 8) << pad_right(num(three.levels), 10)
           << pad_right(num(three.merges), 10) << pad_right(num(three.comparisons), 14)
           << pad_right(num(three.moves), 14) << num(three.inversions) << "\n";
        if (i == 0) {
            first_two = two;
        }
        last_two = two;
        last_three = three;
    }
    os << u8"  两种分法把同一份数据排成同一个序列，逆序对数也相同：分法不改变答案，只改变代价\n";
    os << u8"  n = 4096：二分 " << last_two.comparisons << u8" 次比较、" << last_two.moves
       << u8" 次搬移；三分 " << last_three.comparisons << u8" 次比较、" << last_three.moves
       << u8" 次搬移\n";
    os << u8"  三分的递归层数从 " << last_two.levels << u8" 降到 " << last_three.levels
       << u8"，元素参与归并的轮数少了，搬移从 " << last_two.moves << u8" 降到 "
       << last_three.moves << u8"，少了 " << (last_two.moves - last_three.moves) << u8" 次\n";
    os << u8"  但三路归并每输出一个元素要在三个候选里挑，比较次数反而多了 "
       << (last_three.comparisons - last_two.comparisons) << u8" 次，涨到二分的 "
       << one_decimal(static_cast<double>(last_three.comparisons) * 100.0 /
                      static_cast<double>(last_two.comparisons))
       << u8"%\n";
    os << u8"  规模从 " << sizes[0] << u8" 涨到 " << sizes[2] << u8"（" << (sizes[2] / sizes[0])
       << u8" 倍），二分的比较次数从 " << first_two.comparisons << u8" 涨到 "
       << last_two.comparisons << u8"，是 "
       << one_decimal(static_cast<double>(last_two.comparisons) /
                      static_cast<double>(first_two.comparisons))
       << u8" 倍\n";
    os << u8"  层数只从 " << first_two.levels << u8" 涨到 " << last_two.levels
       << u8"，多出来的那一份就是它：比较次数的增长比规模本身快一点，快的就是层数\n";
    os << u8"  合并代价决定分治划不划算：层数省下的搬移抵不过每次合并多出的比较，三分在这一档不占优\n";
    os << u8"  段数再往上加，每层省下的深度越来越少，一次挑选却要继续变贵，代价就往比较那一头倒\n";
    os << "\n";
}

void append_backtracking(std::ostringstream &os)
{
    os << u8"三、回溯的选择与撤销：N 皇后\n";
    os << u8"  一行一行往下放，放下之前先看这一列、两条对角线是否被占；放下去算选择，返回时拿掉算撤销\n";
    os << "  " << pad_right("n", 6) << pad_right(u8"解数", 10) << pad_right(u8"访问节点数", 14)
       << pad_right(u8"剪掉的候选数", 14) << pad_right(u8"放置次数", 10) << pad_right(u8"撤销次数", 10)
       << u8"峰值深度\n";
    for (int n = 4; n <= 10; ++n) {
        const QueenRun run = queens(n, QueenPrune::Full);
        os << "  " << pad_right(num(n), 6) << pad_right(num(run.solutions), 10)
           << pad_right(num(run.nodes), 14) << pad_right(num(run.rejected), 14)
           << pad_right(num(run.placed), 10) << pad_right(num(run.undone), 10) << num(run.peak_depth)
           << "\n";
    }
    os << u8"  放置次数 = 撤销次数 = 访问节点数 − 1：每一次选择都在这一层返回时原样撤销\n";
    os << u8"  候选总数 = 放置次数 + 剪掉的候选数：试过的位置要么被放下去，要么被剪掉，没有第三种去向\n";
    os << "\n";

    const int n = 6;
    const QueenRun none = queens(n, QueenPrune::None);
    const QueenRun column = queens(n, QueenPrune::ColumnOnly);
    const QueenRun full = queens(n, QueenPrune::Full);
    os << u8"  剪枝的前后对照（n = 6）\n";
    os << "  " << pad_right(u8"剪枝策略", 22) << pad_right(u8"访问节点数", 14) << pad_right(u8"叶子数", 10)
       << u8"公式\n";
    os << "  " << pad_right(u8"完全不剪枝", 22) << pad_right(num(none.nodes), 14)
       << pad_right(num(none.solutions), 10) << u8"Σ n^k = " << free_tree_nodes(n) << "\n";
    os << "  " << pad_right(u8"只要求列不重复", 22) << pad_right(num(column.nodes), 14)
       << pad_right(num(column.solutions), 10) << u8"Σ n!/(n−k)! = " << permutation_tree_nodes(n)
       << "\n";
    os << "  " << pad_right(u8"列加两条对角线", 22) << pad_right(num(full.nodes), 14)
       << pad_right(num(full.solutions), 10) << u8"没有闭式，只能搜\n";
    os << u8"  只剪列的那一档，叶子数就是 n! = " << column.solutions
       << u8"：每个排列都走到了底，只是绝大多数不满足对角线\n";
    os << u8"  加上对角线之后节点数从 " << column.nodes << u8" 降到 " << full.nodes << u8"，只剩 "
       << one_decimal(static_cast<double>(full.nodes) * 100.0 / static_cast<double>(column.nodes))
       << u8"%\n";
    os << u8"  剪枝不改变解的个数，只跳过注定走不通的分支：对角线检查一次，省下的是整棵子树\n";
    os << u8"  完全不剪枝那一档的候选一次都没被拒：没有判据，每一行 n 个位置全部放下去\n";
    os << u8"  排列树节点数的来路：第 k 层有 n!/(n−k)! 个节点，从 k = 0 加到 k = n\n";
}

}   /* namespace */

std::string build_report()
{
    std::ostringstream os;
    append_three_ways(os);
    append_merge_cost(os);
    append_backtracking(os);
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

    void expect_eq_size(std::size_t got, std::size_t want, const std::string &what)
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

    /* 1 三种写法在同一个网格上给出同一个答案 */
    bool grid_same = true;
    for (int m = 0; m <= 8 && grid_same; ++m) {
        for (int n = 0; n <= 8; ++n) {
            const long long want = grid_bottom_up(m, n).value;
            if (grid_naive(m, n).value != want || grid_memo(m, n).value != want) {
                grid_same = false;
                break;
            }
        }
    }
    checks.expect(grid_same, u8"网格路径数：0 ≤ m, n ≤ 8 上朴素、记忆化、自底向上三种写法答案相同");

    /* 2 答案等于组合数 C(m+n, m)，用独立实现对照 */
    bool grid_binom = true;
    for (int m = 0; m <= 9; ++m) {
        for (int n = 0; n <= 9; ++n) {
            if (grid_bottom_up(m, n).value != binomial(m + n, m)) {
                grid_binom = false;
            }
        }
    }
    checks.expect(grid_binom, u8"网格路径数等于 C(m + n, m)：0 ≤ m, n ≤ 9 上逐位相同");

    /* 3 朴素递归的节点数 = 2 × 答案 − 1 */
    bool naive_rule = true;
    for (int m = 0; m <= 8; ++m) {
        for (int n = 0; n <= 8; ++n) {
            const GridRun naive = grid_naive(m, n);
            if (naive.calls != static_cast<std::size_t>(2 * naive.value - 1)) {
                naive_rule = false;
            }
        }
    }
    checks.expect(naive_rule, u8"朴素递归的节点数 = 2 × 答案 − 1，在 0 ≤ m, n ≤ 8 上成立");

    /* 4 记忆化的实算次数 = 不同子问题个数 */
    bool memo_rule = true;
    for (int m = 1; m <= 8; ++m) {
        for (int n = 1; n <= 8; ++n) {
            const GridRun memo = grid_memo(m, n);
            if (memo.computes != static_cast<std::size_t>(m + 1) * static_cast<std::size_t>(n + 1) - 1) {
                memo_rule = false;
            }
            if (memo.calls != memo.computes + memo.hits) {
                memo_rule = false;
            }
        }
    }
    checks.expect(memo_rule, u8"记忆化的实算次数 = (m + 1)(n + 1) − 1，且调用次数 = 实算次数 + 命中缓存");

    /* 5 12x12：朴素与记忆化的差距，以及自底向上的表规模 */
    const GridRun big_naive = grid_naive(12, 12);
    const GridRun big_memo = grid_memo(12, 12);
    const GridRun big_loop = grid_bottom_up(12, 12);
    checks.expect(big_naive.calls == 5408311 && big_memo.computes == 168 && big_memo.calls == 289 &&
                      big_memo.hits == 121,
                  u8"12x12：朴素 5408311 次调用，记忆化实算 168 次、共 289 次调用、命中 121 次");

    /* 6 自底向上：格子数 = (m+1)(n+1)，加法次数 = m × n */
    checks.expect(big_loop.cells == 169 && big_loop.additions == 144 &&
                      big_loop.calls == 0 && big_loop.value == 2704156,
                  u8"12x12 自底向上：169 个格子、144 次加法、0 次函数调用，答案 2704156");

    /* 7 最长公共子序列三种写法一致 */
    const std::string texts_a[] = {"ABCBDAB", "AGGTAB", "ACGTACGTACGT", ""};
    const std::string texts_b[] = {"BDCABA", "GXTXAYB", "TGCATGCATGCA", "ACGT"};
    bool lcs_same = true;
    for (int k = 0; k < 4; ++k) {
        const long long want = lcs_bottom_up(texts_a[k], texts_b[k]).value;
        if (lcs_naive(texts_a[k], texts_b[k]).value != want ||
            lcs_memo(texts_a[k], texts_b[k]).value != want) {
            lcs_same = false;
        }
    }
    checks.expect(lcs_same, u8"最长公共子序列：三种写法在四组输入上答案相同");

    /* 8 手算的答案与退化的空串 */
    checks.expect(lcs_bottom_up("ABCBDAB", "BDCABA").value == 4 &&
                      lcs_bottom_up("AGGTAB", "GXTXAYB").value == 4 &&
                      lcs_bottom_up("", "ACGT").value == 0 &&
                      lcs_bottom_up("", "").value == 0,
                  u8"最长公共子序列：两组长度的答案是 4，空串参与时是 0");

    /* 9 递推版的比较次数 = 两个串长之积；记忆化的实算次数由串的内容决定 */
    const LcsRun lcs_loop = lcs_bottom_up("ACGTACGTACGT", "TGCATGCATGCA");
    const LcsRun lcs_memo_run = lcs_memo("ACGTACGTACGT", "TGCATGCATGCA");
    checks.expect_eq_size(lcs_loop.comparisons, 12u * 12u, u8"递推版比较次数 = 两个串长之积");
    checks.expect(lcs_memo_run.computes == 103 && lcs_memo_run.hits == 46 && lcs_memo_run.calls == 149,
                  u8"这一对 12 长的串：记忆化实算 103 个子问题、命中 46 次、共 149 次调用");

    /* 10 记忆化的账目：调用次数 = 实算次数 + 命中缓存，实算次数不超过表规模 − 1 */
    bool lcs_memo_rule = true;
    const std::string many_a[] = {"ABCBDAB", "AGGTAB", "ACGTACGTACGT", "AAAA", "ABCDEFGH"};
    const std::string many_b[] = {"BDCABA", "GXTXAYB", "TGCATGCATGCA", "AAAA", "HGFEDCBA"};
    for (int k = 0; k < 5; ++k) {
        const LcsRun run = lcs_memo(many_a[k], many_b[k]);
        if (run.calls != run.computes + run.hits) {
            lcs_memo_rule = false;
        }
        if (run.computes + 1 > run.cells) {
            lcs_memo_rule = false;
        }
    }
    checks.expect(lcs_memo_rule, u8"记忆化 LCS 的账目对得上：调用次数 = 实算次数 + 命中缓存，实算次数不超过表规模");

    /* 10 记忆化与朴素在调用次数上的差距 */
    const LcsRun lcs_small_naive = lcs_naive("ABCBDAB", "BDCABA");
    checks.expect(lcs_small_naive.calls > 100 && lcs_memo_run.calls < 400,
                  u8"7 × 6 的朴素 LCS 调用上百次，12 × 12 的记忆化 LCS 调用不到 400 次");

    /* 11 两种分法都排好序，且与 std::sort 一致 */
    bool sorted_ok = true;
    for (std::size_t n = 1; n <= 40; ++n) {
        const std::vector<int> data = make_sequence(n, static_cast<unsigned int>(n * 7 + 1));
        std::vector<int> want = data;
        std::sort(want.begin(), want.end());
        if (merge_sort_counted(data, SplitWay::Two).sorted != want ||
            merge_sort_counted(data, SplitWay::Three).sorted != want) {
            sorted_ok = false;
        }
    }
    checks.expect(sorted_ok, u8"二分与三分归并排序在长度 1 到 40 上都与 std::sort 结果逐位相同");

    /* 12 两种分法的逆序对数相同，且等于 O(n²) 对照实现 */
    bool inv_same = true;
    for (std::size_t n = 1; n <= 60; ++n) {
        const std::vector<int> data = make_sequence(n, static_cast<unsigned int>(n * 131 + 5));
        const long long want = inversions_brute(data);
        if (merge_sort_counted(data, SplitWay::Two).inversions != want ||
            merge_sort_counted(data, SplitWay::Three).inversions != want) {
            inv_same = false;
        }
    }
    checks.expect(inv_same, u8"逆序对数：二分、三分与 O(n²) 对照实现在长度 1 到 60 上相同");

    /* 13 逆序对的两个边界 */
    const std::vector<int> empty;
    std::vector<int> reverse_order;
    for (int k = 64; k >= 1; --k) {
        reverse_order.push_back(k);
    }
    checks.expect(inversions_brute(empty) == 0 && merge_sort_counted(empty, SplitWay::Two).inversions == 0,
                  u8"空数组的逆序对数是 0");
    checks.expect(merge_sort_counted(reverse_order, SplitWay::Two).inversions == 64 * 63 / 2 &&
                      merge_sort_counted(reverse_order, SplitWay::Three).inversions == 64 * 63 / 2,
                  u8"完全逆序的 64 个元素：逆序对数 = 64 × 63 ÷ 2 = 2016");

    /* 14 n 是 2 的幂时，二分归并每一层把整个数组搬一遍，并 n − 1 次 */
    const SortStats pow_two = merge_sort_counted(make_sequence(64, 99u), SplitWay::Two);
    checks.expect(pow_two.moves == 2u * 64u * 6u && pow_two.merges == 63 && pow_two.levels == 6,
                  u8"n = 64 的二分归并：6 层、63 次合并、搬移 2 × 64 × 6 = 768 次");

    /* 15 三分：层数更少、搬移更少，比较更多 */
    const SortStats cmp_two = merge_sort_counted(make_sequence(4096, 20261002u), SplitWay::Two);
    const SortStats cmp_three = merge_sort_counted(make_sequence(4096, 20261002u), SplitWay::Three);
    checks.expect(cmp_three.levels < cmp_two.levels && cmp_three.moves < cmp_two.moves &&
                      cmp_three.comparisons > cmp_two.comparisons,
                  u8"4096 个元素：三分层数与搬移都少于二分，比较次数多于二分");

    /* 16 已经有序的输入：二分归并每一层比较半个数组，搬移仍是整个数组 */
    std::vector<int> ascending;
    for (int k = 1; k <= 64; ++k) {
        ascending.push_back(k);
    }
    const SortStats asc = merge_sort_counted(ascending, SplitWay::Two);
    checks.expect(asc.comparisons == 32u * 6u && asc.moves == 2u * 64u * 6u && asc.inversions == 0,
                  u8"已经有序的 64 个元素：比较 192 次、搬移 768 次、逆序对 0");

    /* 17 N 皇后的解数：n = 1 到 9 的已知序列 */
    const std::size_t known[] = {1, 0, 0, 2, 10, 4, 40, 92, 352};
    bool queen_solutions = true;
    for (int n = 1; n <= 9; ++n) {
        if (queens(n, QueenPrune::Full).solutions != known[static_cast<std::size_t>(n - 1)]) {
            queen_solutions = false;
        }
    }
    checks.expect(queen_solutions, u8"N 皇后 n = 1 到 9 的解数是 1、0、0、2、10、4、40、92、352");

    /* 18 选择与撤销配平，节点数 = 放置次数 + 1 */
    bool queen_balance = true;
    for (int n = 1; n <= 9; ++n) {
        for (const QueenPrune prune : {QueenPrune::ColumnOnly, QueenPrune::Full}) {
            const QueenRun run = queens(n, prune);
            if (run.placed != run.undone || run.nodes != run.placed + 1) {
                queen_balance = false;
            }
            if (run.candidates != run.placed + run.rejected) {
                queen_balance = false;
            }
        }
        if (n <= 6) {
            /* 完全不剪枝的节点数是 n 的幂次和，n = 9 时到四亿，故只测到 6 */
            const QueenRun run = queens(n, QueenPrune::None);
            if (run.placed != run.undone || run.nodes != run.placed + 1) {
                queen_balance = false;
            }
            if (run.candidates != run.placed + run.rejected) {
                queen_balance = false;
            }
        }
    }
    checks.expect(queen_balance, u8"N 皇后：放置次数 = 撤销次数，节点数 = 放置次数 + 1，候选数 = 放置 + 剪掉");

    /* 19 只剪列时，节点数 = 排列树公式，叶子数 = n! */
    bool perm_rule = true;
    unsigned long long factorial = 1;
    for (int n = 1; n <= 9; ++n) {
        factorial *= static_cast<unsigned long long>(n);
        const QueenRun run = queens(n, QueenPrune::ColumnOnly);
        if (run.nodes != permutation_tree_nodes(n) || run.solutions != factorial) {
            perm_rule = false;
        }
    }
    checks.expect(perm_rule, u8"只剪列时节点数 = Σ n!/(n−k)!，叶子数 = n!，n = 1 到 9 都成立");

    /* 20 完全不剪枝时，节点数 = Σ n^k */
    bool free_rule = true;
    for (int n = 1; n <= 6; ++n) {
        const QueenRun run = queens(n, QueenPrune::None);
        if (run.nodes != free_tree_nodes(n) || run.rejected != 0) {
            free_rule = false;
        }
    }
    checks.expect(free_rule, u8"完全不剪枝时节点数 = Σ n^k，且一个候选都没被拒，n = 1 到 6 都成立");

    /* 21 剪枝的档位越高，访问的节点越少；对角线剪枝不会多出解 */
    bool prune_order = true;
    for (int n = 1; n <= 8; ++n) {
        const std::size_t column = queens(n, QueenPrune::ColumnOnly).nodes;
        const std::size_t full = queens(n, QueenPrune::Full).nodes;
        if (column < full) {
            prune_order = false;
        }
        if (queens(n, QueenPrune::Full).solutions > queens(n, QueenPrune::ColumnOnly).solutions) {
            prune_order = false;
        }
        if (n <= 6) {
            /* 完全不剪枝那一档的节点数是 n 的幂次和，n = 7 起涨到千万级，故只测到 6 */
            const std::size_t none = queens(n, QueenPrune::None).nodes;
            if (none < column) {
                prune_order = false;
            }
        }
    }
    checks.expect(prune_order, u8"节点数随剪枝档位单调不增，对角线剪枝不会多出解");

    /* 22 固定种子的序列重跑逐位相同 */
    const std::vector<int> seq_a = make_sequence(128, 7u);
    const std::vector<int> seq_b = make_sequence(128, 7u);
    const std::vector<int> seq_c = make_sequence(128, 8u);
    checks.expect(seq_a == seq_b && seq_a != seq_c,
                  u8"固定种子的序列重跑逐位相同，换一个种子就不同");

    return checks.finish();
}

}   /* namespace mdlab */
