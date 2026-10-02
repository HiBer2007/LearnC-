/* main_cli.cpp —— 练习模板 10 的命令行验收程序（C++）
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
 * 这个文件**不需要改**：它按 4 个阶段调用 sm 里的函数，把结果打印成
 * 《配置步骤.md》里的验收输出。你的实现写在 src/strmatch.cpp 里。
 *
 * 构建与运行（在模板目录下）：
 *     cmake --preset mingw-gdb
 *     cmake --build --preset mingw-gdb
 *     build\mingw\bin\app_cli.exe
 *
 * 输出全部写成 ASCII：换一台代码页不是 65001 的机器也照样能读。
 * 主串与模式串都写死在本文件里，重跑逐位相同。
 */
#include <cstddef>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

#include "strmatch.hpp"

namespace {

/* 写死的主串，分四段拼起来（长度 79，全部是 ASCII）：
 *   30 个 a 接一个 b          长串重复字符，朴素匹配在这里最吃亏
 *   "aabaaab"                 结构段，给失配表留出非平凡的跳法
 *   20 个 a、一个 c
 *   "aabaaab"、12 个 a 接一个 b
 * 主模式 "aaaaaaaaab" 在里面有两次命中。 */
const char *kText =
    "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"     /* 30 个 a */
    "b"
    "aabaaab"
    "aaaaaaaaaaaaaaaaaaaa"               /* 20 个 a */
    "c"
    "aabaaab"
    "aaaaaaaaaaaa"                       /* 12 个 a */
    "b";

const char *kPat = "aaaaaaaaab";         /* 9 个 a 接一个 b */
const char *kAltPat = "aabaaab";         /* 失配表里有跨好几格的回退 */

/* 阶段 4 的演示数据：模式串很短，主串里故意放了两个 "bbl"。
 * 在 base 131、mod 101 下 "bbl" 与 "abc" 的哈希相同，见《配置步骤.md》阶段 4。 */
const char *kDemoPat = "abc";
const char *kDemoText = "xxbblxxabcxxbblxx";

void line(const char *label, long long value)
{
    std::cout << std::left << std::setw(32) << label << ": " << value << "\n";
}

void line(const char *label, const std::string &text)
{
    std::cout << std::left << std::setw(32) << label << ": " << text << "\n";
}

std::string yes_no(bool v)
{
    return v ? "yes" : "no";
}

/* 失配表整行打印时的标签，形如 fail[0..10] */
std::string table_label(const char *name, const std::vector<std::size_t> &v)
{
    if (v.empty()) {
        return std::string(name);
    }
    return std::string(name) + "[0.." + std::to_string(v.size() - 1) + "]";
}

/* 阶段 3 里一个模式串的一组对照：KMP 与朴素版的位置、比较次数 */
void stage3_one(const std::string &text, const std::string &pat)
{
    sm::MatchStats naive_st;
    const std::vector<std::size_t> naive = sm::naive_search(text, pat, naive_st);

    sm::MatchStats kmp_st;
    const std::vector<std::size_t> fail = sm::build_failure(pat);
    const std::vector<std::size_t> kmp = sm::kmp_search(text, pat, fail, kmp_st);

    line("pattern", pat);
    line("kmp positions", sm::join_numbers(kmp));
    line("naive positions", sm::join_numbers(naive));
    line("positions equal", yes_no(sm::same_positions(kmp, naive)));
    line("duplicate positions", sm::count_duplicates(kmp));
    line("kmp comparisons", kmp_st.comparisons);
    line("naive comparisons", naive_st.comparisons);
}

/* 阶段 4 里一个模式串的一组对照：滚动哈希与 KMP 的位置、三个哈希计数 */
void stage4_one(const std::string &text, const std::string &pat)
{
    sm::MatchStats hash_st;
    const std::vector<std::size_t> found = sm::rabin_karp(text, pat, hash_st);

    sm::MatchStats kmp_st;
    const std::vector<std::size_t> fail = sm::build_failure(pat);
    const std::vector<std::size_t> kmp = sm::kmp_search(text, pat, fail, kmp_st);

    line("pattern", pat);
    line("rabin_karp positions", sm::join_numbers(found));
    line("kmp positions", sm::join_numbers(kmp));
    line("positions equal", yes_no(sm::same_positions(found, kmp)));
    line("duplicate positions", sm::count_duplicates(found));
    line("hash updates", hash_st.hash_updates);
    line("hash hits", hash_st.hash_hits);
    line("hash equal, string unequal", hash_st.hash_false);
}

} /* namespace */

int main()
{
    const std::string text(kText);
    const std::string pat(kPat);
    const std::string alt(kAltPat);
    const std::string demo_pat(kDemoPat);
    const std::string demo_text(kDemoText);

    /* ---------------------------------------------------------- 阶段 1 */
    std::cout << "=== Stage 1: naive search, the baseline ===\n";
    {
        sm::MatchStats st;
        const std::vector<std::size_t> naive = sm::naive_search(text, pat, st);

        line("text length", static_cast<long long>(text.size()));
        line("text", text);
        line("pattern", pat);
        line("naive positions", sm::join_numbers(naive));
        line("naive matches", static_cast<long long>(naive.size()));
        line("naive comparisons", st.comparisons);
    }

    /* ---------------------------------------------------------- 阶段 2 */
    std::cout << "\n=== Stage 2: the failure table, cell by cell ===\n";
    {
        const std::vector<std::size_t> f1 = sm::build_failure(pat);
        line("pattern", pat);
        line("table length", static_cast<long long>(f1.size()));
        for (std::size_t i = 0; i < f1.size(); ++i) {
            const std::string cell = "fail[" + std::to_string(i) + "]";
            line(cell.c_str(), static_cast<long long>(f1[i]));
        }

        const std::vector<std::size_t> f2 = sm::build_failure(alt);
        line("pattern", alt);
        line("table length", static_cast<long long>(f2.size()));
        line(table_label("fail", f2).c_str(), sm::join_numbers(f2));

        const std::vector<std::size_t> f3 = sm::build_failure(demo_pat);
        line("pattern", demo_pat);
        line("table length", static_cast<long long>(f3.size()));
        line(table_label("fail", f3).c_str(), sm::join_numbers(f3));
    }

    /* ---------------------------------------------------------- 阶段 3 */
    std::cout << "\n=== Stage 3: KMP, jump with the failure table ===\n";
    {
        stage3_one(text, pat);
        stage3_one(text, alt);
    }

    /* ---------------------------------------------------------- 阶段 4 */
    std::cout << "\n=== Stage 4: rolling hash, shift and verify ===\n";
    {
        line("hash base", sm::kHashBase);
        line("hash modulus", sm::kHashMod);
        stage4_one(text, pat);

        std::cout << "short pattern, collisions by construction:\n";
        line("text", demo_text);
        stage4_one(demo_text, demo_pat);
    }

    return 0;
}
