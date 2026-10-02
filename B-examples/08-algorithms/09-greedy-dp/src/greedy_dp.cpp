/**
 * greedy_dp.cpp —— 贪心与动态规划：同一道题的两条路
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

#include "greedy_dp.hpp"

#include <algorithm>
#include <functional>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

namespace gdp {

/* ===================================================================
 *  一、零钱兑换：贪心与 DP
 * =================================================================== */

GreedyRun greedy_change(const std::vector<int> &denoms, int amount)
{
    GreedyRun run;
    if (amount < 0) {
        return run;
    }

    /* 从大到小排一遍，之后每次取「不超过剩余金额的最大面额」就是第一枪命中 */
    std::vector<int> sorted = denoms;
    std::sort(sorted.begin(), sorted.end(), std::greater<int>());

    int rest = amount;
    while (rest > 0) {
        bool took = false;
        for (const int d : sorted) {
            ++run.probes;                 /* 每看一眼一个面额算一次探查 */
            if (d <= rest) {
                run.picked.push_back(d);
                rest -= d;
                ++run.steps;              /* 每挑出一枚算一步 */
                took = true;
                break;
            }
        }
        if (!took) {
            break;                        /* 最小的面额都超过剩余金额，走不动了 */
        }
    }

    run.remainder = rest;
    run.reachable = (rest == 0);
    run.coins = static_cast<int>(run.picked.size());
    return run;
}

ChangeDp dp_change(const std::vector<int> &denoms, int amount)
{
    ChangeDp run;
    if (amount < 0) {
        return run;
    }

    /* 状态是金额，dp[s] 是凑出 s 的最少枚数。
       哨兵比任何可行解都大，用来表示「还没凑出来」 */
    const int sentinel = amount + 1;
    const std::size_t cols = static_cast<std::size_t>(amount) + 1;
    std::vector<int> dp(cols, sentinel);
    std::vector<int> from(cols, -1);      /* 记下最后用的那枚面额，用来回溯 */
    dp[0] = 0;
    run.states = cols;

    for (std::size_t s = 1; s < cols; ++s) {
        for (const int d : denoms) {
            ++run.transitions;            /* 每个状态对每个面额试一次 */
            if (static_cast<std::size_t>(d) <= s) {
                const int candidate = dp[s - static_cast<std::size_t>(d)] + 1;
                if (candidate < dp[s]) {
                    dp[s] = candidate;
                    from[s] = d;
                }
            }
        }
    }

    if (dp[static_cast<std::size_t>(amount)] >= sentinel) {
        return run;                       /* 凑不出，reachable 保持假 */
    }
    run.reachable = true;
    run.coins = dp[static_cast<std::size_t>(amount)];

    /* 从目标金额倒着走回 0，把用过的面额收集起来；收集顺序是「最后用的在前」 */
    int cursor = amount;
    while (cursor > 0) {
        const int d = from[static_cast<std::size_t>(cursor)];
        if (d <= 0) {
            break;
        }
        run.picked.push_back(d);
        cursor -= d;
    }
    std::sort(run.picked.begin(), run.picked.end(), std::greater<int>());
    return run;
}

ChangeCase run_change_case(const std::vector<int> &denoms, int amount)
{
    ChangeCase one;
    one.denoms = denoms;
    one.amount = amount;
    one.greedy = greedy_change(denoms, amount);
    one.dp = dp_change(denoms, amount);
    return one;
}

namespace {

const int kUnknown = -2;   /* 带记忆递归里的「还没算过」 */

int min_coins_memo_impl(const std::vector<int> &denoms, int amount, std::vector<int> &memo,
                        std::size_t *calls)
{
    if (amount == 0) {
        return 0;
    }
    if (memo[static_cast<std::size_t>(amount)] != kUnknown) {
        return memo[static_cast<std::size_t>(amount)];
    }
    ++*calls;                             /* 只数真正展开的状态 */
    int best = -1;                        /* -1 表示凑不出 */
    for (const int d : denoms) {
        if (d > amount) {
            continue;
        }
        const int sub = min_coins_memo_impl(denoms, amount - d, memo, calls);
        if (sub >= 0 && (best < 0 || sub + 1 < best)) {
            best = sub + 1;
        }
    }
    memo[static_cast<std::size_t>(amount)] = best;
    return best;
}

}   /* namespace */

int min_coins_memo(const std::vector<int> &denoms, int amount, std::size_t *calls)
{
    if (calls != nullptr) {
        *calls = 0;
    }
    if (amount < 0) {
        return -1;
    }
    std::vector<int> memo(static_cast<std::size_t>(amount) + 1, kUnknown);
    memo[0] = 0;
    return min_coins_memo_impl(denoms, amount, memo, calls);
}

int first_greedy_failure(const std::vector<int> &denoms, int max_amount, int *greedy_coins,
                         int *best_coins)
{
    for (int amount = 1; amount <= max_amount; ++amount) {
        const GreedyRun g = greedy_change(denoms, amount);
        const ChangeDp d = dp_change(denoms, amount);
        if (g.coins != d.coins) {
            if (greedy_coins != nullptr) {
                *greedy_coins = g.coins;
            }
            if (best_coins != nullptr) {
                *best_coins = d.coins;
            }
            return amount;
        }
    }
    return 0;
}

GreedyScan scan_greedy_systems(int max_denom, int max_amount)
{
    GreedyScan scan;
    if (max_denom < 2 || max_amount < 1) {
        return scan;
    }

    /* 面额组一律含 1，其余面额从 2 到 max_denom 里取，
       用位掩码从小到大枚举，这样「第一处失败」才有确定的先后 */
    const int extras = max_denom - 1;
    const int masks = 1 << extras;
    for (int mask = 0; mask < masks; ++mask) {
        std::vector<int> denoms;
        denoms.push_back(1);
        for (int bit = 0; bit < extras; ++bit) {
            if ((mask & (1 << bit)) != 0) {
                denoms.push_back(bit + 2);
            }
        }

        /* 每个面额都是前一个的整数倍，才算「成倍数关系」 */
        bool multiple = true;
        for (std::size_t i = 1; i < denoms.size(); ++i) {
            if (denoms[i] % denoms[i - 1] != 0) {
                multiple = false;
                break;
            }
        }

        ++scan.systems;
        if (multiple) {
            ++scan.multiple_systems;
        }

        for (int amount = 1; amount <= max_amount; ++amount) {
            const GreedyRun g = greedy_change(denoms, amount);
            const ChangeDp d = dp_change(denoms, amount);
            ++scan.combos;
            if (multiple) {
                ++scan.multiple_combos;
            }
            if (g.coins != d.coins) {
                ++scan.fails;
                if (multiple) {
                    ++scan.multiple_fails;
                }
                if (!scan.has_first_fail) {
                    scan.has_first_fail = true;
                    scan.first_fail_denoms = denoms;
                    scan.first_fail_amount = amount;
                    scan.first_fail_greedy = g.coins;
                    scan.first_fail_best = d.coins;
                }
            }
        }
    }
    return scan;
}

/* ===================================================================
 *  二、最长上升子序列：三种状态定义
 * =================================================================== */

namespace {

/** 值域：把序列里的最小值和最大值找出来，状态数按这个跨度算 */
struct Span {
    int lo = 0;
    int hi = 0;
    bool empty = true;

    std::size_t size() const { return empty ? 0 : static_cast<std::size_t>(hi - lo + 1); }
};

Span value_span(const std::vector<int> &a)
{
    Span span;
    if (a.empty()) {
        return span;
    }
    span.empty = false;
    span.lo = *std::min_element(a.begin(), a.end());
    span.hi = *std::max_element(a.begin(), a.end());
    return span;
}

/** 序列里出现过多少个不同的值，就是「值」这套定义真正用到的格子数 */
std::size_t distinct_values(const std::vector<int> &a)
{
    std::vector<int> copy = a;
    std::sort(copy.begin(), copy.end());
    copy.erase(std::unique(copy.begin(), copy.end()), copy.end());
    return copy.size();
}

/** 树状数组维护前缀最大值。查询与更新各自把循环步数报出来，
    这样「转移次数」是数出来的，不是按 O(log V) 估的 */
class MaxBit {
public:
    explicit MaxBit(std::size_t n) : tree_(n + 1, 0) {}

    std::size_t query(std::size_t i, std::size_t *best) const
    {
        std::size_t steps = 0;
        *best = 0;
        while (i > 0) {
            ++steps;
            if (tree_[i] > *best) {
                *best = tree_[i];
            }
            i -= lowbit(i);
        }
        return steps;
    }

    std::size_t update(std::size_t i, std::size_t value)
    {
        std::size_t steps = 0;
        while (i < tree_.size()) {
            ++steps;
            if (value > tree_[i]) {
                tree_[i] = value;
            }
            i += lowbit(i);
        }
        return steps;
    }

private:
    static std::size_t lowbit(std::size_t i) { return i & (~i + 1); }

    std::vector<std::size_t> tree_;
};

}   /* namespace */

LisRun lis_end_at_index(const std::vector<int> &a)
{
    LisRun run;
    const std::size_t n = a.size();
    run.states = n;                       /* 状态是下标，一共 n 格 */
    run.used = n;                         /* 每格都会被写一次 */

    std::vector<std::size_t> dp(n, 1);
    for (std::size_t i = 0; i < n; ++i) {
        for (std::size_t j = 0; j < i; ++j) {
            ++run.transitions;            /* 一个候选转移 */
            ++run.compares;               /* 比一次 a[j] 与 a[i] */
            if (a[j] < a[i] && dp[j] + 1 > dp[i]) {
                dp[i] = dp[j] + 1;
            }
        }
    }
    for (std::size_t i = 0; i < n; ++i) {
        run.length = std::max(run.length, dp[i]);
    }
    return run;
}

LisRun lis_by_value_naive(const std::vector<int> &a)
{
    LisRun run;
    const Span span = value_span(a);
    if (span.empty) {
        return run;
    }

    run.states = span.size();             /* 状态是值，一共「值域跨度」格 */
    std::vector<std::size_t> best(span.size(), 0);

    for (const int x : a) {
        std::size_t prev = 0;
        for (int v = span.lo; v < x; ++v) {
            ++run.transitions;            /* 每个更小的值都是一个候选 */
            ++run.compares;
            const std::size_t slot = static_cast<std::size_t>(v - span.lo);
            if (best[slot] > prev) {
                prev = best[slot];
            }
        }
        best[static_cast<std::size_t>(x - span.lo)] = prev + 1;
    }

    for (const std::size_t v : best) {
        run.length = std::max(run.length, v);
    }
    run.used = distinct_values(a);
    return run;
}

LisRun lis_by_value_bit(const std::vector<int> &a)
{
    LisRun run;
    const Span span = value_span(a);
    if (span.empty) {
        return run;
    }

    /* 值 v 映射到树状数组的下标 v - lo + 1，下标从 1 起 */
    run.states = span.size() + 1;
    MaxBit bit(span.size());

    for (const int x : a) {
        const std::size_t index = static_cast<std::size_t>(x - span.lo) + 1;
        std::size_t prev = 0;
        const std::size_t query_steps = bit.query(index - 1, &prev);
        run.transitions += query_steps;
        run.compares += query_steps;
        const std::size_t update_steps = bit.update(index, prev + 1);
        run.transitions += update_steps;
        run.compares += update_steps;
    }

    std::size_t top = 0;
    const std::size_t final_steps = bit.query(span.size(), &top);
    run.transitions += final_steps;
    run.compares += final_steps;
    run.length = top;
    run.used = distinct_values(a);
    return run;
}

LisRun lis_by_length(const std::vector<int> &a)
{
    LisRun run;
    const std::size_t n = a.size();
    run.states = n + 1;                   /* 状态是长度，长度取 0 到 n */

    std::vector<int> tails;
    tails.reserve(n);
    for (const int x : a) {
        ++run.transitions;                /* 每个元素一次「定位加写入」 */
        /* 二分找第一个不小于 x 的位置：tails 是非降的 */
        std::size_t lo = 0;
        std::size_t hi = tails.size();
        while (lo < hi) {
            const std::size_t mid = lo + (hi - lo) / 2;
            ++run.compares;
            if (tails[mid] < x) {
                lo = mid + 1;
            } else {
                hi = mid;
            }
        }
        if (lo == tails.size()) {
            tails.push_back(x);           /* 接在最长的后面，长度加一 */
        } else {
            tails[lo] = x;                /* 把这个长度的最小结尾改小 */
        }
    }

    run.length = tails.size();
    run.used = tails.size() + 1;          /* 长度 0 到 L 都用到了 */
    return run;
}

namespace {

void lis_dfs(const std::vector<int> &a, std::size_t index, int last, std::size_t length,
             std::size_t *best)
{
    if (index == a.size()) {
        *best = std::max(*best, length);
        return;
    }
    lis_dfs(a, index + 1, last, length, best);            /* 不选这个元素 */
    if (a[index] > last) {
        lis_dfs(a, index + 1, a[index], length + 1, best); /* 选，前提是比当前结尾大 */
    }
}

}   /* namespace */

std::size_t lis_bruteforce(const std::vector<int> &a)
{
    std::size_t best = 0;
    lis_dfs(a, 0, std::numeric_limits<int>::min(), 0, &best);
    return best;
}

std::vector<int> make_sequence(std::size_t n, int mul, int add, int modulo)
{
    std::vector<int> a;
    a.reserve(n);
    for (std::size_t i = 0; i < n; ++i) {
        const int index = static_cast<int>(i);
        a.push_back((index * mul + add) % modulo);
    }
    return a;
}

/* ===================================================================
 *  三、0/1 背包：二维表与滚动数组
 * =================================================================== */

std::vector<Item> make_items(std::size_t n)
{
    std::vector<Item> items;
    items.reserve(n);
    for (std::size_t i = 0; i < n; ++i) {
        const int index = static_cast<int>(i);
        Item one;
        one.weight = (index * 7) % 13 + 1;
        one.value = (index * 11) % 17 + 3;
        items.push_back(one);
    }
    return items;
}

namespace {

/** 二维表的公共实现。forward 只决定内层从小到大的写法，
    两种写法读写的都是上一行，结果必然相同 */
Knapsack knapsack_2d_impl(const std::vector<Item> &items, int capacity, bool forward)
{
    Knapsack run;
    run.items = items.size();
    run.capacity = capacity;
    run.forward = forward;

    const std::size_t rows = items.size() + 1;
    const std::size_t cols = static_cast<std::size_t>(capacity) + 1;
    run.states = rows * cols;
    run.bytes = rows * cols * sizeof(int);
    run.overhead = rows * sizeof(std::vector<int>);

    std::vector<std::vector<int>> dp(rows, std::vector<int>(cols, 0));
    for (std::size_t i = 1; i < rows; ++i) {
        const Item &one = items[i - 1];
        for (std::size_t step = 0; step < cols; ++step) {
            const std::size_t j = forward ? step : (cols - 1 - step);
            ++run.transitions;
            int best = dp[i - 1][j];      /* 不拿这一件 */
            if (static_cast<std::size_t>(one.weight) <= j) {
                const int take =
                    dp[i - 1][j - static_cast<std::size_t>(one.weight)] + one.value;
                if (take > best) {
                    best = take;          /* 拿这一件 */
                }
            }
            dp[i][j] = best;
        }
    }
    run.best = dp[rows - 1][cols - 1];
    return run;
}

}   /* namespace */

Knapsack knapsack_2d(const std::vector<Item> &items, int capacity)
{
    return knapsack_2d_impl(items, capacity, false);
}

Knapsack knapsack_2d_forward(const std::vector<Item> &items, int capacity)
{
    return knapsack_2d_impl(items, capacity, true);
}

Knapsack knapsack_rolling(const std::vector<Item> &items, int capacity)
{
    Knapsack run;
    run.items = items.size();
    run.capacity = capacity;
    run.forward = false;

    const std::size_t cols = static_cast<std::size_t>(capacity) + 1;
    run.states = cols;
    run.bytes = cols * sizeof(int);
    run.overhead = sizeof(std::vector<int>);

    std::vector<int> dp(cols, 0);
    /* 内层从大到小：dp[j - w] 读到的还是「上一件物品」的结果 */
    for (const Item &one : items) {
        for (int j = capacity; j >= one.weight; --j) {
            ++run.transitions;
            const std::size_t slot = static_cast<std::size_t>(j);
            const int take = dp[slot - static_cast<std::size_t>(one.weight)] + one.value;
            if (take > dp[slot]) {
                dp[slot] = take;
            }
        }
    }
    run.best = dp[static_cast<std::size_t>(capacity)];
    return run;
}

Knapsack knapsack_rolling_forward(const std::vector<Item> &items, int capacity)
{
    Knapsack run;
    run.items = items.size();
    run.capacity = capacity;
    run.forward = true;

    const std::size_t cols = static_cast<std::size_t>(capacity) + 1;
    run.states = cols;
    run.bytes = cols * sizeof(int);
    run.overhead = sizeof(std::vector<int>);

    std::vector<int> dp(cols, 0);
    /* 内层从小到大：dp[j - w] 可能已经是本件物品更新过的值，一件物品会被拿多次 */
    for (const Item &one : items) {
        for (int j = one.weight; j <= capacity; ++j) {
            ++run.transitions;
            const std::size_t slot = static_cast<std::size_t>(j);
            const int take = dp[slot - static_cast<std::size_t>(one.weight)] + one.value;
            if (take > dp[slot]) {
                dp[slot] = take;
            }
        }
    }
    run.best = dp[static_cast<std::size_t>(capacity)];
    return run;
}

Knapsack knapsack_unbounded(const std::vector<Item> &items, int capacity)
{
    Knapsack run;
    run.items = items.size();
    run.capacity = capacity;

    const std::size_t cols = static_cast<std::size_t>(capacity) + 1;
    run.states = cols;
    run.bytes = cols * sizeof(int);
    run.overhead = sizeof(std::vector<int>);

    /* 按容量递推：每个容量都把每件物品试一遍，允许同一件反复拿 */
    std::vector<int> dp(cols, 0);
    for (std::size_t j = 1; j < cols; ++j) {
        int best = dp[j];
        for (const Item &one : items) {
            ++run.transitions;
            if (static_cast<std::size_t>(one.weight) <= j) {
                const int take = dp[j - static_cast<std::size_t>(one.weight)] + one.value;
                if (take > best) {
                    best = take;
                }
            }
        }
        dp[j] = best;
    }
    run.best = dp[cols - 1];
    return run;
}

namespace {

void knapsack_dfs(const std::vector<Item> &items, std::size_t index, int remaining,
                  int value, int *best)
{
    if (index == items.size()) {
        *best = std::max(*best, value);
        return;
    }
    knapsack_dfs(items, index + 1, remaining, value, best);      /* 不拿 */
    if (items[index].weight <= remaining) {
        knapsack_dfs(items, index + 1, remaining - items[index].weight,
                     value + items[index].value, best);          /* 拿 */
    }
}

}   /* namespace */

int knapsack_bruteforce(const std::vector<Item> &items, int capacity)
{
    int best = 0;
    knapsack_dfs(items, 0, capacity, 0, &best);
    return best;
}

/* ---- 字节数：全部由 sizeof 现算 ---- */

std::size_t table_bytes_2d(std::size_t rows, std::size_t cols)
{
    return rows * cols * sizeof(int);
}

std::size_t table_bytes_1d(std::size_t cols)
{
    return cols * sizeof(int);
}

std::size_t vector_overhead_bytes(std::size_t rows)
{
    return rows * sizeof(std::vector<int>);
}

std::size_t sizeof_int()
{
    return sizeof(int);
}

std::size_t sizeof_vector_int()
{
    return sizeof(std::vector<int>);
}

std::size_t sizeof_size_t()
{
    return sizeof(std::size_t);
}

std::string ratio_text(std::size_t part, std::size_t whole)
{
    if (whole == 0) {
        return std::string(u8"分母为零");
    }
    /* 四舍五入到一位小数。整个过程只用整数，输出逐位可复现 */
    const std::size_t scaled = (part * 10 + whole / 2) / whole;
    std::ostringstream os;
    os << (scaled / 10) << '.' << (scaled % 10);
    return os.str();
}

/* ===================================================================
 *  报告
 * =================================================================== */

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

std::string denoms_text(const std::vector<int> &denoms)
{
    std::ostringstream os;
    os << '{';
    for (std::size_t i = 0; i < denoms.size(); ++i) {
        if (i != 0) {
            os << ", ";
        }
        os << denoms[i];
    }
    os << '}';
    return os.str();
}

std::string picked_text(const std::vector<int> &picked)
{
    if (picked.empty()) {
        return std::string(u8"一枚都没挑出来");
    }
    std::ostringstream os;
    for (std::size_t i = 0; i < picked.size(); ++i) {
        if (i != 0) {
            os << " + ";
        }
        os << picked[i];
    }
    return os.str();
}

/* ---- 第一段：贪心与 DP 对照 ---- */

void append_change(std::ostringstream &os)
{
    os << u8"一、零钱兑换：贪心与最优解对照\n";
    os << u8"  面额无限，每次挑不超过剩余金额的最大面额就是贪心；DP 求的是最少枚数\n";

    struct Sample {
        std::vector<int> denoms;
        int amount;
    };
    const std::vector<Sample> samples = {
        {{1, 3, 4}, 6},
        {{1, 2, 4, 8}, 15},
        {{1, 3, 9, 27}, 40},
    };

    for (const Sample &one : samples) {
        const ChangeCase c = run_change_case(one.denoms, one.amount);
        os << u8"  面额 " << denoms_text(one.denoms) << u8"，目标 " << one.amount << "\n";
        os << u8"    贪心：" << picked_text(c.greedy.picked) << u8"，共 " << c.greedy.coins
           << u8" 枚（贪心步数 " << c.greedy.steps << u8"，面额探查 " << c.greedy.probes
           << u8" 次）\n";
        os << u8"    最优：" << picked_text(c.dp.picked) << u8"，共 " << c.dp.coins
           << u8" 枚（状态 " << c.dp.states << u8" 格，转移 " << c.dp.transitions
           << u8" 次）\n";
        if (c.greedy_is_optimal()) {
            os << u8"    差 0 枚：这组面额上贪心就是最优的\n";
        } else {
            os << u8"    差 " << c.gap() << u8" 枚：这组面额上贪心不是最优的\n";
        }
    }

    /* 两边都凑不出的情形也要给出结果，否则「凑不出」会被误读成 0 枚 */
    {
        const ChangeCase c = run_change_case({3, 4}, 5);
        os << u8"  面额 " << denoms_text(c.denoms) << u8"，目标 " << c.amount << "\n";
        os << u8"    贪心：挑了 " << c.greedy.coins << u8" 枚之后剩余金额 "
           << c.greedy.remainder << u8"，再没有不超过它的面额，停在半路\n";
        os << u8"    最优：DP 表里 " << c.amount << u8" 那一格仍是哨兵值，判为凑不出\n";
        os << u8"    两边都判不可达：面额 {3, 4} 凑不出 5\n";
    }

    os << u8"  穷举扫描：面额组一律含 1，其余面额从 2 到 8 里取，金额扫 1 到 30\n";
    const GreedyScan scan = scan_greedy_systems(8, 30);
    os << u8"    面额组 " << scan.systems << u8" 组，组合 " << scan.combos << u8" 个\n";
    os << u8"    贪心是最优解的 " << (scan.combos - scan.fails) << u8" 个，贪心不是最优的 "
       << scan.fails << u8" 个\n";
    os << u8"    其中每个面额都是前一个整数倍的 " << scan.multiple_systems << u8" 组、组合 "
       << scan.multiple_combos << u8" 个，贪心失手 " << scan.multiple_fails << u8" 个\n";
    if (scan.has_first_fail) {
        os << u8"    第一处失败：面额 " << denoms_text(scan.first_fail_denoms) << u8" 凑 "
           << scan.first_fail_amount << u8"，贪心 " << scan.first_fail_greedy << u8" 枚，最优 "
           << scan.first_fail_best << u8" 枚\n";
    }
    os << u8"  贪心成立的条件：面额成倍数关系时贪心一定最优，这是一条充分条件\n";

    int greedy_coins = 0;
    int best_coins = 0;
    const int failure = first_greedy_failure({1, 5, 10, 25}, 100, &greedy_coins, &best_coins);
    os << u8"    面额 {1, 5, 10, 25} 不成倍数，金额 1 到 100 上却处处最优（第一处失败：";
    if (failure == 0) {
        os << u8"没有）\n";
    } else {
        os << failure << u8"）\n";
    }
    os << u8"    倍数关系只是充分条件：不成倍数也可能成立，成倍数则一定成立\n";
    os << u8"    面额里只要有一处不整齐，就得靠 DP 算，或者先把金额扫一遍验证贪心\n";
    os << "\n";
}

/* ---- 第二段：三种状态定义 ---- */

void print_lis_row(std::ostringstream &os, const std::string &name, const LisRun &run)
{
    os << "  " << pad_right(name, 32) << pad_right(std::to_string(run.states), 10)
       << pad_right(std::to_string(run.used), 10) << pad_right(std::to_string(run.transitions), 10)
       << pad_right(std::to_string(run.compares), 10) << run.length << "\n";
}

void print_lis_head(std::ostringstream &os)
{
    os << "  " << pad_right(u8"定义", 32) << pad_right(u8"状态槽位", 10)
       << pad_right(u8"用到格子", 10) << pad_right(u8"转移次数", 10)
       << pad_right(u8"比较次数", 10) << u8"结果\n";
}

void print_lis_table(std::ostringstream &os, const std::vector<int> &a)
{
    const LisRun by_index = lis_end_at_index(a);
    const LisRun by_value = lis_by_value_naive(a);
    const LisRun by_bit = lis_by_value_bit(a);
    const LisRun by_length = lis_by_length(a);

    print_lis_head(os);
    print_lis_row(os, u8"一、以下标 i 结尾（dp[i]）", by_index);
    print_lis_row(os, u8"二、以值 v 结尾（朴素扫描）", by_value);
    print_lis_row(os, u8"二、以值 v 结尾（树状数组）", by_bit);
    print_lis_row(os, u8"三、以长度 len 做下标（tails）", by_length);
}

void append_lis(std::ostringstream &os)
{
    os << u8"二、同一道题、三种状态定义：最长上升子序列\n";

    const std::vector<int> seq_a = {3, 1, 4, 1, 5, 9, 2, 6, 5, 3,
                                    5, 8, 9, 7, 9, 3, 2, 3, 8, 4};
    os << u8"  序列 A（20 个元素，元素抄自圆周率的小数位）\n";
    os << "    ";
    for (std::size_t i = 0; i < seq_a.size(); ++i) {
        os << (i == 0 ? "" : " ") << seq_a[i];
    }
    os << "\n";
    os << u8"  序列 B（200 个元素，a[i] = (i × 37 + 11) mod 101，值域 0 到 100）\n";
    const std::vector<int> seq_b = make_sequence(200, 37, 11, 101);

    os << u8"  定义一  以下标结尾：dp[i] 是以 a[i] 结尾的最长上升子序列长度，状态是下标\n";
    os << u8"  定义二  以值结尾：  f[v] 是以值 v 结尾的最长上升子序列长度，状态是值\n";
    os << u8"  定义三  以长度做下标：tails[len] 是长度为 len 的上升子序列里最小的结尾值\n";
    os << u8"  序列 A 的三种定义：\n";
    print_lis_table(os, seq_a);
    os << u8"  序列 B 的三种定义：\n";
    print_lis_table(os, seq_b);

    const LisRun a_index = lis_end_at_index(seq_a);
    const LisRun a_length = lis_by_length(seq_a);
    const LisRun b_value = lis_by_value_naive(seq_b);
    const LisRun b_bit = lis_by_value_bit(seq_b);
    const LisRun b_length = lis_by_length(seq_b);

    os << u8"  同一道题换定义，答案不能换：序列 A 的四种实现都给出 " << a_index.length
       << u8"，序列 B 的都给出 " << b_length.length << "\n";
    os << u8"  代价换得了：序列 A 上定义一的转移次数 " << a_index.transitions
       << u8"，定义三只有 " << a_length.transitions << u8"\n";
    os << u8"  值域一大，定义二的朴素扫描就贵了：序列 B 上 " << b_value.transitions
       << u8" 次，同一套定义换成树状数组只用 " << b_bit.transitions << u8" 次\n";

    const std::size_t cut = 18;
    const std::vector<int> prefix(seq_a.begin(), seq_a.begin() + static_cast<std::ptrdiff_t>(cut));
    const std::size_t brute = lis_bruteforce(prefix);
    os << u8"  定义换得了，结论换不了的另一处证据：把序列 A 的前 " << cut
       << u8" 个元素交给暴力枚举（每条子序列都试一遍），得到 " << brute
       << u8"，三种定义在同一个前缀上给出 " << lis_end_at_index(prefix).length << " / "
       << lis_by_value_naive(prefix).length << " / " << lis_by_length(prefix).length << "\n";
    os << "\n";
}

/* ---- 第三段：二维表与滚动数组 ---- */

void append_knapsack(std::ostringstream &os)
{
    const std::size_t item_count = 60;
    const int capacity = 1000;
    const std::vector<Item> items = make_items(item_count);

    os << u8"三、0/1 背包：从二维表到滚动数组\n";
    os << u8"  物品 " << item_count
       << u8" 件（重量 (i × 7) mod 13 + 1，价值 (i × 11) mod 17 + 3），容量 " << capacity
       << "\n";

    const Knapsack two = knapsack_2d(items, capacity);
    const Knapsack two_forward = knapsack_2d_forward(items, capacity);
    const Knapsack roll = knapsack_rolling(items, capacity);
    const Knapsack roll_forward = knapsack_rolling_forward(items, capacity);
    const Knapsack unbounded = knapsack_unbounded(items, capacity);

    const std::size_t rows = item_count + 1;
    const std::size_t cols = static_cast<std::size_t>(capacity) + 1;

    os << u8"  二维表 dp[i][j]：" << rows << u8" 行 × " << cols << u8" 列\n";
    os << u8"    状态格数 " << two.states << u8"，转移次数 " << two.transitions
       << u8"，最大价值 " << two.best << "\n";
    os << u8"    表格数据：" << rows << u8" × " << cols << u8" × sizeof(int) = " << rows << u8" × "
       << cols << u8" × " << sizeof_int() << " = " << two.bytes << u8" 字节\n";
    os << u8"    行控制块：" << rows << u8" × sizeof(std::vector<int>) = " << rows << u8" × "
       << sizeof_vector_int() << " = " << two.overhead << u8" 字节\n";
    os << u8"    合计 " << (two.bytes + two.overhead) << u8" 字节\n";

    os << u8"  滚动数组 dp[j]：1 行 × " << cols << u8" 列\n";
    os << u8"    状态格数 " << roll.states << u8"，转移次数 " << roll.transitions
       << u8"，最大价值 " << roll.best << u8"（与二维表相同）\n";
    os << u8"    表格数据：" << cols << u8" × sizeof(int) = " << cols << u8" × " << sizeof_int()
       << " = " << roll.bytes << u8" 字节\n";
    os << u8"    行控制块：1 × sizeof(std::vector<int>) = " << roll.overhead << u8" 字节\n";
    os << u8"    合计 " << (roll.bytes + roll.overhead) << u8" 字节\n";

    os << u8"  前后对照\n";
    os << u8"    状态格数 " << two.states << u8" 到 " << roll.states << u8"，是 "
       << ratio_text(two.states, roll.states) << u8" 分之一\n";
    os << u8"    字节数 " << (two.bytes + two.overhead) << u8" 到 "
       << (roll.bytes + roll.overhead) << u8"，是 "
       << ratio_text(two.bytes + two.overhead, roll.bytes + roll.overhead) << u8" 分之一\n";
    os << u8"    " << ratio_text(two.states, roll.states)
       << u8" 正好是行数（物品数加一）：滚动数组省掉的就是这一维\n";

    os << u8"  计算顺序为什么必须反过来\n";
    os << u8"    二维表内层从小到大 " << two_forward.best << u8"，从大到小 " << two.best
       << u8"：两者相同，因为它读写的是不同的行\n";
    os << u8"    滚动数组内层" << (roll.forward ? u8"从小到大" : u8"从大到小") << " " << roll.best
       << u8"，" << (roll_forward.forward ? u8"从小到大" : u8"从大到小") << " "
       << roll_forward.best << "\n";
    os << u8"    从小到大那个数等于「每件物品可以拿任意多次」的答案 " << unbounded.best
       << u8"（按容量递推的独立实现算出）\n";
    os << u8"    从小到大扫的时候 dp[j - w] 已经是本件物品更新过的值，等于一件拿了很多次\n";
    os << u8"    所以 0/1 背包的滚动数组内层必须从大到小，读到的是上一件物品的结果\n";
    os << u8"    完全背包正好相反：把它的内层从大到小改成从小到大\n";

    os << u8"  sizeof 实测：sizeof(int) = " << sizeof_int() << u8"，sizeof(std::vector<int>) = "
       << sizeof_vector_int() << u8"，sizeof(std::size_t) = " << sizeof_size_t() << "\n";
    os << u8"  行控制块的大小随实现与平台变化，因此这里一个数都没有写死\n";
    os << "\n";
}

/* ---- 第四段：结构量汇总 ---- */

void append_summary(std::ostringstream &os)
{
    const ChangeCase six = run_change_case({1, 3, 4}, 6);
    const GreedyScan scan = scan_greedy_systems(8, 30);

    const std::vector<int> seq_a = {3, 1, 4, 1, 5, 9, 2, 6, 5, 3,
                                    5, 8, 9, 7, 9, 3, 2, 3, 8, 4};
    const std::vector<int> seq_b = make_sequence(200, 37, 11, 101);

    const std::size_t item_count = 60;
    const int capacity = 1000;
    const std::vector<Item> items = make_items(item_count);
    const Knapsack two = knapsack_2d(items, capacity);
    const Knapsack roll = knapsack_rolling(items, capacity);

    os << u8"四、结构量汇总（全部由程序数出来，重跑逐位相同）\n";
    os << "  " << pad_right(u8"结构量", 44) << u8"值\n";
    os << "  " << pad_right(u8"贪心步数（面额 {1, 3, 4} 凑 6）", 44) << six.greedy.steps << "\n";
    os << "  " << pad_right(u8"贪心的面额探查次数（同一题）", 44) << six.greedy.probes << "\n";
    os << "  " << pad_right(u8"最优枚数（同一题，DP）", 44) << six.dp.coins << "\n";
    os << "  " << pad_right(u8"贪心比最优多用的枚数", 44) << six.gap() << "\n";
    os << "  " << pad_right(u8"零钱 DP 的状态数 / 转移次数", 44) << six.dp.states << " / "
       << six.dp.transitions << "\n";
    os << "  " << pad_right(u8"贪心失手的组合（扫描 " + std::to_string(scan.combos) + u8" 个）", 44)
       << scan.fails << "\n";
    os << "  " << pad_right(u8"倍数组里的失手组合（" + std::to_string(scan.multiple_combos) + u8" 个）",
                          44)
       << scan.multiple_fails << "\n";
    os << "  " << pad_right(u8"序列 A 的三种定义结果", 44) << lis_end_at_index(seq_a).length << " / "
       << lis_by_value_naive(seq_a).length << " / " << lis_by_length(seq_a).length << "\n";
    os << "  " << pad_right(u8"序列 B 的三种定义结果", 44) << lis_end_at_index(seq_b).length << " / "
       << lis_by_value_naive(seq_b).length << " / " << lis_by_length(seq_b).length << "\n";
    os << "  " << pad_right(u8"定义一的转移次数（序列 A / B）", 44)
       << lis_end_at_index(seq_a).transitions << " / " << lis_end_at_index(seq_b).transitions << "\n";
    os << "  " << pad_right(u8"定义三的转移次数（序列 A / B）", 44)
       << lis_by_length(seq_a).transitions << " / " << lis_by_length(seq_b).transitions << "\n";
    os << "  " << pad_right(u8"背包二维表字节数 / 滚动数组字节数", 44)
       << (two.bytes + two.overhead) << " / " << (roll.bytes + roll.overhead) << "\n";
    os << "  " << pad_right(u8"两者之比", 44)
       << ratio_text(two.bytes + two.overhead, roll.bytes + roll.overhead) << "\n";
    os << "  " << pad_right(u8"最长上升子序列的定义数 / 实现数", 44) << "3 / 4\n";
}

}   /* namespace */

std::string build_report()
{
    std::ostringstream os;
    append_change(os);
    append_lis(os);
    append_knapsack(os);
    append_summary(os);
    return os.str();
}

/* ===================================================================
 *  自测
 * =================================================================== */

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

    /* 1—5 零钱兑换的反例与成立例 */
    const ChangeCase six = run_change_case({1, 3, 4}, 6);
    checks.expect(six.greedy.reachable && six.greedy.coins == 3 && six.greedy.steps == 3 &&
                      six.greedy.picked == std::vector<int>({4, 1, 1}),
                  u8"贪心 {1, 3, 4} 凑 6：挑出 4 + 1 + 1，共 3 枚，贪心步数 3");
    checks.expect(six.dp.reachable && six.dp.coins == 2 &&
                      six.dp.picked == std::vector<int>({3, 3}),
                  u8"DP {1, 3, 4} 凑 6：最优 2 枚，回溯出的面额是 3 与 3");
    checks.expect_eq(six.gap(), 1, u8"贪心比最优多用 1 枚：这组面额上贪心不是最优");
    {
        const ChangeCase eight = run_change_case({1, 2, 4, 8}, 15);
        const ChangeCase nine = run_change_case({1, 3, 9, 27}, 40);
        checks.expect(eight.greedy_is_optimal() && eight.dp.coins == 4,
                      u8"面额成倍数 {1, 2, 4, 8} 凑 15：贪心与最优都是 4 枚");
        checks.expect(nine.greedy_is_optimal() && nine.dp.coins == 4,
                      u8"面额成倍数 {1, 3, 9, 27} 凑 40：贪心与最优都是 4 枚");
    }

    /* 6—9 DP 与另一条独立实现对照 */
    {
        const std::vector<std::vector<int>> systems = {{1, 3, 4}, {1, 2, 4, 8}, {3, 4}, {1, 5, 10, 25}};
        bool same = true;
        bool never_better = true;
        std::size_t checked = 0;
        for (const std::vector<int> &denoms : systems) {
            for (int amount = 0; amount <= 80; ++amount) {
                const ChangeDp d = dp_change(denoms, amount);
                std::size_t calls = 0;
                const int memo = min_coins_memo(denoms, amount, &calls);
                const int want = d.reachable ? d.coins : -1;
                if (memo != want) {
                    same = false;
                }
                const GreedyRun g = greedy_change(denoms, amount);
                if (g.reachable && d.reachable && g.coins < d.coins) {
                    never_better = false;
                }
                ++checked;
            }
        }
        checks.expect(same && checked == 4 * 81,
                      u8"dp_change 与带记忆递归在 4 组面额、金额 0 到 80 上逐位相同（324 个组合）");
        checks.expect(never_better, u8"贪心的枚数从来不会少于最优枚数：贪心只会多拿");
    }
    {
        const ChangeCase unreachable = run_change_case({3, 4}, 5);
        checks.expect(!unreachable.greedy.reachable && !unreachable.dp.reachable &&
                          unreachable.greedy.remainder == 1,
                      u8"面额 {3, 4} 凑 5：贪心停在剩余 1，DP 判不可达，两边一致");
    }

    /* 10—13 穷举扫描：倍数组零失手，倍数不是必要条件 */
    const GreedyScan scan = scan_greedy_systems(8, 30);
    checks.expect_eq_size(scan.combos, scan.systems * 30,
                          u8"扫描的组合数等于面额组数乘金额个数（128 × 30 = 3840）");
    checks.expect(scan.multiple_systems > 0 && scan.multiple_combos == scan.multiple_systems * 30,
                  u8"倍数组的组合数等于倍数组数乘金额个数");
    checks.expect_eq_size(scan.multiple_fails, 0,
                          u8"每个面额都是前一个整数倍的组里，贪心一次都没失手");
    checks.expect(scan.has_first_fail && scan.first_fail_denoms == std::vector<int>({1, 3, 4}) &&
                      scan.first_fail_amount == 6 && scan.first_fail_greedy == 3 &&
                      scan.first_fail_best == 2,
                  u8"扫描到的第一处失败是面额 {1, 3, 4} 凑 6：贪心 3 枚，最优 2 枚");
    {
        int g = 0;
        int b = 0;
        const int failure = first_greedy_failure({1, 5, 10, 25}, 100, &g, &b);
        checks.expect_eq(failure, 0,
                         u8"面额 {1, 5, 10, 25} 不成倍数，金额 1 到 100 上贪心却处处最优");
    }

    /* 14—19 三种状态定义 */
    const std::vector<int> seq_a = {3, 1, 4, 1, 5, 9, 2, 6, 5, 3,
                                    5, 8, 9, 7, 9, 3, 2, 3, 8, 4};
    const std::vector<int> seq_b = make_sequence(200, 37, 11, 101);
    {
        const LisRun index_run = lis_end_at_index(seq_a);
        const LisRun value_run = lis_by_value_naive(seq_a);
        const LisRun bit_run = lis_by_value_bit(seq_a);
        const LisRun length_run = lis_by_length(seq_a);
        checks.expect(index_run.length == value_run.length && value_run.length == bit_run.length &&
                          bit_run.length == length_run.length,
                      u8"序列 A 上四种实现给出同一个长度");
        checks.expect_eq_size(lis_end_at_index(seq_b).length, lis_by_length(seq_b).length,
                              u8"序列 B 上定义一与定义三的长度相同");

        const std::size_t cut = 18;
        const std::vector<int> prefix(seq_a.begin(),
                                      seq_a.begin() + static_cast<std::ptrdiff_t>(cut));
        checks.expect_eq_size(lis_bruteforce(prefix), lis_end_at_index(prefix).length,
                              u8"暴力枚举前 18 个元素的每条子序列，与定义一的结果相同");
    }
    {
        /* tails 的不变式：任何时候都是非降的 */
        std::vector<int> tails;
        bool sorted = true;
        for (const int x : seq_b) {
            std::size_t lo = 0;
            std::size_t hi = tails.size();
            while (lo < hi) {
                const std::size_t mid = lo + (hi - lo) / 2;
                if (tails[mid] < x) {
                    lo = mid + 1;
                } else {
                    hi = mid;
                }
            }
            if (lo == tails.size()) {
                tails.push_back(x);
            } else {
                tails[lo] = x;
            }
            if (!std::is_sorted(tails.begin(), tails.end())) {
                sorted = false;
            }
        }
        checks.expect(sorted && tails.size() == lis_by_length(seq_b).length,
                      u8"定义三的 tails 数组全程保持非降，末尾长度与报告一致");
    }
    checks.expect_eq_size(lis_end_at_index(seq_b).transitions, 200 * 199 / 2,
                          u8"定义一在序列 B 上的转移次数是 n(n-1)/2 = 19900");
    checks.expect_eq_size(lis_by_length(seq_b).transitions, 200,
                          u8"定义三在序列 B 上的定位次数等于元素个数 200");
    checks.expect(lis_by_value_bit(seq_b).transitions < lis_by_value_naive(seq_b).transitions,
                  u8"同一套「以值结尾」的定义，树状数组版的转移次数少于朴素扫描版");

    /* 20—24 背包：二维表、滚动数组与完全背包 */
    const std::vector<Item> big = make_items(60);
    const Knapsack two = knapsack_2d(big, 1000);
    const Knapsack two_forward = knapsack_2d_forward(big, 1000);
    const Knapsack roll = knapsack_rolling(big, 1000);
    const Knapsack roll_forward = knapsack_rolling_forward(big, 1000);
    const Knapsack unbounded = knapsack_unbounded(big, 1000);

    checks.expect(two.best == roll.best && two.best == two_forward.best,
                  u8"60 件物品、容量 1000：二维表与滚动数组给出同一个最大价值");
    checks.expect_eq(roll_forward.best, unbounded.best,
                     u8"滚动数组内层从小到大算出的数，等于完全背包独立实现的结果");
    checks.expect(roll_forward.best > roll.best,
                  u8"内层方向一换，答案变大：正序算的是每件可以拿多次");
    {
        const std::vector<Item> small = make_items(15);
        const int want = knapsack_bruteforce(small, 120);
        checks.expect(want == knapsack_2d(small, 120).best &&
                          want == knapsack_rolling(small, 120).best,
                      u8"15 件物品、容量 120：二维表与滚动数组都与暴力枚举子集相同");
    }

    /* 25—27 字节数：关系式必须成立 */
    {
        const std::size_t rows = 61;
        const std::size_t cols = 1001;
        checks.expect(two.bytes == table_bytes_1d(cols) * rows &&
                          roll.bytes == table_bytes_1d(cols) &&
                          two.overhead == roll.overhead * rows,
                      u8"二维表的字节数与行控制块都正好是滚动数组的 61 倍，倍数就是行数");
        checks.expect_eq_size(two.bytes + two.overhead,
                              table_bytes_2d(rows, cols) + vector_overhead_bytes(rows),
                              u8"报告里的背包字节数就是表数据加行控制块，没有另算");
        checks.expect_eq_size(roll.states, cols,
                              u8"滚动数组的状态格数就是容量加一（1001 格）");
    }

    /* 28—29 零钱 DP 的状态数与转移次数都是算式 */
    checks.expect_eq_size(six.dp.states, 7,
                          u8"零钱 DP 的状态数是金额加一（凑 6 是 7 格）");
    checks.expect_eq_size(six.dp.transitions, 6 * 3,
                          u8"零钱 DP 的转移次数是金额个数乘面额个数（6 × 3 = 18）");

    return checks.finish();
}

}   /* namespace gdp */
