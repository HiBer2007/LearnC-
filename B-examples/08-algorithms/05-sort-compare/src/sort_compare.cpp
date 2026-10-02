/**
 * sort_compare.cpp —— 比较排序：比较次数、搬移次数、递归深度与稳定性
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

#include "sort_compare.hpp"

#include <algorithm>
#include <functional>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

namespace scmp {

/* ================= 结构量 ================= */

bool Stats::same_numbers_as(const Stats &other) const
{
    return comparisons == other.comparisons && moves == other.moves &&
           depth == other.depth && cutovers == other.cutovers;
}

/* ================= 输入构造 ================= */

namespace {

/** 线性同余发生器：只用来造输入，重跑逐位相同。
    不借助 std::uniform_int_distribution：那个分布的具体输出随实现而变，
    换一个标准库就可能得到另一组数，而报告里的每个数字都要求可复现。
    无符号整数的溢出是回绕，行为有定义，不依赖任何未指定的东西。 */
class Lcg {
public:
    explicit Lcg(unsigned int seed) : state_(seed) {}

    unsigned int next()
    {
        state_ = state_ * 1664525u + 1013904223u;
        return state_;
    }

    unsigned int below(unsigned int bound) { return next() % bound; }

private:
    unsigned int state_;
};

}   /* namespace */

const char *shape_name(Shape shape)
{
    switch (shape) {
    case Shape::Random:
        return u8"随机";
    case Shape::Sorted:
        return u8"已排序";
    case Shape::Reversed:
        return u8"逆序";
    case Shape::Duplicates:
        return u8"大量重复";
    }
    return u8"未知";
}

std::size_t shape_count()
{
    return 4;
}

std::vector<int> make_input(Shape shape, std::size_t n)
{
    std::vector<int> a(n);
    switch (shape) {
    case Shape::Random: {
        Lcg rng(20261002u);
        for (std::size_t i = 0; i < n; ++i) {
            a[i] = static_cast<int>(rng.below(1000000u));
        }
        break;
    }
    case Shape::Sorted:
        for (std::size_t i = 0; i < n; ++i) {
            a[i] = static_cast<int>(i * 3 + 1);
        }
        break;
    case Shape::Reversed:
        for (std::size_t i = 0; i < n; ++i) {
            a[i] = static_cast<int>(n - i) * 3;
        }
        break;
    case Shape::Duplicates: {
        Lcg rng(20261003u);
        for (std::size_t i = 0; i < n; ++i) {
            a[i] = static_cast<int>(rng.below(10u));
        }
        break;
    }
    }
    return a;
}

std::vector<Record> make_records(std::size_t n, unsigned int seed)
{
    Lcg rng(seed);
    std::vector<Record> a(n);
    for (std::size_t i = 0; i < n; ++i) {
        a[i].key = static_cast<int>(rng.below(5u));
        a[i].origin = static_cast<int>(i);
    }
    return a;
}

/* ================= 计数工具 ================= */

namespace {

/** 记一次元素之间的比较。所有比较都从这里走，不会漏记 */
template <class T>
inline bool less_than(const T &lhs, const T &rhs, Stats &st)
{
    ++st.comparisons;
    return lhs < rhs;
}

/** 把一个元素写进数组：记一次搬移 */
template <class T>
inline void put(std::vector<T> &a, std::size_t i, const T &value, Stats &st)
{
    a[i] = value;
    ++st.moves;
}

/** 交换两个下标的元素。下标相同就直接跳过：自己和自己换不算搬移。
    下标不同时按三次搬移记：暂存一次、两次回写 */
template <class T>
inline void swap_at(std::vector<T> &a, std::size_t i, std::size_t j, Stats &st)
{
    if (i == j) {
        return;
    }
    const T tmp = a[i];
    a[i] = a[j];
    a[j] = tmp;
    st.moves += 3;
}

/** 记一层递归。最外层调用记 1，空区间与单元素区间的调用同样占一层栈帧 */
inline void note_depth(std::size_t depth, Stats &st)
{
    if (depth > st.depth) {
        st.depth = depth;
    }
}

/* ================= 插入排序 ================= */

template <class T>
void insertion_range(std::vector<T> &a, std::size_t lo, std::size_t hi, Stats &st)
{
    for (std::size_t i = lo + 1; i < hi; ++i) {
        const T key = a[i];
        std::size_t j = i;
        /* 严格小于才继续往前挪：相等时停下，同 key 的记录因此保持原始先后 */
        while (j > lo && less_than(key, a[j - 1], st)) {
            put(a, j, a[j - 1], st);
            --j;
        }
        if (j != i) {
            put(a, j, key, st);   /* 位置没变就不回写，搬移次数因此可以是 0 */
        }
    }
}

/* ================= 归并排序 ================= */

template <class T>
void merge_range(std::vector<T> &a, std::vector<T> &buf, std::size_t lo, std::size_t hi,
                 std::size_t depth, Stats &st)
{
    note_depth(depth, st);
    if (hi - lo < 2) {
        return;
    }
    const std::size_t mid = lo + (hi - lo) / 2;
    merge_range(a, buf, lo, mid, depth + 1, st);
    merge_range(a, buf, mid, hi, depth + 1, st);

    std::size_t i = lo;
    std::size_t j = mid;
    std::size_t k = lo;
    while (i < mid && j < hi) {
        /* 相等时先取左半边：归并排序靠这一行才稳定 */
        if (less_than(a[j], a[i], st)) {
            put(buf, k, a[j], st);
            ++j;
        } else {
            put(buf, k, a[i], st);
            ++i;
        }
        ++k;
    }
    while (i < mid) {
        put(buf, k, a[i], st);
        ++i;
        ++k;
    }
    while (j < hi) {
        put(buf, k, a[j], st);
        ++j;
        ++k;
    }
    for (std::size_t t = lo; t < hi; ++t) {
        put(a, t, buf[t], st);   /* 整段写回原数组，这是归并排序搬移次数高的来路 */
    }
}

/* ================= 快速排序的两种划分 ================= */

/** Lomuto 划分：pivot 取区间第一个元素，返回它落定的下标。
    比较次数恰好是区间长度减一 */
template <class T>
std::size_t partition_lomuto(std::vector<T> &a, std::size_t lo, std::size_t hi, Stats &st)
{
    const T pivot = a[lo];
    std::size_t i = lo;
    for (std::size_t j = lo + 1; j < hi; ++j) {
        if (less_than(a[j], pivot, st)) {
            ++i;
            swap_at(a, i, j, st);
        }
    }
    swap_at(a, lo, i, st);
    return i;
}

/** 首、中、尾三个元素比三次，把中位数换到 mid 上 */
template <class T>
std::size_t median3_index(std::vector<T> &a, std::size_t lo, std::size_t hi, Stats &st)
{
    const std::size_t mid = lo + (hi - lo - 1) / 2;
    const std::size_t last = hi - 1;
    if (less_than(a[mid], a[lo], st)) {
        swap_at(a, lo, mid, st);
    }
    if (less_than(a[last], a[lo], st)) {
        swap_at(a, lo, last, st);
    }
    if (less_than(a[last], a[mid], st)) {
        swap_at(a, mid, last, st);
    }
    return mid;
}

/** 三数取中划分：先把中位数换到区间头上，再走同一套 Lomuto */
template <class T>
std::size_t partition_median3(std::vector<T> &a, std::size_t lo, std::size_t hi, Stats &st)
{
    const std::size_t p = median3_index(a, lo, hi, st);
    swap_at(a, lo, p, st);
    return partition_lomuto(a, lo, hi, st);
}

/** 首元素版：pivot 固定取区间第一个元素 */
template <class T>
void quick_first_range(std::vector<T> &a, std::size_t lo, std::size_t hi, std::size_t depth,
                       Stats &st)
{
    note_depth(depth, st);
    if (hi - lo < 2) {
        return;
    }
    const std::size_t p = partition_lomuto(a, lo, hi, st);
    quick_first_range(a, lo, p, depth + 1, st);
    quick_first_range(a, p + 1, hi, depth + 1, st);
}

/** 三数取中版：划分方式与首元素版完全相同，只有挑 pivot 那一处不同 */
template <class T>
void quick_median3_range(std::vector<T> &a, std::size_t lo, std::size_t hi,
                         std::size_t depth, Stats &st)
{
    note_depth(depth, st);
    if (hi - lo < 2) {
        return;
    }
    const std::size_t p = partition_median3(a, lo, hi, st);
    quick_median3_range(a, lo, p, depth + 1, st);
    quick_median3_range(a, p + 1, hi, depth + 1, st);
}

/* ================= 堆排序 ================= */

/** 下沉：把 lo + start 上的元素在大顶堆里往下挪。
    写成循环而不是递归，切过来之后递归栈不再变深 */
template <class T>
void sift_down_range(std::vector<T> &a, std::size_t lo, std::size_t start, std::size_t count,
                     Stats &st)
{
    std::size_t root = start;
    while (true) {
        const std::size_t left = 2 * root + 1;
        if (left >= count) {
            break;
        }
        std::size_t largest = root;
        if (less_than(a[lo + largest], a[lo + left], st)) {
            largest = left;
        }
        const std::size_t right = left + 1;
        if (right < count && less_than(a[lo + largest], a[lo + right], st)) {
            largest = right;
        }
        if (largest == root) {
            break;
        }
        swap_at(a, lo + root, lo + largest, st);
        root = largest;
    }
}

template <class T>
void heap_sort_range(std::vector<T> &a, std::size_t lo, std::size_t hi, Stats &st)
{
    const std::size_t n = hi - lo;
    if (n < 2) {
        return;
    }
    for (std::size_t i = n / 2; i > 0; --i) {
        sift_down_range(a, lo, i - 1, n, st);   /* 建堆：从最后一个内部结点往前下沉 */
    }
    for (std::size_t end = n - 1; end > 0; --end) {
        swap_at(a, lo, lo + end, st);           /* 堆顶换到末尾，它就落定了 */
        sift_down_range(a, lo, 0, end, st);
    }
}

/* ================= 内省排序 ================= */

template <class T>
void intro_range(std::vector<T> &a, std::size_t lo, std::size_t hi, std::size_t budget,
                 std::size_t depth, Stats &st)
{
    note_depth(depth, st);
    if (hi - lo < 2) {
        return;
    }
    if (budget == 0) {
        /* 深度用尽：剩下的这一段改走堆排序。堆排序最坏 O(n log n)、原地、
           不用额外缓冲区，而且这里是迭代版下沉，切过去之后栈不会更深 */
        ++st.cutovers;
        heap_sort_range(a, lo, hi, st);
        return;
    }
    const std::size_t p = partition_median3(a, lo, hi, st);
    intro_range(a, lo, p, budget - 1, depth + 1, st);
    intro_range(a, p + 1, hi, budget - 1, depth + 1, st);
}

/* ================= 六种排序的对外版本 ================= */

template <class T>
Stats run_insertion(std::vector<T> &a)
{
    Stats st;
    insertion_range(a, 0, a.size(), st);
    return st;
}

template <class T>
Stats run_merge(std::vector<T> &a)
{
    Stats st;
    std::vector<T> buf(a.size());   /* 缓冲区的初始化不算搬移，它还不是待排数据 */
    merge_range(a, buf, 0, a.size(), 1, st);
    return st;
}

template <class T>
Stats run_quick_first(std::vector<T> &a)
{
    Stats st;
    quick_first_range(a, 0, a.size(), 1, st);
    return st;
}

template <class T>
Stats run_quick_median3(std::vector<T> &a)
{
    Stats st;
    quick_median3_range(a, 0, a.size(), 1, st);
    return st;
}

template <class T>
Stats run_heap(std::vector<T> &a)
{
    Stats st;
    heap_sort_range(a, 0, a.size(), st);
    return st;
}

template <class T>
Stats run_intro(std::vector<T> &a, std::size_t limit)
{
    Stats st;
    intro_range(a, 0, a.size(), limit, 1, st);
    return st;
}

}   /* namespace */

std::size_t depth_limit_for(std::size_t n)
{
    std::size_t floors = 0;
    std::size_t m = n;
    while (m > 1) {
        m /= 2;
        ++floors;
    }
    return 2 * floors;
}

Stats insertion_sort(std::vector<int> &a)
{
    return run_insertion(a);
}

Stats merge_sort(std::vector<int> &a)
{
    return run_merge(a);
}

Stats quick_sort_first(std::vector<int> &a)
{
    return run_quick_first(a);
}

Stats quick_sort_median3(std::vector<int> &a)
{
    return run_quick_median3(a);
}

Stats heap_sort(std::vector<int> &a)
{
    return run_heap(a);
}

Stats intro_sort(std::vector<int> &a)
{
    return run_intro(a, depth_limit_for(a.size()));
}

Stats intro_sort_with_limit(std::vector<int> &a, std::size_t limit)
{
    return run_intro(a, limit);
}

Stats insertion_sort_records(std::vector<Record> &a)
{
    return run_insertion(a);
}

Stats merge_sort_records(std::vector<Record> &a)
{
    return run_merge(a);
}

Stats quick_sort_first_records(std::vector<Record> &a)
{
    return run_quick_first(a);
}

Stats quick_sort_median3_records(std::vector<Record> &a)
{
    return run_quick_median3(a);
}

Stats heap_sort_records(std::vector<Record> &a)
{
    return run_heap(a);
}

Stats intro_sort_records(std::vector<Record> &a)
{
    return run_intro(a, depth_limit_for(a.size()));
}

/* ================= 稳定性 ================= */

bool records_sorted_by_key(const std::vector<Record> &a)
{
    for (std::size_t i = 1; i < a.size(); ++i) {
        if (a[i].key < a[i - 1].key) {
            return false;
        }
    }
    return true;
}

long long first_stability_violation(const std::vector<Record> &a)
{
    /* 排完之后同一个 key 的记录是挨着的，于是只消在每一段内部看 origin 是否递增。
       等于号的判断不参与，因此这个检查本身与排序算法无关 */
    for (std::size_t i = 0; i < a.size(); ++i) {
        for (std::size_t j = i + 1; j < a.size() && a[j].key == a[i].key; ++j) {
            if (a[j].origin < a[i].origin) {
                return static_cast<long long>(j);
            }
        }
    }
    return -1;
}

StabilityRun stability_of(Stats (*sort)(std::vector<Record> &), std::size_t trials,
                          std::size_t n)
{
    StabilityRun run;
    run.trials = trials;
    for (std::size_t t = 0; t < trials; ++t) {
        std::vector<Record> a = make_records(n, 700000u + static_cast<unsigned int>(t) * 7919u);
        sort(a);
        if (records_sorted_by_key(a) && first_stability_violation(a) < 0) {
            ++run.kept;
        } else if (run.first_failed_trial == 0) {
            run.first_failed_trial = t + 1;
        }
    }
    return run;
}

/* ================= 排序清单 ================= */

const std::vector<Algorithm> &algorithms()
{
    static const std::vector<Algorithm> table = {
        {u8"插入排序", &insertion_sort},
        {u8"归并排序", &merge_sort},
        {u8"快速排序（首元素）", &quick_sort_first},
        {u8"快速排序（三数取中）", &quick_sort_median3},
        {u8"堆排序", &heap_sort},
        {u8"内省排序", &intro_sort},
    };
    return table;
}

const std::vector<RecordAlgorithm> &record_algorithms()
{
    static const std::vector<RecordAlgorithm> table = {
        {u8"插入排序", &insertion_sort_records},
        {u8"归并排序", &merge_sort_records},
        {u8"快速排序（首元素）", &quick_sort_first_records},
        {u8"快速排序（三数取中）", &quick_sort_median3_records},
        {u8"堆排序", &heap_sort_records},
        {u8"内省排序", &intro_sort_records},
    };
    return table;
}

/* ================= 报告的排版工具 ================= */

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

/** 表格的列宽：第 0 列给 first，其余各列给 rest */
std::vector<std::size_t> widths_for(std::size_t count, std::size_t first, std::size_t rest)
{
    std::vector<std::size_t> widths;
    for (std::size_t i = 0; i < count; ++i) {
        widths.push_back(i == 0 ? first : rest);
    }
    return widths;
}

/** 一行表格：每列左对齐补到各自的宽度，最后一列不补，免得行尾拖空格 */
std::string table_row(const std::vector<std::string> &cells,
                      const std::vector<std::size_t> &widths)
{
    std::string line;
    for (std::size_t i = 0; i + 1 < cells.size(); ++i) {
        const std::size_t width = (i < widths.size()) ? widths[i] : widths.back();
        line += pad_right(cells[i], width);
    }
    if (!cells.empty()) {
        line += cells.back();
    }
    return line;
}

std::string fixed2(double value)
{
    std::ostringstream os;
    os << std::fixed << std::setprecision(2) << value;
    return os.str();
}

/** 两个数的比值，分母为 0 时给一个短横 */
std::string ratio_of(std::size_t top, std::size_t bottom)
{
    if (bottom == 0) {
        return "-";
    }
    return fixed2(static_cast<double>(top) / static_cast<double>(bottom));
}

const std::size_t kNameWidth = 22;
const std::size_t kCellWidth = 18;
const std::size_t kShortWidth = 8;
const std::size_t kSizesWidth = 10;

/* algorithms() 那张表里的行号：报告要多处按行取数，写死之后改表时要一起改 */
const std::size_t kInsertion = 0;
const std::size_t kMerge = 1;
const std::size_t kQuickFirst = 2;
const std::size_t kQuickMedian3 = 3;
const std::size_t kHeap = 4;
const std::size_t kIntro = 5;

/** 主表与深度表共用的规模 */
const std::size_t kMainSize = 1000;
const std::size_t kStabilityTrials = 64;
const std::size_t kStabilitySize = 200;

/** 把一批（算法 × 形态）的结构量一次算出来，两处表都用这一份 */
std::vector<std::vector<Stats>> measure_all(std::size_t n)
{
    const std::vector<Algorithm> &algs = algorithms();
    std::vector<std::vector<Stats>> table(algs.size());
    for (std::size_t i = 0; i < algs.size(); ++i) {
        for (std::size_t s = 0; s < shape_count(); ++s) {
            std::vector<int> a = make_input(static_cast<Shape>(s), n);
            table[i].push_back(algs[i].sort(a));
        }
    }
    return table;
}

}   /* namespace */

/* ================= 报告 ================= */

namespace {

void append_metering(std::ostringstream &os)
{
    os << u8"一、计数口径（结构量，重跑逐位相同）\n";
    os << u8"  比较次数  只数元素与元素之间的比较；下标比较（left < count）不计\n";
    os << u8"  搬移次数  元素被写进数组才算一次：写临时变量不计，两个下标相同的交换不计\n";
    os << u8"            一次交换按 3 次搬移记（暂存 1 次 + 两次回写）\n";
    os << u8"  递归深度  同时活跃的调用层数最大值，最外层调用记 1；迭代实现记 0\n";
    os << u8"  切换次数  内省排序在深度用尽时改走堆排序的次数，其余算法恒为 0\n";
    os << u8"  六种排序都原地进行；只有归并排序要一块与输入等长的缓冲区，\n";
    os << u8"  每次合并先把整段写进缓冲区、再写回原数组，两笔都算搬移\n";
    os << "\n";
}

void append_shape_table(std::ostringstream &os, const std::vector<std::vector<Stats>> &table)
{
    os << u8"二、四类输入形态的对照表（n = " << kMainSize << u8"，每格「比较次数 / 搬移次数」）\n";

    std::vector<std::string> head;
    head.push_back(u8"算法");
    for (std::size_t s = 0; s < shape_count(); ++s) {
        head.push_back(shape_name(static_cast<Shape>(s)));
    }
    const std::vector<std::size_t> widths = widths_for(head.size(), kNameWidth, kCellWidth);
    os << "  " << table_row(head, widths) << "\n";

    const std::vector<Algorithm> &algs = algorithms();
    for (std::size_t i = 0; i < algs.size(); ++i) {
        std::vector<std::string> cells;
        cells.push_back(algs[i].name);
        for (std::size_t s = 0; s < shape_count(); ++s) {
            cells.push_back(std::to_string(table[i][s].comparisons) + " / " +
                            std::to_string(table[i][s].moves));
        }
        os << "  " << table_row(cells, widths) << "\n";
    }

    /* 表里的极端值各挑一条出来说，数字直接从上面那份表里取 */
    const std::size_t random_shape = static_cast<std::size_t>(Shape::Random);
    const std::size_t sorted_shape = static_cast<std::size_t>(Shape::Sorted);
    os << u8"  插入排序在已排序输入上搬移 " << table[kInsertion][sorted_shape].moves
       << u8" 次：内层循环一次都没进，元素没被挪动过\n";
    os << u8"  插入排序在随机输入上比较 " << table[kInsertion][random_shape].comparisons
       << u8" 次、搬移 " << table[kInsertion][random_shape].moves
       << u8" 次，两个数几乎相等：随机输入下每次比较几乎都跟着一次挪动\n";
    os << u8"  归并排序的搬移次数四列完全相同（都是 " << table[kMerge][random_shape].moves
       << u8" 次）：合并这一步不看输入是否已经有序，它只跟 n 有关\n";
    os << u8"  首元素快排在已排序输入上搬移 " << table[kQuickFirst][sorted_shape].moves
       << u8" 次：pivot 每趟都留在原地，自交换被跳过\n";
    os << "\n";
}

void append_depth_table(std::ostringstream &os, const std::vector<std::vector<Stats>> &table)
{
    os << u8"三、递归深度（同一批输入，n = " << kMainSize << u8"）\n";

    std::vector<std::string> head;
    head.push_back(u8"算法");
    for (std::size_t s = 0; s < shape_count(); ++s) {
        head.push_back(shape_name(static_cast<Shape>(s)));
    }
    const std::vector<std::size_t> widths = widths_for(head.size(), kNameWidth, kShortWidth);
    os << "  " << table_row(head, widths) << "\n";

    const std::vector<Algorithm> &algs = algorithms();
    for (std::size_t i = 0; i < algs.size(); ++i) {
        std::vector<std::string> cells;
        cells.push_back(algs[i].name);
        for (std::size_t s = 0; s < shape_count(); ++s) {
            cells.push_back(std::to_string(table[i][s].depth));
        }
        os << "  " << table_row(cells, widths) << "\n";
    }
    os << u8"  插入排序与堆排序是迭代实现，深度恒为 0，不占递归栈\n";
    os << u8"  深度限值 2 × floor(log2(" << kMainSize << ")) = " << depth_limit_for(kMainSize) << "\n";
    os << "\n";
}

void append_quick_degradation(std::ostringstream &os)
{
    const std::size_t n = kMainSize;
    std::vector<int> sorted_first = make_input(Shape::Sorted, n);
    std::vector<int> sorted_median = make_input(Shape::Sorted, n);
    const Stats first = quick_sort_first(sorted_first);
    const Stats median = quick_sort_median3(sorted_median);

    os << u8"四、快速排序的退化：同一份已排序输入上的两个版本（n = " << n << u8"）\n";
    os << u8"  首元素版    比较 " << first.comparisons << u8" 次，搬移 " << first.moves
       << u8" 次，递归深度 " << first.depth << "\n";
    os << u8"  三数取中版  比较 " << median.comparisons << u8" 次，搬移 " << median.moves
       << u8" 次，递归深度 " << median.depth << "\n";
    os << u8"  比较次数之比 " << ratio_of(first.comparisons, median.comparisons)
       << u8" 倍，递归深度之比 " << ratio_of(first.depth, median.depth) << u8" 倍\n";
    os << u8"  首元素版每一趟都把 pivot 留在区间最左边：左边空、右边只少一个元素，\n";
    os << u8"  递归深度跟着元素个数一起长，比较次数就是 n × (n - 1) / 2\n";
    os << u8"  规模翻倍时的比较次数（已排序输入）\n";

    const std::size_t sizes[] = {250, 500, 1000, 2000};
    std::vector<std::string> head;
    head.push_back("n");
    head.push_back(u8"首元素版");
    head.push_back(u8"比上一行");
    head.push_back(u8"三数取中版");
    head.push_back(u8"比上一行");
    const std::vector<std::size_t> widths = widths_for(head.size(), kSizesWidth, 14);
    os << "    " << table_row(head, widths) << "\n";

    std::size_t prev_first = 0;
    std::size_t prev_median = 0;
    for (const std::size_t size : sizes) {
        std::vector<int> a = make_input(Shape::Sorted, size);
        std::vector<int> b = make_input(Shape::Sorted, size);
        const Stats f = quick_sort_first(a);
        const Stats m = quick_sort_median3(b);
        std::vector<std::string> cells;
        cells.push_back(std::to_string(size));
        cells.push_back(std::to_string(f.comparisons));
        cells.push_back(prev_first == 0 ? "-" : ratio_of(f.comparisons, prev_first));
        cells.push_back(std::to_string(m.comparisons));
        cells.push_back(prev_median == 0 ? "-" : ratio_of(m.comparisons, prev_median));
        os << "    " << table_row(cells, widths) << "\n";
        prev_first = f.comparisons;
        prev_median = m.comparisons;
    }
    os << u8"  首元素版每翻一倍规模，比较次数就涨到四倍；三数取中版只涨两倍出头\n";

    /* 三数取中挡不住的是另一种输入：元素全部相同 */
    std::vector<int> flat_first(n, 7);
    std::vector<int> flat_median(n, 7);
    const Stats flat_a = quick_sort_first(flat_first);
    const Stats flat_b = quick_sort_median3(flat_median);
    os << u8"  换成全部相同的 " << n << u8" 个 7：首元素版比较 " << flat_a.comparisons
       << u8" 次、深度 " << flat_a.depth << u8"；三数取中版比较 " << flat_b.comparisons
       << u8" 次、深度 " << flat_b.depth << u8"\n";
    os << u8"  三个候选一样大时，中位数与第一个元素没有区别：三数取中在这种输入上一点忙都帮不上，\n";
    os << u8"  每趟还多花三次比较，反而是两个版本里更贵的那个\n";

    /* 内省排序：限值照用，再看把限值压小之后切换会不会真的发生 */
    const std::size_t limit = depth_limit_for(n);
    const std::vector<std::vector<Stats>> intro_table = measure_all(n);
    const std::size_t duplicates = static_cast<std::size_t>(Shape::Duplicates);
    std::size_t triggered_shapes = 0;
    std::size_t cutover_total = 0;
    for (std::size_t s = 0; s < shape_count(); ++s) {
        if (intro_table[kIntro][s].cutovers > 0) {
            ++triggered_shapes;
        }
        cutover_total += intro_table[kIntro][s].cutovers;
    }
    const Stats &median_duplicates = intro_table[kQuickMedian3][duplicates];
    const Stats &intro_duplicates = intro_table[kIntro][duplicates];

    std::vector<int> squeezed = make_input(Shape::Sorted, n);
    const Stats forced = intro_sort_with_limit(squeezed, 4);
    const bool forced_sorted = std::is_sorted(squeezed.begin(), squeezed.end());

    os << u8"  内省排序：三数取中版加一条 " << limit << u8" 层的深度限值，超限就改走堆排序\n";
    os << u8"    四类输入里 " << triggered_shapes << u8" 类触到了限值，合计切换 " << cutover_total
       << u8" 次；最深的调用落在第 " << limit + 1 << u8" 层，那一层不再往下递归\n";
    os << u8"    大量重复那一列最明显：三数取中版比较 " << median_duplicates.comparisons
       << u8" 次、深度 " << median_duplicates.depth << u8"；内省排序压到 "
       << intro_duplicates.comparisons << u8" 次、深度 " << intro_duplicates.depth << "\n";
    os << u8"    限值压到 4：同一份已排序输入切换 " << forced.cutovers << u8" 次，比较 "
       << forced.comparisons << u8" 次，递归深度 " << forced.depth << u8"，排好："
       << (forced_sorted ? u8"是" : u8"否") << "\n";
    os << u8"    切堆排序的理由：堆排序最坏 O(n log n)、原地、不用额外缓冲区，\n";
    os << u8"    而且这里用的是迭代版下沉，切过去之后递归栈不再变深；\n";
    os << u8"    归并排序的最坏情况同样是 O(n log n)，但它要一块与输入等长的缓冲区\n";
    os << "\n";
}

void append_stability(std::ostringstream &os)
{
    os << u8"五、稳定性对照（用带 key 的记录验，不凭记忆）\n";
    os << u8"  每组输入 " << kStabilitySize << u8" 条记录，key 取 0 到 4，origin 是它在输入里的原始下标；\n";
    os << u8"  排完之后同一个 key 的记录若仍按 origin 递增，这一组就算稳定。\n";

    std::vector<std::string> head;
    head.push_back(u8"算法");
    head.push_back(u8"保持稳定的组数");
    head.push_back(u8"判定");
    const std::vector<std::size_t> widths = widths_for(head.size(), kNameWidth, kCellWidth);
    os << "  " << table_row(head, widths) << "\n";

    const std::vector<RecordAlgorithm> &algs = record_algorithms();
    std::size_t stable_count = 0;
    std::string first_counter_example;
    for (std::size_t i = 0; i < algs.size(); ++i) {
        const StabilityRun run = stability_of(algs[i].sort, kStabilityTrials, kStabilitySize);
        std::vector<std::string> cells;
        cells.push_back(algs[i].name);
        cells.push_back(std::to_string(run.kept) + " / " + std::to_string(run.trials));
        cells.push_back(run.keep_all() ? u8"稳定" : u8"不稳定");
        os << "  " << table_row(cells, widths) << "\n";
        if (run.keep_all()) {
            ++stable_count;
        } else if (first_counter_example.empty()) {
            first_counter_example = std::string(algs[i].name) + u8"在第 " +
                                    std::to_string(run.first_failed_trial) + u8" 组输入上出现反例";
        }
    }
    os << u8"  " << algs.size() << u8" 种里 " << stable_count << u8" 种判定为稳定，其余 "
       << algs.size() - stable_count << u8" 种在 64 组输入上都出现过反例。\n";
    os << u8"  判定的方向：判定为不稳定，一组反例就够；判定为稳定，要每一组都不出错\n";
    if (!first_counter_example.empty()) {
        os << u8"  最早的一组反例：" << first_counter_example << "\n";
    }
    os << "\n";
}

void append_correctness(std::ostringstream &os)
{
    const std::size_t n = 200;
    const std::vector<Algorithm> &algs = algorithms();
    bool all_same = true;
    for (std::size_t i = 0; i < algs.size(); ++i) {
        for (std::size_t s = 0; s < shape_count(); ++s) {
            std::vector<int> a = make_input(static_cast<Shape>(s), n);
            std::vector<int> want = a;
            std::sort(want.begin(), want.end());
            algs[i].sort(a);
            if (a != want) {
                all_same = false;
            }
        }
    }
    os << u8"六、正确性交叉检查\n";
    os << u8"  六种排序在四类输入（n = " << n << u8"）上排出的序列与 std::sort 逐位相同："
       << (all_same ? u8"是" : u8"否") << "\n";
}

}   /* namespace */

std::string build_report()
{
    const std::vector<std::vector<Stats>> table = measure_all(kMainSize);

    std::ostringstream os;
    append_metering(os);
    append_shape_table(os, table);
    append_depth_table(os, table);
    append_quick_degradation(os);
    append_stability(os);
    append_correctness(os);
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

    void expect_eq(std::size_t got, std::size_t want, const std::string &what)
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

/** 把一种排序跑在一份输入上，返回排出来的序列与 std::sort 是否逐位相同 */
bool matches_std_sort(Stats (*sort)(std::vector<int> &), const std::vector<int> &input)
{
    std::vector<int> want = input;
    std::sort(want.begin(), want.end());
    std::vector<int> got = input;
    sort(got);
    return got == want;
}

/** n 每次按上取整折半到 1 需要几步，归并排序最深的调用层数就是它 + 1 */
std::size_t halving_levels(std::size_t n)
{
    std::size_t levels = 1;
    std::size_t m = n;
    while (m > 1) {
        m = (m + 1) / 2;
        ++levels;
    }
    return levels;
}

std::size_t distinct_values(const std::vector<int> &a)
{
    std::vector<int> copy = a;
    std::sort(copy.begin(), copy.end());
    return static_cast<std::size_t>(std::unique(copy.begin(), copy.end()) - copy.begin());
}

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
    const std::vector<Algorithm> &algs = algorithms();

    /* 正确性：与 std::sort 交叉对照 */
    {
        bool ok = true;
        for (std::size_t i = 0; i < algs.size(); ++i) {
            for (std::size_t s = 0; s < shape_count(); ++s) {
                if (!matches_std_sort(algs[i].sort, make_input(static_cast<Shape>(s), 200))) {
                    ok = false;
                }
            }
        }
        checks.expect(ok, u8"六种排序在四类输入（n = 200）上排出的序列与 std::sort 逐位相同");
    }

    /* 边界输入：空数组与单元素数组 */
    {
        const std::size_t want_depth[] = {0, 1, 1, 1, 0, 1};
        bool ok = true;
        for (std::size_t i = 0; i < algs.size(); ++i) {
            std::vector<int> empty;
            const Stats st = algs[i].sort(empty);
            if (st.comparisons != 0 || st.moves != 0 || st.depth != want_depth[i]) {
                ok = false;
            }
        }
        checks.expect(ok, u8"空数组：六种排序的比较与搬移都是 0，深度依次是 0、1、1、1、0、1");
    }
    {
        bool ok = true;
        for (std::size_t i = 0; i < algs.size(); ++i) {
            std::vector<int> one(1, 42);
            const Stats st = algs[i].sort(one);
            if (st.comparisons != 0 || st.moves != 0 || one[0] != 42) {
                ok = false;
            }
        }
        checks.expect(ok, u8"单元素数组：六种排序一次比较都没做，元素原样留在原处");
    }

    /* 插入排序的两条恒等式 */
    {
        const std::size_t n = 100;
        std::vector<int> a = make_input(Shape::Sorted, n);
        const Stats st = insertion_sort(a);
        checks.expect_eq(st.comparisons, n - 1, u8"插入排序在已排序输入上比较 n - 1 次");
        checks.expect_eq(st.moves, 0, u8"插入排序在已排序输入上一次都没搬：内层循环没进去，也就没有回写");
    }
    {
        const std::size_t n = 100;
        std::vector<int> a = make_input(Shape::Reversed, n);
        const Stats st = insertion_sort(a);
        const std::size_t pairs = n * (n - 1) / 2;
        checks.expect_eq(st.comparisons, pairs, u8"插入排序在逆序输入上比较 n × (n - 1) / 2 次");
        checks.expect_eq(st.moves, pairs + n - 1, u8"插入排序在逆序输入上搬移 n × (n - 1) / 2 + n - 1 次");
    }

    /* 首元素快排的退化 */
    {
        bool ok = true;
        for (std::size_t n = 2; n <= 60; ++n) {
            std::vector<int> a = make_input(Shape::Sorted, n);
            if (quick_sort_first(a).comparisons != n * (n - 1) / 2) {
                ok = false;
            }
        }
        checks.expect(ok, u8"首元素快排在已排序输入上比较次数就是 n × (n - 1) / 2（n = 2 到 60）");
    }
    {
        std::vector<int> a = make_input(Shape::Sorted, 1000);
        const Stats st = quick_sort_first(a);
        checks.expect_eq(st.depth, 1000, u8"首元素快排在已排序输入（n = 1000）上递归深度是 1000");
    }
    {
        std::vector<int> sorted = make_input(Shape::Sorted, 1000);
        std::vector<int> reversed = make_input(Shape::Reversed, 1000);
        const Stats s1 = quick_sort_first(sorted);
        const Stats s2 = quick_sort_first(reversed);
        checks.expect(s1.comparisons == s2.comparisons && s1.depth == s2.depth,
                      u8"首元素快排在逆序输入上的比较次数与深度与已排序输入相同");
        checks.expect(s2.moves > s1.moves,
                      u8"逆序输入上每个元素都比 pivot 小，交换真的发生，搬移次数因此高于已排序输入");
    }

    /* 三数取中版在有序输入上不退化 */
    {
        bool ok = true;
        for (std::size_t n = 8; n <= 512; n *= 2) {
            const std::size_t bound = 2 * depth_limit_for(n) + 2;
            std::vector<int> ascending = make_input(Shape::Sorted, n);
            std::vector<int> descending = make_input(Shape::Reversed, n);
            if (quick_sort_median3(ascending).depth > bound ||
                quick_sort_median3(descending).depth > bound) {
                ok = false;
            }
        }
        checks.expect(ok, u8"三数取中版在已排序与逆序输入上深度不超过 2 × 深度限值 + 2");
    }

    /* 归并排序的搬移与深度 */
    {
        bool ok = true;
        for (std::size_t n = 2; n <= 1024; n *= 2) {
            std::vector<int> a = make_input(Shape::Random, n);
            const Stats st = merge_sort(a);
            if (st.moves != 2 * n * (st.depth - 1)) {
                ok = false;
            }
        }
        checks.expect(ok, u8"归并排序在 2 的幂规模上搬移次数正好是 2 × n × (深度 - 1)");
    }
    {
        bool ok = true;
        for (std::size_t s = 0; s < shape_count(); ++s) {
            std::vector<int> a = make_input(static_cast<Shape>(s), 1000);
            const Stats st = merge_sort(a);
            if (st.moves > 2 * a.size() * st.depth) {
                ok = false;
            }
        }
        checks.expect(ok, u8"归并排序在四类输入上搬移次数不超过 2 × n × 深度");
    }
    {
        bool ok = true;
        for (std::size_t n = 1; n <= 1000; n += 37) {
            std::vector<int> a = make_input(Shape::Random, n);
            if (merge_sort(a).depth != halving_levels(n)) {
                ok = false;
            }
        }
        checks.expect(ok, u8"归并排序的递归深度等于 n 按上取整反复折半的层数（n 取 28 个规模）");
    }

    /* 内省排序：上界、切换与兜底 */
    {
        const std::size_t n = 1000;
        const std::size_t limit = depth_limit_for(n);
        bool bounded = true;
        bool triggered = false;
        for (std::size_t s = 0; s < shape_count(); ++s) {
            std::vector<int> a = make_input(static_cast<Shape>(s), n);
            std::vector<int> b = a;
            const Stats intro = intro_sort(a);
            const Stats median = quick_sort_median3(b);
            if (intro.comparisons > median.comparisons || intro.depth > limit + 1) {
                bounded = false;
            }
            if (intro.cutovers > 0) {
                triggered = true;
            }
        }
        checks.expect(bounded, u8"内省排序在四类输入上比较次数不超过三数取中版，深度不超过限值 + 1");
        checks.expect(triggered, u8"内省排序在这四类输入上真的触发了深度兜底，切换次数大于 0");
    }
    {
        std::vector<int> a = make_input(Shape::Duplicates, 1000);
        std::vector<int> b = make_input(Shape::Duplicates, 1000);
        const Stats median = quick_sort_median3(a);
        const Stats intro = intro_sort(b);
        checks.expect(median.depth > depth_limit_for(1000) &&
                          intro.depth <= depth_limit_for(1000) + 1 && a == b,
                      u8"大量重复输入上三数取中版深度超过限值，内省排序把它压回限值以内");
    }
    {
        std::vector<int> a = make_input(Shape::Sorted, 1000);
        std::vector<int> want = a;
        std::sort(want.begin(), want.end());
        const Stats st = intro_sort_with_limit(a, 1);
        checks.expect(st.cutovers > 0 && st.depth <= 2 && a == want,
                      u8"限值压到 1：切换真的发生、深度不超过 2、结果仍然排好");
    }

    /* 全部元素相同：三数取中救不了，兜底才有用 */
    {
        const std::size_t n = 1000;
        const std::size_t pairs = n * (n - 1) / 2;
        std::vector<int> flat_first(n, 7);
        std::vector<int> flat_median(n, 7);
        std::vector<int> flat_intro(n, 7);
        const Stats f = quick_sort_first(flat_first);
        const Stats m = quick_sort_median3(flat_median);
        const Stats i = intro_sort(flat_intro);
        checks.expect(f.comparisons == pairs && f.depth == n && f.moves == 0 &&
                          m.comparisons == pairs + 3 * (n - 1) && m.depth == n &&
                          m.moves == 3 * (n - 2),
                      u8"全部元素相同时两个快排版都退化成 n 层；三数取中版每趟还多花 3 次比较与 3 次搬移");
        checks.expect(i.depth <= depth_limit_for(n) + 1 && i.cutovers == 1 &&
                          i.comparisons < pairs / 20,
                      u8"同一份输入交给内省排序：切换 1 次，深度压到限值以内，比较次数降到二十分之一以下");
    }

    /* 堆排序的深度与上界 */
    {
        bool ok = true;
        for (std::size_t s = 0; s < shape_count(); ++s) {
            std::vector<int> a = make_input(static_cast<Shape>(s), 1000);
            const Stats st = heap_sort(a);
            if (st.depth != 0 || st.comparisons > 2 * a.size() * (halving_levels(a.size()) - 1)) {
                ok = false;
            }
        }
        checks.expect(ok, u8"堆排序在四类输入上深度恒为 0，比较次数不超过 2 × n × ceil(log2 n)");
    }

    /* 稳定性：两种稳定与四种不稳定 */
    const std::vector<RecordAlgorithm> &ralgs = record_algorithms();
    {
        const StabilityRun ins = stability_of(insertion_sort_records, 64, 200);
        checks.expect(ins.keep_all() && ins.trials == 64,
                      u8"插入排序在 64 组输入上同 key 记录的先后全部保持");
        const StabilityRun mrg = stability_of(merge_sort_records, 64, 200);
        checks.expect(mrg.keep_all() && mrg.trials == 64,
                      u8"归并排序在 64 组输入上同 key 记录的先后全部保持");
    }
    {
        bool ok = true;
        std::string detail;
        const std::size_t unstable_expect[] = {2, 3, 4, 5};   /* 首元素、三数取中、堆、内省 */
        for (const std::size_t index : unstable_expect) {
            const StabilityRun run = stability_of(ralgs[index].sort, 64, 200);
            if (run.keep_all() || run.first_failed_trial == 0) {
                ok = false;
                detail = ralgs[index].name;
            }
        }
        checks.expect(ok, detail.empty()
                              ? u8"首元素版、三数取中版、堆排序、内省排序各至少有一组输入不稳定"
                              : u8"有排序在 64 组输入上都没出现反例：" + detail);
    }
    /* 稳定性判定本身能证伪 */
    {
        /* 判定函数本身要能证伪：手工摆一个同 key 逆序的数组 */
        std::vector<Record> bad = {{1, 0}, {1, 1}, {1, 2}};
        std::swap(bad[0], bad[2]);
        std::vector<Record> good = {{1, 0}, {1, 1}, {2, 0}, {2, 1}};
        std::vector<Record> unsorted = {{2, 0}, {1, 0}};
        checks.expect(first_stability_violation(bad) == 1 &&
                          first_stability_violation(good) == -1 &&
                          !records_sorted_by_key(unsorted),
                      u8"稳定性判定能证伪：同 key 逆序时报出反例，同 key 正序时放行");
    }

    /* 输入构造 */
    {
        bool ok = true;
        for (std::size_t s = 0; s < shape_count(); ++s) {
            const Shape shape = static_cast<Shape>(s);
            if (make_input(shape, 500) != make_input(shape, 500)) {
                ok = false;
            }
        }
        const std::vector<int> ascending = make_input(Shape::Sorted, 500);
        const std::vector<int> descending = make_input(Shape::Reversed, 500);
        const std::vector<int> random = make_input(Shape::Random, 500);
        const std::vector<int> repeated = make_input(Shape::Duplicates, 500);
        if (!std::is_sorted(ascending.begin(), ascending.end())) {
            ok = false;
        }
        if (!std::is_sorted(descending.begin(), descending.end(), std::greater<int>())) {
            ok = false;
        }
        if (std::is_sorted(random.begin(), random.end()) ||
            std::is_sorted(random.begin(), random.end(), std::greater<int>())) {
            ok = false;
        }
        checks.expect(ok, u8"输入构造可复现：同形态造两次逐位相同；随机形态既非升序也非降序");
        checks.expect(distinct_values(repeated) <= 10 && distinct_values(ascending) == 500,
                      u8"大量重复形态的取值不超过 10 个，已排序形态 500 个值互不相同");
    }

    /* 结构量可重复 */
    {
        bool ok = true;
        for (std::size_t i = 0; i < algs.size(); ++i) {
            for (std::size_t s = 0; s < shape_count(); ++s) {
                const Shape shape = static_cast<Shape>(s);
                std::vector<int> first = make_input(shape, 300);
                std::vector<int> second = make_input(shape, 300);
                if (!algs[i].sort(first).same_numbers_as(algs[i].sort(second))) {
                    ok = false;
                }
            }
        }
        checks.expect(ok, u8"六种排序在四类输入上各跑两遍，四项结构量逐位相同");
    }

    return checks.finish();
}

}   /* namespace scmp */
