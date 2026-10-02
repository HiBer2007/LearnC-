/**
 * recursion_lab.cpp —— 递归：三件套、三种错法与三种改法
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

#include "recursion_lab.hpp"

#include <algorithm>
#include <sstream>
#include <string>
#include <vector>

namespace rlab {

/* ================= 递归树的结构量 ================= */

bool TreeStats::same_as(const TreeStats &other) const
{
    return nodes == other.nodes && leaves == other.leaves && height == other.height &&
           per_depth == other.per_depth;
}

namespace {

/** 记一个节点：节点数加一，落进深度直方图。
    把「记一笔」放在递归函数的最前面，记到的就是调用次数。 */
void note_node(std::size_t depth, TreeStats *stats)
{
    ++stats->nodes;
    if (depth >= stats->per_depth.size()) {
        stats->per_depth.resize(depth + 1, 0);
    }
    ++stats->per_depth[depth];
    if (depth > stats->height) {
        stats->height = depth;
    }
}

long long sum_to_counted(int n, std::size_t depth, TreeStats *stats)
{
    note_node(depth, stats);
    if (n <= 0) {
        ++stats->leaves;              /* 命中基线 */
        return 0;
    }
    return n + sum_to_counted(n - 1, depth + 1, stats);
}

long long fib_counted(int n, std::size_t depth, TreeStats *stats)
{
    note_node(depth, stats);
    if (n <= 1) {
        ++stats->leaves;              /* 命中基线 */
        return n;
    }
    const long long left = fib_counted(n - 1, depth + 1, stats);
    const long long right = fib_counted(n - 2, depth + 1, stats);
    return left + right;              /* 合并 */
}

/** 错法三用的计数递归：合并那一行漏掉了本层的 n */
long long sum_drop_level_impl(int n, std::size_t *calls)
{
    ++*calls;
    if (n <= 0) {
        return 0;
    }
    return sum_drop_level_impl(n - 1, calls);
}

long long sum_tail_impl(int n, long long acc, std::size_t depth, std::size_t *max_depth)
{
    if (depth > *max_depth) {
        *max_depth = depth;
    }
    if (n <= 0) {
        return acc;                   /* 基线：累加器本身就是答案 */
    }
    return sum_tail_impl(n - 1, acc + n, depth + 1, max_depth);
}

/** 三种错法共用的模拟器：显式栈展开一版递归，带步数预算。
    每弹一帧算一步；碰到基线就记下步数。 */
enum class WrongKind { Correct, BaseOne, StepPlus };

BudgetRun run_with_budget(int start, std::size_t budget, WrongKind kind)
{
    BudgetRun run;
    run.budget = budget;
    std::vector<int> stack;
    stack.push_back(start);
    while (!stack.empty()) {
        if (run.steps >= budget) {
            break;                    /* 预算用尽，这版仍然没收敛 */
        }
        ++run.steps;
        const int n = stack.back();
        stack.pop_back();

        bool is_base = false;
        int left_child = 0;
        int right_child = 0;
        if (kind == WrongKind::Correct) {
            is_base = (n <= 1);
            left_child = n - 1;
            right_child = n - 2;
        } else if (kind == WrongKind::BaseOne) {
            is_base = (n == 1);       /* 这版的基线：漏掉了 n == 0 */
            left_child = n - 1;
            right_child = n - 2;
        } else {
            is_base = (n <= 1);       /* 基线本身没错 */
            left_child = n + 1;       /* 推进写反了方向 */
            right_child = n + 2;
        }

        if (is_base) {
            if (!run.hit_base) {
                run.hit_base = true;
                run.first_base_step = run.steps;
            }
            continue;                 /* 基线：不再展开，接着处理栈里剩下的帧 */
        }
        stack.push_back(left_child);
        stack.push_back(right_child);
        if (stack.size() > run.peak_stack) {
            run.peak_stack = stack.size();
        }
    }
    run.stack_at_end = stack.size();
    return run;
}

}   /* namespace */

/* ================= 三件套 ================= */

long long factorial(int n)
{
    if (n <= 1) {
        return 1;
    }
    return static_cast<long long>(n) * factorial(n - 1);
}

long long sum_to(int n)
{
    if (n <= 0) {
        return 0;
    }
    return n + sum_to(n - 1);
}

long long fib_naive(int n)
{
    if (n <= 1) {
        return n;
    }
    return fib_naive(n - 1) + fib_naive(n - 2);
}

long long fib_iterative(int n)
{
    long long a = 0;
    long long b = 1;
    for (int i = 0; i < n; ++i) {
        const long long next = a + b;
        a = b;
        b = next;
    }
    return a;
}

TreeStats sum_to_stats(int n)
{
    TreeStats stats;
    sum_to_counted(n, 0, &stats);
    return stats;
}

TreeStats fib_tree_stats(int n)
{
    TreeStats stats;
    (void)fib_counted(n, 0, &stats);
    return stats;
}

/* ================= 三种错法 ================= */

BudgetRun correct_fib_budget(int start, std::size_t budget)
{
    return run_with_budget(start, budget, WrongKind::Correct);
}

BudgetRun wrong_base_budget(int start, std::size_t budget)
{
    return run_with_budget(start, budget, WrongKind::BaseOne);
}

BudgetRun wrong_step_budget(int start, std::size_t budget)
{
    return run_with_budget(start, budget, WrongKind::StepPlus);
}

long long sum_drop_level(int n, std::size_t *calls)
{
    std::size_t local = 0;
    const long long value = sum_drop_level_impl(n, &local);
    if (calls != nullptr) {
        *calls = local;
    }
    return value;
}

/* ================= 三种改法 ================= */

StackRun fib_explicit_stack(int n)
{
    /* 帧里带 phase：0 还没展开、1 左子问题回来了、2 右子问题回来了 */
    struct Frame {
        int n;
        int phase;
        bool have_left;
        long long left;
        long long acc;
    };

    StackRun run;
    std::vector<Frame> stack;
    stack.push_back(Frame{n, 0, false, 0, 0});
    run.pushes = 1;
    run.peak = 1;

    while (!stack.empty()) {
        Frame &top = stack.back();
        if (top.phase == 0) {
            if (top.n <= 1) {
                top.acc = top.n;
                top.phase = 2;
                continue;
            }
            top.phase = 1;
            /* 参数在 push_back 之前求值，top 在 push 之后不再使用 */
            stack.push_back(Frame{top.n - 1, 0, false, 0, 0});
            ++run.pushes;
            if (stack.size() > run.peak) {
                run.peak = stack.size();
            }
            continue;
        }
        if (top.phase == 1 && !top.have_left) {
            top.left = top.acc;
            top.have_left = true;
            stack.push_back(Frame{top.n - 2, 0, false, 0, 0});
            ++run.pushes;
            if (stack.size() > run.peak) {
                run.peak = stack.size();
            }
            continue;
        }
        if (top.phase == 1) {
            top.acc = top.left + top.acc;   /* 合并 */
            top.phase = 2;
        }
        const long long value = top.acc;
        stack.pop_back();
        ++run.pops;
        if (stack.empty()) {
            run.value = value;
        } else {
            stack.back().acc = value;       /* 把结果交给父帧 */
        }
    }
    return run;
}

AccRun sum_accumulator(int n)
{
    AccRun run;
    run.value = sum_tail_impl(n, 0, 0, &run.max_depth);
    run.iterations = (n > 0) ? static_cast<std::size_t>(n) : 0;
    return run;
}

AccRun sum_loop(int n)
{
    AccRun run;
    long long acc = 0;
    for (int k = n; k > 0; --k) {
        acc += k;
        ++run.iterations;
    }
    run.value = acc;
    run.max_depth = 0;
    return run;
}

std::size_t hanoi_recursive(int n, int from, int to, int via, std::vector<Move> *out)
{
    if (n == 0) {
        return 0;
    }
    std::size_t count = hanoi_recursive(n - 1, from, via, to, out);
    if (out != nullptr) {
        out->push_back(Move{n, from, to});
    }
    ++count;
    count += hanoi_recursive(n - 1, via, to, from, out);
    return count;
}

HanoiRun hanoi_state_machine(int n, int from, int to, int via, std::vector<Move> *out)
{
    struct Frame {
        int n;
        int from;
        int to;
        int via;
        int phase;
    };

    HanoiRun run;
    std::vector<Frame> stack;
    stack.push_back(Frame{n, from, to, via, 0});
    run.pushes = 1;
    run.peak = 1;

    while (!stack.empty()) {
        Frame &top = stack.back();
        if (top.phase == 0) {
            if (top.n == 0) {
                stack.pop_back();
                ++run.pops;
                continue;
            }
            top.phase = 1;
            stack.push_back(Frame{top.n - 1, top.from, top.via, top.to, 0});
            ++run.pushes;
            if (stack.size() > run.peak) {
                run.peak = stack.size();
            }
            continue;
        }
        if (top.phase == 1) {
            top.phase = 2;
            if (out != nullptr) {
                out->push_back(Move{top.n, top.from, top.to});
            }
            ++run.moves;
            continue;
        }
        const Frame done = top;
        stack.pop_back();
        ++run.pops;
        stack.push_back(Frame{done.n - 1, done.via, done.to, done.from, 0});
        ++run.pushes;
        if (stack.size() > run.peak) {
            run.peak = stack.size();
        }
    }
    return run;
}

std::size_t first_illegal_move(const std::vector<Move> &moves, int pegs, int disks, int start_peg)
{
    /* 柱子编号从 1 到 pegs，每根柱子从下往上记着盘子的编号；
       起始状态是 disks 个盘子按大小摞在 start_peg 上 */
    std::vector<std::vector<int>> peg(static_cast<std::size_t>(pegs));
    if (start_peg < 1 || start_peg > pegs) {
        return 0;
    }
    for (int disk = disks; disk >= 1; --disk) {
        peg[static_cast<std::size_t>(start_peg - 1)].push_back(disk);
    }
    for (std::size_t i = 0; i < moves.size(); ++i) {
        const Move &move = moves[i];
        if (move.from < 1 || move.from > pegs || move.to < 1 || move.to > pegs) {
            return i;
        }
        std::vector<int> &src = peg[static_cast<std::size_t>(move.from - 1)];
        std::vector<int> &dst = peg[static_cast<std::size_t>(move.to - 1)];
        if (src.empty() || src.back() != move.disk) {
            return i;                  /* 要搬的盘子不在柱顶 */
        }
        if (!dst.empty() && dst.back() < move.disk) {
            return i;                  /* 大盘压在小盘上 */
        }
        dst.push_back(move.disk);
        src.pop_back();
    }
    return moves.size();
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

std::string histogram(const std::vector<std::size_t> &per_depth)
{
    std::ostringstream os;
    for (std::size_t i = 0; i < per_depth.size(); ++i) {
        if (i != 0) {
            os << ' ';
        }
        os << per_depth[i];
    }
    return os.str();
}

void append_basics(std::ostringstream &os)
{
    const TreeStats ten = sum_to_stats(10);
    const TreeStats hundred = sum_to_stats(100);
    os << u8"一、递归三件套：基线、推进、合并\n";
    os << u8"  以「1 到 n 求和」为例：\n";
    os << u8"    基线  n <= 0 时返回 0，不再往下走\n";
    os << u8"    推进  把规模从 n 缩到 n - 1，每一步都在靠近基线\n";
    os << u8"    合并  本层的 n 加上子问题（n - 1）的结果\n";
    os << "  sum_to(10) = " << sum_to(10) << u8"，调用 " << ten.nodes
       << u8" 次，递归层数 " << ten.height << "\n";
    os << "  sum_to(100) = " << sum_to(100) << u8"，调用 " << hundred.nodes
       << u8" 次，递归层数 " << hundred.height << "\n";
    os << "  factorial(20) = " << factorial(20) << u8"，调用 21 次，递归层数 20\n";
    os << u8"  调用次数 = n + 1：每个规模恰好进去一次，基线那次也算一个节点\n";
    os << "\n";
}

void append_tree_table(std::ostringstream &os)
{
    os << u8"二、朴素斐波那契的递归树（结构量，重跑逐位相同）\n";
    os << u8"  定义：fib(n) = n <= 1 ? n : fib(n - 1) + fib(n - 2)\n";
    os << "  " << pad_right("n", 5) << pad_right(u8"节点数", 12) << pad_right(u8"叶子数", 12)
       << pad_right(u8"树高", 8) << u8"每层节点数（深度 0 起）\n";
    const int sizes[] = {5, 10, 15, 20, 25};
    for (const int n : sizes) {
        const TreeStats stats = fib_tree_stats(n);
        os << "  " << pad_right(std::to_string(n), 5)
           << pad_right(std::to_string(stats.nodes), 12)
           << pad_right(std::to_string(stats.leaves), 12)
           << pad_right(std::to_string(stats.height), 8)
           << histogram(stats.per_depth) << "\n";
    }
    os << u8"  节点数 = 2 × 叶子数 − 1：每个内部节点恰好两个孩子，叶子就是命中基线的那些调用\n";
    os << u8"  树高 = n − 1：最深的一条链是 n → n − 1 → … → 1，递归层数就这么多\n";
    os << u8"  节点数按 n 指数增长，fib(25) 已经有 24 万个节点，n = 50 就跑不完了\n";
    os << "\n";
}

void append_wrong_ways(std::ostringstream &os)
{
    const std::size_t budget = 20000;
    const BudgetRun correct = correct_fib_budget(15, budget);
    const BudgetRun bad_base = wrong_base_budget(0, budget);
    const BudgetRun bad_step = wrong_step_budget(10, budget);

    os << u8"三、三种错法\n";
    os << u8"  错法一：基线写成 n == 1，漏掉 n == 0\n";
    os << u8"    调用 wrong_base(0)：0 不是基线，函数继续调 -1、-2，再往下是 -2、-3，越走越远\n";
    os << u8"    用显式栈模拟，预算 " << budget << u8" 步：走满 " << bad_base.steps
       << u8" 步仍未命中基线，还剩 " << bad_base.stack_at_end << u8" 帧没展开\n";
    os << u8"  错法二：推进写成 n + 1，每一步都在远离基线\n";
    os << u8"    调用 wrong_step(10)：10 往 11、12 走，同样碰不到 n <= 1\n";
    os << u8"    同一预算：" << bad_step.steps << u8" 步未命中基线，还剩 "
       << bad_step.stack_at_end << u8" 帧没展开\n";
    os << u8"  对照：正确版在同样预算下走 fib(15)\n";
    os << u8"    第 " << correct.first_base_step << u8" 步命中第一个基线，共 " << correct.steps
       << u8" 步走完整棵树（节点数与第二段的 " << fib_tree_stats(15).nodes << u8" 相同）\n";

    std::size_t dropped_calls = 0;
    const long long dropped = sum_drop_level(100, &dropped_calls);
    const TreeStats right = sum_to_stats(100);
    os << u8"  错法三：合并漏掉本层，写成 return sum_drop_level(n - 1)\n";
    os << u8"    sum_to(100) 调用 " << right.nodes << u8" 次，结果 " << sum_to(100) << "\n";
    os << u8"    漏合并那版调用 " << dropped_calls << u8" 次，结果 " << dropped << "\n";
    os << u8"    两版的递归树一模一样：错在合并，错在往上带结果的那一行\n";
    os << "\n";
}

void append_rewrites(std::ostringstream &os)
{
    const int fib_n = 20;
    const TreeStats tree = fib_tree_stats(fib_n);
    const StackRun stack_run = fib_explicit_stack(fib_n);
    const AccRun acc = sum_accumulator(100);
    const AccRun loop = sum_loop(100);
    const TreeStats right = sum_to_stats(100);

    os << u8"四、三种改法\n";
    os << u8"  改法一 显式栈：把递归帧搬到堆上的 vector 里\n";
    os << "    fib(" << fib_n << u8") 递归版 " << tree.nodes << u8" 个节点、树高 " << tree.height << "\n";
    os << u8"    显式栈版 push " << stack_run.pushes << u8" 次、pop " << stack_run.pops
       << u8" 次，栈最深 " << stack_run.peak << u8" 帧，结果 " << stack_run.value << "\n";
    os << u8"    push 次数 = 递归节点数，栈深 = 树高 + 1：同一棵树，只是帧的位置换了\n";
    os << u8"  改法二 累加器：把中间结果放进参数，写完之后递归调用处在最后一行\n";
    os << u8"    sum_to(100) 递归版 " << right.nodes << u8" 次调用、层数 " << right.height << "\n";
    os << "    sum_accumulator(100) " << acc.iterations << u8" 次迭代、层数 " << acc.max_depth
       << u8"，结果 " << acc.value << "\n";
    os << "    sum_loop(100) " << loop.iterations << u8" 次迭代、层数 " << loop.max_depth
       << u8"，结果 " << loop.value << "\n";
    os << u8"    写成尾递归不等于编译器会折叠它：这次的层数是数出来的，没有省\n";
    os << u8"  改法三 状态机：帧里带 phase，把「走到哪一步」也存下来\n";
    os << u8"    这份状态机给每帧三个阶段：0 待搬上面的 n − 1 个、1 待移动第 n 个、2 待搬回来\n";
    const int hanoi_n = 10;
    std::vector<Move> rec_moves;
    std::vector<Move> sm_moves;
    const std::size_t rec_count = hanoi_recursive(hanoi_n, 1, 3, 2, &rec_moves);
    const HanoiRun sm = hanoi_state_machine(hanoi_n, 1, 3, 2, &sm_moves);
    os << u8"    汉诺塔 n = " << hanoi_n << u8"：递归版 " << rec_count << u8" 次移动，状态机版 "
       << sm.moves << u8" 步移动\n";
    os << u8"    状态机版 push " << sm.pushes << u8" 次、pop " << sm.pops << u8" 次，栈最深 "
       << sm.peak << u8" 帧\n";
    os << u8"    两个移动序列逐位相同：" << (rec_moves == sm_moves ? u8"是" : u8"否")
       << u8"；每一步都合法：" << (first_illegal_move(sm_moves, 3, hanoi_n, 1) == sm_moves.size() ? u8"是" : u8"否")
       << "\n";
    os << u8"    移动次数 = 2^n − 1，递归树有 2^(n+1) − 1 个节点（含 n == 0 的空帧）\n";
}

}   /* namespace */

std::string build_report()
{
    std::ostringstream os;
    append_basics(os);
    append_tree_table(os);
    append_wrong_ways(os);
    append_rewrites(os);
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
        const bool ok = (got == want);
        if (ok) {
            expect(true, what);
        } else {
            expect(false, what + u8"（得到 " + std::to_string(got) + u8"，应为 " +
                              std::to_string(want) + u8"）");
        }
    }

    void expect_eq_size(std::size_t got, std::size_t want, const std::string &what)
    {
        const bool ok = (got == want);
        if (ok) {
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

    /* 1—2 三件套的结果 */
    checks.expect(factorial(0) == 1 && factorial(1) == 1 && factorial(5) == 120 &&
                      factorial(20) == 2432902008176640000LL,
                  u8"阶乘：0! 与 1! 是 1，5! 是 120，20! 是 2432902008176640000");
    checks.expect(sum_to(0) == 0 && sum_to(1) == 1 && sum_to(100) == 5050,
                  u8"求和：基线 n <= 0 返回 0，sum_to(100) 是 5050");

    /* 3—4 递归树的节点与层数 */
    const TreeStats sum100 = sum_to_stats(100);
    checks.expect(sum100.nodes == 101 && sum100.height == 100 && sum100.per_depth.size() == 101,
                  u8"sum_to(100) 的递归树：101 个节点、层数 100、每层恰好 1 个节点");
    checks.expect(sum_to_stats(0).nodes == 1 && sum_to_stats(0).height == 0 &&
                      sum_to_stats(0).leaves == 1,
                  u8"sum_to(0) 只有一个节点，它就是基线");

    /* 5—7 递归树的恒等式 */
    bool nodes_rule = true;
    bool depth_rule = true;
    bool height_rule = true;
    for (int n = 1; n <= 20; ++n) {
        const TreeStats s = fib_tree_stats(n);
        if (s.nodes != 2 * s.leaves - 1) {
            nodes_rule = false;
        }
        std::size_t sum = 0;
        for (const std::size_t count : s.per_depth) {
            sum += count;
        }
        if (sum != s.nodes || s.per_depth[0] != 1) {
            depth_rule = false;
        }
        if (s.height != static_cast<std::size_t>(n - 1)) {
            height_rule = false;
        }
    }
    checks.expect(nodes_rule, u8"fib 递归树：n = 1 到 20 都满足 节点数 = 2 × 叶子数 − 1");
    checks.expect(depth_rule, u8"fib 递归树：每层节点数之和等于节点数，第 0 层只有根一个");
    checks.expect(height_rule, u8"fib 递归树：树高等于 n − 1");

    /* 8 节点数与斐波那契数列的关系 */
    bool fib_rule = true;
    for (int n = 1; n <= 20; ++n) {
        if (fib_tree_stats(n).leaves != static_cast<std::size_t>(fib_iterative(n + 1))) {
            fib_rule = false;
        }
    }
    checks.expect(fib_rule, u8"fib 递归树的叶子数等于 F(n + 1)，节点数等于 2 × F(n + 1) − 1");

    /* 9 递归版与迭代版结果一致 */
    bool fib_same = true;
    for (int n = 0; n <= 30; ++n) {
        if (fib_naive(n) != fib_iterative(n)) {
            fib_same = false;
        }
    }
    checks.expect(fib_same, u8"fib_naive 与迭代版在 n = 0 到 30 上逐位相同");

    /* 10—11 显式栈版 */
    bool stack_same = true;
    bool stack_count = true;
    bool stack_balanced = true;
    for (int n = 0; n <= 18; ++n) {
        const StackRun run = fib_explicit_stack(n);
        if (run.value != fib_naive(n)) {
            stack_same = false;
        }
        if (run.pushes != fib_tree_stats(n).nodes) {
            stack_count = false;
        }
        if (run.pops != run.pushes) {
            stack_balanced = false;
        }
    }
    checks.expect(stack_same, u8"显式栈版的结果与递归版在 n = 0 到 18 上逐位相同");
    checks.expect(stack_count, u8"显式栈版的 push 次数等于递归版的节点数");
    checks.expect(stack_balanced, u8"显式栈版 push 与 pop 次数配平");

    /* 12 显式栈版的栈深 */
    const StackRun deep = fib_explicit_stack(20);
    checks.expect_eq_size(deep.peak, fib_tree_stats(20).height + 1,
                          u8"显式栈版栈最深处的帧数等于递归版的树高 + 1");

    /* 13—14 累加器与循环 */
    const AccRun acc = sum_accumulator(100);
    const AccRun loop = sum_loop(100);
    checks.expect(acc.value == 5050 && acc.iterations == 100 && acc.max_depth == 100,
                  u8"累加器版 sum_accumulator(100)：100 次迭代、层数 100、结果 5050");
    checks.expect(loop.value == 5050 && loop.iterations == 100 && loop.max_depth == 0,
                  u8"循环版 sum_loop(100)：100 次迭代、层数 0、结果 5050");

    /* 15—17 汉诺塔 */
    bool hanoi_count = true;
    bool hanoi_same = true;
    bool hanoi_legal = true;
    for (int n = 0; n <= 12; ++n) {
        std::vector<Move> rec;
        std::vector<Move> sm;
        const std::size_t rec_count = hanoi_recursive(n, 1, 3, 2, &rec);
        const HanoiRun run = hanoi_state_machine(n, 1, 3, 2, &sm);
        const std::size_t want = (static_cast<std::size_t>(1) << n) - 1;
        if (rec_count != want || run.moves != want || rec.size() != want) {
            hanoi_count = false;
        }
        if (rec != sm) {
            hanoi_same = false;
        }
        if (first_illegal_move(rec, 3, n, 1) != rec.size() ||
            first_illegal_move(sm, 3, n, 1) != sm.size()) {
            hanoi_legal = false;
        }
    }
    checks.expect(hanoi_count, u8"汉诺塔 n = 0 到 12：移动次数都是 2^n − 1");
    checks.expect(hanoi_same, u8"汉诺塔状态机版与递归版的移动序列逐位相同");
    checks.expect(hanoi_legal, u8"两个版本的移动序列每一步都合法：大盘不压小盘");

    /* 18 状态机版的 push 与 pop 配平 */
    const HanoiRun sm10 = hanoi_state_machine(10, 1, 3, 2, nullptr);
    checks.expect(sm10.pushes == sm10.pops && sm10.moves == 1023,
                  u8"汉诺塔 n = 10 的状态机：push 与 pop 配平，移动 1023 次");

    /* 19—20 三种错法 */
    const BudgetRun bad_base = wrong_base_budget(0, 20000);
    const BudgetRun bad_step = wrong_step_budget(10, 20000);
    checks.expect(!bad_base.hit_base && bad_base.steps == 20000,
                  u8"错法一：基线写成 n == 1 时，20000 步预算内一次基线都没碰到");
    checks.expect(!bad_step.hit_base && bad_step.steps == 20000,
                  u8"错法二：推进写成 n + 1 时，同样碰不到基线");

    /* 21 正确版的对照 */
    const BudgetRun good = correct_fib_budget(15, 20000);
    checks.expect(good.hit_base && good.first_base_step == 8,
                  u8"正确版 fib(15) 第 8 步命中第一个基线");

    /* 22 错法三 */
    std::size_t dropped_calls = 0;
    const long long dropped = sum_drop_level(100, &dropped_calls);
    checks.expect(dropped == 0 && dropped_calls == sum_to_stats(100).nodes,
                  u8"错法三：合并漏掉本层时节点数与正确版相同（101），结果却是 0");

    return checks.finish();
}

}   /* namespace rlab */
