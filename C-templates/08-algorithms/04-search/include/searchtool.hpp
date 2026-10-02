/* searchtool.hpp —— 练习模板 04 的核心接口（C++）
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
 * 四种查找，共用同一个探测计数器：
 *
 *     阶段 1  bsearch_closed        闭区间 [lo, hi]
 *     阶段 2  bsearch_half          半开区间 [lo, hi)
 *     阶段 3  lower_bound_of        第一个不小于 key 的位置
 *             upper_bound_of        第一个大于 key 的位置
 *     阶段 4  interp_search         按比例取点
 *
 * 数组全部已经排好序；本模板只碰「怎么找」，不碰排序（那是模板 05 与 06）。
 * 比较次数（probes）是唯一的结构量，它只随算法变，重跑逐位相同。
 */
#ifndef SEARCHTOOL_HPP
#define SEARCHTOOL_HPP

#include <cstddef>

namespace st {

/* 已给出：一次查找里做了几次「与 key 的比较」 */
struct Stats {
    long long probes = 0;
};

/* 已给出：线性查找，作为对照 */
int linear_search(const int *a, int n, int key, Stats &s);

/* 已给出：数组是不是非递减的 */
bool is_sorted(const int *a, int n);

/* 已给出：标准库的两个边界，用来对答案 */
int std_lower_bound(const int *a, int n, int key);
int std_upper_bound(const int *a, int n, int key);

/* ---------- 阶段 1：闭区间二分 ---------- */

/* 区间写成 [lo, hi]，两端都含。找到返回下标，找不到返回 -1。
 * 留空的是循环条件与每次的收缩方式。 */
int bsearch_closed(const int *a, int n, int key, Stats &s);

/* ---------- 阶段 2：半开区间二分 ---------- */

/* 区间写成 [lo, hi)，右端不含。找到返回下标，找不到返回 -1。
 * 留空的是循环条件与每次的收缩方式。 */
int bsearch_half(const int *a, int n, int key, Stats &s);

/* ---------- 阶段 3：两个边界 ---------- */

/* 第一个「不小于」key 的位置；全都比 key 小的时候返回 n。
 * 留空的是收缩方式：命中之后往哪边走。 */
int lower_bound_of(const int *a, int n, int key, Stats &s);

/* 第一个「大于」key 的位置；没有比 key 大的时候返回 n。 */
int upper_bound_of(const int *a, int n, int key, Stats &s);

/* ---------- 阶段 4：插值查找 ---------- */

/* 每次不取中点，而是按 key 在两端值之间的相对位置按比例取点。
 * 越界保护、探测计数与两端的收缩都已经给出，留空的只有「取点」那一步。 */
int interp_search(const int *a, int n, int key, Stats &s);

} /* namespace st */

#endif /* SEARCHTOOL_HPP */
