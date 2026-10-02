/**
 * search_lab.hpp —— 查找：从线性到索引
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
 * 报告里出现的每个数字都由这里的计数器产出，重跑逐位相同：
 *
 *   iterations   循环体执行的次数，也就是「查找步数」
 *   comparisons  拿元素（或键）与目标值比较的次数
 *   bounds       与元素无关的边界判断次数：线性查找的下标上界判断、
 *                插值查找的区间守卫都记在这一项
 *
 * 表里的值与查找的目标一律用 long long：插值查找那一节要放 2 的幂，
 * 2^62 用 int 装不下，统一成一种宽度之后，同一张表可以交给任何写法去查。
 *
 * 三种二分写法读的是同一份数据，比较次数的差别只来自边界怎么处理，
 * 与数据本身无关。哈希索引是手写的开放寻址表：iterations 记探查过的槽数，
 * comparisons 记与槽里键的比较次数——探到空槽的那一次不算键比较。
 */
#ifndef SEARCH_LAB_HPP
#define SEARCH_LAB_HPP

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace slab {

/* ================= 结构量 ================= */

/** 未找到时返回的位置 */
constexpr std::size_t kNotFound = static_cast<std::size_t>(-1);

/** 一次查找付了多少钱。三个计数都是数出来的，不是估的 */
struct SearchStats {
    std::size_t iterations = 0;   /**< 循环体执行的次数，也就是查找步数 */
    std::size_t comparisons = 0;  /**< 与元素（或键）比较的次数 */
    std::size_t bounds = 0;       /**< 与元素无关的边界判断次数 */
};

/** 一次查找的结果。index 的含义随写法不同，见各函数的说明 */
struct SearchOutcome {
    std::size_t index = kNotFound;
    bool found = false;
    SearchStats stats;
};

using Table = std::vector<long long>;

/* ================= 数据构造 ================= */

/** 等差表：a[i] = first + step * i，元素互不相同且已升序 */
Table make_arithmetic_table(std::size_t n, long long step, long long first);

/** 分块重复表：每 block 个位置放同一个值，值按 step 递增 */
Table make_blocked_table(std::size_t n, std::size_t block, long long step);

/** 均匀分布表：a[i] = step * i，供插值查找用 */
Table make_uniform_table(std::size_t n, long long step);

/** 等比表：a[i] = 2 的 i 次方。n 不得超过 63，否则最后一次移位会溢出 */
Table make_power_table(std::size_t n);

/** 用 Lehmer 生成器造 n 个互不相同的正数键。
    递推式 x ← 48271 × x mod 2147483647，非零种子在 2^31 − 2 步之内不重复，
    因此取前几万个键一定互不相同。同一个种子必然给出同一串键 */
Table make_lehmer_keys(std::size_t n, unsigned int seed);

/* ================= 线性查找 ================= */

/** 普通线性查找：每轮判断下标有没有越界，再判断这个元素是不是目标值。
    index 是命中的下标，未命中时为 kNotFound */
SearchOutcome linear_plain(const Table &table, long long target);

/** 哨兵线性查找：把目标值写进副本末尾多出来的那一格，循环里只判断元素。
    每轮的下标上界判断因此归零，代价是未命中时要与哨兵多比一次。
    未命中时 index 等于哨兵槽的下标，也就是原表长，found 为假 */
SearchOutcome linear_sentinel(const Table &table, long long target);

/* ================= 二分查找的三种边界写法 ================= */

/** 闭区间写法。循环不变式：若目标值在表里，它的下标一定落在闭区间 [lo, hi] 内；
    lo > hi 时候选范围为空。内部用带符号下标，因为 hi = mid − 1 会取到 −1 */
SearchOutcome binary_closed(const Table &table, long long target);

/** 半开区间写法。循环不变式：候选范围是 [lo, hi)，hi 指向范围之后的第一格；
    lo == hi 时范围为空。命中即返回，未命中时 index 为 kNotFound */
SearchOutcome binary_half_open(const Table &table, long long target);

/** 下界写法：返回第一个满足 table[pos] >= target 的位置，pos 可能等于表长。
    循环不变式：[0, lo) 里的值都小于目标值，[hi, n) 里的值都不小于目标值，
    lo == hi 时答案就是 lo。每轮只做一次元素比较，收尾再比一次判断是否相等，
    相等时 found 为真。index 在未命中时是插入点，不是 kNotFound */
SearchOutcome lower_bound_probe(const Table &table, long long target);

/* ================= 插值查找 ================= */

/** 插值查找：按值的比例估计下一格的位置，而不是一律取中点。
    只对升序表有效；两端的值相等时插值公式会除以 0，实现里对这种情况做了保护。
    每轮另外做两次区间守卫判断（目标值落在两端之外就收工），记在 bounds 里；
    index 在未命中时是插入点 */
SearchOutcome interpolation_search(const Table &table, long long target);

/* ================= 手写的哈希索引 ================= */

/** 开放寻址 + 线性探查的哈希索引。表长取不小于 2n 的 2 的幂，
    装载因子因此不超过 0.5。插入的键必须互不相同 */
class HashIndex {
public:
    /** 散列策略。Mixed 走 64 位混合，IdentityMask 直接取键的低位，
        后者用来演示坏散列函数会把开放寻址退化成顺序扫描 */
    enum class Hash { Mixed, IdentityMask };

    explicit HashIndex(std::size_t expected_keys, Hash kind = Hash::Mixed);

    /** 插入一个键。探查次数记进 insert_probes()，含最后探到的那一个空槽 */
    void insert(long long key);

    /** 查找一个键。iterations 是探查过的槽数，comparisons 是键比较次数 */
    SearchOutcome find(long long key) const;

    std::size_t table_size() const { return slots_.size(); }
    std::size_t size() const { return size_; }
    double load_factor() const;
    std::size_t insert_probes() const { return insert_probes_; }
    std::size_t max_insert_probe() const { return max_insert_probe_; }
    Hash kind() const { return kind_; }

private:
    std::size_t start_slot(long long key) const;

    std::vector<long long> slots_;
    std::vector<unsigned char> used_;
    std::size_t size_ = 0;
    std::size_t insert_probes_ = 0;
    std::size_t max_insert_probe_ = 0;
    Hash kind_ = Hash::Mixed;
};

/* ================= 报告与自测 ================= */

/** 自测结果。不打印，交给界面决定怎么显示 */
struct CheckResult {
    std::size_t total = 0;
    std::size_t passed = 0;
    std::size_t failed = 0;
    std::vector<std::string> lines;   /**< 每项一行，例如 "[通过] 3. ……" */

    bool all_passed() const { return failed == 0; }
    std::string summary() const;      /**< "28 项中 28 项通过，全部通过" */
};

/** 项目输出：线性、二分三种边界、重复元素、插值、哈希与二分、直方图六段。
    返回多行 UTF-8 文本，末尾带一个换行 */
std::string build_report();

/** 计时实验，只在 --timing 下调用。数字随机器与优化等级变化，不进报告正文 */
std::string run_timing();

/** 逐项核对查找结果、比较次数、结构量与直方图 */
CheckResult run_self_tests();

}   /* namespace slab */

#endif /* SEARCH_LAB_HPP */
