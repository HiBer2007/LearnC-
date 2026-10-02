/**
 * memo_lab.hpp —— 记忆化、分治与回溯
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
 * 报告里出现的每个数字都由这里的计数函数产出，重跑逐位相同：
 *
 *   第一部分  同一道题写三遍：朴素递归、记忆化（自顶向下）、自底向上（递推）。
 *             数的是函数进入次数 calls、真正算过的次数 computes、命中缓存的次数 hits
 *             与表内加法次数 additions。computes 就是「不重复子问题的个数」。
 *   第二部分  归并式分治：合并阶段的比较次数 comparisons 与搬移次数 moves，
 *             外加逆序对数 inversions。同一条代码走二分与三分两种分法。
 *   第三部分  回溯：搜索树节点数 nodes、被剪掉的候选数 rejected、
 *             选择次数 placed 与撤销次数 undone，用来检查两者是否配平。
 *
 * 计时一律不做：报告里的量全是结构量，与机器快慢无关，重复运行逐位相同。
 * 数据全部在源码里就地构造，不读任何外部文件，随机序列用固定种子。
 */
#ifndef MEMO_LAB_HPP
#define MEMO_LAB_HPP

#include <cstddef>
#include <string>
#include <vector>

namespace mdlab {

/* ================= 第一部分：同一题的三种写法 ================= */

/** 网格路径数一次运行记下来的量。
 *
 *  calls     函数进入次数，也就是朴素递归那棵树的节点数
 *  computes  真正算过的次数：记忆化版里就是不同子问题的个数
 *  hits      进入函数后发现表里已有答案、直接返回的次数
 *  cells     状态表的格子总数
 *  additions 表内加法次数，自底向上版数的是它
 *
 *  朴素递归版 computes == calls（每次都算），hits 与 cells 都是 0。
 */
struct GridRun {
    long long value = 0;
    std::size_t calls = 0;
    std::size_t computes = 0;
    std::size_t hits = 0;
    std::size_t cells = 0;
    std::size_t additions = 0;
};

/** 从 (0, 0) 走到 (m, n) 的单调路径数，只许向右或向上。
    朴素递归：调用次数 = 2 × 答案 − 1，按指数增长。负数按 0 处理。 */
GridRun grid_naive(int m, int n);

/** 同一个递推式的记忆化版：算过的子问题写进表里，再问就直接取。 */
GridRun grid_memo(int m, int n);

/** 同一个递推式的自底向上版：按行填表，没有一次函数调用。 */
GridRun grid_bottom_up(int m, int n);

/** 组合数 C(n, k)，用连乘连除算，作为网格路径数的独立对照实现。 */
long long binomial(int n, int k);

/** 最长公共子序列一次运行记下来的量。
 *
 *  comparisons 字符比较次数；calls、computes、hits、cells 同上。
 */
struct LcsRun {
    long long value = 0;
    std::size_t calls = 0;
    std::size_t computes = 0;
    std::size_t hits = 0;
    std::size_t cells = 0;
    std::size_t comparisons = 0;
};

/** 最长公共子序列的三种写法：朴素递归、记忆化、自底向上（从右下角倒着填）。 */
LcsRun lcs_naive(const std::string &a, const std::string &b);
LcsRun lcs_memo(const std::string &a, const std::string &b);
LcsRun lcs_bottom_up(const std::string &a, const std::string &b);

/* ================= 第二部分：分治的合并代价 ================= */

/** 分法：把当前区间切成两段还是三段 */
enum class SplitWay { Two, Three };

/** 归并式分治一次运行记下来的量。
 *
 *  comparisons 合并阶段的关键字比较次数，只在两段各出候选时记一次
 *  moves       合并阶段的元素搬移次数：写进缓冲区一次、拷回来再一次
 *  merges      一共合并了多少次
 *  levels      递归层数的最大值
 *  inversions  逆序对数（i < j 且 a[i] > a[j] 的对数）
 *  sorted      排好序的结果，供自测比对
 */
struct SortStats {
    std::size_t comparisons = 0;
    std::size_t moves = 0;
    std::size_t merges = 0;
    std::size_t levels = 0;
    long long inversions = 0;
    std::vector<int> sorted;
};

/** 归并排序加逆序对计数，way 决定二路归并还是三路归并。 */
SortStats merge_sort_counted(const std::vector<int> &data, SplitWay way);

/** 双重循环数逆序对，O(n²)，作为归并版的独立对照实现。 */
long long inversions_brute(const std::vector<int> &data);

/** 用固定种子的线性同余序列造一份数据，重跑逐位相同。 */
std::vector<int> make_sequence(std::size_t length, unsigned int seed);

/* ================= 第三部分：回溯的选择与撤销 ================= */

/** 剪枝到哪一档：
 *    None        完全不剪枝，每一行都在 n 个位置里任选
 *    ColumnOnly  只要求同一列不出现两个皇后，得到的是一棵排列树
 *    Full        再加上两条对角线，才是真正的 N 皇后
 */
enum class QueenPrune { None, ColumnOnly, Full };

/** N 皇后一次运行记下来的量。
 *
 *  solutions  走到第 n 行、放满棋盘的次数
 *  nodes      搜索树访问过的节点数，进一次 dfs 算一个
 *  placed     选择次数：把皇后放上棋盘的次数
 *  undone     撤销次数：把皇后从棋盘上拿下来的次数
 *  rejected   剪掉的候选数：试过但不能放的位置
 *  candidates 试过的候选总数，等于 placed + rejected
 *  peak_depth 递归到过的最深一行
 */
struct QueenRun {
    std::size_t solutions = 0;
    std::size_t nodes = 0;
    std::size_t placed = 0;
    std::size_t undone = 0;
    std::size_t rejected = 0;
    std::size_t candidates = 0;
    std::size_t peak_depth = 0;
};

QueenRun queens(int n, QueenPrune prune);

/** 只要求列不重复时的搜索树节点数：n!/(n-0)! + n!/(n-1)! + … + n!/0! */
unsigned long long permutation_tree_nodes(int n);

/** 完全不剪枝时的搜索树节点数：n^0 + n^1 + … + n^n */
unsigned long long free_tree_nodes(int n);

/* ================= 报告与自测 ================= */

/** 自测结果。不打印，交给界面决定怎么显示 */
struct CheckResult {
    std::size_t total = 0;
    std::size_t passed = 0;
    std::size_t failed = 0;
    std::vector<std::string> lines;   /**< 每项一行，例如 "[通过] 3. ……" */

    bool all_passed() const { return failed == 0; }
    std::string summary() const;      /**< "22 项中 22 项通过，全部通过" */
};

/** 项目输出：三种写法、合并代价、回溯三段。
    返回多行 UTF-8 文本，末尾带一个换行；由界面层决定怎么显示 */
std::string build_report();

/** 逐项核对三种写法的一致性、合并代价的恒等式与回溯的配平 */
CheckResult run_self_tests();

}   /* namespace mdlab */

#endif /* MEMO_LAB_HPP */
