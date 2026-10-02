/* searchtool.cpp —— 练习模板 04 的实现（C++）
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
 * 本模板的 5 处 TODO 全在这个文件里，按阶段编号：
 *
 *     阶段 1-1   bsearch_closed    闭区间的循环条件与收缩
 *     阶段 2-1   bsearch_half      半开区间的循环条件与收缩
 *     阶段 3-1   lower_bound_of    命中之后往哪边走（不小于）
 *     阶段 3-2   upper_bound_of    命中之后往哪边走（大于）
 *     阶段 4-1   interp_search     按比例取点
 */
#include "searchtool.hpp"

#include <algorithm>

namespace st {

/* ==================================================================
 * 已给出的对照与工具
 * ================================================================== */

int linear_search(const int *a, int n, int key, Stats &s)
{
    for (int i = 0; i < n; ++i) {
        ++s.probes;
        if (a[i] == key) {
            return i;
        }
    }
    return -1;
}

bool is_sorted(const int *a, int n)
{
    for (int i = 1; i < n; ++i) {
        if (a[i - 1] > a[i]) {
            return false;
        }
    }
    return true;
}

int std_lower_bound(const int *a, int n, int key)
{
    const int *p = std::lower_bound(a, a + n, key);
    return static_cast<int>(p - a);
}

int std_upper_bound(const int *a, int n, int key)
{
    const int *p = std::upper_bound(a, a + n, key);
    return static_cast<int>(p - a);
}

/* ==================================================================
 * 阶段 1：闭区间二分
 * ================================================================== */

int bsearch_closed(const int *a, int n, int key, Stats &s)
{
    int lo = 0;
    int hi = n - 1;

    /* TODO（阶段 1-1）：
     * 补上循环条件与每轮的收缩。三件事要想清楚：
     *   1. 区间是两端都含的，什么条件下区间里还**有**元素？
     *   2. 探测点怎么由 lo 与 hi 算出来（overflow 由题目规模兜住，本模板的数组很小）；
     *   3. 探测点比 key 小、比 key 大、正好相等这三种情形各自把哪一端挪到哪里。
     * 每做一次「与 key 的比较」让 s.probes 加一——相等那一次也算。
     * 判据：16 个互不相同的键，逐个查找全部命中，探测次数序列逐位固定；
     *       找不到的键返回 -1（见《配置步骤.md》阶段 1）。 */

    /* 占位实现：不比较、不收缩，直接说「没找到」 */
    (void)a;
    (void)key;
    (void)s;
    (void)lo;
    (void)hi;
    return -1;
}

/* ==================================================================
 * 阶段 2：半开区间二分
 * ================================================================== */

int bsearch_half(const int *a, int n, int key, Stats &s)
{
    int lo = 0;
    int hi = n;

    /* TODO（阶段 2-1）：
     * 同一件事换成半开区间 [lo, hi) 来写。三件事与阶段 1 相同，
     * 但右端是「不含」的，因此初值、循环条件与右端的收缩都要跟着换一种写法。
     * 判据：16 个键逐个查找的**探测次数序列与阶段 1 逐位相同**，
     *       找不到的键返回 -1（见《配置步骤.md》阶段 2）。 */

    /* 占位实现 */
    (void)a;
    (void)key;
    (void)s;
    (void)lo;
    (void)hi;
    return -1;
}

/* ==================================================================
 * 阶段 3：两个边界
 * ================================================================== */

int lower_bound_of(const int *a, int n, int key, Stats &s)
{
    int lo = 0;
    int hi = n;

    /* TODO（阶段 3-1）：
     * 求「第一个不小于 key 的位置」。命中之后**不能立刻返回**：
     * 前面还可能有相等的元素，因此要把答案所在的那一半留住、把另一半丢掉。
     * 区间收缩到空时，lo 就是答案。
     * 每做一次比较让 s.probes 加一。
     * 判据：重复键 5 的 lower 是 1、20 的 lower 是 8；
     *       全都比 key 小时返回 n（见《配置步骤.md》阶段 3）。 */

    /* 占位实现 */
    (void)a;
    (void)key;
    (void)s;
    (void)lo;
    (void)hi;
    return 0;
}

int upper_bound_of(const int *a, int n, int key, Stats &s)
{
    int lo = 0;
    int hi = n;

    /* TODO（阶段 3-2）：
     * 与上一处只差一个字的判断：找的是「第一个大于 key 的位置」。
     * 把这两处的比较各写一遍，会发现它们只在一个符号上不同——
     * 正是这个符号决定了重复键是落在左边还是右边。
     * 判据：重复键 5 的 upper 是 4、20 的 upper 是 12；
     *       upper 减 lower 正好是重复的个数（见《配置步骤.md》阶段 3）。 */

    /* 占位实现 */
    (void)a;
    (void)key;
    (void)s;
    (void)lo;
    (void)hi;
    return 0;
}

/* ==================================================================
 * 阶段 4：插值查找
 * ================================================================== */

int interp_search(const int *a, int n, int key, Stats &s)
{
    int lo = 0;
    int hi = n - 1;

    /* 已给出：越界保护 */
    if (n == 0 || key < a[0] || key > a[n - 1]) {
        return -1;
    }

    while (lo <= hi) {
        int mid;

        if (a[hi] == a[lo]) {
            mid = lo;               /* 已给出：区间里所有键相同，这一步不做除法 */
        } else {
            /* TODO（阶段 4-1）：
             * 二分每次都取中点，插值查找按 key 在 a[lo] 与 a[hi] 之间的相对位置取点：
             * 位置的比例与值的比例一致。算出来的点必须落在 lo 到 hi 之间（含两端）。
             * 中间乘积会超过 int 的范围，因此算的时候要用更宽的类型再收回来。
             * 判据：均匀分布的 16 个键上，插值查找的总探测次数**明显少于**二分；
             *       指数分布的那一组上恰好相反（见《配置步骤.md》阶段 4）。 */
            mid = lo;               /* 占位实现：每次退回左端，退化成线性扫描 */
        }

        ++s.probes;
        if (a[mid] == key) {
            return mid;
        }
        if (a[mid] < key) {
            lo = mid + 1;
        } else {
            hi = mid - 1;
        }
    }

    return -1;
}

} /* namespace st */
