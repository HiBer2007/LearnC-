/* sortnc.cpp —— 练习模板 06 的实现（C++）
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
 *     阶段 1-1   count_keys         范围校验与计数
 *     阶段 2-1   block_starts       每一块的起点
 *     阶段 2-2   count_sort         回填
 *     阶段 3-1   radix_sort         按位分配
 *     阶段 4-1   distribute_stable  两种扫描方向的稳定性对照
 *
 * 每个 TODO 上面写明「要做什么」，下面的「判据」一行给出填完之后
 * 应当看到的数——那些数都由 src/main_cli.cpp 打印，逐个对得上才算做完。
 * 两处留空都在阶段 2，阶段 3 与阶段 4 各一处。
 */
#include "sortnc.hpp"

#include <algorithm>
#include <ostream>
#include <vector>

namespace snc {

/* ==================================================================
 * 阶段 1：把每个键的个数记下来
 * ================================================================== */

bool count_keys(const Item *a, int n, int *counts, int counts_size, Counters &c)
{
    /* 已给出：计数数组的清零。桶自身的整理，两个计数器都不记。 */
    for (int k = 0; k < counts_size; ++k) {
        counts[k] = 0;
    }

    /* TODO（阶段 1-1）：
     * 两件事，分成两趟走：
     *
     *   第一趟只验不动——n 个键里只要有一个落在计数数组管不到的范围，
     *   这次调用就返回 false，而且计数数组一个格子也不许被动过
     *   （验收程序把它清零之后才调用，事后单独打一行「动过没有」）。
     *
     *   第二趟才记数——计数数组有一整排格子，哪个格子管哪个键？
     *   把每个键的个数加到它该去的那个格子上。
     *   每记一个元素，照已经给出的口径维护两个计数器：改写了数组的一个格子
     *   记一次写入，让一个元素进了一次桶记一次桶操作（清零那几行不算）。
     *
     * 判据（见《配置步骤.md》阶段 1）：
     *       counts 的 16 个格子是 8 0 0 0 8 0 0 0 0 8 0 0 0 8 0 8，合计 40；
     *       writes 是 40、bucket ops 是 40；
     *       键 16 与键 -1 那两次都报 rejected，且计数数组仍然全是 0。 */

    /* 占位实现：一个格子也没记，越界的键也没报 */
    (void)a;
    (void)n;
    (void)c;
    return true;
}

/* ==================================================================
 * 阶段 2：每一块的起点，以及回填
 * ================================================================== */

void prefix_sum(int *counts, int counts_size)
{
    /* 已给出：counts[k] 变成「键不大于 k 的元素共有几个」。
     * 桶自身的整理，两个计数器都不记。 */
    for (int k = 1; k < counts_size; ++k) {
        counts[k] = counts[k] + counts[k - 1];
    }
}

void block_starts(const int *counts, int counts_size, int *starts)
{
    /* TODO（阶段 2-1）：
     * counts 已经是前缀和：counts[k] 是「键不大于 k 的元素共有几个」。
     * 这句话里同时藏着每一块的终点与下一块的起点。
     * 把「键 k 的那一块从输出数组的哪个下标开始」写进 starts[k]。
     *
     * 判据（见《配置步骤.md》阶段 2）：
     *       starts 是 0 8 8 8 8 16 16 16 16 16 24 24 24 24 32 32；
     *       最后一个键的那一块从 32 开始，到 40 结束——40 就是元素总数。 */

    /* 占位实现：一个起点也没写，验收程序看到的是被清零之后的一排 0 */
    (void)counts;
    (void)counts_size;
    (void)starts;
}

bool count_sort(const Item *a, int n, Item *out, int *counts, int counts_size, Counters &c)
{
    /* 已给出：先数一遍每个键有几个 */
    if (!count_keys(a, n, counts, counts_size, c)) {
        return false;
    }

    /* 已给出：再对计数数组求前缀和，每个键的落点就定了 */
    prefix_sum(counts, counts_size);

    /* TODO（阶段 2-2）：
     * 回填：把 a 里的元素按前缀和给出的落点搬进 out。两件事要想清楚：
     *
     *   - 一个元素的落点怎么从前缀和里读出来；
     *   - 按什么次序取 a 里的元素。次序错了，键的序列看起来照样是对的，
     *     但相等键的先后会被打乱——验收程序专门把这一列打印出来。
     *
     * 若你把落点直接记回 counts 里，那么放完之后 counts 剩下的东西
     * 也应当说得通（验收程序不查这一项，自查时可以把它剩下的东西写出来）。
     * 每搬一个元素，照已经给出的口径维护两个计数器。
     *
     * 判据（见《配置步骤.md》阶段 2）：
     *       out 的键序列是 0 八个、4 八个、9 八个、13 八个、15 八个；
     *       五个键的原始下标各自严格递增；
     *       与 std::stable_sort 的结果逐位相同；
     *       writes 是 120、bucket ops 是 80（含计数阶段那 40 与 40）。 */

    /* 占位实现：out 一个元素也没放，保持调用方清零之后的样子 */
    (void)a;
    (void)n;
    (void)out;
    return true;
}

/* ==================================================================
 * 阶段 3：基数排序，一轮一位
 * ================================================================== */

bool radix_sort(const Item *a, int n, Item *out, int *round_counts, Counters &c)
{
    /* 已给出：基数排序要求键非负——取位函数对负数没有意义 */
    for (int i = 0; i < n; ++i) {
        if (a[i].key < 0) {
            return false;
        }
    }

    /* 已给出：轮数 = 最大值的十进制位数（最大值是 0 时算一轮） */
    const int rounds = radix_rounds(a, n);

    /* 已给出：工作副本。每一轮的「分配进桶、再接回来」都在这份副本上做，
     * 调用方传进来的数据不动。 */
    std::vector<Item> buf(a, a + n);

    for (int r = 0; r < rounds; ++r) {
        /* 已给出：这一轮看哪一位 */
        const int exp = pow10(r);

        /* 已给出：10 个桶，每个桶装元素本身 */
        std::vector<Item> bucket[kRadixSlots];

        /* TODO（阶段 3-1）：
         * 把工作副本里的每个元素按当前位分到 10 个桶里的某一个。
         * 桶号由这一位上的数字决定，取位函数已经给出。
         *
         * 次序上有一件事要想清楚：同一个桶里攒起来的元素，先后取决于你
         * 扫描工作副本的方向。LSD 基数排序的立足点是「上一轮排好的次序
         * 不能被这一轮打乱」，所以这个方向不是随便挑的。
         *
         * 每分一个元素，照已经给出的口径维护两个计数器——写回那一段就写在
         * 下面，照它记。
         *
         * 判据（见《配置步骤.md》阶段 3）：
         *       40 个元素那一组走 2 轮，逐轮的桶大小是
         *         第 1 轮 8 0 0 8 8 8 0 0 0 8、第 2 轮 24 16 0 0 0 0 0 0 0 0；
         *       24 个元素那一组走 3 轮，逐轮的桶大小见验收输出；
         *       两组的结果都与 std::stable_sort 逐位相同；
         *       40 个元素那一组的 writes 是 200、bucket ops 是 160。 */

        /* 占位实现：这一行随 TODO 一起删掉 */
        (void)exp;

        /* 已给出：把这一轮各桶的元素个数抄进 round_counts，验收程序要打印 */
        for (int d = 0; d < kRadixSlots; ++d) {
            round_counts[r * kRadixSlots + d] = static_cast<int>(bucket[d].size());
        }

        /* 已给出：按桶号从小到大把各桶接回工作副本。
         * 两个计数器怎么记，看下面两行；分配那一段照这个口径记。 */
        int k = 0;
        for (int d = 0; d < kRadixSlots; ++d) {
            for (std::size_t j = 0; j < bucket[d].size(); ++j) {
                buf[static_cast<std::size_t>(k)] = bucket[d][j];
                ++k;
                ++c.writes;     /* 一个元素被写回数组 */
                ++c.bucket_ops; /* 从桶里取回一个元素 */
            }
        }
    }

    /* 已给出：最后一份结果拷进 out（拷一次也算一次写入） */
    for (int i = 0; i < n; ++i) {
        out[i] = buf[static_cast<std::size_t>(i)];
        ++c.writes;
    }

    return true;
}

/* ==================================================================
 * 阶段 4：稳定性是保住的，还是丢掉的
 * ================================================================== */

int bucket_of(int key)
{
    return key;     /* 按整个键分：键 0 到 kKeyMax 各一个桶 */
}

void distribute_stable(const Item *a, int n, Item *out, bool forward, Counters &c)
{
    /* 已给出：键 0 到 kKeyMax 各一个桶，桶里装元素本身 */
    std::vector<Item> bucket[kKeySlots];

    /* TODO（阶段 4-1）：
     * 分配与写回都在这里，一共两件事：
     *
     *   - 桶内的元素怎么追加；
     *   - 写回 out 的时候，从每个桶的哪一头取。
     *
     * forward 决定扫描输入的方向：true 从前往后，false 从后往前。
     * 两次调用除这一个 bool 之外必须一模一样——「除方向外没有别的差别」
     * 正是这一阶段的全部意义，否则两种结果对不上号。
     *
     * 每处理一个元素，照阶段 3 写回那一段的口径维护两个计数器：
     * 一个元素被写进数组记一次写入，把一个元素分配进桶、从桶里取回一个元素
     * 各记一次桶操作。
     *
     * 判据（见《配置步骤.md》阶段 4）：
     *       两种方向的 out 都按键排好；
     *       从前往后的那一次，相等键的原始下标严格递增；
     *       从后往前的那一次不递增（同一个键的下标恰好倒过来）；
     *       两次的 writes 都是 80、bucket ops 都是 80。 */

    /* 占位实现：一个元素也没分，out 保持调用方清零之后的样子 */
    (void)a;
    (void)n;
    (void)out;
    (void)forward;
    (void)c;
    (void)bucket;
}

/* ==================================================================
 * 已给出的工具
 * ================================================================== */

int digit_at(int key, int exp)
{
    return (key / exp) % 10;
}

int pow10(int round)
{
    int v = 1;
    for (int i = 0; i < round; ++i) {
        v *= 10;
    }
    return v;
}

int max_key(const Item *a, int n)
{
    int m = 0;
    for (int i = 0; i < n; ++i) {
        if (a[i].key > m) {
            m = a[i].key;
        }
    }
    return m;
}

int radix_rounds(const Item *a, int n)
{
    int rounds = 1;
    for (int v = max_key(a, n); v >= 10; v /= 10) {
        ++rounds;
    }
    return rounds;
}

void print_keys(const Item *a, int n, std::ostream &os)
{
    for (int i = 0; i < n; ++i) {
        if (i != 0) {
            os << ' ';
        }
        os << a[i].key;
    }
    os << '\n';
}

void print_indices_of_key(const Item *out, int n, int key, std::ostream &os)
{
    bool any = false;
    for (int i = 0; i < n; ++i) {
        if (out[i].key != key) {
            continue;
        }
        if (any) {
            os << ' ';
        }
        os << out[i].index;
        any = true;
    }
    if (!any) {
        os << "(none)";
    }
    os << '\n';
}

bool indices_ascending(const Item *out, int n, int key)
{
    bool seen = false;
    int prev = 0;
    for (int i = 0; i < n; ++i) {
        if (out[i].key != key) {
            continue;
        }
        if (seen && out[i].index <= prev) {
            return false;
        }
        prev = out[i].index;
        seen = true;
    }
    return true;
}

bool all_indices_ascending(const Item *out, int n)
{
    for (int i = 1; i < n; ++i) {
        if (out[i].key == out[i - 1].key && out[i].index <= out[i - 1].index) {
            return false;
        }
    }
    return true;
}

bool same_items(const Item *a, const Item *b, int n)
{
    for (int i = 0; i < n; ++i) {
        if (a[i].key != b[i].key || a[i].index != b[i].index) {
            return false;
        }
    }
    return true;
}

bool is_sorted_by_key(const Item *a, int n)
{
    for (int i = 1; i < n; ++i) {
        if (a[i].key < a[i - 1].key) {
            return false;
        }
    }
    return true;
}

void stable_sort_reference(const Item *a, int n, Item *out)
{
    for (int i = 0; i < n; ++i) {
        out[i] = a[i];
    }
    std::stable_sort(out, out + n,
                     [](const Item &x, const Item &y) { return x.key < y.key; });
}

} /* namespace snc */
