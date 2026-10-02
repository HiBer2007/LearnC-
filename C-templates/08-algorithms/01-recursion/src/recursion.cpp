/* recursion.cpp —— 练习模板 01 的实现（C++）
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
 * 本模板的 6 处 TODO 全在这个文件里，按阶段编号：
 *
 *     阶段 1-1 / 1-2 / 1-3   sum_first        基线、推进、合并
 *     阶段 2-1               fib_tree_nodes   递归树的节点数
 *     阶段 3-1               sum_loop         尾递归改循环
 *     阶段 4-1               hanoi_stack      显式栈替代递归
 *
 * 每个 TODO 上面写明「要做什么」，下面的「判据」一行给出填完之后
 * 应当看到的数量——那些数都由 src/main_cli.cpp 打印，逐个对得上才算做完。
 */
#include "recursion.hpp"

#include <ostream>

namespace rec {

/* ==================================================================
 * 阶段 1：递归三件套
 * ================================================================== */

long long sum_first(const int *a, int n, Counters &c)
{
    DepthGuard guard(c);        /* 已给出：记一次调用，出函数时自动退回上一层 */

    /* TODO（阶段 1-1，基线）：
     * 规模缩到不能再缩，是哪种情形？那个情形下「前 n 个元素之和」是多少？
     * 把判据与对应的返回值写在这里。没有这一条，递归会一直往下走。
     * 提示：本模板里 n 只会往下减，最小到 0；空区间的和是 0。
     * 判据（见《配置步骤.md》阶段 1）：sum_first(n=0) 那一行是 0。 */

    /* TODO（阶段 1-2，推进）：
     * 把同一件事交给「自己」去做，但规模必须比现在小。
     * 写清楚交给自己的是哪一个子问题，以及哪一个元素留给这一层自己处理——
     * 那一个元素不再进入子问题。
     * 判据：sum_first(n=8) 的 calls 是 9，depth_max 是 9。 */

    /* TODO（阶段 1-3，合并）：
     * 把「这一层留下来的那个元素」与「子问题算出来的结果」合成答案。
     * 判据：数组 1..8 的 sum_first(n=8) 是 36，sum_first(n=2048) 是 2098176。 */

    /* 占位实现：三处 TODO 都写完之后，下面三行应当整段删掉 */
    (void)a;
    (void)n;
    return 0;
}

/* ==================================================================
 * 阶段 2：递归树有多少个节点
 * ================================================================== */

long long fib_naive(int n, Counters &c)
{
    DepthGuard guard(c);

    if (n < 2) {
        return n;
    }
    return fib_naive(n - 1, c) + fib_naive(n - 2, c);
}

long long fib_tree_nodes(int n)
{
    /* TODO（阶段 2-1）：
     * 不要真的展开 fib(n)——n=20 就要展开两万多次。
     * 照着递归树的形状把节点数递推出来：一棵树有「自己这一个」节点，
     * 再加上两棵子树的节点数；子树什么时候不再往下长，就是递归的基线。
     * 判据：fib_tree_nodes(5) 与 fib_naive(5) 实测的 calls 相同，都是 15；
     *       fib_tree_nodes(10) 是 177，fib_tree_nodes(20) 是 21891。
     *       三者与 fib_tree_draw 画出来的行数也要对得上。 */
    (void)n;
    return 0;       /* 占位实现 */
}

/* 已给出：把递归树按缩进画出来，返回行数 */
static long long fib_draw_sub(int n, std::ostream &os, int level)
{
    for (int i = 0; i < level; ++i) {
        os << "  ";
    }
    os << "fib(" << n << ")\n";

    long long lines = 1;
    if (n >= 2) {
        lines += fib_draw_sub(n - 1, os, level + 1);
        lines += fib_draw_sub(n - 2, os, level + 1);
    }
    return lines;
}

long long fib_tree_draw(int n, std::ostream &os)
{
    return fib_draw_sub(n, os, 0);
}

/* ==================================================================
 * 阶段 3：尾递归，以及它的真相
 * ================================================================== */

long long sum_tail(const int *a, int n, long long acc, Counters &c)
{
    DepthGuard guard(c);

    if (n == 0) {
        return acc;
    }
    return sum_tail(a, n - 1, acc + a[n - 1], c);
}

long long sum_loop(const int *a, int n, Counters &c)
{
    /* TODO（阶段 3-1）：
     * 把上面那个尾递归改写成循环：尾递归里的累加器变成循环里的一个变量，
     * 每走一轮做两件事——把当前元素累加进去、把规模减一；
     * 循环继续的条件正好是尾递归基线的反面。
     * 每走一轮让 c.iterations 加一，验收程序要读这个数。
     * 判据：n=8 的 answer 是 36、iterations 是 8、depth_max 是 0；
     *       n=2048 的 answer 是 2098176、iterations 是 2048。
     *       depth_max 必须是 0——循环不再吃栈帧，这正是它相对尾递归的差别。 */
    (void)a;
    (void)n;
    (void)c;
    return 0;       /* 占位实现 */
}

/* ==================================================================
 * 阶段 4：显式栈
 * ================================================================== */

static void hanoi_rec(int n, char from, char aux, char to, std::vector<Move> &out)
{
    if (n == 0) {
        return;
    }
    hanoi_rec(n - 1, from, to, aux, out);

    Move m;
    m.disk = n;
    m.from = from;
    m.to = to;
    out.push_back(m);

    hanoi_rec(n - 1, aux, from, to, out);
}

std::vector<Move> hanoi_recursive(int n, char from, char aux, char to)
{
    std::vector<Move> out;
    hanoi_rec(n, from, aux, to, out);
    return out;
}

std::vector<Move> hanoi_stack(int n, char from, char aux, char to)
{
    /* TODO（阶段 4-1）：
     * 用自己维护的栈替掉函数调用栈。栈里放的是「待办的事」，一共两种：
     * 一种是「把 k 个盘子从某柱搬到某柱」（还要再拆），
     * 另一种是「第 k 个盘子的那一步移动」（拆到底了，直接写进结果）。
     * 递归里的执行次序是「先搬上面一摞、再走中间这一步、最后搬另一摞」，
     * 而栈是后进先出，因此压栈的次序要与执行次序**相反**。
     * 提示：盘子的编号由递归的形参决定，不要另算。
     * 判据：与 hanoi_recursive 的序列逐项相同（验收程序打印 same 那一行）；
     *       n=3 是 7 步、n=10 是 1023 步、n=16 是 65535 步，都等于 2 的 n 次方减一。 */
    (void)n;
    (void)from;
    (void)aux;
    (void)to;
    return std::vector<Move>();     /* 占位实现：一步也不移动 */
}

/* ==================================================================
 * 已给出的工具
 * ================================================================== */

bool same_moves(const std::vector<Move> &x, const std::vector<Move> &y)
{
    if (x.size() != y.size()) {
        return false;
    }
    for (std::size_t i = 0; i < x.size(); ++i) {
        if (x[i].disk != y[i].disk || x[i].from != y[i].from || x[i].to != y[i].to) {
            return false;
        }
    }
    return true;
}

void print_moves(const std::vector<Move> &moves, std::ostream &os, std::size_t max_lines)
{
    const std::size_t n = moves.size() < max_lines ? moves.size() : max_lines;
    for (std::size_t i = 0; i < n; ++i) {
        os << "  disk " << moves[i].disk << ": " << moves[i].from << " -> " << moves[i].to << "\n";
    }
    if (moves.size() > n) {
        os << "  ... (" << (moves.size() - n) << " more)\n";
    }
}

} /* namespace rec */
