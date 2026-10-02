/* strmatch.cpp —— 练习模板 10 的实现（C++）
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
 * 本模板的 4 处 TODO 全在这个文件里，按阶段编号：
 *
 *     阶段 2-1   build_failure   失配表的递推
 *     阶段 3-1   kmp_search      失配时往前跳
 *     阶段 4-1   rabin_karp      窗口右移一格时哈希怎么更新
 *     阶段 4-2   rabin_karp      命中之后怎么确认、怎么去重
 *
 * 每个 TODO 上面写明「要做什么」，下面的「判据」一行给出填完之后
 * 应当看到的数——那些数都由 src/main_cli.cpp 打印，逐个对得上才算做完。
 */
#include "strmatch.hpp"

namespace sm {

/* ==================================================================
 * 已给出的工具
 * ================================================================== */

bool char_eq(const std::string &text, std::size_t i,
             const std::string &pat, std::size_t j, MatchStats &st)
{
    ++st.comparisons;
    return text[i] == pat[j];
}

std::string join_numbers(const std::vector<std::size_t> &v)
{
    if (v.empty()) {
        return "(none)";
    }

    std::string out;
    for (std::size_t i = 0; i < v.size(); ++i) {
        if (i != 0) {
            out += ' ';
        }
        out += std::to_string(v[i]);
    }
    return out;
}

bool same_positions(const std::vector<std::size_t> &x, const std::vector<std::size_t> &y)
{
    if (x.size() != y.size()) {
        return false;
    }
    for (std::size_t i = 0; i < x.size(); ++i) {
        if (x[i] != y[i]) {
            return false;
        }
    }
    return true;
}

long long count_duplicates(const std::vector<std::size_t> &v)
{
    long long n = 0;
    for (std::size_t i = 1; i < v.size(); ++i) {
        if (v[i] == v[i - 1]) {
            ++n;
        }
    }
    return n;
}

/* ==================================================================
 * 阶段 1：朴素匹配（已给出）
 * ================================================================== */

std::vector<std::size_t> naive_search(const std::string &text, const std::string &pat,
                                      MatchStats &st)
{
    std::vector<std::size_t> out;
    const std::size_t n = text.size();
    const std::size_t m = pat.size();
    if (m == 0 || m > n) {
        return out;
    }

    /* 每一个可能的起点都从头比一遍：比到对不上就换下一个起点。
     * 对不上的那一次比较同样记进计数——它也是一次真实的比较。 */
    for (std::size_t i = 0; i + m <= n; ++i) {
        std::size_t k = 0;
        while (k < m) {
            if (!char_eq(text, i + k, pat, k, st)) {
                break;
            }
            ++k;
        }
        if (k == m) {
            out.push_back(i);
        }
    }
    return out;
}

/* ==================================================================
 * 阶段 2：失配表的递推
 * ================================================================== */

std::vector<std::size_t> build_failure(const std::string &pat)
{
    const std::size_t m = pat.size();

    /* 已给出：表比模式串长一格（下标 0 到 m），先整张置 0 */
    std::vector<std::size_t> fail(m + 1, 0);

    /* 已给出：前两格的初值。空串与单个字符都没有真前后缀，答案都是 0。 */
    fail[0] = 0;
    if (m >= 1) {
        fail[1] = 0;
    }

    /* 已给出：从第三格开始，一格一格往后填 */
    for (std::size_t i = 2; i <= m; ++i) {
        /* TODO（阶段 2-1，失配表的递推）：
         * 现在要填 fail[i]。问的是「模式串的前 i 个字符」这一段里，最长的
         * 「既是它的真前缀、又是它的真后缀」有多长。新进来的那个字符是这一段
         * 的最后一个字符，它给这件事提供了唯一的增量。
         *
         * 前一段（前 i-1 个字符）的答案就在表里，紧挨着这一格。拿新字符去与
         * 那个答案所指的位置比：对得上，答案就是那个数加一。
         * 对不上时不能就此打住，也不能凭空去试别的长度——把已经对上的那一段
         * 再缩短，缩短之后是多少，同一个表里前面已经算好了。照着这个办法一直
         * 缩到能接上，或者缩到没有前缀可用（0）为止。
         *
         * 判据（见《配置步骤.md》阶段 2）：整张表逐格打印出来，三个模式串依次是
         *       aaaaaaaaab  →  0 0 1 2 3 4 5 6 7 8 0
         *       aabaaab     →  0 0 1 0 1 2 2 3
         *       abc         →  0 0 0 0
         *       中间那一个的表里有跨好几格的回退，阶段 3 还要用它。 */

        /* 占位实现：这一格不填，留着 0——表能返回、程序能跑，但整张表都是 0，
         * 阶段 3 的跳转也就无从谈起。 */
    }

    return fail;
}

/* ==================================================================
 * 阶段 3：KMP，失配时往前跳
 * ================================================================== */

std::vector<std::size_t> kmp_search(const std::string &text, const std::string &pat,
                                    const std::vector<std::size_t> &fail, MatchStats &st)
{
    std::vector<std::size_t> out;
    const std::size_t n = text.size();
    const std::size_t m = pat.size();
    if (m == 0 || m > n) {
        return out;
    }

    std::size_t j = 0;      /* 已给出：到 text[i-1] 为止已经对上的长度 */

    for (std::size_t i = 0; i < n; ++i) {   /* 已给出：主串从左到右走一遍 */
        /* TODO（阶段 3-1，失配跳转）：
         * 走到这里时 j 是「到 text[i-1] 为止已经对上的长度」，接下来要拿 text[i]
         * 与模式串的第 j 个字符比。可是这个位置未必对得上，而两种图省事的做法都不对：
         *
         *   - 把 j 清零：前面辛苦攒下的匹配全扔了，主串的那个字符要重新去过一遍
         *     模式串的开头，KMP 也就不比朴素快多少；
         *   - 让 j 一次只往回退一格：退得不够。模式串开头那一小段很可能与
         *     「已经对上的那一段」的尾巴重合，重合的长度不止一格。
         *
         * j 该退到哪一格，阶段 2 那张表正是为这件事算的（表的含义见
         * 《strmatch.hpp》里 build_failure 上方的注释）。退完再比一次，
         * 还是对不上就接着退，直到对上或者退无可退为止——退到 0 就该停，
         * 再退没有意义。
         *
         * 每一次比较都必须走已经给出的 char_eq()，它会把比较记进计数；
         * 自己写 == 的话，比较次数那一列的判据就对不上。
         *
         * 判据（见《配置步骤.md》阶段 3）：位置与朴素版逐位相同，而比较次数
         *       少得多。模式串 aaaaaaaaab 的两列是 21 69 / 比较 133 对 502；
         *       模式串 aabaaab 的两列是 31 59 / 比较 138 对 220。
         *       后一个模式串是特意挑的：它的表里有跨好几格的回退，
         *       「一次只退一格」那种写法在它上面会多报出一个位置。 */

        /* 占位实现：只拿 text[i] 与 pat[j] 比一次，对不上也不往回退。
         * 能编译、能跑，但结果里会混进假的匹配位置（阶段 3 的判据就是冲着它来的）。 */
        bool hit = char_eq(text, i, pat, j, st);

        if (hit) {      /* 已给出：对上了就把匹配长度加一，加到 m 就是一次命中 */
            ++j;
            if (j == m) {
                out.push_back(i + 1 - m);   /* 已给出：结果的收集 */
                j = fail[m];                /* 已给出：命中之后从表里接着往下走 */
            }
        }
    }
    return out;
}

/* ==================================================================
 * 阶段 4：滚动哈希
 * ================================================================== */

std::vector<std::size_t> rabin_karp(const std::string &text, const std::string &pat,
                                    MatchStats &st)
{
    std::vector<std::size_t> out;
    const std::size_t n = text.size();
    const std::size_t m = pat.size();
    if (m == 0 || m > n) {
        return out;
    }

    /* 已给出：进制的 0 到 m-1 次幂（都取过模）。power[k] 是窗口里第 k 个字符
     * 在哈希里的权，窗口右移时用它把走出窗口的那个字符减掉。 */
    std::vector<long long> power(m, 1);
    for (std::size_t k = 1; k < m; ++k) {
        power[k] = power[k - 1] * kHashBase % kHashMod;
    }

    /* 已给出：模式串的哈希与第一个窗口的哈希。两者用的是同一条递推：
     * 每读进一个字符，先把当前的哈希乘上进制、再加上这个字符，最后取模。 */
    long long pat_hash = 0;
    for (std::size_t k = 0; k < m; ++k) {
        pat_hash = (pat_hash * kHashBase + static_cast<unsigned char>(pat[k])) % kHashMod;
    }

    long long win_hash = 0;
    for (std::size_t k = 0; k < m; ++k) {
        win_hash = (win_hash * kHashBase + static_cast<unsigned char>(text[k])) % kHashMod;
    }

    for (std::size_t i = 0; i + m <= n; ++i) {
        /* 已给出：进入这一轮时 win_hash 还是上一个窗口的哈希，先把它右移到窗口 i。
         * 第一轮（i 为 0）不用移，首窗口的哈希上面已经算好。 */
        if (i > 0) {
            /* TODO（阶段 4-1，窗口右移一格）：
             * 窗口从 i-1 挪到 i，前后只差两个字符：左边走掉一个、右边进来一个。
             * 因此不必把窗口里的 m 个字符重新乘一遍，把走掉的那一个字符在哈希里
             * 的贡献减掉，剩下的整体「升一位」（乘一次进制），再加上新进来的那个
             * 字符，就是新窗口的哈希。它自己在哈希里的权，power 表里写着。
             *
             * 两处都要取模：减出负数时先补一个模数再取，不然会得到一个负数，
             * 后面越算越远。这一步算完，win_hash 必须落在 0 到 kHashMod 减一之间。
             *
             * 判据（见《配置步骤.md》阶段 4）：哈希更新的次数是「窗口个数减一」，
             *       主数据集上 69 次、演示数据集上 14 次；位置那一列与 KMP 相同。 */

            /* 占位实现：这里什么都不做，win_hash 一直停在首窗口的哈希上。 */
            ++st.hash_updates;      /* 已给出：右移一次记一次 */
        }

        /* TODO（阶段 4-2，命中之后怎么确认、怎么去重）：
         * 这一轮的窗口哈希与模式串的哈希相等，只说明「有可能」在这里。
         * 哈希把任意长的字符串压成 0 到 100 之间的一个数，不同的字符串压到
         * 同一个数是常有的事——阶段 4 的演示数据集里就有两个这样的窗口。
         * 因此收下这个位置之前还要逐字符确认一次：确认通过才把位置收进结果，
         * 确认不过就把「哈希相等而字符串不等」的次数加一，那个数在演示数据集上
         * 必须非零（主数据集上模数虽小，70 个窗口里恰好没有碰到假命中，因此不能
         * 拿「假命中是 0」当判据）。
         *
         * 两个计数在《strmatch.hpp》的 MatchStats 里：命中的次数与
         * 命中之后确认不过的次数，都要记。
         * 同一个位置只收一次——不要在「哈希相等」和「逐字符确认通过」两处
         * 各收一遍，那样位置列表里会出现重复。
         *
         * 判据（见《配置步骤.md》阶段 4）：位置与 KMP 逐位相同、位置列表里没有
         *       重复，主数据集上哈希命中 2 次、其中假命中 0 次；演示数据集
         *       （模式串 abc，主串里放着 bbl）上命中 3 次、其中假命中 2 次。 */

        /* 占位实现：这里什么都不做，结果列表一直是空的。 */
    }

    return out;
}

} /* namespace sm */
