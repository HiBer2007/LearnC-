/* memo.cpp —— 练习模板 02 的实现（C++）
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
 *     阶段 1-1 / 1-2   fib_memo                  缓存的读与写
 *     阶段 2-1         cross_sum                 分治的合并段
 *     阶段 3-1 / 3-2   queen_place / _remove    回溯的选择与撤销
 *     阶段 4-1         count_distinct_calls     不同子问题的个数
 *
 * 每个 TODO 上面写明「要做什么」，下面的「判据」一行给出填完之后
 * 应当看到的数量——那些数都由 src/main_cli.cpp 打印，逐个对得上才算做完。
 */
#include "memo.hpp"

namespace memo {

/* 缓存里「还没算过」的标记：fib 的值永远不是负数，因此 -1 可以安全地当标记 */
static const long long kNotComputed = -1;

/* ==================================================================
 * 阶段 1：记忆化的缓存读与写
 * ================================================================== */

long long fib_naive(int n, Counters &c)
{
    DepthGuard guard(c);

    if (n < 2) {
        return n;
    }
    return fib_naive(n - 1, c) + fib_naive(n - 2, c);
}

std::vector<long long> make_cache(int n)
{
    return std::vector<long long>(static_cast<std::size_t>(n) + 1, kNotComputed);
}

long long fib_memo(int n, std::vector<long long> &cache, Counters &c)
{
    DepthGuard guard(c);

    /* TODO（阶段 1-1，读缓存）：
     * 进函数先看一眼缓存：这一格已经是算好的值，还是那个「还没算过」的标记？
     * 已经算好就把它直接交出去，不要再往下展开——记忆化省下的调用全在这一步。
     * 命中时把 c.hits 加一，验收程序要读这个数。
     * 判据：fib_memo(25) 的 calls 是 49、hits 是 23（见《配置步骤.md》阶段 1）。 */

    /* TODO（阶段 1-2，写缓存）：
     * 基线（n 小于 2）与递归算完之后，都要把结果记回缓存的那一格，
     * 否则同一个子问题下一次进来还是要重算。
     * 提示：本函数有两条出口，别只写一条。
     * 判据：fib_memo(25) 与 fib_naive(25) 的函数值相同（75025），
     *       而 calls 从 242785 降到 49。 */

    /* 占位实现：三行都写完之后整段删掉 */
    (void)cache;
    (void)n;
    return 0;
}

/* ==================================================================
 * 阶段 2：分治的合并段
 * ================================================================== */

long long max_subarray_linear(const int *a, int n)
{
    long long best = a[0];
    long long cur = a[0];
    for (int i = 1; i < n; ++i) {
        cur = (cur > 0) ? cur + a[i] : a[i];
        if (cur > best) {
            best = cur;
        }
    }
    return best;
}

long long cross_sum(const int *a, int lo, int mid, int hi, Counters &c)
{
    /* TODO（阶段 2-1，合并段）：
     * 求「一定跨越中点」的那一段的最大和。它由两截拼成：
     * 一截以 mid-1 结尾、向左尽量延伸；另一截从 mid 开始、向右尽量延伸。
     * 两截都必须非空——全负数时正确答案是最大的那个负数，而不是 0。
     * 每扫过一个元素让 c.iterations 加一，验收程序要读这个数。
     * 判据：三组数据上的结果都与 max_subarray_linear 相同；
     *       cross_sum 的 iterations 合计在 A（8 个元素）上是 24、
     *       在 C（32 个元素）上是 160（见《配置步骤.md》阶段 2）。 */
    (void)a;
    (void)lo;
    (void)mid;
    (void)hi;
    (void)c;
    return 0;       /* 占位实现：跨越中点的和当成 0 */
}

long long max_subarray_dc(const int *a, int lo, int hi, Counters &c)
{
    DepthGuard guard(c);

    if (hi - lo == 1) {
        return a[lo];
    }

    const int mid = lo + (hi - lo) / 2;
    const long long left = max_subarray_dc(a, lo, mid, c);
    const long long right = max_subarray_dc(a, mid, hi, c);
    const long long cross = cross_sum(a, lo, mid, hi, c);

    long long best = (left > right) ? left : right;
    return (cross > best) ? cross : best;
}

/* ==================================================================
 * 阶段 3：回溯的选择与撤销
 * ================================================================== */

bool queen_can_place(const QueensBoard &b, int row, int col)
{
    return b.col[static_cast<std::size_t>(col)] == 0 &&
           b.diag1[static_cast<std::size_t>(row + col)] == 0 &&
           b.diag2[static_cast<std::size_t>(row - col + b.n - 1)] == 0;
}

void queen_place(QueensBoard &b, int row, int col)
{
    /* TODO（阶段 3-1，选择）：
     * 决定在第 row 行第 col 列放一个皇后，就把 queen_can_place 查的那三个位置
     * 全部标记成「被占」。三个位置的下标换算与 queen_can_place 里完全一致。
     * 判据：n_queens(6) 的 solutions 是 4、nodes 是 153；
     *       n_queens(7) 的 solutions 是 40、nodes 是 552（见《配置步骤.md》阶段 3）。 */
    (void)b;
    (void)row;
    (void)col;
}

void queen_remove(QueensBoard &b, int row, int col)
{
    /* TODO（阶段 3-2，撤销）：
     * 回溯返回上一层之前，把这一步立的三个标记恢复成「没被占」。
     * 少撤销一个，后面所有分支都会莫名其妙地被判成冲突。
     * 判据：只补齐「选择」、这一处留空时，n_queens(6) 的 solutions 会小于 4；
     *       两处都补齐才是 4、nodes 是 153、pruned 是 742（见《配置步骤.md》阶段 3）。 */
    (void)b;
    (void)row;
    (void)col;
}

static void queens_solve(QueensBoard &b, int row)
{
    ++b.nodes;

    if (row == b.n) {
        ++b.solutions;
        return;
    }

    for (int col = 0; col < b.n; ++col) {
        if (!queen_can_place(b, row, col)) {
            ++b.pruned;
            continue;
        }
        queen_place(b, row, col);
        queens_solve(b, row + 1);
        queen_remove(b, row, col);
    }
}

void n_queens(int n, QueensBoard &b)
{
    b.n = n;
    b.col.assign(static_cast<std::size_t>(n), 0);
    b.diag1.assign(static_cast<std::size_t>(2 * n - 1), 0);
    b.diag2.assign(static_cast<std::size_t>(2 * n - 1), 0);
    b.solutions = 0;
    b.nodes = 0;
    b.pruned = 0;

    queens_solve(b, 0);
}

/* ==================================================================
 * 阶段 4：同一棵树上不同的子问题有几个
 * ================================================================== */

long long count_distinct_calls(int n)
{
    /* TODO（阶段 4-1）：
     * 递归树的节点数已经被 fib_naive 数过了，这一处要数的是另一件事：
     * 这些节点里，**参数互不相同的**有多少个。
     * 不用真的展开树：把「所有可能出现的参数」列出来数一遍就够了，
     * 因为参数只会在 0 到 n 之间取值。
     * 判据：count_distinct_calls(25) 是 26，与 main_cli 数出来的
     *       「cache filled cells」相同；而 fib_naive(25) 的节点数是 242785
     *       （见《配置步骤.md》阶段 4）。 */
    (void)n;
    return 0;       /* 占位实现 */
}

} /* namespace memo */
