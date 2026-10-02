/* greedydp.cpp —— 练习模板 09 的实现（C++）
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
 * 本模板的 4 处 TODO 全在这个文件里，按阶段编号：
 *
 *     阶段 1-1   interval_schedule   循环体里的取舍与记账
 *     阶段 2-1   greedy_knapsack     这一件收不收
 *     阶段 3-1   knapsack_dp         这一格的转移
 *     阶段 4-1   knapsack_rolling    容量这一维往哪边走
 *
 * 每个 TODO 上面写明「要做什么」，下面的「判据」一行给出填完之后
 * 应当看到的数——那些数都由 src/main_cli.cpp 打印，逐个对得上才算做完。
 */
#include "greedydp.hpp"

#include <algorithm>
#include <ostream>

namespace gdp {

/* ==================================================================
 * 已给出：四个排序器
 *
 * 同键时一律用原始下标收尾，这样排序结果与算法库的实现无关，
 * 同一份数据重跑逐位相同。
 * ================================================================== */

void sort_by_right(std::vector<Interval> &iv)
{
    std::sort(iv.begin(), iv.end(), [](const Interval &a, const Interval &b) {
        if (a.hi != b.hi) {
            return a.hi < b.hi;
        }
        return a.index < b.index;
    });
}

void sort_by_left(std::vector<Interval> &iv)
{
    std::sort(iv.begin(), iv.end(), [](const Interval &a, const Interval &b) {
        if (a.lo != b.lo) {
            return a.lo < b.lo;
        }
        return a.index < b.index;
    });
}

void sort_by_length(std::vector<Interval> &iv)
{
    std::sort(iv.begin(), iv.end(), [](const Interval &a, const Interval &b) {
        const int la = a.hi - a.lo;
        const int lb = b.hi - b.lo;
        if (la != lb) {
            return la < lb;
        }
        return a.index < b.index;
    });
}

void sort_by_unit_value(std::vector<Item> &items)
{
    std::sort(items.begin(), items.end(), [](const Item &a, const Item &b) {
        /* 用交叉相乘代替除法，免得浮点误差改变次序 */
        const long long lhs = static_cast<long long>(a.value) * b.weight;
        const long long rhs = static_cast<long long>(b.value) * a.weight;
        if (lhs != rhs) {
            return lhs > rhs;
        }
        return a.index < b.index;
    });
}

bool overlaps(const Interval &a, const Interval &b)
{
    return a.lo < b.hi && b.lo < a.hi;
}

/* ==================================================================
 * 已给出：打印工具
 * ================================================================== */

std::string indices_to_string(const std::vector<int> &idx)
{
    if (idx.empty()) {
        return "(none)";
    }
    std::string s;
    for (std::size_t i = 0; i < idx.size(); ++i) {
        if (i != 0) {
            s += ' ';
        }
        s += std::to_string(idx[i]);
    }
    return s;
}

void print_intervals(const std::vector<Interval> &iv, std::ostream &os)
{
    for (std::size_t i = 0; i < iv.size(); ++i) {
        os << "  [" << iv[i].index << "] [" << iv[i].lo << ", " << iv[i].hi << ")\n";
    }
}

void print_items(const std::vector<Item> &items, std::ostream &os)
{
    for (std::size_t i = 0; i < items.size(); ++i) {
        os << "  [" << items[i].index << "] w=" << items[i].weight
           << " v=" << items[i].value << "\n";
    }
}

/* ==================================================================
 * 阶段 1：区间调度
 * ================================================================== */

std::vector<int> interval_schedule(const std::vector<Interval> &iv)
{
    std::vector<Interval> sorted = iv;
    sort_by_right(sorted);      /* 已给出：按右端点升序，同键时按下标 */

    std::vector<int> picked;

    for (std::size_t i = 0; i < sorted.size(); ++i) {
        const Interval &cur = sorted[i];

        /* TODO（阶段 1-1）：
         * 决定这一条区间收不收，收下之后还要给下一条留下判断的依据。
         * 判断依据的是「这一条与上一条收下的那一条在位置上是什么关系」；
         * 而留给下一条的那个量，是一个数，不是一整条区间——想清楚它是
         * 这一条的哪一端，以及它在循环开始前应当取什么初值。
         * 收下的那条要把它自己的原始下标留下，别把排序后的位置当成下标。
         * 判据（见《配置步骤.md》阶段 1）：schedule count 是 5，
         *       schedule indices 是 1 2 4 5 6。 */
        (void)cur;              /* 占位实现：这一行随 TODO 一起删掉 */
    }

    return picked;
}

/* ---------------------------------------------------------------- 错误策略 */

/* 已给出：错误策略 A —— 按左端点升序。
 * 做法：收下当前开始最早的那一条，再把所有与它重叠的区间从候选里划掉，
 * 对剩下的重复。每一步都从「还剩什么」重新开始，不留跨轮次的量。 */
std::vector<int> schedule_by_left(const std::vector<Interval> &iv)
{
    std::vector<Interval> sorted = iv;
    sort_by_left(sorted);

    std::vector<int> picked;
    std::vector<Interval> rest = sorted;

    while (!rest.empty()) {
        const Interval cur = rest.front();
        picked.push_back(cur.index);

        std::vector<Interval> keep;
        for (std::size_t i = 1; i < rest.size(); ++i) {
            if (!overlaps(cur, rest[i])) {
                keep.push_back(rest[i]);
            }
        }
        rest.swap(keep);
    }
    return picked;
}

/* 已给出：错误策略 B —— 按区间长度升序。
 * 做法：从最短的开始逐条试放，跟已经收下的每一条都验一遍，
 * 都不重叠才收下。 */
std::vector<int> schedule_by_length(const std::vector<Interval> &iv)
{
    std::vector<Interval> sorted = iv;
    sort_by_length(sorted);

    std::vector<Interval> chosen;
    for (std::size_t i = 0; i < sorted.size(); ++i) {
        bool fits = true;
        for (std::size_t k = 0; k < chosen.size(); ++k) {
            if (overlaps(chosen[k], sorted[i])) {
                fits = false;
                break;
            }
        }
        if (fits) {
            chosen.push_back(sorted[i]);
        }
    }

    std::vector<int> picked;
    for (std::size_t k = 0; k < chosen.size(); ++k) {
        picked.push_back(chosen[k].index);
    }
    return picked;
}

/* ==================================================================
 * 阶段 2：0/1 背包的单位价值贪心
 * ================================================================== */

KnapsackResult greedy_knapsack(const std::vector<Item> &items, int capacity)
{
    std::vector<Item> sorted = items;
    sort_by_unit_value(sorted);     /* 已给出：单位价值高的排在前面 */

    KnapsackResult r;
    int remain = capacity;          /* 已给出：还剩多少容量 */

    for (std::size_t i = 0; i < sorted.size(); ++i) {
        const Item &it = sorted[i];

        /* TODO（阶段 2-1）：
         * 这一件收不收，看的是它自己的重量与「还剩多少容量」的关系。
         * 收不下的时候要决定：是停下来不看了，还是跳过它接着看后面的。
         * 两种选择都会让程序跑完，但只有一种能让贪心尽量多拿——
         * 想清楚停下来意味着什么。
         * 判据（见《配置步骤.md》阶段 2）：greedy value 是 36，
         *       greedy weight 是 20，greedy taken 是 0 1 4 7。 */
        (void)remain;           /* 占位实现：这一行随 TODO 一起删掉 */

        /* 已给出：收下之后的记账 */
        r.taken.push_back(it.index);
        r.value += it.value;
        r.weight += it.weight;
        remain -= it.weight;
    }

    return r;
}

int knapsack_brute_force(const std::vector<Item> &items, int capacity)
{
    int best = 0;
    const unsigned long long total = 1ULL << items.size();
    for (unsigned long long mask = 0; mask < total; ++mask) {
        int w = 0;
        int v = 0;
        for (std::size_t i = 0; i < items.size(); ++i) {
            if ((mask >> i) & 1ULL) {
                w += items[i].weight;
                v += items[i].value;
            }
        }
        if (w <= capacity && v > best) {
            best = v;
        }
    }
    return best;
}

/* ==================================================================
 * 阶段 3：二维表的动态规划
 * ================================================================== */

DpResult knapsack_dp(const std::vector<Item> &items, int capacity)
{
    DpResult r;
    const int n = static_cast<int>(items.size());

    std::vector<std::vector<int> > dp(static_cast<std::size_t>(n) + 1,
                                      std::vector<int>(static_cast<std::size_t>(capacity) + 1, 0));

    /* 已给出：第 0 行——一件物品都不拿，任何容量下的价值都是 0。
     * 每写一格让 cells 加一，这样它数出来的就是填表碰过的格子数。 */
    for (int c = 0; c <= capacity; ++c) {
        dp[0][static_cast<std::size_t>(c)] = 0;
        ++r.cells;
    }

    for (int i = 1; i <= n; ++i) {
        for (int c = 0; c <= capacity; ++c) {
            ++r.cells;          /* 已给出：这一格总要算一遍 */

            /* TODO（阶段 3-1）：
             * dp[i][c] 是「只考虑前 i 件、容量为 c」时的最大价值。
             * 第 i 件物品有两个前途：不收它，答案就是少一件、同容量的那个格子；
             * 收它，就要先从容量里腾出它的重量，再看腾出来的那个格子。
             * 两个前途取更好的那个，写进这一格。收不下的时候只剩一个前途。
             * 那两个格子的行号与列号分别是什么，是这一步的关键。
             * 判据（见《配置步骤.md》阶段 3）：dp best 是 41，cells 是 210，
             *       chosen 是 1 3，且 dp best 与穷举出来的最优值相同。 */
        }
    }

    /* 已给出：从头走一遍，看最优解里到底收了哪几件。
     * 这一格如果与「不收这一件」的那一格相等，说明这一件没进最优解。 */
    int c = capacity;
    for (int i = n; i >= 1 && c >= 0; --i) {
        const Item &it = items[static_cast<std::size_t>(i - 1)];
        if (dp[static_cast<std::size_t>(i)][static_cast<std::size_t>(c)] !=
            dp[static_cast<std::size_t>(i - 1)][static_cast<std::size_t>(c)]) {
            r.chosen.push_back(it.index);
            c -= it.weight;
        }
    }
    /* 已给出：回溯是从后往前走的，翻过来变成从小到大的下标次序 */
    std::reverse(r.chosen.begin(), r.chosen.end());

    r.best = dp[static_cast<std::size_t>(n)][static_cast<std::size_t>(capacity)];
    return r;
}

/* ==================================================================
 * 阶段 4：滚动数组
 * ================================================================== */

RollingResult knapsack_rolling(const std::vector<Item> &items, int capacity)
{
    RollingResult r;
    std::vector<int> dp(static_cast<std::size_t>(capacity) + 1, 0);
    r.cells = static_cast<long long>(dp.size());    /* 已给出：用掉的格子数就是数组长度 */

    for (std::size_t i = 0; i < items.size(); ++i) {
        const Item &it = items[i];
        (void)it;               /* 占位实现：这一行随 TODO 一起删掉 */

        /* TODO（阶段 4-1）：
         * 一维数组只有一行，它同时扮演「上一件物品那一行」与「这一件物品这一行」。
         * 要让每一格用到的都是上一件物品留下的结果，容量这一维就得挑一个方向走；
         * 换一个方向，用到的格子刚刚才被这一件物品改过，那已经不是 0/1 背包了
         * （给出的对照版本走的就是那个方向，它算的是完全背包）。
         * 想清楚：从哪一格开始、到哪一格停、每次走几格。
         * 判据（见《配置步骤.md》阶段 4）：rolling best 与二维版相同，都是 41；
         *       rolling array cells 是 21；unbounded best 是 45。 */
    }

    r.best = dp[static_cast<std::size_t>(capacity)];
    return r;
}

RollingResult knapsack_unbounded(const std::vector<Item> &items, int capacity)
{
    RollingResult r;
    std::vector<int> dp(static_cast<std::size_t>(capacity) + 1, 0);
    r.cells = static_cast<long long>(dp.size());

    for (std::size_t i = 0; i < items.size(); ++i) {
        const Item &it = items[i];
        for (int c = it.weight; c <= capacity; ++c) {       /* 已给出：正序 */
            const int cand = dp[static_cast<std::size_t>(c - it.weight)] + it.value;
            if (cand > dp[static_cast<std::size_t>(c)]) {
                dp[static_cast<std::size_t>(c)] = cand;
            }
        }
    }

    r.best = dp[static_cast<std::size_t>(capacity)];
    return r;
}

} /* namespace gdp */
