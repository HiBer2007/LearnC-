/* memo.hpp —— 练习模板 02 的核心接口（C++）
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
 * 记忆化、分治与回溯三者共用同一棵递归树，本模板用同一个计数器把它们量出来：
 *
 *     阶段 1  fib_memo              记忆化的缓存读与写
 *     阶段 2  cross_sum             分治的合并段
 *     阶段 3  queen_place / _remove 回溯的选择与撤销
 *     阶段 4  count_distinct_calls  同一棵树上「不同的子问题」有几个
 *
 * 只碰算法本身：用到的容器是 std::vector，它的接口与代价属于
 * 09-高阶数据结构 板块，本模板不重复讲。
 */
#ifndef MEMO_HPP
#define MEMO_HPP

#include <cstddef>
#include <vector>

namespace memo {

/* 已给出：调用次数、命中次数与深度，用法与模板 01 相同 */
struct Counters {
    long long calls = 0;        /* 进入被计数函数的次数（递归树的节点数） */
    long long hits = 0;         /* 记忆化命中缓存的次数 */
    long long iterations = 0;   /* 顺序扫描过的元素个数 */
    long long depth = 0;
    long long depth_max = 0;
};

struct DepthGuard {
    Counters &c;

    explicit DepthGuard(Counters &cc) : c(cc) {
        ++c.calls;
        ++c.depth;
        if (c.depth > c.depth_max) {
            c.depth_max = c.depth;
        }
    }
    ~DepthGuard() { --c.depth; }

    DepthGuard(const DepthGuard &) = delete;
    DepthGuard &operator=(const DepthGuard &) = delete;
};

/* ---------- 阶段 1：记忆化的缓存读与写 ---------- */

/* 已给出：没有缓存时的样子，用来对照 */
long long fib_naive(int n, Counters &c);

/* 斐波那契的记忆化版。缓存按 n 直接寻址，第 n 格放 fib(n)。
 * 有两个 TODO：缓存怎么读、算出来之后怎么写回去。 */
long long fib_memo(int n, std::vector<long long> &cache, Counters &c);

/* 已给出：建一个长度为 n+1 的缓存，每一格先填上「还没算过」的标记。
 * fib 的值永远不是负数，因此拿 -1 当标记是安全的。 */
std::vector<long long> make_cache(int n);

/* ---------- 阶段 2：分治的合并段 ---------- */

/* 最大连续子段和。a[lo, hi) 是一段区间，递归取左右两半，
 * 再把「跨越中点」的那一段并进来——合并段就是 cross_sum。 */
long long max_subarray_dc(const int *a, int lo, int hi, Counters &c);

/* 阶段 2 的合并段：区间穿过 mid 时的最大和。
 * 它必须真的包含 a[mid-1] 与 a[mid] 这两个元素，否则合并出来的不是「跨越中点」的和。 */
long long cross_sum(const int *a, int lo, int mid, int hi, Counters &c);

/* 已给出：O(n) 的对照写法，分治的结果必须与它相同 */
long long max_subarray_linear(const int *a, int n);

/* ---------- 阶段 3：回溯的选择与撤销 ---------- */

/* n 皇后用的棋盘状态。三个数组分别记「哪一列被占」「哪条主对角线被占」
 * 「哪条副对角线被占」，下标换算见 src/memo.cpp 里已给出的 queen_can_place。 */
struct QueensBoard {
    int n = 0;
    std::vector<char> col;      /* 下标是列号 */
    std::vector<char> diag1;    /* 下标是 row + col */
    std::vector<char> diag2;    /* 下标是 row - col + n - 1 */
    int solutions = 0;
    long long nodes = 0;        /* 搜索树的节点数 */
    long long pruned = 0;       /* 因为冲突被剪掉的候选数 */
};

/* 已给出：建棋盘并搜一遍，返回解的个数 */
void n_queens(int n, QueensBoard &b);

/* 已给出：这一格能不能放（三个数组都没被占） */
bool queen_can_place(const QueensBoard &b, int row, int col);

/* 阶段 3 的两处 TODO：选中一格之后把三个标记立起来，回溯返回时再把它们放回去。 */
void queen_place(QueensBoard &b, int row, int col);
void queen_remove(QueensBoard &b, int row, int col);

/* ---------- 阶段 4：同一棵树上不同的子问题有几个 ---------- */

/* 不展开递归，只算 fib(n) 的递归树里「参数不同的调用」有多少个。
 * 这个数就是记忆化最多能省下多少的依据。 */
long long count_distinct_calls(int n);

} /* namespace memo */

#endif /* MEMO_HPP */
