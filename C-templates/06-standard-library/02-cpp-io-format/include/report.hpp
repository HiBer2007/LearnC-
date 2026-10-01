/**
 * report.hpp —— 练习模板 02 的核心接口（C++）
 *
 * 接口已经定好，src/main_cli.cpp 按 4 个阶段调用它们。
 * 你要做的是在 src/report.cpp 里把标了 TODO 的函数实现出来。
 *
 * 四个阶段与函数的对应关系：
 *     阶段 1   read_all、count_lines        <fstream> 与流的状态
 *     阶段 2   collect_words、sort_entries  <sstream> 分词，容器与排序
 *     阶段 3   format_report                <iomanip> 格式化
 *     阶段 4   write_file                   写报表文件
 *
 * 报表格式与 01-c-stdlib-toolbox（C 版）完全一致，两份程序的输出可以逐行对照。
 */
#ifndef REPORT_HPP
#define REPORT_HPP

#include <string>
#include <vector>

namespace rep {

/* 报表里给出前多少名 */
constexpr int kTopN = 10;

/* 一个词与它的次数 */
struct Entry {
    std::string word;
    long        count = 0;
};

/* 一次分析的全部结果 */
struct Stats {
    std::string        path;
    long               bytes  = 0;
    long               lines  = 0;
    long               words  = 0;   /* 词的总个数（含重复） */
    long               unique = 0;   /* 不同词的个数 */
    std::vector<Entry> entries;      /* 排好序的词表，长度为 unique */
};

/* ---------- 阶段 1：读取与数行 ---------- */

/* 用 std::ifstream 把整个文件读进 text。
 * 失败时返回 false，把原因（含 std::strerror(errno)）写进 err。 */
bool read_all(const std::string &path, std::string &text, std::string &err);

/* 数行数：'\n' 的个数；最后一个字节不是 '\n' 且文本非空时再加一。 */
long count_lines(const std::string &text);

/* ---------- 阶段 2：分词与排序 ---------- */

/* 统计词频，结果追加进 entries。
 * 词的规则：连续的字母（std::isalpha）算一个词，统一转小写，
 * 超过 63 个字符的部分截断；其余字符一律当分隔符。
 * words 得到词的总个数（含重复）。 */
void collect_words(const std::string &text, std::vector<Entry> &entries, long &words);

/* 次数降序；次数相同时按字典序升序。 */
void sort_entries(std::vector<Entry> &entries);

/* ---------- 阶段 3：格式化 ---------- */

/* 用 <iomanip> 的操纵器把报表拼成字符串，格式与 01 的 ts_write_report 一致。 */
std::string format_report(const Stats &stats);

/* ---------- 阶段 4：写文件 ---------- */

/* 把内容写成文件，失败时返回 false 并写 err。 */
bool write_file(const std::string &path, const std::string &text, std::string &err);

/* ---------- 已给出：把阶段 1 到阶段 3 串起来 ---------- */

bool analyze(const std::string &path, Stats &stats, std::string &err);

} /* namespace rep */

#endif /* REPORT_HPP */
