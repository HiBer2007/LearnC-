/**
 * noncompare_lab.hpp —— 计数排序、基数排序（LSD）与桶排序
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
 * 三种排序都不靠元素之间的比较来定次序，靠的是把键换算成一个下标。
 * 报告里的每个数字都由这里的计数器产出，重跑逐位相同：
 *
 *   reads            从待排数据或缓冲区里取出一个元素的次数
 *   writes           把一个元素写进缓冲区的次数
 *   bucket_compares  桶内为定次序做的元素间比较次数
 *   aux_arrays       用到的辅助数组个数
 *   aux_slots        辅助数组的元素槽总数
 *   aux_bytes        辅助空间字节数
 *   allocations      动态分配次数，每个非空辅助数组记一次
 *   passes           基数排序的趟数，其余排序恒为 1
 *   buckets          桶排序的桶数，其余排序恒为 0
 *   max_bucket_size  桶排序里最满的一个桶装了多少个元素
 *
 * 计数口径的两条约定：
 *   一、只数元素的读写，不数计数器（频次数组、前缀和、游标）的读写；
 *   二、辅助空间按「槽数 × 元素宽度」算，不含 std::vector 自身的对象头。
 *
 * 计数排序的稳定性由落点方式决定：
 *   稳定版    前缀和给每段的起点，取值与落点都从前往后走
 *   不稳定版  前缀和给每段的终点，取值从前往后、落点却从后往前
 * 两者都排出有序的结果，只有前者保住同键元素之间的原有次序。
 */
#ifndef NONCOMPARE_LAB_HPP
#define NONCOMPARE_LAB_HPP

#include <cstddef>
#include <string>
#include <vector>

namespace nclab {

/** 整数键。三种排序都要求键能换算成一个下标，因此这里不从模板参数出发 */
using Key = unsigned int;

/* ================= 结构量 ================= */

/** 一次排序的账。除 passes、buckets、max_bucket_size 外都按累加记 */
struct SortStats {
    std::size_t reads = 0;
    std::size_t writes = 0;
    std::size_t bucket_compares = 0;
    std::size_t aux_arrays = 0;
    std::size_t aux_slots = 0;
    std::size_t aux_bytes = 0;
    std::size_t allocations = 0;
    std::size_t passes = 0;
    std::size_t buckets = 0;
    std::size_t max_bucket_size = 0;
};

/** 稳定性。计数排序的两版由它切换，基数排序与桶排序固定用稳定版 */
enum class Stability {
    Stable,
    Unstable
};

/* ================= 计数排序 ================= */

/** 计数排序：整数键，键域 [key_min, key_max] 必须已知。
 *  key_max < key_min 时返回空数组。stats 为累加，传 nullptr 表示不记账。
 *  前提：每个键都必须落在 [key_min, key_max] 之内；越界的键会算出越界的下标。 */
std::vector<Key> counting_sort(const std::vector<Key> &input, Key key_min, Key key_max,
                               Stability stability, SortStats *stats);

/* ================= 记录与多关键字排序 ================= */

/** 一条记录：主键、次键，外加原始位置，便于在结果里认出是哪一条 */
struct Record {
    Key primary = 0;
    Key secondary = 0;
    int id = 0;
};

inline bool operator==(const Record &lhs, const Record &rhs)
{
    return lhs.primary == rhs.primary && lhs.secondary == rhs.secondary && lhs.id == rhs.id;
}

inline bool operator!=(const Record &lhs, const Record &rhs)
{
    return !(lhs == rhs);
}

/** 按哪一列排 */
enum class KeyField {
    Primary,
    Secondary
};

/** 按主键或次键做一趟计数排序，键域是 [0, key_max] */
std::vector<Record> counting_sort(const std::vector<Record> &input, KeyField field,
                                  Key key_min, Key key_max, Stability stability,
                                  SortStats *stats);

/** LSD 思路的两趟排序：先按次键排一趟，再按主键排一趟。
 *  两趟都稳定，得到的结果才是「主键优先、次键次之」。
 *  两趟都不稳定，结果仍然按主键有序，同主键的那些记录却乱了。 */
std::vector<Record> sort_two_keys_lsd(const std::vector<Record> &input, Key primary_max,
                                      Key secondary_max, Stability stability,
                                      SortStats *stats);

/** 对照做法：把（主键，次键）打包成一个键（主键 × (secondary_max + 1) + 次键），
 *  用一趟稳定计数排序排完。键的位数够少时，这与 sort_two_keys_lsd 的结果逐位相同。 */
std::vector<Record> sort_two_keys_packed(const std::vector<Record> &input, Key primary_max,
                                         Key secondary_max, SortStats *stats);

/** 一批记录是否满足「主键优先、次键次之」 */
bool ordered_primary_then_secondary(const std::vector<Record> &records);

/* ================= 基数排序（LSD） ================= */

/** 基数排序，低位优先，256 进制。趟数由 key_max 的位数定。
 *  每一趟内部是稳定计数排序，这是 LSD 能成立的前提。 */
std::vector<Key> radix_sort_lsd(const std::vector<Key> &input, Key key_max, SortStats *stats);

/** 256 进制下排完 [0, key_max] 需要几趟 */
std::size_t radix_passes(Key key_max);

/* ================= 桶排序 ================= */

/** 桶排序：键域 [domain_min, domain_max] 均分成 bucket_count 个桶，
 *  元素按桶装进一块连续缓冲区，桶内用插入排序收尾。
 *  bucket_count 为 0 或 domain_max < domain_min 时返回空数组。
 *  前提：每个键都必须落在 [domain_min, domain_max] 之内，越界的键会算出越界的桶号。 */
std::vector<Key> bucket_sort(const std::vector<Key> &input, std::size_t bucket_count,
                             Key domain_min, Key domain_max, SortStats *stats);

/* ================= 报告与自测 ================= */

/** 自测结果。不打印，交给界面决定怎么显示 */
struct CheckResult {
    std::size_t total = 0;
    std::size_t passed = 0;
    std::size_t failed = 0;
    std::vector<std::string> lines;   /**< 每项一行，例如 "[通过] 3. ……" */

    bool all_passed() const { return failed == 0; }
    std::string summary() const;      /**< "N 项中 N 项通过，全部通过" */
};

/** 项目输出：结构量对照、前提与退化、稳定性与多关键字、三者对照四段。
 *  返回多行 UTF-8 文本，末尾带一个换行；由界面层决定怎么显示 */
std::string build_report();

/** 逐项核对三种排序的结果、结构量公式、退化情形与稳定性 */
CheckResult run_self_tests();

}   /* namespace nclab */

#endif /* NONCOMPARE_LAB_HPP */
