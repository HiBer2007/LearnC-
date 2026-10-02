/* sortcmp.cpp —— 练习模板 05 的实现（C++）
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
 *     阶段 1-1   merge_range           归并的合并段
 *     阶段 1-2   merge_sort_bottomup   同一件事自底向上做
 *     阶段 2-1   partition_lomuto      Lomuto 分区
 *     阶段 3-1   choose_pivot          三数取中
 *     阶段 4-1   sift_down             堆排序的下沉
 *
 * 每个 TODO 上面写明「要做什么」，末尾的「判据」一行给出填完之后
 * 应当看到的数——那些数都由 src/main_cli.cpp 打印，逐个对得上才算做完。
 * 计数只走 less_key / swap_items / move_item 这三个已给出的函数，
 * 别的地方不要去动 Stats。
 */
#include "sortcmp.hpp"

#include <algorithm>
#include <ostream>

namespace sc {

/* ==================================================================
 * 已给出的记账工具
 * ================================================================== */

bool less_key(int left_key, int right_key, Stats &s)
{
    ++s.compares;
    return left_key < right_key;
}

void swap_items(Item *a, int i, int j, Stats &s)
{
    const Item t = a[i];
    a[i] = a[j];
    a[j] = t;
    s.moves += 2;       /* 两个位置各被写了一次 */
}

void move_item(Item *dst, int di, const Item &v, Stats &s)
{
    dst[di] = v;
    ++s.moves;          /* 一个元素被搬到了新位置 */
}

/* ==================================================================
 * 已给出的数据
 * ================================================================== */

std::vector<Item> make_blocks(int n, int distinct)
{
    /* 片长按这张固定表循环，四种键轮流上：同一把键分成好几片，
     * 片与片之间隔着别的键。稳定排序要把每把键的各片按原次序接起来。 */
    static const int run_len[8] = {3, 5, 2, 6, 4, 2, 7, 3};

    std::vector<Item> v;
    v.reserve(static_cast<std::size_t>(n));
    int run = 0;
    while (static_cast<int>(v.size()) < n) {
        const int len = run_len[run % 8];
        const int key = run % distinct;
        for (int i = 0; i < len && static_cast<int>(v.size()) < n; ++i) {
            Item it;
            it.key = key;
            it.src = static_cast<int>(v.size());
            v.push_back(it);
        }
        ++run;
    }
    return v;
}

std::vector<Item> make_random(int n, unsigned seed, int key_max)
{
    /* 线性同余，种子写死：没有读文件，也没有拿时间当种子。 */
    std::vector<Item> v;
    v.reserve(static_cast<std::size_t>(n));
    unsigned x = seed;
    for (int i = 0; i < n; ++i) {
        x = x * 1103515245u + 12345u;
        Item it;
        it.key = static_cast<int>((x >> 16) % static_cast<unsigned>(key_max));
        it.src = i;
        v.push_back(it);
    }
    return v;
}

std::vector<Item> make_ascending(int n)
{
    std::vector<Item> v;
    v.reserve(static_cast<std::size_t>(n));
    for (int i = 0; i < n; ++i) {
        Item it;
        it.key = i;
        it.src = i;
        v.push_back(it);
    }
    return v;
}

/* ==================================================================
 * 已给出的判据工具
 * ================================================================== */

bool is_sorted(const Item *a, int n)
{
    for (int i = 1; i < n; ++i) {
        if (a[i].key < a[i - 1].key) {
            return false;
        }
    }
    return true;
}

bool stable_ok(const Item *a, int n)
{
    if (!is_sorted(a, n)) {
        return false;
    }
    int i = 0;
    while (i < n) {
        int j = i + 1;
        while (j < n && a[j].key == a[i].key) {
            if (a[j].src < a[j - 1].src) {
                return false;
            }
            ++j;
        }
        i = j;
    }
    return true;
}

bool same_seq(const Item *a, const Item *b, int n)
{
    for (int i = 0; i < n; ++i) {
        if (a[i].key != b[i].key || a[i].src != b[i].src) {
            return false;
        }
    }
    return true;
}

void print_keys(const Item *a, int n, std::ostream &os)
{
    for (int i = 0; i < n; ++i) {
        os << (i == 0 ? "" : " ") << a[i].key;
    }
    os << "\n";
}

std::vector<Item> reference_order(std::vector<Item> v)
{
    std::sort(v.begin(), v.end(), [](const Item &x, const Item &y) {
        if (x.key != y.key) {
            return x.key < y.key;
        }
        return x.src < y.src;
    });
    return v;
}

long long std_sort_compares(std::vector<Item> v)
{
    long long calls = 0;
    std::sort(v.begin(), v.end(), [&calls](const Item &x, const Item &y) {
        ++calls;
        return x.key < y.key;
    });
    return calls;
}

/* ==================================================================
 * 阶段 1：归并的合并段
 * ================================================================== */

static void merge_sort_rec(Item *a, int lo, int hi, Item *buf, Stats &s)
{
    DepthGuard guard(s);        /* 已给出：记一层，出函数时自动退回 */

    if (hi - lo < 2) {
        return;
    }
    const int mid = lo + (hi - lo) / 2;
    merge_sort_rec(a, lo, mid, buf, s);
    merge_sort_rec(a, mid, hi, buf, s);
    merge_range(a, lo, mid, hi, buf, s);
}

void merge_sort(Item *a, int n, Stats &s)
{
    if (n < 2) {
        return;
    }
    std::vector<Item> buf(static_cast<std::size_t>(n));
    merge_sort_rec(a, 0, n, buf.data(), s);
}

void merge_range(Item *a, int lo, int mid, int hi, Item *buf, Stats &s)
{
    /* 已给出：两段各自是有序的——左段 [lo, mid)、右段 [mid, hi)。
     * 任务是把这两段并成一段有序的，写回 a 的同一段区间。 */

    /* TODO（阶段 1-1，合并段）：
     * 要达成什么：左段 [lo, mid) 与右段 [mid, hi) 各自已经有序，
     *             把这两段并成一段有序的，写回 a 的同一段区间。
     * 自己问自己：
     *   —— 每一步该看两边的哪一个元素？
     *   —— 两边相等时先拿哪一边，才能让相等键的原始下标排完仍然升序？
     *   —— 有一边先拿完之后，另一段还要不要继续比较？（compares 由它定）
     *   —— 直接从 a 的左边往右写，会不会盖掉还没比过的元素？
     *   —— 往缓冲区写、再从缓冲区搬回来，这两个动作各走哪个函数？
     * 记数契约：一次「决定拿哪一边」调用一次 less_key；
     *           搬一个元素调用一次 move_item（进缓冲区、搬回来各算一次）。
     * 判据（见《配置步骤.md》阶段 1）：32 个元素、4 把键那一组，
     *       sorted、stable、same as reference 三行都是 yes，
     *       compares 是 93、moves 是 320、depth max 是 6；
     *       1024 个元素那一组 compares 是 8965、moves 是 20480、
     *       depth max 是 11（std::sort 的对照是 12018）。 */

    /* 占位实现：两段原样不动，等于没合并。整段替换掉即可 */
    (void)a;
    (void)lo;
    (void)mid;
    (void)hi;
    (void)buf;
    (void)s;
}

void merge_sort_bottomup(Item *a, int n, Stats &s)
{
    /* TODO（阶段 1-2，自底向上）：
     * 要达成什么：上面那套骨架是自上而下二分的，这一处改由循环驱动、
     *             不再递归，合并仍旧交给 merge_range，一行也不用另写。
     * 自己问自己：
     *   —— 一开始每一段有多长？每一轮之后段长变成多少？循环何时停？
     *   —— 末尾只剩一段、没有右邻时，这一轮怎么办？
     *   —— 右邻不足一整段时，这一段的右边界是 n 还是别的什么？
     *   —— 缓冲区要准备几份、在哪里准备？
     * 判据（见《配置步骤.md》阶段 1）：bottom-up 的 sorted、stable、
     *       same as reference 三行都是 yes，compares 是 93、moves 是 320，
     *       与递归版逐项相同；1024 个元素那一组 compares 也是 8965。 */

    /* 占位实现：什么都不做。整段替换掉即可 */
    (void)a;
    (void)n;
    (void)s;
}

/* ==================================================================
 * 阶段 2：快排的分区
 * ================================================================== */

static void quick_last_rec(Item *a, int lo, int hi, Stats &s)
{
    DepthGuard guard(s);

    if (hi - lo < 2) {
        return;
    }
    const int m = partition_lomuto(a, lo, hi, s);
    quick_last_rec(a, lo, m, s);
    quick_last_rec(a, m + 1, hi, s);
}

void quick_sort_last(Item *a, int n, Stats &s)
{
    if (n < 2) {
        return;
    }
    quick_last_rec(a, 0, n, s);
}

int partition_lomuto(Item *a, int lo, int hi, Stats &s)
{
    /* 已给出：方案定死为 Lomuto 分区，枢轴就是区间最后一个元素 a[hi - 1]，
     * 区间写作 [lo, hi)。定死方案是为了让固定输入下的交换次数唯一。 */

    /* TODO（阶段 2-1，Lomuto 分区）：
     * 要达成什么：除枢轴以外的元素分成两拨——比枢轴小的在前、
     *             不小于枢轴的在后面；分完之后枢轴自己站到两拨之间，
     *             函数返回它落在的下标。
     * 自己问自己：
     *   —— 从头扫到尾的过程中，你要随时记住的是哪一条边界？
     *   —— 扫完之后，枢轴该去的位置与这条边界是什么关系？
     *   —— 枢轴自己也在区间里，后面几次换位会不会把它挪走？
     *      拿什么去和每个元素比才比得下去？
     *   —— 两个下标相同时要不要跳过那次 swap_items？
     *      （契约定的是照记两次）
     * 记数契约：每扫过一个元素调用一次 less_key；每一次换位都走
     *           swap_items，连枢轴最后那一下也是。
     * 判据（见《配置步骤.md》阶段 2）：32 个元素那一组 sorted 是 yes、
     *       stable 是 no、compares 是 158、moves 是 144、depth max 是 10；
     *       1024 个元素那一组 compares 是 11416、moves 是 13020、
     *       depth max 是 23（std::sort 的对照是 12018）。 */

    /* 占位实现：不做任何分区，永远说枢轴就落在区间开头。
     * 整段替换掉即可 */
    (void)a;
    (void)hi;
    (void)s;
    return lo;
}

/* ==================================================================
 * 阶段 3：三数取中
 * ================================================================== */

static void quick_med_rec(Item *a, int lo, int hi, Stats &s)
{
    DepthGuard guard(s);

    if (hi - lo < 2) {
        return;
    }
    /* 已给出：三个候选取区间首、区间正中间、区间尾 */
    const int mid = lo + (hi - lo - 1) / 2;
    const int p = choose_pivot(a, lo, mid, hi - 1, s);
    swap_items(a, p, hi - 1, s);        /* 已给出：换到区间末尾，走阶段 2 的分区 */
    const int m = partition_lomuto(a, lo, hi, s);
    quick_med_rec(a, lo, m, s);
    quick_med_rec(a, m + 1, hi, s);
}

void quick_sort_med(Item *a, int n, Stats &s)
{
    if (n < 2) {
        return;
    }
    quick_med_rec(a, 0, n, s);
}

int choose_pivot(const Item *a, int i, int j, int k, Stats &s)
{
    /* 已给出：三个候选的位置由 quick_med_rec 挑好传进来，
     * 分别是区间首、区间正中间、区间尾，次序就是 i、j、k。
     * 三者互不相同（区间长度不足 2 时根本不会调到这里）。 */

    /* TODO（阶段 3-1，三数取中）：
     * 要达成什么：在三个候选里挑出「键排在正中间」的那一个，返回它的下标。
     * 自己问自己：
     *   —— 只比两次能不能保证挑对？三次之内怎么把它挑出来？
     *   —— 形参 a 是 const 的，那么能动的只有什么？
     * 契约：无论数据怎么排，本函数恰好记 3 次比较（做法不止一种）。
     *       验收程序要拿「末元素枢轴」与「三数取中」两组数字比高低，
     *       靠的就是这个固定次数。
     * 判据（见《配置步骤.md》阶段 3）：已经升序的 128 个元素上，
     *       末元素枢轴 compares 是 8128、depth max 是 128；
     *       三数取中 compares 是 841、depth max 是 8，两者 sorted 都是 yes。
     *       1024 个伪随机元素上换过来：compares 11773 对 11416（三数取中
     *       略多），depth max 16 对 23（三数取中更浅）。 */

    /* 占位实现：永远返回第一个候选。整段替换掉即可 */
    (void)a;
    (void)j;
    (void)k;
    (void)s;
    return i;
}

/* ==================================================================
 * 阶段 4：堆排序的下沉
 * ================================================================== */

static void sift_down(Item *a, int n, int i, Stats &s)
{
    /* 已给出：把 a[0, n) 看成完全二叉树，i 号节点的两个孩子是
     * 2 * i + 1 与 2 * i + 2，父子的键满足「父不小于子」才是堆。 */

    /* TODO（阶段 4-1，下沉）：
     * 要达成什么：把以 i 为根的这棵子树捋成堆——只有根这一处可能违反
     *             「父不小于子」，把根一路往下挪到不违反为止。
     * 自己问自己：
     *   —— 每一层要比较哪两个东西、比较几次，compares 那一列才定得下来？
     *   —— 只有一个孩子时还要不要先比较？孩子根本不存在时怎么办？
     *   —— 形参 n 是「堆里还剩几个元素」还是「数组长度」？两者何时相等？
     *   —— 往下走一层之后，当前节点落在哪个下标上？
     * 记数契约：真要换位的时候走 swap_items（一次记两次搬移）。
     * 判据（见《配置步骤.md》阶段 4）：32 个元素那一组 sorted 是 yes、
     *       stable 是 no、compares 是 204、moves 是 216；
     *       1024 个元素那一组 compares 是 17295、moves 是 18658、
     *       depth max 是 0（堆排序不吃栈帧）。 */

    /* 占位实现：什么都不做，堆永远建不起来。整段替换掉即可 */
    (void)a;
    (void)n;
    (void)i;
    (void)s;
}

void heap_sort(Item *a, int n, Stats &s)
{
    /* 已给出：先把整个区间调整成一个堆（从最后一个非叶节点往回走），
     * 再把堆顶换到当前末尾、堆缩小一格，换过来的元素重新下沉。 */
    for (int i = n / 2 - 1; i >= 0; --i) {
        sift_down(a, n, i, s);
    }
    for (int end = n - 1; end > 0; --end) {
        swap_items(a, 0, end, s);
        sift_down(a, end, 0, s);
    }
}

} /* namespace sc */
