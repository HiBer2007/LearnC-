/* sortcmp.hpp —— 练习模板 05 的核心接口（C++）
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
 * 比较排序的三种策略拆成 4 个阶段，留空的地方都在 src/sortcmp.cpp 里：
 *
 *     阶段 1  merge_range / merge_sort_bottomup   归并的合并段
 *     阶段 2  partition_lomuto                    Lomuto 分区
 *     阶段 3  choose_pivot                        三数取中
 *     阶段 4  sift_down                           堆排序的下沉
 *
 * 数据是 (键, 原始下标) 两个字段的数组：比较只看键，原始下标用来判稳定性。
 * 三种策略的可比性靠两件事——同一份固定数据、同一套计数规则。
 * 计数规则见下面「已给出的记账工具」一段：任何一次比较都走 less_key，
 * 任何一次把元素挪到别处都走 swap_items 或 move_item，
 * 不许在别的地方直接去加 Stats 里的计数器。
 *
 * 本模板只碰 int 键的比较排序，不涉及容器与数据结构的算法——
 * 那些属于 09-高阶数据结构 板块。
 */
#ifndef SORTCMP_HPP
#define SORTCMP_HPP

#include <cstddef>
#include <iosfwd>
#include <vector>

namespace sc {

/* ==================================================================
 * 数据与计数器
 * ================================================================== */

/* 一个待排序的元素。比较只看 key；src 是它在输入里的原始下标，
 * 稳定性判据问的就是「键相同的那些元素排完之后 src 还升不升序」。 */
struct Item {
    int key = 0;
    int src = 0;
};

/* 贯穿全模板的计数器，外加递归深度。
 *
 *     compares   比较次数：每调用一次 less_key 加一
 *     moves      交换（搬移）次数：每把一个元素写到一个新位置加一，
 *                因此一次交换算两次搬移——两个元素各自换了位置
 *     depth      当前递归深度
 *     depth_max  到过的最深一层
 */
struct Stats {
    long long compares = 0;
    long long moves = 0;
    long long depth = 0;
    long long depth_max = 0;
};

/* 已给出：递归函数里声明一个局部对象，进函数时记一层、出函数时自动退回。
 * 与 01-recursion 模板里的 Counters 与 DepthGuard 是同一个手法。 */
struct DepthGuard {
    Stats &s;

    explicit DepthGuard(Stats &st) : s(st) {
        ++s.depth;
        if (s.depth > s.depth_max) {
            s.depth_max = s.depth;
        }
    }
    ~DepthGuard() { --s.depth; }

    DepthGuard(const DepthGuard &) = delete;
    DepthGuard &operator=(const DepthGuard &) = delete;
};

/* ==================================================================
 * 已给出的记账工具
 *
 * 全模板只有这两个函数会动计数器。这样三种策略的比较次数与搬移次数
 * 才是同一把尺子量出来的，数字之间才能横向比。
 * ================================================================== */

/* 记一次比较，回答「左键是否小于右键」 */
bool less_key(int left_key, int right_key, Stats &s);

/* 记两次搬移，交换 a[i] 与 a[j]（两个下标相同时也照样记两次） */
void swap_items(Item *a, int i, int j, Stats &s);

/* 记一次搬移，把 v 写到 dst[di] */
void move_item(Item *dst, int di, const Item &v, Stats &s);

/* ==================================================================
 * 已给出的数据：写死在源码里或固定种子，重跑逐位相同
 * ================================================================== */

/* n 个元素，只有 distinct 把键；同一把键分成好几片，片长按固定表循环。
 * 相等键成片出现是稳定性判据的前提——没有相等的键就分不出稳定与不稳定。 */
std::vector<Item> make_blocks(int n, int distinct);

/* n 个元素，键由固定种子的线性同余发生器给出，落在 [0, key_max) */
std::vector<Item> make_random(int n, unsigned seed, int key_max);

/* n 个元素，键已经升序（0, 1, 2, ...） */
std::vector<Item> make_ascending(int n);

/* ==================================================================
 * 已给出的判据工具
 * ================================================================== */

/* 键是不是非降的 */
bool is_sorted(const Item *a, int n);

/* 稳定性判据：先要求键非降，再要求同一把键的原始下标升序。
 * 只看后一条是不够的——没排序的输入也满足它。 */
bool stable_ok(const Item *a, int n);

/* 两个序列是不是逐项相同（键与原始下标都比） */
bool same_seq(const Item *a, const Item *b, int n);

/* 把键按顺序打印成一行 */
void print_keys(const Item *a, int n, std::ostream &os);

/* 已给出的标尺：把序列按 (键, 原始下标) 排一遍，得到稳定排序唯一可能的那个结果。
 * 它不参与计数，只用来说明「稳定排完之后应当长什么样」。 */
std::vector<Item> reference_order(std::vector<Item> v);

/* 已给出的对照：用 std::sort 排同一个序列，返回它调了多少次比较函数。
 * 这个数与标准库的实现有关，验收标准里记的是 libstdc++ 上的实测值。 */
long long std_sort_compares(std::vector<Item> v);

/* ==================================================================
 * 阶段 1：归并的合并段
 * ================================================================== */

/* TODO（阶段 1-1）：把 a 的两段 [lo, mid) 与 [mid, hi) 合并成一段有序的。
 * 两段各自已经有序；合并结果先写进 buf 的 [lo, hi)，再整段搬回 a。
 * 判据：见 src/sortcmp.cpp 里这一处 TODO 的注释。 */
void merge_range(Item *a, int lo, int mid, int hi, Item *buf, Stats &s);

/* 已给出：归并排序的驱动——准备一份与 a 等长的缓冲区，交给二分骨架 */
void merge_sort(Item *a, int n, Stats &s);

/* TODO（阶段 1-2）：同一件事自底向上做，把二分那套递归换成循环，
 * 合并仍旧交给上面那个 merge_range。 */
void merge_sort_bottomup(Item *a, int n, Stats &s);

/* ==================================================================
 * 阶段 2：快排的分区
 * ================================================================== */

/* TODO（阶段 2-1）：Lomuto 分区。枢轴固定是区间最后一个元素 a[hi-1]，
 * 返回它排好之后落在的下标；区间写作 [lo, hi)。 */
int partition_lomuto(Item *a, int lo, int hi, Stats &s);

/* 已给出：快排骨架，枢轴固定取区间最后一个元素。 */
void quick_sort_last(Item *a, int n, Stats &s);

/* ==================================================================
 * 阶段 3：三数取中
 * ================================================================== */

/* TODO（阶段 3-1）：在 a[i]、a[j]、a[k] 三个候选里挑出键排在中间的那一个，
 * 返回它的下标。契约：无论数据怎么排，本函数恰好记 3 次比较。 */
int choose_pivot(const Item *a, int i, int j, int k, Stats &s);

/* 已给出：快排骨架的第二版。它先用 choose_pivot 取枢轴下标、换到区间末尾，
 * 再走阶段 2 的分区——两版共用同一个分区函数。 */
void quick_sort_med(Item *a, int n, Stats &s);

/* ==================================================================
 * 阶段 4：堆排序的下沉
 * ================================================================== */

/* 已给出：建堆循环，以及「堆顶换到末尾、堆缩小一格」的循环。
 * 每一轮里的下沉由 src/sortcmp.cpp 里的 sift_down 完成（TODO 阶段 4-1）。 */
void heap_sort(Item *a, int n, Stats &s);

} /* namespace sc */

#endif /* SORTCMP_HPP */
