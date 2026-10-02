/* sampling.hpp —— 练习模板 12 的核心接口（C++）
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
 * 随机与采样这一件事拆成 4 个阶段，每个阶段的实现写在 src/sampling.cpp 里：
 *
 *     阶段 1  uniform_int       在一个闭区间上取均匀整数
 *     阶段 2  shuffle           Fisher-Yates 的交换
 *     阶段 3  reservoir_sample  蓄水池抽样的替换判据
 *     阶段 4  （已给出，不留空）固定种子与可复现
 *
 * 本模板所有的随机数都由 std::mt19937 产生，种子写死在 kSeed 里，
 * 因此同一个可执行文件重跑多少次，输出都逐位相同——这是本模板的判据
 * 能成立的前提，也是这一章的要点之一。唯一一处例外是阶段 4 的
 * 「不设种子」对照，它专门用来说明没有固定种子时会怎样，
 * 那一组数字不进验收标准。
 */
#ifndef SAMPLING_HPP
#define SAMPLING_HPP

#include <cstddef>
#include <cstdint>
#include <random>
#include <string>
#include <vector>

namespace rnd {

/* ==================================================================
 * 已给出：引擎
 * ================================================================== */

/* 本模板唯一的种子。改这个数，全部判据数字都会变，因此不要改。 */
inline constexpr std::uint32_t kSeed = 20260812u;

/* 按 kSeed 造一台引擎。两次调用得到的两台引擎，输出逐位相同。 */
std::mt19937 make_engine();

/* 不给种子：整台程序共用一个 std::random_device，两次调用取到的是它
 * 序列里相邻的两个数，因此两台引擎的种子不同、输出也不同。
 * 阶段 4 用它做对照——这一路每次运行都会变，不能进验收标准。 */
std::mt19937 make_unseeded_engine();

/* ==================================================================
 * 阶段 1：均匀整数区间
 * ================================================================== */

/* 在闭区间 [lo, hi] 上取一个均匀分布的整数。
 * TODO（阶段 1-1）在这个函数上，见 src/sampling.cpp。
 * 判据：《配置步骤.md》阶段 1 的两张直方图。 */
int uniform_int(std::mt19937 &engine, int lo, int hi);

/* 已给出：错误对照。C 的 rand() % n，返回值落在 [0, n)。
 * 学生不必写它，名字沿用它的典型用法（取 [0, n) 里的一个整数）。
 * 本模板不调用 srand，因此它也是可复现的：不设种子的 rand() 等价于 srand(1)。 */
int rand_percent(int n);

/* 已给出：一份直方图 */
struct Histogram {
    std::vector<long long> counts;  /* 每个桶里的次数 */
    long long out_of_range = 0;     /* 取到的值落在 [0, hi_exclusive) 之外的次数 */
};

/* 已给出：把 [0, hi_exclusive) 等宽分成 buckets 个桶，取 draws 次数，
 * 数出每个桶落了多少次。内部调用 uniform_int。 */
Histogram histogram_uniform(std::mt19937 &engine, int hi_exclusive,
                            int buckets, long long draws);

/* 已给出：同一批桶数下的 rand_percent 直方图。内部调用 rand_percent。 */
Histogram histogram_rand(int n, int buckets, long long draws);

/* 已给出：一列计数与期望值之间的最大绝对偏差。
 * 随机波动落在几百这个量级，系统性偏差会顶到几万，两者一眼可分。 */
long long max_abs_deviation(const std::vector<long long> &counts, long long expected);

/* ==================================================================
 * 阶段 2：Fisher-Yates 的交换
 * ================================================================== */

/* 已给出：造一副牌，内容依次是 0, 1, ..., n-1 */
std::vector<int> make_deck(int n);

/* 已给出：错误的对照版本。每一个位置都从整个数组里取一个位置交换，
 * 对手位置与当前位置没有关系。
 * TODO（阶段 2-1）在下面的 shuffle 上，不在这个函数上。 */
std::vector<int> shuffle_wrong(std::vector<int> a, std::mt19937 &engine);

/* 把 a 洗匀。循环骨架已经给出，留空的是每一轮的交换。
 * 判据：《配置步骤.md》阶段 2 的位置矩阵。 */
void shuffle(std::vector<int> &a, std::mt19937 &engine);

/* 已给出：一张「元素落在哪里」的计数表 */
struct PositionMatrix {
    std::vector<std::vector<long long>> cells;  /* cells[元素][位置] */
    long long bad_values = 0;                   /* 洗完之后出现的越界元素次数 */
};

/* 已给出：洗 rounds 次，数出每个元素落在每个位置上的次数。
 * 行下标是元素的值，列下标是位置。两次调用各用一台 kSeed 引擎，
 * 全部轮次共用同一台引擎——每轮都新建引擎的话，每轮的随机序列会
 * 一模一样，统计就没有意义了。 */
PositionMatrix count_positions_correct(int n, long long rounds);
PositionMatrix count_positions_wrong(int n, long long rounds);

/* 已给出：把矩阵按行展平成一列计数，便于交给 max_abs_deviation */
std::vector<long long> flatten_counts(const std::vector<std::vector<long long>> &m);

/* 已给出：a 是不是 0..n-1 的一个排列（长度、取值、重复都查） */
bool is_permutation_of_deck(const std::vector<int> &a);

/* ==================================================================
 * 阶段 3：蓄水池抽样
 * ================================================================== */

/* 已给出：造一段流数据，内容依次是 0, 1, ..., n-1 */
std::vector<int> make_stream(int n);

/* 从流里等概率抽出 k 个元素，返回池子。
 * 池子的填充、遍历流数据的外层循环、结果收集都已经给出，
 * 留空的是替换判据。
 * 判据：《配置步骤.md》阶段 3 的下标序列与次数表。 */
std::vector<int> reservoir_sample(const std::vector<int> &stream, std::size_t k,
                                  std::mt19937 &engine);

/* 已给出：多轮抽样的统计结果 */
struct SampleStats {
    long long rounds = 0;         /* 一共抽了多少轮 */
    long long expected_size = 0;  /* 池子应有的大小（流不够长时是流的长度） */
    long long bad_size = 0;       /* 池子大小不等于 expected_size 的轮数 */
    long long duplicates = 0;     /* 池子里出现重复元素的轮数 */
    std::vector<long long> hits;  /* 每个元素被抽中的次数 */
};

/* 已给出：抽 rounds 轮，数出每个元素被抽中的次数，并统计池子大小。
 * 全部轮次共用一台 kSeed 引擎，理由同 count_positions_correct。 */
SampleStats count_selected(int n, std::size_t k, long long rounds);

/* 已给出：把池子里的元素排好序，便于打印成「下标序列」 */
std::vector<int> sorted_copy(const std::vector<int> &v);

/* ==================================================================
 * 阶段 4：固定种子与可复现（已给出，不留空）
 * ================================================================== */

/* 已给出：直接从引擎取 count 个原始输出，不经过任何分布。
 * 这样阶段 4 就不依赖前三处 TODO，骨架状态下它已经是对的。 */
std::vector<std::uint32_t> raw_draws(std::mt19937 &engine, int count);

/* 已给出：两个序列是否逐位相同（长度与每一项都比较） */
bool same_u32(const std::vector<std::uint32_t> &a, const std::vector<std::uint32_t> &b);

/* 已给出：把序列拼成一行，用空格隔开 */
std::string join_u32(const std::vector<std::uint32_t> &v);
std::string join_ints(const std::vector<int> &v);

} /* namespace rnd */

#endif /* SAMPLING_HPP */
