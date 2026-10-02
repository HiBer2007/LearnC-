/**
 * recursion_lab.hpp —— 递归：三件套、三种错法与三种改法
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
 *   nodes       递归树的节点数，等于函数被调用的次数
 *   leaves      命中基线的调用次数，也就是递归树的叶子数
 *   height      递归树的高（根到叶最长路径上的边数），也就是递归层数的最大值
 *   per_depth   每个深度上有几个节点
 *
 * 三种改法共用同一份数据：显式栈版的 push 次数必须等于递归版的 nodes；
 * 累加器版把层数从 n 压到 1，循环版压到 0；状态机版把「递归走到哪一步」写进帧里。
 *
 * 递归树用真的递归走一遍（节点数最大的一个例子是 fib(25)，24 万个节点，
 * 运行时间可以忽略）。错法那三个不能真的跑——它们不收敛——
 * 所以改用显式栈模拟，并给出步数预算，见 wrong_base_budget 与 wrong_step_budget。
 */
#ifndef RECURSION_LAB_HPP
#define RECURSION_LAB_HPP

#include <cstddef>
#include <string>
#include <vector>

namespace rlab {

/* ================= 递归树的结构量 ================= */

struct TreeStats {
    std::size_t nodes = 0;
    std::size_t leaves = 0;
    std::size_t height = 0;
    std::vector<std::size_t> per_depth;

    bool same_as(const TreeStats &other) const;
};

/* ================= 三件套：基线、推进、合并 ================= */

/** n 的阶乘。基线 n <= 1，推进 n - 1，合并 n * 子问题 */
long long factorial(int n);

/** 1 + 2 + ... + n。基线 n <= 0，推进 n - 1，合并 n + 子问题 */
long long sum_to(int n);

/** 朴素递归斐波那契。基线 n <= 1，推进 n - 1 与 n - 2，合并两数之和 */
long long fib_naive(int n);

/** 迭代版斐波那契，只作对照用 */
long long fib_iterative(int n);

/** 带计数的 sum_to：节点数 n + 1，树高 n */
TreeStats sum_to_stats(int n);

/** 带计数的 fib_naive：节点数 2 × 叶子数 − 1 */
TreeStats fib_tree_stats(int n);

/* ================= 三种错法 ================= */

/** 一版递归在预算内走到哪一步。
    用显式栈模拟，不真的递归：这两版错法都不收敛，真跑会爆栈。
    hit_base 为假表示预算用尽仍未碰到基线。 */
struct BudgetRun {
    std::size_t budget = 0;
    std::size_t steps = 0;           /**< 展开的节点数，每弹一帧算一步 */
    bool hit_base = false;           /**< 预算内是否碰到过基线 */
    std::size_t first_base_step = 0; /**< 第几步碰到第一个基线，没碰到是 0 */
    std::size_t peak_stack = 0;      /**< 待展开的帧最多时有多少个 */
    std::size_t stack_at_end = 0;    /**< 预算用尽时还剩多少帧没展开 */
};

/** 对照：正确版在同样预算下走完整棵树用多少步 */
BudgetRun correct_fib_budget(int start, std::size_t budget);

/** 错法一：基线写成 n == 1，漏掉 n == 0；调用 n == 0 时越走越远 */
BudgetRun wrong_base_budget(int start, std::size_t budget);

/** 错法二：推进写成 n + 1，每一步都在远离基线 */
BudgetRun wrong_step_budget(int start, std::size_t budget);

/** 错法三：合并漏掉本层，节点数与正确版相同、答案不同 */
long long sum_drop_level(int n, std::size_t *calls);

/* ================= 三种改法 ================= */

/** 显式栈版：把递归帧搬到堆上的 vector 里 */
struct StackRun {
    std::size_t pushes = 0;
    std::size_t pops = 0;
    std::size_t peak = 0;   /**< 栈最深时有多少帧，对应递归版的 height + 1 */
    long long value = 0;
};

StackRun fib_explicit_stack(int n);

/** 累加器版：把中间结果放进参数，基线一到就返回答案本身 */
struct AccRun {
    std::size_t iterations = 0;  /**< 累加次数 / 循环次数 */
    std::size_t max_depth = 0;   /**< 调用层数；循环版是 0 */
    long long value = 0;
};

AccRun sum_accumulator(int n);
AccRun sum_loop(int n);

/** 汉诺塔的一步：把编号为 disk 的盘子从 from 柱搬到 to 柱，编号越大盘子越大 */
struct Move {
    int disk = 0;
    int from = 0;
    int to = 0;
};

/** 两个版本产出的移动序列要逐位比较，因此需要逐成员的相等 */
inline bool operator==(const Move &lhs, const Move &rhs)
{
    return lhs.disk == rhs.disk && lhs.from == rhs.from && lhs.to == rhs.to;
}

inline bool operator!=(const Move &lhs, const Move &rhs)
{
    return !(lhs == rhs);
}

std::size_t hanoi_recursive(int n, int from, int to, int via, std::vector<Move> *out);

/** 状态机版：帧里带 phase，0 待展开、1 待移动、2 待搬回 */
struct HanoiRun {
    std::size_t moves = 0;
    std::size_t pushes = 0;
    std::size_t pops = 0;
    std::size_t peak = 0;
};

HanoiRun hanoi_state_machine(int n, int from, int to, int via, std::vector<Move> *out);

/** 按移动序列在柱子上走一遍，检查每一步是否合法。
    柱子编号从 1 到 pegs；起始状态是 disks 个盘子全部按大小摞在 start_peg 上。
    返回第一个非法步的下标，全部合法时返回序列长度。 */
std::size_t first_illegal_move(const std::vector<Move> &moves, int pegs, int disks,
                               int start_peg);

/* ================= 报告与自测 ================= */

/** 自测结果。不打印，交给界面决定怎么显示 */
struct CheckResult {
    std::size_t total = 0;
    std::size_t passed = 0;
    std::size_t failed = 0;
    std::vector<std::string> lines;   /**< 每项一行，例如 "[通过] 3. ……" */

    bool all_passed() const { return failed == 0; }
    std::string summary() const;      /**< "20 项中 20 项通过，全部通过" */
};

/** 项目输出：三件套、递归树、三种错法、三种改法四段。
    返回多行 UTF-8 文本，末尾带一个换行；由界面层决定怎么显示 */
std::string build_report();

/** 逐项核对三件套、递归树结构量、错法表现与三种改法 */
CheckResult run_self_tests();

}   /* namespace rlab */

#endif /* RECURSION_LAB_HPP */
