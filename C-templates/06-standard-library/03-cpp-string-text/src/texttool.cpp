/**
 * texttool.cpp —— 练习模板 03 的核心逻辑（std::string 与 string_view）
 *
 * 4 个阶段的实现都写在这个文件里。骨架给的是占位实现：
 * 能编译、能运行、结果明显不对。
 * 各阶段的任务、验收标准与自查方法见同目录《配置步骤.md》。
 *
 * 构建与运行（在模板目录下）：
 *     cmake --preset mingw-gdb
 *     cmake --build --preset mingw-gdb
 *     build\mingw\bin\app_cli.exe
 */
#include "texttool.hpp"

#include <cctype>
#include <stdexcept>

namespace tt {

/* ==================================================================
 * 阶段 1：构造与容量
 * ================================================================== */

/* TODO（阶段 1-1）：量出 size、capacity，并判断缓冲区是否在对象内部。
 *
 * 提示：
 *   1. s.size() 与 s.capacity() 直接取；
 *   2. 判断「在对象内部」的办法是把三个地址转成 const char *：
 *        const char *data = s.data();
 *        const char *self = reinterpret_cast<const char *>(&s);
 *        落在 [self, self + sizeof(s)) 区间内就是内部缓冲（短串优化）；
 *   3. 不要打印地址本身：每次运行都不一样，验收里只比对布尔判据。
 *
 * 验收：《配置步骤.md》阶段 1 的三行输出（容量是实现定义，以本机 libstdc++ 的值为准）。 */
Capacity probe(const std::string &s)
{
    (void)s;

    Capacity c;
    return c;   /* 占位实现：全 0，一眼就能看出还没实现 */
}

/* ==================================================================
 * 阶段 2：修改与查找
 * ================================================================== */

/* TODO（阶段 2-1）：去掉两端的空白。
 *
 * 提示：find_first_not_of(" \t\r\n") 找第一个不是空白的字符，
 *       find_last_not_of 找最后一个；两者都返回 npos 时说明整串都是空白，
 *       返回空串。最后用 substr 取中间那一段。 */
std::string trim(const std::string &s)
{
    (void)s;
    return "(TODO stage 2-1)";
}

/* TODO（阶段 2-2）：整体转小写。
 *
 * 提示：形参是按值传进来的，可以直接在原地改：for (char &c : s) c = ...；
 *       传给 std::tolower 之前把 char 转成 unsigned char，避免负值带来的未定义行为。 */
std::string to_lower(std::string s)
{
    (void)s;
    return "(TODO stage 2-2)";
}

/* TODO（阶段 2-3）：按分隔符切分。
 *
 * 提示：用 find 找分隔符、substr 取段、从 pos + 1 继续；
 *       循环结束条件是 find 返回 npos，此时把最后一段也 push 进去。 */
std::vector<std::string> split(const std::string &s, char sep)
{
    (void)s;
    (void)sep;
    return {};
}

/* TODO（阶段 2-4）：数 needle 在 hay 里出现的次数（不重叠）。
 *
 * 提示：循环里用 hay.find(needle, pos)，找到之后 pos 要跳过整个 needle，
 *       否则 "aaa" 里找 "aa" 会数出两次。找不到就返回当前计数。 */
long count_occurrences(const std::string &hay, const std::string &needle)
{
    (void)hay;
    (void)needle;
    return -1;
}

/* TODO（阶段 2-5）：把所有的 from 换成 to。
 *
 * 提示：循环 find + replace，注意 replace 之后 pos 要往后跳 to.size()，
 *       否则 to 里含有 from 时会一直替换下去。 */
std::string replace_all(const std::string &s, const std::string &from, const std::string &to)
{
    (void)s;
    (void)from;
    (void)to;
    return "(TODO stage 2-5)";
}

/* ==================================================================
 * 阶段 3：string_view
 * ================================================================== */

/* TODO（阶段 3-1）：与 split 一样切分，但返回视图，不复制字符。
 *
 * 提示：std::string_view 的 find/substr 与 std::string 用法相同；
 *       因为不复制，返回的视图全部指向传进来的那段字符，调用方要保证它活着。
 *       顺手想一想：如果把这里改成返回 std::vector<std::string_view> 的函数
 *       接收一个临时 std::string，会发生什么。 */
std::vector<std::string_view> split_view(std::string_view s, char sep)
{
    (void)s;
    (void)sep;
    return {};
}

/* TODO（阶段 3-2）：视图版的 trim。
 *
 * 提示：先用 find_first_not_of 与 find_last_not_of 定出区间，再 substr；
 *       也可以先 remove_prefix 再去尾部，两种写法的结果要一致。 */
std::string_view trim_view(std::string_view s)
{
    (void)s;
    return "(TODO stage 3-2)";
}

/* ---------------- 已给出：两种切分各复制了多少字符 ---------------- */

long copied_chars(const std::vector<std::string> &tokens)
{
    long total = 0;
    for (const std::string &t : tokens) {
        total += static_cast<long>(t.size());
    }
    return total;
}

long copied_chars(const std::vector<std::string_view> &tokens)
{
    long total = 0;
    for (std::string_view v : tokens) {
        /* 视图不拥有字符，这里记的是「为切分而新复制的字符数」 */
        (void)v;
        total += 0;
    }
    return total;
}

/* ==================================================================
 * 阶段 4：数字与 UTF-8
 * ================================================================== */

/* TODO（阶段 4-1）：把五种数字互转的结果填进 Numbers。
 *
 * 提示：
 *   1. std::stoi("42")、std::stod("3.5")、std::to_string(42)；
 *   2. std::stoi("7x") 不会抛异常：它解析出前缀 7 就停下，这是常见误解；
 *   3. std::stoi("hello") 抛 std::invalid_argument，用 try/catch 接住并把
 *      bad_caught 置 true；漏掉 catch 会让程序直接退出。
 *
 * 验收：《配置步骤.md》阶段 4 的前五行。 */
Numbers probe_numbers()
{
    Numbers n;
    return n;   /* 占位实现：全 0，一眼就能看出还没实现 */
}

/* TODO（阶段 4-2）：数字节数与非续接字节的个数。
 *
 * 提示：
 *   1. bytes 就是 s.size()；
 *   2. UTF-8 里一个码点的首字节不是 0b10xxxxxx，续接字节才是；
 *      所以数出 (unsigned char)c 与 0xC0 按位与之后不等于 0x80 的字节个数，
 *      就是码点数；
 *   3. 这不是完整的 UTF-8 校验，只是计数；非法序列怎么处理见
 *      《06-标准库/B-02-std-string 与 string_view.md》第 8 节。
 *
 * 验收：《配置步骤.md》阶段 4 的最后两行：9 字节、5 个码点。 */
Utf8 count_utf8(const std::string &s)
{
    Utf8 u;
    u.bytes = static_cast<long>(s.size());
    u.codepoints = -1;   /* 占位值 */
    return u;
}

} /* namespace tt */
