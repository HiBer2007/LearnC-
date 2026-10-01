/**
 * main_cli.cpp —— 练习模板 02 的命令行验收程序（C++）
 *
 * 这个文件**不需要改**：它按 4 个阶段调用 report.cpp 里的函数，
 * 把结果打印成《配置步骤.md》里的验收输出。你的实现写在 src/report.cpp 里。
 *
 * 构建与运行（在模板目录下）：
 *     cmake --preset mingw-gdb
 *     cmake --build --preset mingw-gdb
 *     build\mingw\bin\app_cli.exe
 *     build\mingw\bin\app_cli.exe data\sample.txt report.txt
 *
 * 阶段的划分：
 *     阶段 1  <fstream> 读取与流的状态
 *     阶段 2  <sstream> 分词、计数与排序
 *     阶段 3  <iomanip> 格式化报表
 *     阶段 4  写报表文件，并与 printf 的结果对照
 */
#include <cstdio>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <string>

#include "report.hpp"

/* 骨架自带的兜底样例：文件打不开时用它，好让阶段 2 到阶段 4 仍然能跑起来。 */
static const char kFallback[] =
    "the quick brown fox jumps over the lazy dog\n"
    "the fox is quick and the dog is lazy\n";

int main(int argc, char **argv)
{
    const std::string path     = (argc > 1) ? argv[1] : "data/sample.txt";
    const std::string out_path = (argc > 2) ? argv[2] : "build/report.txt";

    std::string text;
    std::string err;
    rep::Stats  stats;

    /* ---------------------------------------------------------- 阶段 1 */
    std::cout << "=== Stage 1: read file ===\n";
    std::cout << "path    : " << path << "\n";

    if (!rep::read_all(path, text, err)) {
        std::cout << "open failed : " << err << "\n";
        std::cout << "fallback    : built-in two-line sample (so the later stages still run)\n";
        text = kFallback;
    }

    stats.path  = path;
    stats.bytes = static_cast<long>(text.size());
    stats.lines = rep::count_lines(text);
    std::cout << "bytes   : " << stats.bytes << "\n";
    std::cout << "lines   : " << stats.lines << "\n";

    /* ---------------------------------------------------------- 阶段 2 */
    std::cout << "\n=== Stage 2: tokenize and sort ===\n";
    rep::collect_words(text, stats.entries, stats.words);
    rep::sort_entries(stats.entries);
    stats.unique = static_cast<long>(stats.entries.size());
    std::cout << "words   : " << stats.words << "\n";
    std::cout << "unique  : " << stats.unique << "\n";
    std::cout << "top 5   :\n";
    for (std::size_t i = 0; i < stats.entries.size() && i < 5; ++i) {
        std::cout << "  " << std::right << std::setw(2) << (i + 1) << "  "
                  << std::left << std::setw(14) << stats.entries[i].word << " "
                  << std::right << std::setw(4) << stats.entries[i].count << "\n";
    }

    /* ---------------------------------------------------------- 阶段 3 */
    std::cout << "\n=== Stage 3: format with iomanip ===\n";
    const std::string report = rep::format_report(stats);
    std::cout << report;

    /* ---------------------------------------------------------- 阶段 4 */
    std::cout << "\n=== Stage 4: write file and compare with printf ===\n";
    std::cout << "out file : " << out_path << "\n";

    if (rep::write_file(out_path, report, err)) {
        std::cout << "written  : " << report.size() << " bytes\n";
    } else {
        std::cout << "write failed : " << err << "\n";
    }

    /* 同一份报表再用 printf 拼一遍，长度与内容都应当相同。
       这一段是《配置步骤.md》里「与 printf 对照」的落点。 */
    char buffer[8192];
    int  used = std::snprintf(buffer, sizeof(buffer),
                              "file    : %s\nbytes   : %ld\nlines   : %ld\n"
                              "words   : %ld\nunique  : %ld\ntop %d  :\n",
                              stats.path.c_str(), stats.bytes, stats.lines,
                              stats.words, stats.unique, rep::kTopN);
    const std::size_t shown = (stats.entries.size() < static_cast<std::size_t>(rep::kTopN))
                                  ? stats.entries.size()
                                  : static_cast<std::size_t>(rep::kTopN);
    for (std::size_t i = 0; i < shown && used > 0; ++i) {
        char bar[48];
        const long n = stats.entries[i].count;
        const std::size_t m = (n > 40) ? 40u : static_cast<std::size_t>(n);
        for (std::size_t k = 0; k < m; ++k) {
            bar[k] = '#';
        }
        bar[m] = '\0';
        used += std::snprintf(buffer + used, sizeof(buffer) - static_cast<std::size_t>(used),
                              "  %2ld  %-14s %4ld  %s\n",
                              static_cast<long>(i + 1), stats.entries[i].word.c_str(), n, bar);
        if (static_cast<std::size_t>(used) >= sizeof(buffer)) {
            break;
        }
    }

    const bool identical = (report.size() == static_cast<std::size_t>(used))
                        && (report.compare(0, report.size(), buffer,
                                           static_cast<std::size_t>(used)) == 0);
    std::cout << "identical: " << (identical ? "yes" : "no") << "\n";

    return 0;
}
