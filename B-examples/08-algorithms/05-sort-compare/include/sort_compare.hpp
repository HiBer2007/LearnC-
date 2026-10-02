/**
 * sort_compare.hpp —— 比较排序：比较次数、搬移次数、递归深度与稳定性
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
 * 报告里的每个数字都由这里的计数器产出，重跑逐位相同：
 *
 *   comparisons  元素与元素之间的比较次数；下标比较（left < count）不计
 *   moves        元素被写进数组的次数；写临时变量不计，两个下标相同的交换不计
 *   depth        同时活跃的调用层数的最大值，最外层调用记 1；迭代实现记 0
 *   cutovers     内省排序在深度用尽时改走堆排序的次数，其余算法恒为 0
 *
 * 六种排序共用同一套计数口径，因此同一格里的两个数可以横向对照。
 * 插入排序与堆排序是迭代实现，深度恒为 0；归并排序要一块与输入等长的缓冲区，
 * 每次合并都要先把整段写进缓冲区、再写回来，搬移次数因此是 2 × n × 层数 的量级。
 *
 * 稳定性另用带 key 的记录验：同一个 key 可以有多条记录，每条记着它的原始下标。
 * 排完之后同一个 key 的记录若仍按原始下标递增，这一组就算稳定。
 * 每种排序在若干组输入上各跑一遍，给出保持稳定的组数，并指出第一组反例的组号。
 */
#ifndef SORT_COMPARE_HPP
#define SORT_COMPARE_HPP

#include <cstddef>
#include <string>
#include <vector>

namespace scmp {

/* ================= 结构量 ================= */

/** 一次排序过程量出来的结构量，四项都是整数，重跑逐位相同 */
struct Stats {
    std::size_t comparisons = 0;  /**< 元素与元素之间的比较次数 */
    std::size_t moves = 0;        /**< 元素被写进数组的次数 */
    std::size_t depth = 0;        /**< 递归深度的最大值，最外层调用记 1 */
    std::size_t cutovers = 0;     /**< 内省排序改走堆排序的次数 */

    bool same_numbers_as(const Stats &other) const;
};

/* ================= 四类输入形态 ================= */

enum class Shape {
    Random,      /**< 随机：固定种子的线性同余发生器造出来的数 */
    Sorted,      /**< 已排序：严格递增 */
    Reversed,    /**< 逆序：严格递减 */
    Duplicates   /**< 大量重复：取值只有 0 到 9 十个 */
};

/** 形态的名字，UTF-8 编码 */
const char *shape_name(Shape shape);

/** 形态的个数，报告的表头按它铺列 */
std::size_t shape_count();

/** 按形态造一个长度为 n 的数组。种子写死在实现里，重复调用结果逐位相同 */
std::vector<int> make_input(Shape shape, std::size_t n);

/* ================= 六种比较排序 ================= */

/** 插入排序：把当前元素往前挪到该在的位置。迭代实现，深度记 0 */
Stats insertion_sort(std::vector<int> &a);

/** 归并排序：自顶向下对半切，合并时先写进缓冲区再写回来。稳定 */
Stats merge_sort(std::vector<int> &a);

/** 快速排序（朴素取首元素）：Lomuto 划分，pivot 固定取区间第一个元素 */
Stats quick_sort_first(std::vector<int> &a);

/** 快速排序（三数取中）：取首、中、尾三个元素的中位数作 pivot */
Stats quick_sort_median3(std::vector<int> &a);

/** 堆排序：先建大顶堆，再逐个把堆顶换到末尾。迭代版下沉，深度记 0 */
Stats heap_sort(std::vector<int> &a);

/** 内省排序：三数取中版加快度限值，深度用尽时改走堆排序 */
Stats intro_sort(std::vector<int> &a);

/** 同上，限值由调用方给定，用来观察切换真的会发生 */
Stats intro_sort_with_limit(std::vector<int> &a, std::size_t limit);

/** 深度限值：2 × floor(log2(n))，与内省排序的判断用的是同一个函数 */
std::size_t depth_limit_for(std::size_t n);

/* ================= 稳定性 ================= */

/** 带 key 的记录：同一个 key 可以有多条，origin 是它在输入里的原始下标 */
struct Record {
    int key = 0;
    int origin = 0;
};

/** 只比 key，不比 origin：同 key 的记录在排序算法眼里完全等价 */
inline bool operator<(const Record &lhs, const Record &rhs)
{
    return lhs.key < rhs.key;
}

inline bool operator==(const Record &lhs, const Record &rhs)
{
    return lhs.key == rhs.key && lhs.origin == rhs.origin;
}

inline bool operator!=(const Record &lhs, const Record &rhs)
{
    return !(lhs == rhs);
}

/** 造一组稳定性测试输入：key 取 0 到 4，origin 就是下标 */
std::vector<Record> make_records(std::size_t n, unsigned int seed);

/** 记录是否已按 key 升序 */
bool records_sorted_by_key(const std::vector<Record> &a);

/** 同一个 key 的记录是否保持原始先后。保持时返回 -1，否则返回第一个反例的下标 */
long long first_stability_violation(const std::vector<Record> &a);

/** 一种排序在若干组输入上的稳定性结果 */
struct StabilityRun {
    std::size_t trials = 0;            /**< 一共跑了几组输入 */
    std::size_t kept = 0;              /**< 保持原始先后的组数 */
    std::size_t first_failed_trial = 0; /**< 第一组反例的组号，从 1 起；全稳时为 0 */

    bool keep_all() const { return trials != 0 && kept == trials; }
};

/** 把一种记录排序跑 trials 组输入，逐组检查同 key 记录的先后 */
StabilityRun stability_of(Stats (*sort)(std::vector<Record> &), std::size_t trials,
                          std::size_t n);

/* ---- 同一批排序的记录版，只供稳定性对照使用 ---- */

Stats insertion_sort_records(std::vector<Record> &a);
Stats merge_sort_records(std::vector<Record> &a);
Stats quick_sort_first_records(std::vector<Record> &a);
Stats quick_sort_median3_records(std::vector<Record> &a);
Stats heap_sort_records(std::vector<Record> &a);
Stats intro_sort_records(std::vector<Record> &a);

/* ================= 排序清单 ================= */

/** 报告与自测都按这张表走，表格行序与之一致 */
struct Algorithm {
    const char *name;                 /**< UTF-8 名字 */
    Stats (*sort)(std::vector<int> &); /**< 对应的排序函数 */
};

const std::vector<Algorithm> &algorithms();

struct RecordAlgorithm {
    const char *name;
    Stats (*sort)(std::vector<Record> &);
};

const std::vector<RecordAlgorithm> &record_algorithms();

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

/** 项目输出的六段：计数口径、四类形态对照、递归深度、快速排序的退化、
    稳定性对照、正确性交叉检查。返回多行 UTF-8 文本，末尾带一个换行 */
std::string build_report();

/** 逐项核对恒等式、边界输入、退化行为与稳定性判定 */
CheckResult run_self_tests();

}   /* namespace scmp */

#endif /* SORT_COMPARE_HPP */
