/* recursion.hpp —— 练习模板 01 的核心接口（C++）
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
 * 递归这一件事拆成 4 个阶段，每个阶段的实现写在 src/recursion.cpp 里：
 *
 *     阶段 1  sum_first         递归三件套：基线、推进、合并
 *     阶段 2  fib_tree_nodes    递归树有多少个节点
 *     阶段 3  sum_loop          尾递归改循环
 *     阶段 4  hanoi_stack       显式栈替代递归
 *
 * 本模板只碰「递归」这一件事：用的数组是普通 int 数组，
 * 不涉及容器与数据结构的算法——那些属于 09-高阶数据结构 板块。
 */
#ifndef RECURSION_HPP
#define RECURSION_HPP

#include <cstddef>
#include <iosfwd>
#include <vector>

namespace rec {

/* ------------------------------------------------------------------
 * 已给出：递归调用的计数与深度。
 *
 * DepthGuard 构造时记一次调用、析构时退回上一层，用的是 C++ 的
 * 作用域与析构——递归函数里声明一个局部对象，出函数时它自动析构。
 * 因此 calls 就是递归树的节点数，depth_max 是展开到过的最深一层。
 * ------------------------------------------------------------------ */
struct Counters {
    long long calls = 0;        /* 递归调用次数（递归树的节点数） */
    long long iterations = 0;   /* 循环写法走的轮数 */
    long long depth = 0;        /* 当前深度 */
    long long depth_max = 0;    /* 到过的最深一层 */
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

/* ---------- 阶段 1：递归三件套 ---------- */

/* 数组前 n 个元素之和（n 为 0 时是空区间，和为 0）。
 * 三处 TODO 都在这个函数上：基线、推进、合并。 */
long long sum_first(const int *a, int n, Counters &c);

/* ---------- 阶段 2：递归树有多少个节点 ---------- */

/* 已给出：朴素递归版斐波那契，只用来数调用次数，不追求性能 */
long long fib_naive(int n, Counters &c);

/* 不真的展开 fib(n)，只算出展开之后递归树会有多少个节点。
 * 判据：它与 fib_naive(n) 实测到的 calls 逐位相同（n 取 5、10、20）。 */
long long fib_tree_nodes(int n);

/* 已给出：把 fib(n) 的递归树按缩进画出来，返回画出的行数。
 * 行数就是递归树的节点数，可以和上面的两个数对上。 */
long long fib_tree_draw(int n, std::ostream &os);

/* ---------- 阶段 3：尾递归，以及它的真相 ---------- */

/* 已给出：尾递归版。累加器 acc 一路传下去，最后一步只剩返回 */
long long sum_tail(const int *a, int n, long long acc, Counters &c);

/* 把上面那个尾递归写成循环。循环不该再吃栈帧：
 * 验收程序会打印它的返回值、轮数与深度。 */
long long sum_loop(const int *a, int n, Counters &c);

/* ---------- 阶段 4：显式栈 ---------- */

struct Move {
    int disk = 0;
    char from = 'A';
    char to = 'A';
};

/* 已给出：递归版汉诺塔，把每一步写进 moves */
std::vector<Move> hanoi_recursive(int n, char from, char aux, char to);

/* 用显式栈（std::vector 当栈用）得到同一串移动。
 * 判据：与 hanoi_recursive 的结果逐项相同，且长度是 2 的 n 次方减一。 */
std::vector<Move> hanoi_stack(int n, char from, char aux, char to);

/* 已给出：两个移动序列是否逐项相同（长度与每一项都比较） */
bool same_moves(const std::vector<Move> &x, const std::vector<Move> &y);

/* 已给出：把移动序列打印出来，最多 max_lines 行 */
void print_moves(const std::vector<Move> &moves, std::ostream &os, std::size_t max_lines);

} /* namespace rec */

#endif /* RECURSION_HPP */
