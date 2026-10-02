/**
 * string_match_lab.cpp —— 字符串匹配：朴素、KMP 与 Rabin–Karp，以及按字节还是按码点
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

#include "string_match_lab.hpp"

#include <algorithm>
#include <iomanip>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace smlab {

/* ================= 匹配结果 ================= */

bool SearchResult::same_positions_as(const SearchResult &other) const
{
    return positions == other.positions;
}

/* ================= 朴素匹配与 KMP 的共用引擎 ================= */

namespace {

/** 朴素匹配：序列的元素类型由 Seq 决定，std::string 给的是字节，
    std::vector<char32_t> 给的是码点。两处用的是同一段代码。 */
template <typename Seq>
SearchResult naive_impl(const Seq &text, const Seq &pattern)
{
    SearchResult result;
    const std::size_t n = text.size();
    const std::size_t m = pattern.size();
    if (m == 0 || m > n) {
        return result;
    }
    for (std::size_t i = 0; i + m <= n; ++i) {
        std::size_t j = 0;
        while (j < m) {
            ++result.comparisons;
            if (text[i + j] != pattern[j]) {
                break;
            }
            ++j;
        }
        if (j == m) {
            result.positions.push_back(i);
        }
    }
    return result;
}

/** 失配表。每次比较都记一笔，包括回退之后再比的那几次。 */
template <typename Seq>
std::vector<std::size_t> prefix_impl(const Seq &pattern, std::size_t *comparisons)
{
    const std::size_t m = pattern.size();
    std::vector<std::size_t> failure(m, 0);
    std::size_t count = 0;
    std::size_t k = 0;                     /* 当前位置上已经匹配上的前缀长度 */
    for (std::size_t i = 1; i < m; ++i) {
        while (true) {
            ++count;
            if (pattern[i] == pattern[k]) {
                ++k;
                break;
            }
            if (k == 0) {
                break;                     /* 退到底还是不相等，π[i] 就是 0 */
            }
            k = failure[k - 1];            /* 回退到上一个可能的前缀 */
        }
        failure[i] = k;
    }
    if (comparisons != nullptr) {
        *comparisons = count;
    }
    return failure;
}

/** KMP：失配表只看模式，匹配阶段文本下标 i 只往前走，不回退。 */
template <typename Seq>
KmpRun kmp_impl(const Seq &text, const Seq &pattern)
{
    KmpRun run;
    const std::size_t m = pattern.size();
    if (m == 0) {
        return run;
    }
    run.failure = prefix_impl(pattern, &run.build_comparisons);

    const std::size_t n = text.size();
    std::size_t k = 0;                     /* 已经匹配上的模式前缀长度 */
    for (std::size_t i = 0; i < n; ++i) {
        while (true) {
            ++run.match.comparisons;
            if (text[i] == pattern[k]) {
                ++k;
                break;
            }
            if (k == 0) {
                break;                     /* 这个文本字符谁都对不上，换下一个 */
            }
            k = run.failure[k - 1];        /* 文本字符留着，模式往回退 */
        }
        if (k == m) {
            run.match.positions.push_back(i + 1 - m);
            k = run.failure[k - 1];        /* 允许重叠的下一处匹配 */
        }
    }
    return run;
}

}   /* namespace */

SearchResult naive_search(const std::string &text, const std::string &pattern)
{
    return naive_impl(text, pattern);
}

std::vector<std::size_t> prefix_function(const std::string &pattern, std::size_t *comparisons)
{
    return prefix_impl(pattern, comparisons);
}

KmpRun kmp_search(const std::string &text, const std::string &pattern)
{
    return kmp_impl(text, pattern);
}

std::vector<std::size_t> prefix_function_brute(const std::string &pattern)
{
    /* 独立实现：对每个下标枚举所有真前缀长度，逐个字符核对，取最长的那个。
       复杂度 O(m³)，只用来给自测当参照。 */
    const std::size_t m = pattern.size();
    std::vector<std::size_t> failure(m, 0);
    for (std::size_t i = 1; i < m; ++i) {
        std::size_t best = 0;
        for (std::size_t length = 1; length <= i; ++length) {
            bool same = true;
            for (std::size_t k = 0; k < length; ++k) {
                if (pattern[k] != pattern[i + 1 - length + k]) {
                    same = false;
                    break;
                }
            }
            if (same) {
                best = length;
            }
        }
        failure[i] = best;
    }
    return failure;
}

/* ================= Rabin–Karp ================= */

std::size_t rabin_karp_recompute_cost(const HashRun &run, std::size_t pattern_length)
{
    return run.window_count * pattern_length;
}

bool HashRun::positions_same_as(const std::vector<std::size_t> &other) const
{
    return positions == other;
}

HashRun rabin_karp(const std::string &text, const std::string &pattern,
                   std::size_t modulus, std::size_t base)
{
    HashRun run;
    run.modulus = modulus;
    run.base = base;
    const std::size_t n = text.size();
    const std::size_t m = pattern.size();
    if (modulus == 0 || m == 0 || m > n) {
        return run;
    }
    run.window_count = n - m + 1;

    const std::size_t b = base % modulus;

    /* 乘法先抬到 64 位再取模：模数与进制的平方会越过 32 位 size_t 的范围 */
    auto mulmod = [modulus](std::size_t lhs, std::size_t rhs) {
        return static_cast<std::size_t>(
            (static_cast<unsigned long long>(lhs) * static_cast<unsigned long long>(rhs)) %
            modulus);
    };
    auto fresh_hash = [&](const std::string &source, std::size_t start) {
        std::size_t h = 0;
        for (std::size_t k = 0; k < m; ++k) {
            h = (mulmod(h, b) +
                 static_cast<std::size_t>(static_cast<unsigned char>(source[start + k]))) %
                modulus;
        }
        return h;
    };

    std::size_t high = 1 % modulus;         /* b^(m−1) mod q，滚动时用来减掉首字符 */
    for (std::size_t k = 1; k < m; ++k) {
        high = mulmod(high, b);
    }

    run.pattern_hash = fresh_hash(pattern, 0);
    ++run.fresh_hashes;
    std::size_t h = fresh_hash(text, 0);
    ++run.fresh_hashes;

    for (std::size_t i = 0; i < run.window_count; ++i) {
        if (i != 0) {
            /* 滚到下一个窗口：先减掉上一个窗口的首字符，再乘进制补上末尾的新字符 */
            std::size_t next =
                (h + modulus -
                 mulmod(static_cast<std::size_t>(static_cast<unsigned char>(text[i - 1])),
                        high)) %
                modulus;
            next = (mulmod(next, b) +
                    static_cast<std::size_t>(static_cast<unsigned char>(text[i + m - 1]))) %
                   modulus;
            h = next;
            ++run.roll_steps;
        }
        run.window_hashes.push_back(h);

        if (h != run.pattern_hash) {
            continue;                       /* 哈希不等，这个窗口一定不是匹配 */
        }
        ++run.hash_hits;
        ++run.verifications;                /* 哈希相等只是「可能」，必须复核 */
        std::size_t j = 0;
        while (j < m) {
            ++run.verify_comparisons;
            if (text[i + j] != pattern[j]) {
                break;
            }
            ++j;
        }
        if (j == m) {
            run.positions.push_back(i);
        } else {
            ++run.collisions;
            run.collision_list.push_back(HashRun::Collision{i, j});
        }
    }
    return run;
}

/* ================= UTF-8：字节与码点两种口径 ================= */

namespace {

char to_byte(unsigned int value)
{
    return static_cast<char>(static_cast<unsigned char>(value));
}

std::size_t utf8_length_of(char32_t cp)
{
    const std::uint32_t v = static_cast<std::uint32_t>(cp);
    if (v < 0x80u) {
        return 1;
    }
    if (v < 0x800u) {
        return 2;
    }
    if (v < 0x10000u) {
        return 3;
    }
    return 4;
}

}   /* namespace */

std::size_t Utf8Text::codepoint_index_of(std::size_t byte_off) const
{
    if (byte_offset.empty()) {
        return 0;
    }
    const std::vector<std::size_t>::const_iterator it =
        std::upper_bound(byte_offset.begin(), byte_offset.end(), byte_off);
    const std::size_t index = static_cast<std::size_t>(it - byte_offset.begin());
    return index == 0 ? 0 : index - 1;
}

bool Utf8Text::is_boundary(std::size_t byte_off) const
{
    if (byte_off > bytes.size()) {
        return false;
    }
    if (byte_off == bytes.size()) {
        return true;                        /* 末尾算一个边界，方便定位子串 */
    }
    for (const std::size_t start : byte_offset) {
        if (start == byte_off) {
            return true;
        }
    }
    return false;
}

std::vector<std::size_t> Utf8Text::count_by_length() const
{
    std::vector<std::size_t> counts(5, 0);  /* 下标 1 到 4 有效，下标 0 空着 */
    for (const char32_t cp : codepoints) {
        const std::size_t length = utf8_length_of(cp);
        if (length <= 4) {
            counts[length]++;
        }
    }
    return counts;
}

Utf8Text decode_utf8(const std::string &bytes)
{
    Utf8Text out;
    out.bytes = bytes;
    std::size_t i = 0;
    while (i < bytes.size()) {
        const unsigned char lead = static_cast<unsigned char>(bytes[i]);
        std::size_t length = 0;
        char32_t cp = 0;
        if (lead < 0x80u) {
            length = 1;
            cp = static_cast<char32_t>(lead);
        } else if ((lead & 0xE0u) == 0xC0u) {
            length = 2;
            cp = static_cast<char32_t>(lead & 0x1Fu);
        } else if ((lead & 0xF0u) == 0xE0u) {
            length = 3;
            cp = static_cast<char32_t>(lead & 0x0Fu);
        } else if ((lead & 0xF8u) == 0xF0u) {
            length = 4;
            cp = static_cast<char32_t>(lead & 0x07u);
        } else {
            out.valid = false;              /* 续字节当首字节、或 5 字节以上的首字节 */
            out.bad_offset = i;
            break;
        }
        if (i + length > bytes.size()) {
            out.valid = false;              /* 序列被截断 */
            out.bad_offset = i;
            break;
        }
        bool ok = true;
        for (std::size_t k = 1; k < length; ++k) {
            const unsigned char cont = static_cast<unsigned char>(bytes[i + k]);
            if ((cont & 0xC0u) != 0x80u) {
                ok = false;
                out.bad_offset = i + k;     /* 坏在续字节上 */
                break;
            }
            cp = static_cast<char32_t>((static_cast<std::uint32_t>(cp) << 6) |
                                       (static_cast<std::uint32_t>(cont) & 0x3Fu));
        }
        if (!ok) {
            out.valid = false;
            break;
        }
        if (cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF)) {
            out.valid = false;              /* 超出码点范围或落在代理区 */
            out.bad_offset = i;
            break;
        }
        out.byte_offset.push_back(i);
        out.codepoints.push_back(cp);
        i += length;
    }
    out.byte_offset.push_back(bytes.size());   /* 哨兵 */
    return out;
}

std::string encode_utf8(const std::vector<char32_t> &codepoints)
{
    std::string out;
    for (const char32_t cp : codepoints) {
        const std::uint32_t v = static_cast<std::uint32_t>(cp);
        if (v < 0x80u) {
            out.push_back(to_byte(v));
        } else if (v < 0x800u) {
            out.push_back(to_byte(0xC0u | (v >> 6)));
            out.push_back(to_byte(0x80u | (v & 0x3Fu)));
        } else if (v < 0x10000u) {
            out.push_back(to_byte(0xE0u | (v >> 12)));
            out.push_back(to_byte(0x80u | ((v >> 6) & 0x3Fu)));
            out.push_back(to_byte(0x80u | (v & 0x3Fu)));
        } else {
            out.push_back(to_byte(0xF0u | (v >> 18)));
            out.push_back(to_byte(0x80u | ((v >> 12) & 0x3Fu)));
            out.push_back(to_byte(0x80u | ((v >> 6) & 0x3Fu)));
            out.push_back(to_byte(0x80u | (v & 0x3Fu)));
        }
    }
    return out;
}

SearchResult naive_search_codepoints(const std::vector<char32_t> &text,
                                     const std::vector<char32_t> &pattern)
{
    return naive_impl(text, pattern);
}

KmpRun kmp_search_codepoints(const std::vector<char32_t> &text,
                             const std::vector<char32_t> &pattern)
{
    return kmp_impl(text, pattern);
}

/* ================= 报告 ================= */

namespace {

/** 表格按显示宽度对齐：CJK 与全角标点算 2 列，其余算 1 列 */
bool is_wide_codepoint(char32_t cp)
{
    const std::uint32_t v = static_cast<std::uint32_t>(cp);
    return (v >= 0x1100u && v <= 0x115Fu) || v == 0x2329u || v == 0x232Au ||
           (v >= 0x2E80u && v <= 0xA4CFu && v != 0x303Fu) ||
           (v >= 0xAC00u && v <= 0xD7A3u) || (v >= 0xF900u && v <= 0xFAFFu) ||
           (v >= 0xFE30u && v <= 0xFE6Fu) || (v >= 0xFF00u && v <= 0xFF60u) ||
           (v >= 0xFFE0u && v <= 0xFFE6u) || (v >= 0x20000u && v <= 0x3FFFDu);
}

std::size_t display_width(const std::string &text)
{
    std::size_t width = 0;
    for (std::size_t i = 0; i < text.size();) {
        const unsigned char lead = static_cast<unsigned char>(text[i]);
        std::size_t length = 1;
        std::uint32_t cp = lead;
        if (lead >= 0xF0u) {
            length = 4;
            cp = lead & 0x07u;
        } else if (lead >= 0xE0u) {
            length = 3;
            cp = lead & 0x0Fu;
        } else if (lead >= 0xC0u) {
            length = 2;
            cp = lead & 0x1Fu;
        }
        for (std::size_t k = 1; k < length && i + k < text.size(); ++k) {
            cp = (cp << 6) | (static_cast<unsigned char>(text[i + k]) & 0x3Fu);
        }
        width += is_wide_codepoint(static_cast<char32_t>(cp)) ? 2 : 1;
        i += length;
    }
    return width;
}

std::string pad_right(const std::string &text, std::size_t width)
{
    const std::size_t shown = display_width(text);
    if (shown >= width) {
        return text;
    }
    return text + std::string(width - shown, ' ');
}

std::string join_numbers(const std::vector<std::size_t> &values)
{
    std::ostringstream os;
    for (std::size_t i = 0; i < values.size(); ++i) {
        if (i != 0) {
            os << ' ';
        }
        os << values[i];
    }
    return os.str();
}

std::string positions_text(const std::vector<std::size_t> &positions)
{
    return positions.empty() ? std::string(u8"（无）") : join_numbers(positions);
}

std::string ratio_text(std::size_t numerator, std::size_t denominator)
{
    std::ostringstream os;
    if (denominator == 0) {
        return std::string(u8"未定义");
    }
    os << std::fixed << std::setprecision(2)
       << static_cast<double>(numerator) / static_cast<double>(denominator);
    return os.str();
}

std::string hex_bytes(const std::string &bytes, std::size_t start, std::size_t count)
{
    static const char *digits = "0123456789ABCDEF";
    std::string out;
    for (std::size_t i = 0; i < count && start + i < bytes.size(); ++i) {
        if (i != 0) {
            out.push_back(' ');
        }
        const unsigned char value = static_cast<unsigned char>(bytes[start + i]);
        out.push_back(digits[value >> 4]);
        out.push_back(digits[value & 0x0Fu]);
    }
    return out;
}

std::string repeat_text(const std::string &unit, std::size_t times)
{
    std::string out;
    for (std::size_t i = 0; i < times; ++i) {
        out += unit;
    }
    return out;
}

/** 表格里的模式列：短模式照原样列出，长模式只列形状，免得把表格撑开 */
std::string pattern_label(const std::string &pattern)
{
    if (pattern.empty()) {
        return std::string(u8"（空）");
    }
    if (pattern.size() <= 12) {
        return std::string("\"") + pattern + "\"";
    }
    bool shaped = true;
    for (std::size_t i = 0; i + 1 < pattern.size(); ++i) {
        if (pattern[i] != pattern.front()) {
            shaped = false;
        }
    }
    if (shaped && pattern.back() != pattern.front()) {
        return std::to_string(pattern.size() - 1) + u8" 个 '" + pattern.front() + u8"' 加 '" +
               pattern.back() + u8"'";
    }
    return u8"长模式（" + std::to_string(pattern.size()) + u8" 个字符）";
}

/** 报告第一、二段共用同一组文本与模式 */
struct TextCase {
    std::string label;
    std::string text;
    std::string pattern;
};

const std::string &english_text()
{
    static const std::string text = "the quick brown fox jumps over the lazy dog";
    return text;
}

std::vector<TextCase> report_cases()
{
    return {
        {u8"(1)", english_text(), "the"},
        {u8"(2)", std::string(32, 'a'), "aaab"},
        {u8"(3)", std::string(512, 'a'), std::string(63, 'a') + "b"},
    };
}

void append_naive(std::ostringstream &os)
{
    const std::vector<TextCase> cases = report_cases();

    os << u8"一、朴素匹配：把每个对齐位置都比一遍\n";
    os << u8"  规则：对齐位置从 0 数到 n − m，逐个元素比较，遇到不等就换下一个位置\n";
    os << u8"  (1) 一句 43 字节的英文里找 \"the\"\n";
    os << u8"  (2) 32 个 'a' 里找 \"aaab\"：每个位置都比到第 4 个字符才失配，这是最坏形状\n";
    os << u8"  (3) 把 (2) 放大：512 个 'a' 里找 63 个 'a' 加一个 'b'\n";
    os << "  " << pad_right(u8"组合", 8) << pad_right("n", 8) << pad_right("m", 8)
       << pad_right(u8"对齐位置数", 14) << pad_right(u8"比较次数", 12)
       << pad_right(u8"(n−m+1)×m", 14) << pad_right(u8"n×m", 10) << u8"匹配位置\n";

    for (const TextCase &item : cases) {
        const SearchResult result = naive_search(item.text, item.pattern);
        const std::size_t n = item.text.size();
        const std::size_t m = item.pattern.size();
        const std::size_t alignments = (n >= m && m > 0) ? (n - m + 1) : 0;
        os << "  " << pad_right(item.label, 8) << pad_right(std::to_string(n), 8)
           << pad_right(std::to_string(m), 8) << pad_right(std::to_string(alignments), 14)
           << pad_right(std::to_string(result.comparisons), 12)
           << pad_right(std::to_string(alignments * m), 14)
           << pad_right(std::to_string(n * m), 10) << positions_text(result.positions) << "\n";
    }

    const std::size_t worst = naive_search(cases[1].text, cases[1].pattern).comparisons;
    const std::size_t worst_alignments =
        cases[1].text.size() - cases[1].pattern.size() + 1;
    const std::size_t worst_m = cases[1].pattern.size();
    const std::size_t worst_theory = cases[1].text.size() * worst_m;
    const std::size_t big = naive_search(cases[2].text, cases[2].pattern).comparisons;
    const std::size_t big_alignments = cases[2].text.size() - cases[2].pattern.size() + 1;
    os << u8"  (2) 的 " << worst_alignments << u8" 个对齐位置每个都比满 " << worst_m
       << u8" 次：前 3 个字符都是 'a'，第 4 个撞上 'b'\n";
    os << u8"    比较次数 " << worst << u8"，恰好等于 (n − m + 1) × m = " << worst_alignments
       << u8" × " << worst_m << "\n";
    os << u8"  教科书里的上界 n × m = " << worst_theory
       << u8"，它把最后 m − 1 个「不足一个模式长」的对齐位置也算了进去；\n";
    os << u8"    实现到 n − m 就停，实际拿到的上界是 (n − m + 1) × m = "
       << worst_alignments * worst_m << u8"，两者只差 " << (worst_theory - worst_alignments * worst_m)
       << "\n";
    os << u8"  (3) 的比较次数 " << big << u8" = " << big_alignments << u8" × "
       << cases[2].pattern.size() << u8"，而这段文本里模式一次都没出现\n";
    os << u8"  文本每长一位就多比一遍整个模式：朴素匹配比较次数的上界是 Θ(n × m)\n";
    os << "\n";
}

void append_kmp(std::ostringstream &os)
{
    const std::vector<TextCase> cases = report_cases();

    os << u8"二、KMP：先把失配表算出来\n";
    os << u8"  失配表 π[i]：模式前 i + 1 个字符里，最长的「既是前缀又是后缀」的真前缀长度\n";
    const std::string shown[] = {"aaab", "aabaaab", "the"};
    for (const std::string &pattern : shown) {
        std::size_t build = 0;
        const std::vector<std::size_t> failure = prefix_function(pattern, &build);
        os << u8"  模式 \"" << pattern << u8"\"：m = " << pattern.size()
           << u8"，构建失配表比较 " << build << u8" 次\n";
        os << u8"    下标";
        for (std::size_t i = 0; i < pattern.size(); ++i) {
            os << " " << pad_right(std::to_string(i), 2);
        }
        os << "\n";
        os << u8"    字符";
        for (const char c : pattern) {
            os << " " << pad_right(std::string(1, c), 2);
        }
        os << "\n";
        os << u8"    π   ";
        for (const std::size_t value : failure) {
            os << " " << pad_right(std::to_string(value), 2);
        }
        os << "\n";
    }
    os << u8"  失配表只跟模式有关，与文本无关：换一段文本不用重建\n";
    os << "\n";

    os << "  " << pad_right(u8"组合", 8) << pad_right(u8"模式", 20) << pad_right("m", 8)
       << pad_right(u8"构建比较", 12) << pad_right(u8"匹配比较", 12)
       << pad_right(u8"合计", 10) << pad_right(u8"朴素比较", 12) << u8"合计/朴素\n";

    std::size_t worst_naive = 0;
    std::size_t worst_kmp = 0;
    std::size_t big_naive = 0;
    std::size_t big_kmp = 0;
    for (const TextCase &item : cases) {
        const SearchResult plain = naive_search(item.text, item.pattern);
        const KmpRun run = kmp_search(item.text, item.pattern);
        os << "  " << pad_right(item.label, 8) << pad_right(pattern_label(item.pattern), 20)
           << pad_right(std::to_string(item.pattern.size()), 8)
           << pad_right(std::to_string(run.build_comparisons), 12)
           << pad_right(std::to_string(run.match.comparisons), 12)
           << pad_right(std::to_string(run.total_comparisons()), 10)
           << pad_right(std::to_string(plain.comparisons), 12)
           << ratio_text(plain.comparisons, run.total_comparisons()) << "\n";
        if (item.label == u8"(2)") {
            worst_naive = plain.comparisons;
            worst_kmp = run.total_comparisons();
        }
        if (item.label == u8"(3)") {
            big_naive = plain.comparisons;
            big_kmp = run.total_comparisons();
        }
    }
    os << u8"  (2) 上朴素 " << worst_naive << u8" 次、KMP 合计 " << worst_kmp
       << u8" 次，只差 " << ratio_text(worst_naive, worst_kmp)
       << u8" 倍：m 只有 4，回退省下来的比较有限\n";
    os << u8"  (3) 上朴素 " << big_naive << u8" 次、KMP 合计 " << big_kmp << u8" 次，差 "
       << ratio_text(big_naive, big_kmp) << u8" 倍：m 越大，省下来的越多\n";
    os << u8"  KMP 合计比较次数的上界是 2(n + m)：失配表那一遍只看模式，匹配那一遍 i 只往前走\n";
    os << "\n";
}

void append_rabin_karp(std::ostringstream &os)
{
    struct RunCase {
        std::string label;
        std::string text;
        std::string pattern;
        std::size_t modulus;
        std::size_t base;
    };
    const std::string short_text = "baaabc";
    const std::string degenerate = repeat_text("axc", 12);
    const std::vector<RunCase> runs = {
        {u8"(1)", english_text(), "the", 1000003, 31},
        {u8"(2)", short_text, "abc", 8, 31},
        {u8"(3)", degenerate, "abc", 31, 31},
    };

    os << u8"三、Rabin–Karp：滚动哈希，以及命中之后为什么还要逐字符复核\n";
    os << u8"  哈希定义：H(w) = (w[0]·b^(m−1) + w[1]·b^(m−2) + … + w[m−1]) mod q\n";
    os << u8"  滚动更新：H(下一窗) = ((H(当前) − w[首]·b^(m−1))·b + w[新]) mod q，b = base mod q\n";
    os << u8"  (1) 文本 43 字节的英文，模式 \"the\"，模数 1000003，进制 31\n";
    os << u8"  (2) 文本 \"baaabc\"（6 字节），模式 \"abc\"，模数 8，进制 31 —— 小模数，专门等碰撞\n";
    os << u8"  (3) 文本 12 个 \"axc\"（36 字节），模式 \"abc\"，模数 31，进制 31 —— 进制与模数相等\n";
    os << "  " << pad_right(u8"轮", 6) << pad_right(u8"窗口数", 10) << pad_right(u8"从头算", 10)
       << pad_right(u8"滚动", 8) << pad_right(u8"逐窗重算的乘法", 18)
       << pad_right(u8"哈希命中", 12) << pad_right(u8"复核", 8) << pad_right(u8"碰撞", 8)
       << pad_right(u8"复核比较", 12) << u8"匹配位置\n";

    std::vector<HashRun> results;
    for (const RunCase &item : runs) {
        const HashRun run = rabin_karp(item.text, item.pattern, item.modulus, item.base);
        results.push_back(run);
        os << "  " << pad_right(item.label, 6)
           << pad_right(std::to_string(run.window_count), 10)
           << pad_right(std::to_string(run.fresh_hashes), 10)
           << pad_right(std::to_string(run.roll_steps), 8)
           << pad_right(std::to_string(rabin_karp_recompute_cost(run, item.pattern.size())), 18)
           << pad_right(std::to_string(run.hash_hits), 12)
           << pad_right(std::to_string(run.verifications), 8)
           << pad_right(std::to_string(run.collisions), 8)
           << pad_right(std::to_string(run.verify_comparisons), 12)
           << positions_text(run.positions) << "\n";
    }
    os << u8"  「从头算」= 算一次模式的哈希加算一次首个窗口的哈希；此后每个窗口都是 O(1) 滚动\n";
    os << u8"  「逐窗重算的乘法」= 窗口数 × m：不滚动、每个窗口都从头乘一遍的话要这么多步\n";

    os << u8"  (1) 模式哈希 " << results[0].pattern_hash << u8"，"
       << results[0].window_count << u8" 个窗口里命中 " << results[0].hash_hits
       << u8" 次，复核全部通过，碰撞 " << results[0].collisions << u8" 次\n";
    os << u8"  (2) 模式哈希 " << results[1].pattern_hash << u8"，4 个窗口的哈希依次是 "
       << join_numbers(results[1].window_hashes) << u8"，命中 " << results[1].hash_hits
       << u8" 次、复核 " << results[1].verifications << u8" 次、碰撞 " << results[1].collisions
       << u8" 次，真正的匹配落在偏移 " << positions_text(results[1].positions) << "\n";
    for (std::size_t k = 0; k < results[1].collision_list.size(); ++k) {
        const HashRun::Collision &hit = results[1].collision_list[k];
        os << u8"      第 " << (k + 1) << u8" 次碰撞：窗口从偏移 " << hit.window_start
           << u8" 起，内容是 \"" << short_text.substr(hit.window_start, runs[1].pattern.size())
           << u8"\"，哈希与模式同为 " << results[1].pattern_hash << u8"，复核在第 "
           << (hit.first_diff + 1) << u8" 个元素上停住\n";
    }
    os << u8"  (3) 进制与模数相等时 b mod q = 0，哈希退化成只看窗口最后一个字符：\n";
    os << u8"      " << results[2].window_count << u8" 个窗口里有 " << results[2].hash_hits
       << u8" 个以 'c' 结尾，它们的哈希全是 " << results[2].pattern_hash
       << u8"，于是全部命中\n";
    os << u8"      " << results[2].verifications << u8" 次复核全部失败，碰撞 " << results[2].collisions
       << u8" 次，匹配 " << results[2].positions.size() << u8" 处\n";
    if (!results[2].collision_list.empty()) {
        const HashRun::Collision &hit = results[2].collision_list.front();
        os << u8"      第一处碰撞：窗口从偏移 " << hit.window_start << u8" 起，内容是 \""
           << degenerate.substr(hit.window_start, runs[2].pattern.size()) << u8"\"，复核在第 "
           << (hit.first_diff + 1) << u8" 个元素上停住\n";
        if (results[2].collision_list.size() > 1) {
            os << u8"      （其余 " << (results[2].collision_list.size() - 1)
               << u8" 次同类，都是「末位相同、前面不同」）\n";
        }
    }
    os << u8"  哈希相等只说明「可能是匹配」：命中的窗口必须逐字符复核，答案才对得上朴素版\n";
    os << "\n";
}

void append_utf8(std::ostringstream &os)
{
    const std::string text_bytes = u8"字符串匹配：在字符串里找 pattern";
    const std::string pattern_bytes = u8"字符串";
    const Utf8Text text = decode_utf8(text_bytes);
    const Utf8Text pattern = decode_utf8(pattern_bytes);
    const SearchResult by_byte = naive_search(text_bytes, pattern_bytes);
    const SearchResult by_codepoint =
        naive_search_codepoints(text.codepoints, pattern.codepoints);
    const std::vector<std::size_t> lengths = text.count_by_length();

    os << u8"四、中文文本：按字节与按码点是两种口径\n";
    os << u8"  按字节：元素是 char，一个 std::string 的长度就是字节数\n";
    os << u8"  按码点：先解码成 char32_t 序列，元素是一个字符，一个汉字一个\n";
    os << u8"  文本：\"" << text_bytes << "\"\n";
    os << u8"    字节数 " << text.bytes.size() << u8"，码点数 " << text.codepoints.size()
       << u8"，UTF-8 合法：" << (text.valid ? u8"是" : u8"否") << "\n";
    os << u8"    按编码长度分布：单字节 " << lengths[1] << u8" 个、两字节 " << lengths[2]
       << u8" 个、三字节 " << lengths[3] << u8" 个、四字节 " << lengths[4] << u8" 个\n";
    os << u8"    字节数 − 码点数 = " << (text.bytes.size() - text.codepoints.size())
       << u8"，正好等于每个三字节字符多出来的 2 个续字节之和\n";
    os << u8"  模式：\"" << pattern_bytes << u8"\"，字节数 " << pattern.bytes.size()
       << u8"，码点数 " << pattern.codepoints.size() << "\n";
    os << u8"  按字节朴素匹配：对齐位置 " << (text.bytes.size() - pattern.bytes.size() + 1)
       << u8" 个，比较 " << by_byte.comparisons << u8" 次，位置 " << positions_text(by_byte.positions)
       << "\n";
    os << u8"  按码点朴素匹配：对齐位置 " << (text.codepoints.size() - pattern.codepoints.size() + 1)
       << u8" 个，比较 " << by_codepoint.comparisons << u8" 次，位置 "
       << positions_text(by_codepoint.positions) << "\n";
    os << u8"  同一处匹配，两种口径各记一个位置（字节偏移 → 码点下标）：\n";
    for (std::size_t k = 0; k < by_byte.positions.size() && k < by_codepoint.positions.size(); ++k) {
        const std::size_t offset = by_byte.positions[k];
        os << u8"    第 " << (k + 1) << u8" 处  字节偏移 " << offset << u8"  →  码点下标 "
           << by_codepoint.positions[k] << u8"  是字符边界："
           << (text.is_boundary(offset) ? u8"是" : u8"否") << "\n";
    }

    const std::string fragment = text_bytes.substr(1, 3);
    const SearchResult fragment_hits = naive_search(text_bytes, fragment);
    const Utf8Text fragment_decoded = decode_utf8(fragment);
    os << u8"  从字符中间切出来的模式：取文本的第 1 到第 3 个字节，得到 "
       << hex_bytes(fragment, 0, fragment.size()) << u8"（" << fragment.size() << u8" 字节）\n";
    os << u8"    按字节搜索：位置 " << positions_text(fragment_hits.positions) << u8"，比较 "
       << fragment_hits.comparisons << u8" 次\n";
    if (!fragment_hits.positions.empty()) {
        for (const std::size_t offset : fragment_hits.positions) {
            const std::size_t index = text.codepoint_index_of(offset);
            os << u8"      偏移 " << offset << u8" 落在第 " << index
               << u8" 个字符的内部（该字符占字节 " << text.byte_offset[index] << u8" 到 "
               << (text.byte_offset[index + 1] - 1) << u8"），是字符边界："
               << (text.is_boundary(offset) ? u8"是" : u8"否") << "\n";
        }
    }
    os << u8"    按码点解码：UTF-8 合法：" << (fragment_decoded.valid ? u8"是" : u8"否")
       << u8"，第一个坏字节在偏移 " << fragment_decoded.bad_offset
       << u8"，解出的码点数 " << fragment_decoded.codepoints.size() << "\n";
    os << u8"      这段字节在码点口径里根本不存在，所以码点匹配给不出这些位置\n";
    os << u8"  合法 UTF-8 模式的首字节要么是 ASCII（小于 0x80），要么是首字节（0xC2 到 0xF4）；\n";
    os << u8"    文本里非边界位置上的字节是续字节（0x80 到 0xBF），两者不可能相等\n";
    os << u8"    真正会出问题的是模式本身：用 substr 按字节切出来的片段不是合法 UTF-8，\n";
    os << u8"    按字节却能找到位置，报出来的位置就落在字符中间\n";
    os << u8"  按字节给的是字节偏移，按码点给的是第几个字符，两者之间的换算不是乘法\n";
}

/** 去掉每行末尾的空格。表格靠 pad_right 补齐，最后一列会留下行尾空白；
    这份报告常被整块贴进文档，行尾空白会在版本控制的差异里显出来。 */
std::string strip_line_trailing_spaces(const std::string &text)
{
    std::string out;
    out.reserve(text.size());
    std::size_t start = 0;
    while (start < text.size()) {
        const std::size_t newline = text.find('\n', start);
        const std::size_t stop = (newline == std::string::npos) ? text.size() : newline;
        std::size_t last = stop;
        while (last > start && text[last - 1] == ' ') {
            --last;
        }
        out.append(text, start, last - start);
        if (newline == std::string::npos) {
            break;
        }
        out.push_back('\n');
        start = newline + 1;
    }
    return out;
}

}   /* namespace */

std::string build_report()
{
    std::ostringstream os;
    append_naive(os);
    append_kmp(os);
    append_rabin_karp(os);
    append_utf8(os);
    return strip_line_trailing_spaces(os.str());
}

/* ================= 自测 ================= */

namespace {

class Checks {
public:
    void expect(bool ok, const std::string &what)
    {
        ++total_;
        if (ok) {
            ++passed_;
            lines_.push_back(u8"[通过] " + std::to_string(total_) + ". " + what);
        } else {
            ++failed_;
            lines_.push_back(u8"[失败] " + std::to_string(total_) + ". " + what);
        }
    }

    void expect_eq_size(std::size_t got, std::size_t want, const std::string &what)
    {
        if (got == want) {
            expect(true, what);
        } else {
            expect(false, what + u8"（得到 " + std::to_string(got) + u8"，应为 " +
                              std::to_string(want) + u8"）");
        }
    }

    CheckResult finish() const
    {
        CheckResult result;
        result.total = total_;
        result.passed = passed_;
        result.failed = failed_;
        result.lines = lines_;
        return result;
    }

private:
    std::size_t total_ = 0;
    std::size_t passed_ = 0;
    std::size_t failed_ = 0;
    std::vector<std::string> lines_;
};

/** 用标准库的 find 当独立参照，逐个位置收集（允许重叠） */
std::vector<std::size_t> find_all(const std::string &text, const std::string &pattern)
{
    std::vector<std::size_t> out;
    if (pattern.empty() || pattern.size() > text.size()) {
        return out;
    }
    std::size_t pos = text.find(pattern, 0);
    while (pos != std::string::npos) {
        out.push_back(pos);
        pos = text.find(pattern, pos + 1);
    }
    return out;
}

std::vector<std::pair<std::string, std::string>> test_pairs()
{
    return {
        {"the quick brown fox jumps over the lazy dog", "the"},
        {std::string(32, 'a'), "aaab"},
        {std::string(512, 'a'), std::string(63, 'a') + "b"},
        {"ababababab", "abab"},
        {"aaaa", "aa"},
        {"mississippi", "issi"},
    };
}

/** 固定种子的线性同余发生器：伪随机但可复现 */
class Lcg {
public:
    explicit Lcg(std::uint32_t seed) : state_(seed) {}

    std::uint32_t next()
    {
        state_ = state_ * 1103515245u + 12345u;
        return (state_ >> 16) & 0x7FFFu;
    }

private:
    std::uint32_t state_;
};

std::string random_text(Lcg *lcg, std::size_t length, const std::string &alphabet)
{
    std::string out;
    for (std::size_t i = 0; i < length; ++i) {
        out.push_back(alphabet[lcg->next() % alphabet.size()]);
    }
    return out;
}

/** 逐个窗口从头算哈希，给滚动版本当参照 */
std::size_t fresh_hash_of(const std::string &text, std::size_t start, std::size_t length,
                          std::size_t modulus, std::size_t base)
{
    const std::size_t b = base % modulus;
    std::size_t h = 0;
    for (std::size_t k = 0; k < length; ++k) {
        h = (h * b + static_cast<std::size_t>(static_cast<unsigned char>(text[start + k]))) %
            modulus;
    }
    return h;
}

}   /* namespace */

std::string CheckResult::summary() const
{
    std::ostringstream os;
    os << total << u8" 项中 " << passed << u8" 项通过";
    if (failed == 0) {
        os << u8"，全部通过";
    } else {
        os << u8"，" << failed << u8" 项不符";
    }
    return os.str();
}

CheckResult run_self_tests()
{
    Checks checks;

    const std::vector<std::pair<std::string, std::string>> pairs = test_pairs();

    /* 1 朴素匹配与标准库的 find 对照 */
    bool naive_matches_library = true;
    for (const std::pair<std::string, std::string> &item : pairs) {
        if (naive_search(item.first, item.second).positions != find_all(item.first, item.second)) {
            naive_matches_library = false;
        }
    }
    checks.expect(naive_matches_library,
                  u8"朴素匹配与 std::string::find 在 6 组文本/模式上位置逐位相同");

    /* 2 最坏输入的比较次数 */
    const SearchResult worst = naive_search(std::string(32, 'a'), "aaab");
    checks.expect_eq_size(worst.comparisons, 116,
                          u8"最坏输入 32 个 'a' 里找 \"aaab\"：比较次数 116 = (n − m + 1) × m");
    checks.expect(worst.comparisons == (32 - 4 + 1) * 4 && worst.positions.empty(),
                  u8"比较次数等于 (n − m + 1) × m 这个上界，且这段文本里没有匹配");

    /* 3 放大版的比较次数 */
    const std::string big_text(512, 'a');
    const std::string big_pattern = std::string(63, 'a') + "b";
    checks.expect_eq_size(naive_search(big_text, big_pattern).comparisons, 28736,
                          u8"放大版 512 个 'a' 里找 63 个 'a' 加 'b'：比较次数 28736 = 449 × 64");

    /* 4 退化输入 */
    bool degenerate_ok = true;
    if (naive_search("abc", "").comparisons != 0 || !naive_search("abc", "").positions.empty()) {
        degenerate_ok = false;
    }
    if (!naive_search("ab", "abc").positions.empty() || naive_search("ab", "abc").comparisons != 0) {
        degenerate_ok = false;
    }
    if (!naive_search("", "a").positions.empty() || naive_search("", "a").comparisons != 0) {
        degenerate_ok = false;
    }
    if (!kmp_search("abc", "").match.positions.empty() || !kmp_search("ab", "abc").match.positions.empty()) {
        degenerate_ok = false;
    }
    checks.expect(degenerate_ok,
                  u8"空模式、模式比文本长、空文本三种退化输入都返回空位置且比较次数为 0");

    /* 5 KMP 与朴素对照 */
    bool kmp_matches_naive = true;
    for (const std::pair<std::string, std::string> &item : pairs) {
        const SearchResult plain = naive_search(item.first, item.second);
        const KmpRun run = kmp_search(item.first, item.second);
        if (!run.match.same_positions_as(plain)) {
            kmp_matches_naive = false;
        }
    }
    checks.expect(kmp_matches_naive, u8"KMP 与朴素在 6 组文本/模式上位置逐位相同");

    /* 6 固定种子伪随机文本上的对照 */
    bool random_ok = true;
    Lcg lcg(20261002u);
    for (int round = 0; round < 400; ++round) {
        const std::size_t text_length = 4 + lcg.next() % 37;
        const std::size_t pattern_length = 1 + lcg.next() % 5;
        const std::string alphabet = (round % 2 == 0) ? "ab" : "abc";
        const std::string text = random_text(&lcg, text_length, alphabet);
        const std::string pattern = random_text(&lcg, pattern_length, alphabet);
        const SearchResult plain = naive_search(text, pattern);
        const KmpRun run = kmp_search(text, pattern);
        if (!run.match.same_positions_as(plain) || plain.positions != find_all(text, pattern)) {
            random_ok = false;
        }
    }
    checks.expect(random_ok,
                  u8"固定种子伪随机文本上 KMP 与朴素的位置逐位相同（400 组）");

    /* 7 失配表的三条性质 */
    const std::string property_patterns[] = {"a", "aa", "ab", "aaab", "aabaaab", "ababaca",
                                             "abcabcabd"};
    bool property_ok = true;
    for (const std::string &pattern : property_patterns) {
        const std::vector<std::size_t> failure = prefix_function(pattern, nullptr);
        if (failure.size() != pattern.size() || (!failure.empty() && failure[0] != 0)) {
            property_ok = false;
            continue;
        }
        for (std::size_t i = 1; i < failure.size(); ++i) {
            if (failure[i] > i || failure[i] > failure[i - 1] + 1) {
                property_ok = false;
            }
        }
    }
    checks.expect(property_ok,
                  u8"失配表三条性质：π[0] 是 0、π[i] ≤ i、π[i] ≤ π[i−1] + 1");

    /* 8 失配表与独立实现对照 */
    bool failure_matches_brute = true;
    for (const std::string &pattern : property_patterns) {
        if (prefix_function(pattern, nullptr) != prefix_function_brute(pattern)) {
            failure_matches_brute = false;
        }
    }
    checks.expect(failure_matches_brute,
                  u8"失配表与逐前缀枚举的独立实现在 7 个模式上逐位相同");

    /* 9 两个模式的失配表取值 */
    const std::vector<std::size_t> aaab = {0, 1, 2, 0};
    const std::vector<std::size_t> aabaaab = {0, 1, 0, 1, 2, 2, 3};
    checks.expect(prefix_function("aaab", nullptr) == aaab &&
                      prefix_function("aabaaab", nullptr) == aabaaab,
                  u8"\"aaab\" 的失配表是 0 1 2 0，\"aabaaab\" 是 0 1 0 1 2 2 3");

    /* 10 最坏输入上 KMP 的两段比较次数 */
    const KmpRun worst_kmp = kmp_search(std::string(32, 'a'), "aaab");
    checks.expect(worst_kmp.build_comparisons == 5 && worst_kmp.match.comparisons == 61 &&
                      worst_kmp.total_comparisons() == 66 && worst.positions.empty() &&
                      worst_kmp.match.positions.empty(),
                  u8"最坏输入上 KMP 合计 66 次（构建 5 + 匹配 61），朴素是 116 次");

    /* 11 KMP 的线性上界 */
    bool bound_ok = true;
    for (const std::pair<std::string, std::string> &item : pairs) {
        const KmpRun run = kmp_search(item.first, item.second);
        if (run.total_comparisons() > 2 * (item.first.size() + item.second.size())) {
            bound_ok = false;
        }
    }
    checks.expect(bound_ok, u8"6 组文本上 KMP 合计比较次数都不超过 2(n + m)");

    /* 12 放大版上 KMP 与朴素的差距 */
    const KmpRun big_kmp = kmp_search(big_text, big_pattern);
    const std::size_t big_naive = naive_search(big_text, big_pattern).comparisons;
    checks.expect(big_kmp.total_comparisons() * 20 < big_naive,
                  u8"放大版上 KMP 合计比较次数不到朴素的 1/20");

    /* 13 滚动哈希与逐窗重算对照 */
    bool rolling_ok = true;
    const std::string rolling_texts[] = {"baaabc", english_text(), repeat_text("axc", 12)};
    const std::size_t moduli[] = {8, 31, 1000003};
    for (const std::string &text : rolling_texts) {
        for (const std::size_t modulus : moduli) {
            const HashRun run = rabin_karp(text, "abc", modulus, 31);
            if (run.pattern_hash != fresh_hash_of("abc", 0, 3, modulus, 31)) {
                rolling_ok = false;
            }
            if (run.window_hashes.size() != run.window_count) {
                rolling_ok = false;
                continue;
            }
            for (std::size_t i = 0; i < run.window_hashes.size(); ++i) {
                if (run.window_hashes[i] != fresh_hash_of(text, i, 3, modulus, 31)) {
                    rolling_ok = false;
                }
            }
        }
    }
    checks.expect(rolling_ok, u8"滚动哈希与逐窗从头算的值在 3 个模数下逐位相同");

    /* 14 大模数下位置与朴素一致 */
    bool hash_positions_ok = true;
    for (const std::pair<std::string, std::string> &item : pairs) {
        const HashRun run = rabin_karp(item.first, item.second, 1000003, 31);
        if (!run.positions_same_as(naive_search(item.first, item.second).positions)) {
            hash_positions_ok = false;
        }
    }
    checks.expect(hash_positions_ok, u8"大模数下 Rabin–Karp 与朴素的位置逐位相同");

    /* 15 哈希守恒式 */
    bool conservation_ok = true;
    for (const std::pair<std::string, std::string> &item : pairs) {
        const HashRun run = rabin_karp(item.first, item.second, 8, 31);
        if (run.hash_hits != run.positions.size() + run.collisions ||
            run.verifications != run.hash_hits || run.fresh_hashes != 2 ||
            run.roll_steps + 1 != run.window_count ||
            run.collision_list.size() != run.collisions) {
            conservation_ok = false;
        }
    }
    checks.expect(conservation_ok,
                  u8"哈希守恒式：命中 = 匹配 + 碰撞，复核次数 = 命中次数，滚动次数 = 窗口数 − 1");

    /* 16 小模数下的碰撞 */
    const HashRun small = rabin_karp("baaabc", "abc", 8, 31);
    const std::vector<std::size_t> small_hashes = {2, 1, 2, 2};
    const std::vector<std::size_t> small_positions = {3};
    checks.expect(small.window_hashes == small_hashes && small.hash_hits == 3 &&
                      small.verifications == 3 && small.collisions == 2 &&
                      small.positions == small_positions,
                  u8"小模数 q = 8 的 \"baaabc\" 找 \"abc\"：窗口哈希 2 1 2 2，命中 3、碰撞 2、匹配在偏移 3");

    /* 17 每次碰撞都是「哈希相等而内容不等」 */
    bool collision_detail_ok = small.collision_list.size() == 2;
    for (const HashRun::Collision &hit : small.collision_list) {
        const std::string window = std::string("baaabc").substr(hit.window_start, 3);
        if (window == "abc") {
            collision_detail_ok = false;
        }
        if (fresh_hash_of(window, 0, 3, 8, 31) != small.pattern_hash) {
            collision_detail_ok = false;
        }
    }
    checks.expect(collision_detail_ok,
                  u8"两次碰撞的窗口内容都与模式不同，而它们的哈希与模式相等");

    /* 18 退化参数 */
    const HashRun degenerate = rabin_karp(repeat_text("axc", 12), "abc", 31, 31);
    checks.expect(degenerate.positions.empty() && degenerate.hash_hits == 12 &&
                      degenerate.collisions == 12,
                  u8"退化参数 base == modulus：36 字节文本里命中 12 次、碰撞 12 次、匹配 0 处");

    /* 19 UTF-8 解码与编码往返 */
    const std::string chinese = u8"字符串匹配：在字符串里找 pattern";
    const Utf8Text decoded = decode_utf8(chinese);
    checks.expect(decoded.valid && decoded.bytes.size() == 44 && decoded.codepoints.size() == 20 &&
                      encode_utf8(decoded.codepoints) == chinese,
                  u8"UTF-8 解码：中文文本 44 字节、20 码点，编码回去逐位相同");

    /* 20 按编码长度分布 */
    const std::vector<std::size_t> lengths = decoded.count_by_length();
    checks.expect(lengths[1] == 8 && lengths[2] == 0 && lengths[3] == 12 && lengths[4] == 0 &&
                      decoded.bytes.size() - decoded.codepoints.size() == 2 * lengths[3],
                  u8"按编码长度分布：8 个单字节码点、12 个三字节码点，字节数 − 码点数 = 24");

    /* 21 非法 UTF-8 */
    const Utf8Text fragment = decode_utf8(chinese.substr(1, 3));
    const Utf8Text truncated = decode_utf8(std::string(u8"字").substr(0, 2));
    std::string bad_bytes;
    bad_bytes.push_back(static_cast<char>(0xE5));   /* 三字节字符的首字节 */
    bad_bytes.push_back('A');                       /* 本该是续字节 */
    bad_bytes.push_back('B');
    const Utf8Text bad_continuation = decode_utf8(bad_bytes);
    checks.expect(!fragment.valid && fragment.bad_offset == 0 && !truncated.valid &&
                      truncated.bad_offset == 0 && !bad_continuation.valid &&
                      bad_continuation.bad_offset == 1,
                  u8"截断与非法续字节都被判为不合法，并指出第一个坏字节的位置");

    /* 22 两种口径的位置对照 */
    const std::string pattern_bytes = u8"字符串";
    const Utf8Text pattern = decode_utf8(pattern_bytes);
    const SearchResult by_byte = naive_search(chinese, pattern_bytes);
    const SearchResult by_codepoint =
        naive_search_codepoints(decoded.codepoints, pattern.codepoints);
    bool position_map_ok = by_byte.positions.size() == 2 && by_codepoint.positions.size() == 2 &&
                           by_byte.positions[0] == 0 && by_byte.positions[1] == 21 &&
                           by_codepoint.positions[0] == 0 && by_codepoint.positions[1] == 7;
    for (const std::size_t offset : by_byte.positions) {
        if (!decoded.is_boundary(offset)) {
            position_map_ok = false;
        }
    }
    checks.expect(position_map_ok,
                  u8"\"字符串\" 按字节命中 0 与 21、按码点命中 0 与 7，字节 21 落在码点 7 上");

    /* 23 从字符中间切出来的片段 */
    const SearchResult fragment_hits = naive_search(chinese, chinese.substr(1, 3));
    const std::vector<std::size_t> fragment_positions = {1, 22};
    bool fragment_ok = fragment_hits.positions == fragment_positions;
    for (const std::size_t offset : fragment_hits.positions) {
        if (decoded.is_boundary(offset)) {
            fragment_ok = false;
        }
    }
    checks.expect(fragment_ok,
                  u8"从字符中间切下来的 3 字节片段按字节在偏移 1 与 22 命中，两处都不是字符边界");

    /* 24 码点口径下 KMP 与朴素一致，失配表另用一份码点版枚举实现对照 */
    auto brute_codepoints = [](const std::vector<char32_t> &pattern) {
        const std::size_t m = pattern.size();
        std::vector<std::size_t> failure(m, 0);
        for (std::size_t i = 1; i < m; ++i) {
            for (std::size_t length = 1; length <= i; ++length) {
                bool same = true;
                for (std::size_t k = 0; k < length; ++k) {
                    if (pattern[k] != pattern[i + 1 - length + k]) {
                        same = false;
                        break;
                    }
                }
                if (same) {
                    failure[i] = length;
                }
            }
        }
        return failure;
    };
    const KmpRun codepoint_kmp = kmp_search_codepoints(decoded.codepoints, pattern.codepoints);
    checks.expect(codepoint_kmp.match.same_positions_as(by_codepoint) &&
                      codepoint_kmp.failure == brute_codepoints(pattern.codepoints),
                  u8"码点口径下 KMP 与朴素的位置逐位相同，失配表与码点版枚举实现相同");

    return checks.finish();
}

}   /* namespace smlab */
