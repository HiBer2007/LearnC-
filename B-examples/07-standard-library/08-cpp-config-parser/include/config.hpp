/**
 * config.hpp —— INI 风格配置解析器，不依赖任何界面
 *
 * 三件事在这里定型：
 *   · 一个配置值用 std::variant<bool, long long, double, std::string> 表示，
 *     类型是值本身的一部分，不必另存一个「类型标记」再手工比对；
 *   · 「这个键可能没有」用 std::optional 表达，取不到就是 nullopt，
 *     不靠特殊值（0、空串、-1）去暗示失败；
 *   · 取值时用 <type_traits> 的类型特征做约束：只有那四种类型能取，
 *     整数可以放宽成浮点，反过来不行。
 *
 * 解析器只收集问题、不抛异常：出错的行记下行号、说明与该行原文，
 * 其余行照常收下，一份文件里有一处笔误不会让整份配置作废。
 */
#ifndef CONFIG_HPP
#define CONFIG_HPP

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <variant>
#include <vector>

namespace cfg {

/** 一个配置值：四种类型之一 */
using Value = std::variant<bool, long long, double, std::string>;

/** 摊平后的四元组 (段, 键, 值文本, 类型名)，配合结构化绑定遍历 */
using FlatEntry = std::tuple<std::string, std::string, std::string, std::string>;

/** 一条配置：段、键、值 */
struct Entry {
    std::string section;
    std::string key;
    Value value;
};

/** 解析时记下的一处问题 */
struct ParseError {
    std::size_t line = 0;       /**< 行号，从 1 起 */
    std::string message;        /**< 说明 */
    std::string text;           /**< 出错那一行的原文 */
};

namespace detail {

/** 允许出现在配置里的四种类型，其余类型在 get<T>() 里被 static_assert 挡下 */
template <typename T>
inline constexpr bool is_config_value_v =
    std::is_same_v<std::remove_cv_t<T>, bool>
    || std::is_same_v<std::remove_cv_t<T>, long long>
    || std::is_same_v<std::remove_cv_t<T>, double>
    || std::is_same_v<std::remove_cv_t<T>, std::string>;

}   /* namespace detail */

/** 值里装的是不是整数：判定走类型特征，bool 在类型特征上也是整数，但配置里的 true 不是数 */
bool is_integer_value(const Value &value);

/** 值的类型名："bool" / "long long" / "double" / "string" */
std::string type_name(const Value &value);

/** 值渲染成文本：布尔写 true/false，浮点保留两位小数 */
std::string to_text(const Value &value);

/** 去掉首尾空白 */
std::string_view trim(std::string_view text);

/** 去掉行尾注释：`#` 或 `;` 前面有一个空白、且不在双引号里时才算注释 */
std::string_view strip_comment(std::string_view text);

/** 把字面量文本翻成值：true/false → bool，整体读成整数 → long long，
 *  整体读成小数 → double，其余（含双引号围起来的）→ string */
Value parse_literal(std::string_view text);

class Config {
public:
    /** 逐行解析。出错的行走 errors()，其余行照常收下 */
    void parse(std::string_view text);

    /** 从文件读入再解析；打不开时返回 false 并把原因写进 error */
    bool load_file(const std::string &path, std::string &error);

    /** 取一个值。缺段、缺键、类型不匹配都返回 nullopt */
    template <typename T>
    std::optional<T> get(const std::string &section, const std::string &key) const;

    /** 取一个值，没有就用 fallback */
    template <typename T>
    T get_or(const std::string &section, const std::string &key, T fallback) const;

    /** 段名，按首次出现顺序 */
    const std::vector<std::string> &sections() const { return sections_; }

    /** 全部条目，按出现顺序 */
    const std::vector<Entry> &entries() const { return entries_; }

    /** 摊平成 (段, 键, 值文本, 类型名) 四元组 */
    std::vector<FlatEntry> flatten() const;

    /** 某个段下的条目数 */
    std::size_t count_in(const std::string &section) const;

    /** 解析时记下的问题 */
    const std::vector<ParseError> &errors() const { return errors_; }

    /** 条目数 */
    std::size_t size() const { return entries_.size(); }

private:
    const Value *find(const std::string &section, const std::string &key) const;
    void put(const std::string &section, const std::string &key, Value value);

    std::vector<std::string> sections_;
    std::vector<Entry> entries_;
    std::vector<ParseError> errors_;
};

template <typename T>
std::optional<T> Config::get(const std::string &section, const std::string &key) const
{
    static_assert(detail::is_config_value_v<T>,
                  "配置值只有 bool / long long / double / std::string 四种，别的类型取不出来");

    const Value *found = find(section, key);
    if (found == nullptr) {
        return std::nullopt;            /* 缺段或缺键 */
    }
    if (const T *exact = std::get_if<T>(found)) {
        return *exact;                  /* 类型正好对上 */
    }
    if constexpr (std::is_floating_point_v<T>) {
        /* 整数放宽成浮点：8 可以当 8.0 用；反过来不行，0.75 取 long long 是 nullopt */
        if (const long long *as_integer = std::get_if<long long>(found)) {
            return static_cast<T>(*as_integer);
        }
    }
    return std::nullopt;                /* 类型不匹配 */
}

template <typename T>
T Config::get_or(const std::string &section, const std::string &key, T fallback) const
{
    const std::optional<T> found = get<T>(section, key);
    return found.has_value() ? *found : fallback;
}

/** 自测结果。不打印，交给调用方决定怎么显示 */
struct CheckResult {
    std::vector<std::string> lines;     /**< 每项一行，已含「[通过] N. …」 */
    std::size_t passed = 0;
    std::size_t failed = 0;

    bool all_passed() const { return failed == 0; }
    std::string summary() const;        /**< "20 项中 20 项通过，全部通过" */
};

/** 逐项核对解析、类型判定、取值约束与行号报错 */
CheckResult run_self_tests();

}   /* namespace cfg */

#endif /* CONFIG_HPP */
