/* sortnc.hpp —— 练习模板 06 的核心接口（C++）
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
 * 「不比较也能排」这一件事拆成 4 个阶段，每个阶段的实现写在 src/sortnc.cpp 里：
 *
 *     阶段 1  count_keys         计数：每个键的个数记在哪个格子里
 *     阶段 2  block_starts       前缀和读出来的「每一块从哪开始」
 *             count_sort        回填：按落点把元素放进输出数组
 *     阶段 3  radix_sort         按位分配到 10 个桶里，一轮一位
 *     阶段 4  distribute_stable  同样的桶分配，两种扫描方向的稳定性对照
 *
 * 排序的元素是 Item：一个键，加上它在原始数组里的下标。下标一路跟着元素走，
 * 排完之后按「键相等」分组打印这些下标，就能看出相等键的先后有没有被打乱。
 * 这就是稳定性的判据，也是本模板与比较排序的分工所在：判据落在下标的次序上，
 * 而不是落在比较次数上——这两类排序的代价本来就不在同一个地方。
 */
#ifndef SORTNC_HPP
#define SORTNC_HPP

#include <iosfwd>

namespace snc {

/* 键的取值范围是 0 到 kKeyMax（含），计数数组的格子数与之一一对应。
 * 数据里只有 5 个不同的键，格子难免空着——空着的格子也是判据的一部分。 */
const int kKeyMax = 15;
const int kKeySlots = kKeyMax + 1;  /* 16 个格子 */
const int kRadixSlots = 10;         /* 一位十进制数字有 10 种取值 */

/* 待排序的元素：一个键，加上它在原始数组里的下标。 */
struct Item {
    int key = 0;
    int index = 0;
};

/* 两个计数器。口径固定，与你在代码里怎么写那一行无关：
 *
 *   writes      写入次数：每把一个值写进数组的一个格子，加一。
 *               写计数桶的格子、把元素放进桶、把元素写回数组都算；读不算。
 *   bucket_ops  桶操作次数：每让一个元素与桶打一次交道，加一。
 *               把一个元素分配进桶算一次，从桶里取回落点或取回元素也算一次。
 *
 * 桶自身的整理——计数数组清零、求前缀和、抄桶的大小——不计入这两个数：
 * 那几步是准备，没有元素在动。
 */
struct Counters {
    long long writes = 0;
    long long bucket_ops = 0;
};

/* ---------- 阶段 1：计数 ---------- */

/* 数出每个键出现几次，写进 counts。
 *
 * counts 与 counts_size 由调用方给：counts_size 个格子，第 k 个格子管键 k。
 * 函数自己负责把 counts 清零。
 * 返回 false 表示某个键落在了 0..counts_size-1 之外。
 *
 * 判据：counts 的 16 个格子、writes、bucket_ops，以及越界键的返回值。 */
bool count_keys(const Item *a, int n, int *counts, int counts_size, Counters &c);

/* ---------- 阶段 2：每一块的起点，以及回填 ---------- */

/* 已给出：把 counts 变成前缀和——counts[k] 成为「键不大于 k 的元素共有几个」。
 * 桶自身的整理，两个计数器都不记。 */
void prefix_sum(int *counts, int counts_size);

/* 把前缀和读成「键 k 的那一块从输出数组的哪个下标开始」，写进 starts。
 *
 * 判据：starts 的 16 个数，以及它与 counts 的对应关系。 */
void block_starts(const int *counts, int counts_size, int *starts);

/* 计数排序：先数、再求前缀和、最后回填到 out。
 *
 * counts 与 counts_size 由调用方给，函数会把它当成工作区用；
 * 返回 false 表示计数阶段发现了越界的键，此时 out 不动。
 *
 * 判据：out 的键序列、相等键的原始下标序列、与 std::stable_sort 的逐位比较。 */
bool count_sort(const Item *a, int n, Item *out, int *counts, int counts_size, Counters &c);

/* ---------- 阶段 3：基数排序 ---------- */

/* 基数排序（LSD，从最低位到最高位）。
 *
 * 每一轮按一位数字把元素分到 10 个桶里，再把桶按桶号接回来。
 * 轮数由已经给出的 radix_rounds 决定。
 * round_counts 要有 radix_rounds(a, n) * kRadixSlots 个格子，
 * 第 r 轮第 d 个桶的元素个数写进 round_counts[r * kRadixSlots + d]。
 * 返回 false 表示有键是负数。
 *
 * 判据：逐轮的桶大小、总轮数、排序结果与 std::stable_sort 的逐位比较。 */
bool radix_sort(const Item *a, int n, Item *out, int *round_counts, Counters &c);

/* ---------- 阶段 4：稳定性保持 ---------- */

/* 已给出：键 key 该进哪个桶。这一阶段按整个键分，键 0 到 kKeyMax 各一个桶。 */
int bucket_of(int key);

/* 按整个键把元素分到 kKeySlots 个桶里，再把桶接回 out。
 *
 * forward 为 true 表示从前往后扫描输入，false 表示从后往前。
 * 两次调用除扫描方向外一模一样——两次结果的差别只能来自这一个 bool。
 *
 * 判据：两种方向各自打印的「相等键的原始下标序列」。 */
void distribute_stable(const Item *a, int n, Item *out, bool forward, Counters &c);

/* ---------- 已给出的工具 ---------- */

/* 键 key 的第 exp 位上的数字（exp 为 1、10、100……） */
int digit_at(int key, int exp);

/* 10 的 round 次方（round 从 0 开始，得到 1、10、100……） */
int pow10(int round);

/* 数据里最大的键；全是 0 或没有元素时是 0 */
int max_key(const Item *a, int n);

/* 最大值的十进制位数（最大值是 0 时算一位） */
int radix_rounds(const Item *a, int n);

/* 把 n 个键按原次序打印成一行，末尾换行 */
void print_keys(const Item *a, int n, std::ostream &os);

/* 把 out 里键等于 key 的那些元素的原始下标，按它们在 out 里的先后打印成一行；
 * 一个也没有时打印 (none)。末尾换行。 */
void print_indices_of_key(const Item *out, int n, int key, std::ostream &os);

/* out 里键等于 key 的那些元素，原始下标是不是严格递增（这就是稳定性） */
bool indices_ascending(const Item *out, int n, int key);

/* out 里所有相等键的原始下标是不是都严格递增 */
bool all_indices_ascending(const Item *out, int n);

/* a 与 b 的键与原始下标是不是逐位相同 */
bool same_items(const Item *a, const Item *b, int n);

/* 键是不是从小到大排好了 */
bool is_sorted_by_key(const Item *a, int n);

/* 已给出的对照：std::stable_sort 排出来的结果，写进 out */
void stable_sort_reference(const Item *a, int n, Item *out);

} /* namespace snc */

#endif /* SORTNC_HPP */
