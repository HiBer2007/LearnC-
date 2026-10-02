/* sampling.cpp —— 练习模板 12 的实现（C++）
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
 * 本模板的 3 处 TODO 全在这个文件里，按阶段编号：
 *
 *     阶段 1-1   uniform_int        闭区间上的均匀整数
 *     阶段 2-1   shuffle            Fisher-Yates 的交换
 *     阶段 3-1   reservoir_sample   蓄水池的替换判据
 *
 * 阶段 4 不给 TODO：它的代码与判据都是现成的，用来验证前三处的
 * 结果之所以能当判据，靠的是固定种子。
 *
 * 每个 TODO 上面写明「要做什么」，末尾的「判据」一行给出填完之后
 * 应当看到的数——那些数都由 src/main_cli.cpp 打印，逐个对得上才算做完。
 */
#include "sampling.hpp"

#include <cstdlib>

namespace rnd {

/* ==================================================================
 * 已给出：引擎
 * ================================================================== */

std::mt19937 make_engine()
{
    return std::mt19937(static_cast<std::mt19937::result_type>(kSeed));
}

std::mt19937 make_unseeded_engine()
{
    /* 整台程序共用这一个 random_device：两次调用取到它序列里相邻的两个数。
     * 反过来，每次要种子都新建一个 random_device，循环极快时可能拿到
     * 同一个值，得到两串一模一样的「随机」序列。 */
    static std::random_device rd;
    return std::mt19937(static_cast<std::mt19937::result_type>(rd()));
}

/* ==================================================================
 * 阶段 1：均匀整数区间
 * ================================================================== */

int uniform_int(std::mt19937 &engine, int lo, int hi)
{
    /* TODO（阶段 1-1）：
     * 取一个落在闭区间 [lo, hi] 上的整数，区间里每个值出现的概率相同。
     * 三件事要想清楚：
     *   1. <random> 把「引擎」与「分布」分成两层：引擎只吐原始比特，
     *      「落在 [lo, hi] 上的均匀整数」这一层归分布管。
     *   2. 库里有一个专门做这件事的分布，两个端点由构造时给出；
     *      同一个分布对象可以反复配合同一台引擎取数。
     *   3. 自己拿引擎的原始输出去做除法或取模，就是本阶段
     *      rand_percent 那一列的做法，它的偏差写在验收输出里。
     * 判据（《配置步骤.md》阶段 1）：480000 次取数按 4096 分页，
     *   每页次数都在 80000 附近，最大偏差不到 300；
     *   而 rand_percent 那一列的前两页在 120000 附近、后四页在 60000 附近。
     *   全文重跑逐位相同。 */
    (void)engine;
    (void)lo;
    (void)hi;
    return lo;      /* 占位实现：永远返回区间左端，次数会全堆在第一个桶里 */
}

int rand_percent(int n)
{
    /* 已给出：错误对照，C 里最常见的那一行。
     * 本模板不调用 srand，rand() 等价于 srand(1)，序列是固定的——
     * 因此这一列也逐位可复现，但那是「碰巧固定」，不是「可控」。 */
    return std::rand() % n;
}

Histogram histogram_uniform(std::mt19937 &engine, int hi_exclusive,
                            int buckets, long long draws)
{
    Histogram h;
    h.counts.assign(static_cast<std::size_t>(buckets), 0);

    for (long long i = 0; i < draws; ++i) {
        const int v = uniform_int(engine, 0, hi_exclusive - 1);
        if (v < 0 || v >= hi_exclusive) {
            ++h.out_of_range;
            continue;
        }
        ++h.counts[static_cast<std::size_t>(v) * static_cast<std::size_t>(buckets)
                   / static_cast<std::size_t>(hi_exclusive)];
    }
    return h;
}

Histogram histogram_rand(int n, int buckets, long long draws)
{
    Histogram h;
    h.counts.assign(static_cast<std::size_t>(buckets), 0);

    for (long long i = 0; i < draws; ++i) {
        const int v = rand_percent(n);
        if (v < 0 || v >= n) {
            ++h.out_of_range;
            continue;
        }
        ++h.counts[static_cast<std::size_t>(v) * static_cast<std::size_t>(buckets)
                   / static_cast<std::size_t>(n)];
    }
    return h;
}

long long max_abs_deviation(const std::vector<long long> &counts, long long expected)
{
    long long worst = 0;
    for (std::size_t i = 0; i < counts.size(); ++i) {
        const long long d = counts[i] - expected;
        const long long ad = (d < 0) ? -d : d;
        if (ad > worst) {
            worst = ad;
        }
    }
    return worst;
}

/* ==================================================================
 * 阶段 2：Fisher-Yates 的交换
 * ================================================================== */

std::vector<int> make_deck(int n)
{
    std::vector<int> a;
    for (int i = 0; i < n; ++i) {
        a.push_back(i);
    }
    return a;
}

std::vector<int> shuffle_wrong(std::vector<int> a, std::mt19937 &engine)
{
    /* 已给出：错误对照。每一个位置都从整个数组里取一个位置交换。 */
    if (a.size() < 2) {
        return a;
    }
    for (std::size_t i = 0; i < a.size(); ++i) {
        const int j = uniform_int(engine, 0, static_cast<int>(a.size()) - 1);
        if (j < 0 || j >= static_cast<int>(a.size())) {
            continue;               /* 对手位置越界时这一轮不动，免得写出界 */
        }
        const int t = a[i];
        a[i] = a[static_cast<std::size_t>(j)];
        a[static_cast<std::size_t>(j)] = t;
    }
    return a;
}

void shuffle(std::vector<int> &a, std::mt19937 &engine)
{
    for (std::size_t i = a.size(); i > 1; --i) {
        /* TODO（阶段 2-1）：每一轮把 a[i-1] 与哪一个位置交换？
         * 三件事要想清楚：
         *   1. 对手位置要不要限制在一个下标段里？如果要，是哪一段——
         *      已经定下来的那一段，还是还没有定下来的那一段？
         *      这一段与 i 是什么关系？
         *   2. 对手位置可以是 i-1 自己吗（也就是这一轮什么都不换）？
         *      可以的时候，每一种排列被走到的路径数会怎样？不可以的时候
         *      又会怎样——想想 n 等于 2 时还剩下哪几种走法。
         *   3. 对照版本 shuffle_wrong 是「每个位置都从整个数组里取对手」，
         *      它取到的对手位置与当前位置没有关系，因此是错的。
         *      错在哪里，看验收输出里两张位置矩阵。
         * 判据（《配置步骤.md》阶段 2）：n=6、洗 240000 次，
         *   36 个格子的次数都在 40000 附近，最大偏差不到 600；
         *   对照版本在同一批次数下，格子次数从 32000 出头铺到 49000 出头。 */
    }
    (void)engine;   /* 占位实现：一轮也不换，牌序原样不动 */
}

static PositionMatrix count_positions(int n, long long rounds, bool wrong)
{
    PositionMatrix m;
    m.cells.assign(static_cast<std::size_t>(n), std::vector<long long>(static_cast<std::size_t>(n), 0));

    std::mt19937 engine = make_engine();
    for (long long r = 0; r < rounds; ++r) {
        std::vector<int> deck = make_deck(n);
        if (wrong) {
            deck = shuffle_wrong(deck, engine);
        } else {
            shuffle(deck, engine);
        }
        for (int p = 0; p < n; ++p) {
            const int v = deck[static_cast<std::size_t>(p)];
            if (v < 0 || v >= n) {
                ++m.bad_values;
                continue;
            }
            ++m.cells[static_cast<std::size_t>(v)][static_cast<std::size_t>(p)];
        }
    }
    return m;
}

PositionMatrix count_positions_correct(int n, long long rounds)
{
    return count_positions(n, rounds, false);
}

PositionMatrix count_positions_wrong(int n, long long rounds)
{
    return count_positions(n, rounds, true);
}

std::vector<long long> flatten_counts(const std::vector<std::vector<long long>> &m)
{
    std::vector<long long> out;
    for (std::size_t i = 0; i < m.size(); ++i) {
        for (std::size_t j = 0; j < m[i].size(); ++j) {
            out.push_back(m[i][j]);
        }
    }
    return out;
}

bool is_permutation_of_deck(const std::vector<int> &a)
{
    std::vector<bool> seen(a.size(), false);
    for (std::size_t i = 0; i < a.size(); ++i) {
        const int v = a[i];
        if (v < 0 || static_cast<std::size_t>(v) >= a.size() || seen[static_cast<std::size_t>(v)]) {
            return false;
        }
        seen[static_cast<std::size_t>(v)] = true;
    }
    return true;
}

/* ==================================================================
 * 阶段 3：蓄水池抽样
 * ================================================================== */

std::vector<int> make_stream(int n)
{
    std::vector<int> s;
    for (int i = 0; i < n; ++i) {
        s.push_back(i);
    }
    return s;
}

std::vector<int> reservoir_sample(const std::vector<int> &stream, std::size_t k,
                                  std::mt19937 &engine)
{
    std::vector<int> pool;

    /* 已给出：流不够长时池子就只装得下这么多 */
    const std::size_t kk = (k < stream.size()) ? k : stream.size();

    /* 已给出：先把池子填满 */
    for (std::size_t i = 0; i < kk; ++i) {
        pool.push_back(stream[i]);
    }

    /* 已给出：从这里开始遍历剩下的流数据 */
    for (std::size_t i = kk; i < stream.size(); ++i) {
        /* TODO（阶段 3-1，替换判据）：
         * 池子已经装满 kk 个，现在轮到流里的第 i 个元素（i 从 kk 数起）。
         * 它留下还是不留？留下的话，替掉池子里的哪一个位置？
         * 三件事要想清楚：
         *   1. 目标：流里每一个元素，最终留在池子里的概率完全一样。
         *      不是「前 kk 个留下」，也不是「越靠前越容易留下」。
         *      那么留下的概率与 i 是什么关系？靠后的元素是更容易
         *      挤进来还是更难？想清楚这一点，再想「替掉谁」。
         *   2. 每一轮向引擎要一次随机数就够了，而且这一次要同时定下
         *      「留不留」与「替掉谁」。工程上另一种常见写法是先按概率
         *      决定要不要替换、再单独取一个替换位置，它同样均匀，
         *      但每轮要两次随机数，引擎的取数序列会跟着变，验收输出里
         *      那几行下标序列就对不上。两种写法都对；对不上时以
         *      「每个元素被抽中的次数」那张表为准。
         *   3. 池子的大小整个过程不变，池子里也不该出现重复元素。
         * 判据（《配置步骤.md》阶段 3）：16 个元素、k=4、抽 60000 轮，
         *   每个元素被抽中的次数都在 15000 附近，最大偏差不到 300；
         *   次数总和是 240000；池子大小不对的轮数是 0。 */
    }
    (void)engine;   /* 占位实现：一个也不替换，池子里始终是前 kk 个元素 */

    return pool;
}

SampleStats count_selected(int n, std::size_t k, long long rounds)
{
    SampleStats st;
    st.rounds = rounds;
    st.hits.assign(static_cast<std::size_t>(n), 0);

    const std::vector<int> stream = make_stream(n);
    st.expected_size = static_cast<long long>((k < stream.size()) ? k : stream.size());

    std::mt19937 engine = make_engine();
    for (long long r = 0; r < rounds; ++r) {
        const std::vector<int> pool = reservoir_sample(stream, k, engine);
        if (static_cast<long long>(pool.size()) != st.expected_size) {
            ++st.bad_size;
        }
        bool dup = false;
        for (std::size_t i = 0; i < pool.size(); ++i) {
            if (pool[i] >= 0 && pool[i] < n) {
                ++st.hits[static_cast<std::size_t>(pool[i])];
            }
            for (std::size_t j = i + 1; j < pool.size(); ++j) {
                if (pool[i] == pool[j]) {
                    dup = true;
                }
            }
        }
        if (dup) {
            ++st.duplicates;
        }
    }
    return st;
}

std::vector<int> sorted_copy(const std::vector<int> &v)
{
    std::vector<int> out = v;
    for (std::size_t i = 1; i < out.size(); ++i) {
        const int key = out[i];
        std::size_t j = i;
        while (j > 0 && out[j - 1] > key) {
            out[j] = out[j - 1];
            --j;
        }
        out[j] = key;
    }
    return out;
}

/* ==================================================================
 * 阶段 4：固定种子与可复现（已给出，不留空）
 * ================================================================== */

std::vector<std::uint32_t> raw_draws(std::mt19937 &engine, int count)
{
    std::vector<std::uint32_t> out;
    for (int i = 0; i < count; ++i) {
        out.push_back(static_cast<std::uint32_t>(engine()));
    }
    return out;
}

bool same_u32(const std::vector<std::uint32_t> &a, const std::vector<std::uint32_t> &b)
{
    if (a.size() != b.size()) {
        return false;
    }
    for (std::size_t i = 0; i < a.size(); ++i) {
        if (a[i] != b[i]) {
            return false;
        }
    }
    return true;
}

std::string join_u32(const std::vector<std::uint32_t> &v)
{
    std::string s;
    for (std::size_t i = 0; i < v.size(); ++i) {
        if (i != 0) {
            s += ' ';
        }
        s += std::to_string(v[i]);
    }
    return s;
}

std::string join_ints(const std::vector<int> &v)
{
    std::string s;
    for (std::size_t i = 0; i < v.size(); ++i) {
        if (i != 0) {
            s += ' ';
        }
        s += std::to_string(v[i]);
    }
    return s;
}

} /* namespace rnd */
