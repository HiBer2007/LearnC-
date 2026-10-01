/**
 * config.hpp —— 练习模板 08 的核心接口（C++）
 *
 * 接口已经定好，src/main_cli.cpp 按 4 个阶段调用它们。
 * 你要做的是在 src/config.cpp（以及本文件末尾的模板函数）里把标了 TODO 的部分实现出来。
 *
 * 四个阶段的对应关系：
 *     阶段 1   parse_int、parse_double、parse_bool、lookup   optional
 *     阶段 2   Config（set、has、find、all、dump）            variant 与 visit
 *     阶段 3   parse_lines、first_item                        tuple 与结构化绑定
 *     阶段 4   get_as、type_name_of                           type_traits 约束
 */
#ifndef CONFIG_HPP
#define CONFIG_HPP

#include <optional>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace cfg {

/* ==================================================================
 * 阶段 1：optional
 * ================================================================== */

/* 解析一个整数。整串必须是合法数字，多余字符算失败（与 std::stoi 的前缀解析不同）。
 * 失败返回 std::nullopt。 */
std::optional<long long> parse_int(const std::string &text);

/* 解析一个浮点数，规则同上 */
std::optional<double> parse_double(const std::string &text);

/* 解析布尔：true/false/1/0/yes/no/on/off（不区分大小写），其余返回 nullopt */
std::optional<bool> parse_bool(const std::string &text);

/* 按 key 取值，找不到返回 nullopt（本模板用它演示 value_or 给默认值） */
std::optional<std::string> lookup(const std::string &key);

/* ==================================================================
 * 阶段 2：variant 与 visit
 * ================================================================== */

/* 配置值的类型：monostate 表示「没有值」，其余是四种可用的类型 */
using Value = std::variant<std::monostate, bool, long long, double, std::string>;

/* 已给出：visit 用的 overloaded 小工具（C++17 写法） */
template <class... Ts>
struct overloaded : Ts... {
    using Ts::operator()...;
};
template <class... Ts>
overloaded(Ts...) -> overloaded<Ts...>;

/* 把值渲染成 "类型名 = 值" 的一行，类型名取 int / bool / double / string / none */
std::string dump_value(const Value &v);

class Config {
public:
    /* TODO（阶段 2-1）：把一条配置存进来：key 原样，raw 按下面顺序尝试解析：
     *   parse_bool -> parse_int -> parse_double -> 原样字符串
     * 同一个 key 重复 set 时覆盖旧值（保持第一次出现的位置也行，验收里按 key 排序）。 */
    void set(const std::string &key, const std::string &raw);

    bool has(const std::string &key) const;

    /* TODO（阶段 2-2）：找到就返回指向该值的指针，找不到返回 nullptr。
     * 提示：用 std::get_if 之前要先确认类型，或者直接遍历 items_ 比较 first。 */
    const Value *find(const std::string &key) const;

    /* TODO（阶段 2-3）：按 key 升序返回全部条目 */
    std::vector<std::pair<std::string, Value>> all() const;

    std::size_t size() const { return items_.size(); }

private:
    std::vector<std::pair<std::string, Value>> items_;
};

/* ==================================================================
 * 阶段 3：tuple 与结构化绑定
 * ================================================================== */

/* 解析一段 "key = value" 形式的文本，每行一条，'#' 开头与空行跳过。
 * 返回 (key, value, ok)：ok 为 false 表示这一行不合法（缺等号、key 为空等），
 * 此时 key 里放原始行、value 放 monostate。 */
std::vector<std::tuple<std::string, Value, bool>> parse_lines(const std::string &text);

/* 返回第一条合法的 (key, value)；一条都没有时返回 ("", monostate{}) */
std::pair<std::string, Value> first_item(const std::string &text);

/* ==================================================================
 * 阶段 4：type_traits 约束
 * ================================================================== */

/* 类型名，用于阶段 4 的打印：int / bool / double / string / none */
std::string type_name_of(const Value &v);

/* TODO（阶段 4-1）：只接受算术类型的取值函数。
 *
 * 要求：
 *   1. 用 std::enable_if_t 与 std::is_arithmetic_v 把 T 限制在算术类型上
 *      （模板参数写成 template <class T, class = std::enable_if_t<std::is_arithmetic_v<T>>>）；
 *   2. key 对应的值类型与 T 相符时返回转换后的值：
 *      long long 与 double 之间可以互转，bool 与整数之间也允许；
 *      类型不符（例如要 int 但存的是字符串）时返回 fallback；
 *   3. key 不存在时返回 fallback；
 *   4. 这个函数是模板，因此实现要写在本文件里（不能放进 config.cpp）。
 */
template <class T, class = std::enable_if_t<std::is_arithmetic_v<T>>>
T get_as(const Config &c, const std::string &key, T fallback)
{
    (void)c;
    (void)key;
    return fallback;   /* 占位实现：一律返回默认值 */
}

} /* namespace cfg */

#endif /* CONFIG_HPP */
