/**
 * config.cpp —— 练习模板 08 的核心逻辑（variant / optional / tuple）
 *
 * 4 个阶段的实现都写在这个文件里（阶段 4-1 的模板函数在 config.hpp 里）。
 * 骨架给的是占位实现：能编译、能运行、结果明显不对（返回 nullopt 或空表）。
 * 各阶段的任务、验收标准与自查方法见同目录《配置步骤.md》。
 *
 * 构建与运行（在模板目录下）：
 *     cmake --preset mingw-gdb
 *     cmake --build --preset mingw-gdb
 *     build\mingw\bin\app_cli.exe
 */
#include "config.hpp"

#include <algorithm>
#include <cctype>
#include <sstream>

namespace cfg {

/* ==================================================================
 * 阶段 1：optional
 * ================================================================== */

/* TODO（阶段 1-1）：解析整数。
 *
 * 提示：
 *   1. 整串必须都是数字（可以带一个正负号），因此不能直接用 std::stoi——
 *      它是前缀解析：stoi("12abc") 会得到 12 而不是报错；
 *   2. 用 std::from_chars（<charconv>）或者自己扫一遍字符；
 *   3. 空串、只有符号、带空格、带其它字符都返回 std::nullopt；
 *   4. 循环里判溢出也要小心：本模板的输入不会溢出，但值得想一想。
 *
 * 验收：parse_int("8080") 得 8080，parse_int("12abc") 得 nullopt。 */
std::optional<long long> parse_int(const std::string &text)
{
    (void)text;
    return std::nullopt;
}

/* TODO（阶段 1-2）：解析浮点数，规则同上（允许小数点与一个符号）。 */
std::optional<double> parse_double(const std::string &text)
{
    (void)text;
    return std::nullopt;
}

/* TODO（阶段 1-3）：解析布尔。
 *
 * 提示：先把输入转小写，再逐个比较 "true" / "false" / "1" / "0" / "yes" / "no" /
 *       "on" / "off"；其余一律 nullopt（"maybe" 也是 nullopt，不要当成 false）。 */
std::optional<bool> parse_bool(const std::string &text)
{
    (void)text;
    return std::nullopt;
}

/* 已给出：一张固定的小表，用于演示 lookup 与 value_or */
std::optional<std::string> lookup(const std::string &key)
{
    static const std::pair<const char *, const char *> table[] = {
        {"name", "demo"},
        {"port", "8080"},
        {"debug", "true"},
    };

    for (const auto &kv : table) {
        if (key == kv.first) {
            return std::string(kv.second);
        }
    }
    return std::nullopt;
}

/* ==================================================================
 * 阶段 2：variant 与 visit
 * ================================================================== */

/* 已给出：把值渲染成 "类型名 = 值" */
std::string dump_value(const Value &v)
{
    return std::visit(overloaded{
        [](std::monostate) { return std::string("none"); },
        [](bool b) { return std::string("bool = ") + (b ? "true" : "false"); },
        [](long long i) { return std::string("int = ") + std::to_string(i); },
        [](double d) { return std::string("double = ") + std::to_string(d); },
        [](const std::string &s) { return std::string("string = ") + s; },
    }, v);
}

/* TODO（阶段 2-1）：把一条配置存进 items_。
 *
 * 提示：
 *   1. 解析顺序是 bool -> int -> double -> 原样字符串：
 *      parse_bool("1") 与 parse_int("1") 都能成功，本模板规定先试 bool，
 *      因此 "1" 会被当成 true、8080 会被当成 int；
 *   2. 用 value_or 或者对 optional 做 if 判断都可以；
 *   3. 已存在同名 key 时覆盖旧值；
 *   4. items_ 是 vector<pair<string, Value>>，push_back 之前记得 emplace 的写法。 */
void Config::set(const std::string &key, const std::string &raw)
{
    (void)key;
    (void)raw;
    /* 占位实现：什么都没做 */
}

bool Config::has(const std::string &key) const
{
    return find(key) != nullptr;
}

/* TODO（阶段 2-2）：按 key 查找，找不到返回 nullptr。
 *
 * 提示：遍历 items_ 比较 first；返回值是 const Value *，
 *       因此返回 &it->second 一类地址即可。 */
const Value *Config::find(const std::string &key) const
{
    (void)key;
    return nullptr;
}

/* TODO（阶段 2-3）：按 key 升序返回全部条目。
 *
 * 提示：先复制一份 items_，再用 std::sort 与一个比较 pair::first 的 lambda。 */
std::vector<std::pair<std::string, Value>> Config::all() const
{
    return {};
}

/* ==================================================================
 * 阶段 3：tuple 与结构化绑定
 * ================================================================== */

/* TODO（阶段 3-1）：按行解析 "key = value"。
 *
 * 提示：
 *   1. 用 std::istringstream 配合 std::getline(stream, line) 逐行读；
 *   2. 跳过空行与 '#' 开头的行（可以先去掉行首空白再判断）；
 *   3. 用 line.find('=') 找等号，找不到就把 ok 置 false；
 *   4. key 与 value 两端都要去掉空白（自己写一个小函数，或者复用 std::string 的
 *      find_first_not_of / find_last_not_of）；
 *   5. 用 emplace_back(key, value, true) 放进结果里（tuple 支持这种就地构造）；
 *   6. 返回类型是 vector<tuple<string, Value, bool>>，用结构化绑定取用：
 *        for (const auto &[k, v, ok] : items) { ... }
 *
 * 验收：data/app.ini 解析出 5 条合法行。 */
std::vector<std::tuple<std::string, Value, bool>> parse_lines(const std::string &text)
{
    (void)text;
    return {};
}

/* TODO（阶段 3-2）：返回第一条合法的 (key, value)。
 *
 * 提示：调用 parse_lines 之后找到第一个 ok 为 true 的条目；
 *       也可以用 std::apply 把一个 tuple 展开成实参——本函数用不上，
 *       main_cli.cpp 里有一段用它演示。
 *       一条都没有时返回 std::make_pair(std::string(), Value{})。 */
std::pair<std::string, Value> first_item(const std::string &text)
{
    (void)text;
    return {std::string(), Value{}};
}

/* ==================================================================
 * 阶段 4：type_traits 约束
 * ================================================================== */

/* TODO（阶段 4-2）：类型名，用于阶段 4 的打印。
 *
 * 提示：用 std::visit 与 overloaded，返回 "int" / "bool" / "double" / "string" / "none"。 */
std::string type_name_of(const Value &v)
{
    (void)v;
    return "(TODO stage 4-2)";
}

} /* namespace cfg */
