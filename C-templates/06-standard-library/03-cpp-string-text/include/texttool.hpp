/**
 * texttool.hpp —— 练习模板 03 的核心接口（C++）
 *
 * 接口已经定好，src/main_cli.cpp 按 4 个阶段调用它们。
 * 你要做的是在 src/texttool.cpp 里把标了 TODO 的函数实现出来。
 *
 * 四个阶段的对应关系：
 *     阶段 1   probe                     std::string 的构造与容量
 *     阶段 2   trim、to_lower、split、
 *              count_occurrences、
 *              replace_all               修改与查找
 *     阶段 3   split_view、trim_view     string_view 的零拷贝切分
 *     阶段 4   probe_numbers、count_utf8 数字互转与 UTF-8 字节处理
 */
#ifndef TEXTTOOL_HPP
#define TEXTTOOL_HPP

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace tt {

/* ---------- 阶段 1：构造与容量 ---------- */

struct Capacity {
    std::size_t size = 0;
    std::size_t capacity = 0;
    bool        inside_object = false;  /* data() 是否落在 string 对象自身的存储里 */
};

/* 量一个 string 的长度、容量，以及它的缓冲区是否就在对象内部（短串优化）。
 * 判断办法：把 data() 的地址与对象自己的地址区间比一比。 */
Capacity probe(const std::string &s);

/* ---------- 阶段 2：修改与查找 ---------- */

/* 去掉两端的空白（空格、制表符、换行） */
std::string trim(const std::string &s);

/* 整体转小写（按字节，用 std::tolower） */
std::string to_lower(std::string s);

/* 按单一分隔符切分，返回每一段；空段要不要保留由你决定，本模板的验收里没有空段 */
std::vector<std::string> split(const std::string &s, char sep);

/* needle 在 hay 里出现的次数（不重叠）；needle 为空时返回 0 */
long count_occurrences(const std::string &hay, const std::string &needle);

/* 把所有的 from 换成 to；from 为空时原样返回 */
std::string replace_all(const std::string &s, const std::string &from, const std::string &to);

/* ---------- 阶段 3：string_view ---------- */

/* 与 split 同样的切分，但返回的是指向原串的视图，一个字符也不复制。
 * 注意：视图的生命期不能超过原串，原串一旦被修改或销毁，视图就悬垂了。 */
std::vector<std::string_view> split_view(std::string_view s, char sep);

/* 视图版的 trim */
std::string_view trim_view(std::string_view s);

/* 已给出：两种切分各自「复制了多少个字符」，用来对照零拷贝 */
long copied_chars(const std::vector<std::string> &tokens);
long copied_chars(const std::vector<std::string_view> &tokens);

/* ---------- 阶段 4：数字与 UTF-8 ---------- */

struct Numbers {
    int         i = 0;            /* stoi("42") */
    double      d = 0.0;          /* stod("3.5") */
    std::string back;             /* to_string(42) */
    int         prefix = 0;       /* stoi("7x")：前缀解析，不抛异常 */
    bool        bad_caught = false;/* stoi("hello") 抛出的 invalid_argument 是否被接住 */
};

Numbers probe_numbers();

struct Utf8 {
    long bytes = 0;               /* 字节数 */
    long codepoints = 0;          /* 码点数：非续接字节（0b10xxxxxx）的个数 */
};

Utf8 count_utf8(const std::string &s);

} /* namespace tt */

#endif /* TEXTTOOL_HPP */
