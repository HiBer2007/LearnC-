/**
 * string_match_lab.hpp —— 字符串匹配：朴素、KMP 与 Rabin–Karp，以及按字节还是按码点
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
 * 报告里出现的每个数字都由这里的计数器产出，重跑逐位相同：
 *
 *   comparisons        逐元素比较的次数（朴素与 KMP 的匹配阶段）
 *   build_comparisons  构建失配表时的比较次数
 *   failure            失配表（前缀函数）的取值，逐个下标列出
 *   fresh_hashes       从头算一次哈希的次数（模式 1 次 + 首个窗口 1 次）
 *   roll_steps         滚动更新的次数，每次都是 O(1)
 *   hash_hits          哈希与模式相等的窗口数
 *   verifications      命中之后逐字符复核的次数
 *   collisions         复核失败的次数：哈希相等而内容不等
 *   verify_comparisons 复核里逐元素比较的次数
 *
 * 三条口径固定，报告与自测读的是同一份数：
 *
 *   朴素匹配  对齐位置 i 从 0 到 n − m，逐个元素比较，遇到不等就换下一个位置。
 *             比较次数的上界是 (n − m + 1) × m；教科书里的 n × m 是更松的上界，
 *             多算了最后 m − 1 个「不足一个模式长」的对齐位置。
 *   KMP       先按模式算失配表（只跟模式有关），匹配阶段文本指针只往前走。
 *             合计比较次数的上界是 2(n + m)。
 *   Rabin–Karp 哈希相等只说明「可能是匹配」，必须逐字符复核才敢报位置。
 *             模数取小、或者进制与模数有公因子时，命中次数会大幅上升，
 *             复核把这些假命中一条条挡回去，答案依然正确，代价落在复核次数上。
 *
 * 中文那一段用同一套引擎跑两遍：一遍喂 std::string（元素是字节），
 * 一遍喂解码后的 char32_t 序列（元素是码点）。两次的位置单位不同，
 * 字节偏移可能落在字符中间，见 Utf8Text::is_boundary 与报告第四段。
 */
#ifndef STRING_MATCH_LAB_HPP
#define STRING_MATCH_LAB_HPP

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace smlab {

/* ================= 匹配结果 ================= */

/** 一次匹配的全部产出：位置（从 0 数起）与比较次数 */
struct SearchResult {
    std::vector<std::size_t> positions;
    std::size_t comparisons = 0;

    bool same_positions_as(const SearchResult &other) const;
};

/* ================= 朴素匹配 ================= */

/** 文本与模式都是字节序列；模式为空、或比文本长时没有位置，比较次数为 0 */
SearchResult naive_search(const std::string &text, const std::string &pattern);

/* ================= KMP ================= */

/** 失配表（前缀函数）：failure[i] 是模式前 i + 1 个字符里最长的
    「既是前缀又是后缀」的真前缀长度。comparisons 收比较次数，可传 nullptr。 */
std::vector<std::size_t> prefix_function(const std::string &pattern,
                                         std::size_t *comparisons);

/** 独立的对照实现：对每个下标枚举所有真前缀长度，逐个字符核对。
    复杂度是 O(m³)，只用来给自测当参照，不进报告。 */
std::vector<std::size_t> prefix_function_brute(const std::string &pattern);

struct KmpRun {
    SearchResult match;                  /**< 位置与匹配阶段的比较次数 */
    std::vector<std::size_t> failure;    /**< 失配表取值 */
    std::size_t build_comparisons = 0;   /**< 构建失配表的比较次数 */

    std::size_t total_comparisons() const
    {
        return build_comparisons + match.comparisons;
    }
};

KmpRun kmp_search(const std::string &text, const std::string &pattern);

/* ================= Rabin–Karp ================= */

/** 一次滚动哈希匹配的全部计数。哈希定义：
        H(w) = (w[0]·b^(m−1) + w[1]·b^(m−2) + … + w[m−1]) mod q
    滚动更新：
        H(下一窗) = ((H(当前) − w[首]·b^(m−1)) · b + w[新]) mod q
    b = base mod q。b 与 q 有公因子（最极端是 base == q，此时 b = 0）时，
    哈希只反映窗口末尾的少数几个字符，命中次数会明显上升。 */
struct HashRun {
    struct Collision {
        std::size_t window_start = 0;   /**< 碰撞窗口的起始字节偏移 */
        std::size_t first_diff = 0;     /**< 复核在第几个元素上停住 */
    };

    std::size_t modulus = 0;
    std::size_t base = 0;
    std::size_t window_count = 0;        /**< 滑动窗口个数 = n − m + 1 */
    std::size_t fresh_hashes = 0;        /**< 从头算哈希的次数 */
    std::size_t roll_steps = 0;          /**< 滚动更新次数 */
    std::size_t hash_hits = 0;           /**< 哈希与模式相等的窗口数 */
    std::size_t verifications = 0;       /**< 逐字符复核次数 */
    std::size_t collisions = 0;          /**< 复核失败次数 */
    std::size_t verify_comparisons = 0;  /**< 复核里的逐元素比较次数 */
    std::size_t pattern_hash = 0;
    std::vector<std::size_t> positions;
    std::vector<std::size_t> window_hashes;    /**< 每个窗口的哈希值 */
    std::vector<Collision> collision_list;

    bool positions_same_as(const std::vector<std::size_t> &other) const;
};

/** 逐窗重算（不滚动）时，每个窗口要从头乘 m 次：窗口数 × m。给报告当对照列 */
std::size_t rabin_karp_recompute_cost(const HashRun &run, std::size_t pattern_length);

HashRun rabin_karp(const std::string &text, const std::string &pattern,
                   std::size_t modulus, std::size_t base);

/* ================= UTF-8：字节与码点两种口径 ================= */

/** 一段 UTF-8 文本的两种读法：字节序列与解码后的码点序列。
    byte_offset[i] 是第 i 个码点的起始字节，末尾补一个 bytes.size() 当哨兵。 */
struct Utf8Text {
    std::string bytes;
    std::vector<char32_t> codepoints;
    std::vector<std::size_t> byte_offset;
    bool valid = true;                /**< 整段是否都是合法 UTF-8 */
    std::size_t bad_offset = 0;       /**< 第一个不合法的字节偏移 */

    /** 某个字节偏移落在第几个码点里（不要求它是字符边界） */
    std::size_t codepoint_index_of(std::size_t byte_off) const;
    /** 某个字节偏移是不是字符边界 */
    bool is_boundary(std::size_t byte_off) const;
    /** 按 UTF-8 编码长度统计码点个数，下标 1 到 4，下标 0 空着 */
    std::vector<std::size_t> count_by_length() const;
};

Utf8Text decode_utf8(const std::string &bytes);
std::string encode_utf8(const std::vector<char32_t> &codepoints);

/** 同一套朴素与 KMP 引擎，元素换成码点 */
SearchResult naive_search_codepoints(const std::vector<char32_t> &text,
                                     const std::vector<char32_t> &pattern);
KmpRun kmp_search_codepoints(const std::vector<char32_t> &text,
                             const std::vector<char32_t> &pattern);

/* ================= 报告与自测 ================= */

/** 自测结果。不打印，交给界面决定怎么显示 */
struct CheckResult {
    std::size_t total = 0;
    std::size_t passed = 0;
    std::size_t failed = 0;
    std::vector<std::string> lines;   /**< 每项一行，例如 "[通过] 3. ……" */

    bool all_passed() const { return failed == 0; }
    std::string summary() const;      /**< 形如 "N 项中 M 项通过，全部通过" */
};

/** 项目输出：朴素、KMP、Rabin–Karp、中文两种口径四段。
    返回多行 UTF-8 文本，末尾带一个换行；由界面层决定怎么显示 */
std::string build_report();

/** 逐项核对三种算法的位置、结构量、哈希守恒式与 UTF-8 两种口径 */
CheckResult run_self_tests();

}   /* namespace smlab */

#endif /* STRING_MATCH_LAB_HPP */
