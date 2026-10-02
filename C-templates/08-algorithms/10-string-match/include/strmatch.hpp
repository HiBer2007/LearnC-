/* strmatch.hpp —— 练习模板 10 的核心接口（C++）
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
 * 字符串匹配这一件事拆成 4 个阶段，每个阶段的实现写在 src/strmatch.cpp 里：
 *
 *     阶段 1  naive_search     朴素匹配（已给出），比较次数的基线
 *     阶段 2  build_failure    失配表的递推
 *     阶段 3  kmp_search       用失配表跳：位置与朴素版逐位相同、比较次数更少
 *     阶段 4  rabin_karp       滚动哈希：窗口右移时哈希怎么变、命中之后怎么确认
 *
 * 本模板只碰「单串匹配」这一件事：主串与模式串都是写死在 src/main_cli.cpp 里的
 * ASCII 字符串，不涉及正则表达式，也不涉及字符编码的转换。
 */
#ifndef STRMATCH_HPP
#define STRMATCH_HPP

#include <cstddef>
#include <string>
#include <vector>

namespace sm {

/* ------------------------------------------------------------------
 * 已给出：一次搜索里数出来的结构量。
 *
 * comparisons 这一栏的尺子全模板统一：char_eq() 每被调用一次记一次，
 * 一次调用对应一次真正的字符比较（对不上的那一次也算）。阶段 1 的朴素匹配
 * 与阶段 3 的 KMP 都走这把尺子，两个数才能并排比。
 *
 * 后三栏只有阶段 4 用：哈希相等只是「有可能」，并不等于字符串相等。
 * ------------------------------------------------------------------ */
struct MatchStats {
    long long comparisons = 0;   /* 逐字符比较的次数（阶段 1 与阶段 3） */
    long long hash_updates = 0;  /* 窗口右移一格的次数（阶段 4） */
    long long hash_hits = 0;     /* 窗口哈希与模式串哈希相等的次数（阶段 4） */
    long long hash_false = 0;    /* 哈希相等、逐字符确认却不相等的次数（阶段 4） */
};

/* 已给出：拿主串第 i 个字符与模式串第 j 个字符比一次，并把这一次记进
 * st.comparisons。阶段 3 里的每一次比较都要走这个函数，否则比较次数
 * 那一列的判据对不上（见《配置步骤.md》阶段 3）。 */
bool char_eq(const std::string &text, std::size_t i,
             const std::string &pat, std::size_t j, MatchStats &st);

/* ---------- 阶段 1：朴素匹配（已给出） ---------- */

/* 主串里每一个可能的起点都比一遍，返回所有匹配的起点（升序）。
 * 比较次数记进 st.comparisons，它是后面两个阶段的对照基线。 */
std::vector<std::size_t> naive_search(const std::string &text, const std::string &pat,
                                      MatchStats &st);

/* ---------- 阶段 2：失配表的递推 ---------- */

/* 失配表。表比模式串长一格（下标 0 到 m），fail[i] 的含义是：
 * 「模式串的前 i 个字符」这一段里，最长的「既是它的真前缀、又是它的真后缀」
 * 的长度。它在阶段 3 里回答同一个问题：已经对上了 j 个字符、下一个字符却对不上时，
 * 还能保住几个字符的匹配。
 * 判据：整张表逐格打印出来（见《配置步骤.md》阶段 2）。 */
std::vector<std::size_t> build_failure(const std::string &pat);

/* ---------- 阶段 3：KMP，失配时往前跳 ---------- */

/* 用阶段 2 的表决定失配之后从哪里接着比，返回所有匹配的起点（升序）。
 * 判据：与 naive_search 的位置逐位相同，而比较次数明显更少。 */
std::vector<std::size_t> kmp_search(const std::string &text, const std::string &pat,
                                    const std::vector<std::size_t> &fail, MatchStats &st);

/* ---------- 阶段 4：滚动哈希 ---------- */

/* 已给出：滚动哈希用的进制与模数。
 * 模数取得不大是有意的：哈希把任意长的字符串压成 0 到 100 之间的一个数，
 * 不同的字符串压到同一个数是常有的事，因此命中之后必须逐字符确认。
 * 这两个数不要改，改了阶段 4 的判据就对不上。 */
constexpr long long kHashBase = 131;
constexpr long long kHashMod = 101;

/* 把主串切成一个个等长的窗口，先用哈希筛一遍，命中了的再逐字符确认，
 * 返回所有匹配的起点（升序）。
 * 判据：与 KMP 的位置逐位相同、哈希更新的次数、以及
 *       「哈希相等而字符串不等」的次数（见《配置步骤.md》阶段 4）。 */
std::vector<std::size_t> rabin_karp(const std::string &text, const std::string &pat,
                                    MatchStats &st);

/* ---------- 已给出的工具 ---------- */

/* 把一串数拼成一行文本（空格分隔），空序列写成 "(none)"。
 * 位置序列与失配表都用它打印。 */
std::string join_numbers(const std::vector<std::size_t> &v);

/* 两个位置序列是否逐项相同（先比长度，再逐项比） */
bool same_positions(const std::vector<std::size_t> &x, const std::vector<std::size_t> &y);

/* 位置序列里重复出现的位置有几个（序列是升序的，比相邻两项即可） */
long long count_duplicates(const std::vector<std::size_t> &v);

} /* namespace sm */

#endif /* STRMATCH_HPP */
