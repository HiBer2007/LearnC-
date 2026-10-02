/**
 * greedy_dp.hpp —— 贪心与动态规划：同一道题的两条路
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
 * 报告里出现的每个数字都由这里数出来，重跑逐位相同：
 *
 *   steps        贪心步数：每挑出一枚硬币算一步
 *   probes       面额探查次数：每看一眼一个面额算一次
 *   states       状态槽位数：DP 表里一共有多少格
 *   used         结束时真正写过值的格子数
 *   transitions  转移次数：执行一次候选转移算一次
 *   compares     比较次数：元素之间比大小算一次
 *   bytes        表格占的字节数，一律用 sizeof 现算，不写死
 *
 * 三条线各自独立：
 *
 *   一、零钱兑换。同一组面额、同一个目标金额，贪心走一遍、DP 走一遍，
 *       把两边的枚数与步数都报出来；反例（{1, 3, 4} 凑 6）与成立例
 *       （{1, 2, 4, 8} 凑 15）并排给出，再用穷举扫描数出贪心失败的组合有多少。
 *
 *   二、最长上升子序列。同一道题写四种实现，状态定义分三族：
 *       以下标结尾、以值结尾、以长度做下标。四种实现必须给出同一个长度。
 *
 *   三、0/1 背包。二维表与滚动数组各跑一遍，字节数由 sizeof 算出，
 *       并把滚动数组内层正序（等于完全背包）与逆序（0/1 背包）的结果对照。
 */
#ifndef GREEDY_DP_HPP
#define GREEDY_DP_HPP

#include <cstddef>
#include <string>
#include <vector>

namespace gdp {

/* ================= 一、零钱兑换：贪心与 DP ================= */

/** 一次贪心的结果。面额无限，每次取不超过剩余金额的最大面额 */
struct GreedyRun {
    bool reachable = false;         /**< 能不能凑出这个金额 */
    int coins = 0;                  /**< 挑出的硬币枚数 */
    std::size_t steps = 0;          /**< 贪心步数：每挑出一枚算一步 */
    std::size_t probes = 0;         /**< 面额探查次数：每看一眼一个面额算一次 */
    int remainder = 0;              /**< 走完时的剩余金额；凑不出时大于 0 */
    std::vector<int> picked;        /**< 依次挑出的面额 */
};

/** 一次 DP 的结果：面额无限、求最少枚数 */
struct ChangeDp {
    bool reachable = false;
    int coins = 0;
    std::size_t states = 0;         /**< 状态槽位数，金额 0 到 amount 各一格 */
    std::size_t transitions = 0;    /**< 转移次数，每个状态对每个面额试一次 */
    std::vector<int> picked;        /**< 回溯出来的一组最优解，面额从大到小 */
};

/** 同一组面额、同一个金额上，两条路的结果并排放在一起 */
struct ChangeCase {
    std::vector<int> denoms;
    int amount = 0;
    GreedyRun greedy;
    ChangeDp dp;

    /** 两边都能凑出、且枚数相同，才算贪心在这组面额、这个金额上是最优的 */
    bool greedy_is_optimal() const
    {
        return greedy.reachable && dp.reachable && greedy.coins == dp.coins;
    }

    /** 贪心比最优多用的枚数；两边都不可达时是 0，要看 reachable 才作数 */
    int gap() const { return greedy.coins - dp.coins; }
};

GreedyRun greedy_change(const std::vector<int> &denoms, int amount);

ChangeDp dp_change(const std::vector<int> &denoms, int amount);

ChangeCase run_change_case(const std::vector<int> &denoms, int amount);

/** 交叉验证用：带记忆的递归，和 dp_change 是两条独立的代码路径。
    calls 记下递归真正展开的状态数；返回最少枚数，不可达时返回 -1 */
int min_coins_memo(const std::vector<int> &denoms, int amount, std::size_t *calls);

/** 穷举扫描：面额组一律含 1，其余面额从 2 到 max_denom 里取，金额扫 1 到 max_amount */
struct GreedyScan {
    std::size_t systems = 0;            /**< 面额组数 */
    std::size_t multiple_systems = 0;   /**< 其中每个面额都是前一个整数倍的组数 */
    std::size_t combos = 0;             /**< （面额组，金额）组合总数 */
    std::size_t multiple_combos = 0;    /**< 倍数组贡献的组合数 */
    std::size_t fails = 0;              /**< 贪心枚数多于最优枚数的组合数 */
    std::size_t multiple_fails = 0;     /**< 倍数组里失败的组合数 */

    bool has_first_fail = false;
    std::vector<int> first_fail_denoms;
    int first_fail_amount = 0;
    int first_fail_greedy = 0;
    int first_fail_best = 0;
};

GreedyScan scan_greedy_systems(int max_denom, int max_amount);

/** 某一组面额在金额 1 到 max_amount 上，第一处贪心不是最优的金额；
    处处最优时返回 0。greedy_coins 与 best_coins 给出那一处的两个枚数 */
int first_greedy_failure(const std::vector<int> &denoms, int max_amount,
                         int *greedy_coins, int *best_coins);

/* ================= 二、最长上升子序列的三种状态定义 ================= */

/** 一种实现跑完之后的代价与结果。定义见 greedy_dp.cpp 里各函数的注释 */
struct LisRun {
    std::size_t length = 0;         /**< 最长严格上升子序列的长度 */
    std::size_t states = 0;         /**< 状态槽位数：这套定义一共需要多少格 */
    std::size_t used = 0;           /**< 结束时写过值的格子数 */
    std::size_t transitions = 0;    /**< 转移次数 */
    std::size_t compares = 0;       /**< 比较次数 */
};

/** 定义一：dp[i] 是以 a[i] 结尾的最长上升子序列长度，状态是下标 */
LisRun lis_end_at_index(const std::vector<int> &a);

/** 定义二：f[v] 是以值 v 结尾的最长上升子序列长度，状态是值。
    每个元素扫一遍比它小的值 */
LisRun lis_by_value_naive(const std::vector<int> &a);

/** 定义二换个实现：树状数组维护前缀最大值，查询从 O(V) 降到 O(log V)。
    状态定义与上一个函数完全相同，代价不同 */
LisRun lis_by_value_bit(const std::vector<int> &a);

/** 定义三：tails[len] 是长度为 len 的上升子序列里最小的结尾值，状态是长度 */
LisRun lis_by_length(const std::vector<int> &a);

/** 暴力：递归枚举每一条子序列，只用来交叉验证，元素个数不宜超过 20 */
std::size_t lis_bruteforce(const std::vector<int> &a);

/** 固定序列：a[i] = (i * mul + add) % modulo。重复运行逐位相同，不用随机数 */
std::vector<int> make_sequence(std::size_t n, int mul, int add, int modulo);

/* ================= 三、0/1 背包：二维表与滚动数组 ================= */

struct Item {
    int weight = 0;
    int value = 0;
};

struct Knapsack {
    std::size_t items = 0;
    int capacity = 0;
    int best = 0;                   /**< 最大价值 */
    std::size_t states = 0;         /**< 状态格数 */
    std::size_t transitions = 0;    /**< 转移次数 */
    std::size_t bytes = 0;          /**< 表格数据占的字节数，用 sizeof 算出 */
    std::size_t overhead = 0;       /**< 每行一个控制块，这部分也照样算出来 */
    bool forward = false;           /**< 内层是否从小到大；只对滚动数组有意义 */
};

/** 固定物品：重量 (i * 7) % 13 + 1，价值 (i * 11) % 17 + 3 */
std::vector<Item> make_items(std::size_t n);

/** 二维表 dp[i][j]：前 i 件物品、容量不超过 j 的最大价值 */
Knapsack knapsack_2d(const std::vector<Item> &items, int capacity);

/** 二维表，内层从小到大。读写的是不同的行，方向不影响结果，
    这一点与滚动数组正好相反 */
Knapsack knapsack_2d_forward(const std::vector<Item> &items, int capacity);

/** 滚动数组，内层从大到小：0/1 背包的正确写法 */
Knapsack knapsack_rolling(const std::vector<Item> &items, int capacity);

/** 滚动数组，内层从小到大：算出来的是完全背包 */
Knapsack knapsack_rolling_forward(const std::vector<Item> &items, int capacity);

/** 完全背包的独立实现：按容量递推，每件物品可以拿任意多次 */
Knapsack knapsack_unbounded(const std::vector<Item> &items, int capacity);

/** 暴力枚举子集，只用来交叉验证，物品数不宜超过 20 */
int knapsack_bruteforce(const std::vector<Item> &items, int capacity);

/* ---- 字节数：一律用 sizeof 现算，函数名就是算式 ---- */

std::size_t table_bytes_2d(std::size_t rows, std::size_t cols);
std::size_t table_bytes_1d(std::size_t cols);
std::size_t vector_overhead_bytes(std::size_t rows);
std::size_t sizeof_int();
std::size_t sizeof_vector_int();
std::size_t sizeof_size_t();

/** 两个字节数的比值，写成一位小数，例如 "61.0"。用整数算，不引入浮点 */
std::string ratio_text(std::size_t part, std::size_t whole);

/* ================= 报告与自测 ================= */

/** 自测结果。不打印，交给界面决定怎么显示 */
struct CheckResult {
    std::size_t total = 0;
    std::size_t passed = 0;
    std::size_t failed = 0;
    std::vector<std::string> lines;   /**< 每项一行，例如 "[通过] 3. ……" */

    bool all_passed() const { return failed == 0; }
    std::string summary() const;      /**< "29 项中 29 项通过，全部通过" */
};

/** 项目输出：贪心与 DP 对照、三种状态定义、滚动数组、结构量汇总四段。
    返回多行 UTF-8 文本，末尾带一个换行；由界面层决定怎么显示 */
std::string build_report();

/** 逐项核对贪心、DP、三种状态定义与滚动数组 */
CheckResult run_self_tests();

}   /* namespace gdp */

#endif /* GREEDY_DP_HPP */
