/* greedydp.hpp —— 练习模板 09 的核心接口（C++）
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
 * 贪心与动态规划拆成 4 个阶段，每个阶段的实现写在 src/greedydp.cpp 里：
 *
 *     阶段 1  interval_schedule   区间调度：按什么次序选、选完维护哪个量
 *     阶段 2  greedy_knapsack     单位价值贪心：拿不下的时候怎么办
 *     阶段 3  knapsack_dp         二维表：这一格的转移怎么写
 *     阶段 4  knapsack_rolling    滚动数组：用哪一行的数据、容量往哪边走
 *
 * 本模板只碰这两件事本身：数据结构就是普通的聚合结构体与 vector，
 * 不涉及容器与数据结构的实现——那些属于 09-高阶数据结构 板块。
 */
#ifndef GREEDYDP_HPP
#define GREEDYDP_HPP

#include <cstddef>
#include <iosfwd>
#include <string>
#include <vector>

namespace gdp {

/* ==================================================================
 * 已给出：两个问题各自的数据形状
 * ================================================================== */

/* 一个区间，左闭右开：占住 [lo, hi) 这一段。
 * index 是它在输入里的原始下标，排序之后就靠它认回原来是谁。 */
struct Interval {
    int lo = 0;
    int hi = 0;
    int index = 0;
};

/* 背包里的一件物品。index 的含义同上。 */
struct Item {
    int weight = 0;
    int value = 0;
    int index = 0;
};

/* ==================================================================
 * 已给出：四个排序器
 *
 * 三种区间次序各有一个排序器，学生要判断该用哪一个；
 * 物品的排序器按单位价值（value / weight）从高到低排。
 * 四个排序器都用原始下标做同键时的次序，因此重跑逐位相同。
 * ================================================================== */

void sort_by_right(std::vector<Interval> &iv);   /* 右端点升序 */
void sort_by_left(std::vector<Interval> &iv);    /* 左端点升序 */
void sort_by_length(std::vector<Interval> &iv);  /* 区间长度升序 */
void sort_by_unit_value(std::vector<Item> &items);

/* ==================================================================
 * 已给出：两条区间是否重叠
 *
 * 左闭右开，因此端点相接（a.hi 等于 b.lo）不算重叠。
 * ================================================================== */
bool overlaps(const Interval &a, const Interval &b);

/* ==================================================================
 * 已给出：打印工具
 * ================================================================== */

/* 把下标序列拼成一行，例如 "1 2 4 5 6"；空序列拼成 "(none)" */
std::string indices_to_string(const std::vector<int> &idx);

/* 把区间按给定次序逐行打印，每行形如 "  [0] [1, 100)" */
void print_intervals(const std::vector<Interval> &iv, std::ostream &os);

/* 把物品按给定次序逐行打印，每行形如 "  [0] w=4 v=9" */
void print_items(const std::vector<Item> &items, std::ostream &os);

/* ==================================================================
 * 阶段 1：区间调度
 * ================================================================== */

/* 选出尽量多互不重叠的区间，返回它们的原始下标，按选中的先后排列。
 * 已给出：按右端点升序排好之后的遍历骨架；
 * 留空的是循环体里的取舍与记账（TODO 阶段 1-1）。
 * 判据：区间个数是 5，下标序列是 1 2 4 5 6。 */
std::vector<int> interval_schedule(const std::vector<Interval> &iv);

/* 已给出：两个错误策略，只作对照，学生不必写也不必改。
 * schedule_by_left   按左端点升序之后套同一个「不重叠就收」的想法
 * schedule_by_length 按区间长度升序之后逐条试放 */
std::vector<int> schedule_by_left(const std::vector<Interval> &iv);
std::vector<int> schedule_by_length(const std::vector<Interval> &iv);

/* ==================================================================
 * 阶段 2：0/1 背包的单位价值贪心
 * ================================================================== */

struct KnapsackResult {
    int value = 0;              /* 收下的物品价值之和 */
    int weight = 0;             /* 收下的物品重量之和 */
    std::vector<int> taken;     /* 收下的物品的原始下标，按收下的先后排列 */
};

/* 按单位价值从高到低依次考虑每一件，拿不下就跳过，继续看后面的。
 * 已给出：排序、遍历骨架、收下之后的记账；
 * 留空的是「这一件收不收」（TODO 阶段 2-1）。
 * 判据：value 是 36，weight 是 20，taken 是 0 1 4 7；
 *       它比真正的最优值 41 少 5。 */
KnapsackResult greedy_knapsack(const std::vector<Item> &items, int capacity);

/* 已给出：穷举所有子集算出 0/1 背包的真正最优值，只用于对照。
 * 物品件数在 20 以内才用得起，它不是动态规划。 */
int knapsack_brute_force(const std::vector<Item> &items, int capacity);

/* ==================================================================
 * 阶段 3：二维表的动态规划
 * ================================================================== */

struct DpResult {
    int best = 0;               /* 最优值 */
    long long cells = 0;        /* 填表一共碰过多少格（结构量） */
    std::vector<int> chosen;    /* 最优解里选中的物品原始下标，从小到大 */
};

/* 二维 dp 表：dp[i][c] 表示只考虑前 i 件、容量为 c 时的最大价值。
 * 已给出：表的声明与第 0 行的初值、填表的双重循环、走完之后的回溯；
 * 留空的是循环体里那一格的转移（TODO 阶段 3-1）。
 * 判据：best 是 41，cells 是 210，chosen 是 1 3。 */
DpResult knapsack_dp(const std::vector<Item> &items, int capacity);

/* ==================================================================
 * 阶段 4：滚动数组
 * ================================================================== */

struct RollingResult {
    int best = 0;               /* 最优值 */
    long long cells = 0;        /* 用掉的格子数：一维数组的长度（结构量） */
};

/* 只留一行的 0/1 背包。
 * 已给出：一维数组与它的初值、外层循环；
 * 留空的是内层容量维的方向与转移（TODO 阶段 4-1）。
 * 判据：best 与二维版相同，都是 41；cells 是 21，二维版是 210。 */
RollingResult knapsack_rolling(const std::vector<Item> &items, int capacity);

/* 已给出：同一个一维数组，但容量维度正序遍历。
 * 这样算出来的不是 0/1 背包——同一件物品会被反复收下。
 * 拿它当对照：best 是 45，比 0/1 背包的 41 大。 */
RollingResult knapsack_unbounded(const std::vector<Item> &items, int capacity);

} /* namespace gdp */

#endif /* GREEDYDP_HPP */
