/**
 * config.cpp —— INI 风格配置解析器的实现
 *
 * 没有界面代码，也不打印：解析结果与问题清单都交回调用方。
 * 命令行版与自测共用这一份实现。
 */
#include "config.hpp"

#include <algorithm>
#include <cerrno>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <utility>

namespace cfg {

namespace {

/* 值的渲染走 visit + if constexpr：分支按类型在编译期选定，
   新增一种类型时这里会立刻编不过，不必再维护一张类型表 */
std::string render_value(const Value &value)
{
    return std::visit([](const auto &item) -> std::string {
        using T = std::decay_t<decltype(item)>;
        std::ostringstream os;
        if constexpr (std::is_same_v<T, bool>) {
            os << (item ? "true" : "false");
        } else if constexpr (std::is_same_v<T, double>) {
            os << std::fixed << std::setprecision(2) << item;
        } else {
            os << item;
        }
        return os.str();
    }, value);
}

/* 自测的小工具：逐项记录，不打印 */
class Checker {
public:
    void check(bool ok, const std::string &what, const std::string &detail = std::string())
    {
        std::ostringstream line;
        const std::size_t number = result_.lines.size() + 1;
        if (ok) {
            ++result_.passed;
            line << "[通过] " << number << ". " << what;
        } else {
            ++result_.failed;
            line << "[失败] " << number << ". " << what;
            if (!detail.empty()) {
                line << "（" << detail << "）";
            }
        }
        result_.lines.push_back(line.str());
    }

    CheckResult take() { return std::move(result_); }

private:
    CheckResult result_;
};

/* 按 段 + 键 找到值，找不到返回 nullptr：自测里核对类型名时用 */
const Value *lookup(const Config &config, const std::string &section, const std::string &key)
{
    for (const Entry &entry : config.entries()) {
        if (entry.section == section && entry.key == key) {
            return &entry.value;
        }
    }
    return nullptr;
}

/* 把 optional 渲染成一行文本，失败时用来说清「期望什么、拿到什么」 */
template <typename T>
std::string show(const std::optional<T> &value)
{
    return value.has_value() ? to_text(Value{*value}) : std::string("没有");
}

}   /* namespace */

bool is_integer_value(const Value &value)
{
    return std::visit([](const auto &item) {
        using T = std::decay_t<decltype(item)>;
        return std::is_integral_v<T> && !std::is_same_v<T, bool>;
    }, value);
}

std::string type_name(const Value &value)
{
    return std::visit([](const auto &item) -> std::string {
        using T = std::decay_t<decltype(item)>;
        if constexpr (std::is_same_v<T, bool>) {
            return "bool";
        } else if constexpr (std::is_same_v<T, long long>) {
            return "long long";
        } else if constexpr (std::is_same_v<T, double>) {
            return "double";
        } else {
            return "string";
        }
    }, value);
}

std::string to_text(const Value &value)
{
    return render_value(value);
}

std::string_view trim(std::string_view text)
{
    std::size_t begin = 0;
    while (begin < text.size()
           && (text[begin] == ' ' || text[begin] == '\t' || text[begin] == '\r')) {
        ++begin;
    }
    std::size_t end = text.size();
    while (end > begin
           && (text[end - 1] == ' ' || text[end - 1] == '\t' || text[end - 1] == '\r')) {
        --end;
    }
    return text.substr(begin, end - begin);
}

std::string_view strip_comment(std::string_view text)
{
    bool in_quotes = false;
    for (std::size_t i = 0; i < text.size(); ++i) {
        const char ch = text[i];
        if (ch == '"') {
            in_quotes = !in_quotes;
            continue;
        }
        if (in_quotes) {
            continue;
        }
        /* 注释符前面要有空白，否则 `a#b` 这种值会被截断 */
        if ((ch == '#' || ch == ';') && i > 0 && (text[i - 1] == ' ' || text[i - 1] == '\t')) {
            return text.substr(0, i);
        }
    }
    return text;
}

Value parse_literal(std::string_view text)
{
    const std::string body(text);
    if (body == "true" || body == "false") {
        return Value{body == "true"};
    }
    if (body.size() >= 2 && body.front() == '"' && body.back() == '"') {
        return Value{body.substr(1, body.size() - 2)};
    }

    /* 整体读下来才算数：end 必须走到末尾，`1024x768`、`12abc` 因此落进字符串 */
    const char *const begin = body.c_str();
    char *end = nullptr;
    errno = 0;
    const long long as_integer = std::strtoll(begin, &end, 10);
    if (end != begin && end == begin + body.size() && errno != ERANGE) {
        return Value{as_integer};
    }

    errno = 0;
    end = nullptr;
    const double as_double = std::strtod(begin, &end);
    if (end != begin && end == begin + body.size() && errno != ERANGE) {
        return Value{as_double};
    }

    return Value{body};
}

void Config::parse(std::string_view text)
{
    std::istringstream stream{std::string(text)};
    std::string raw_line;
    std::string section;
    std::size_t line_number = 0;

    while (std::getline(stream, raw_line)) {
        ++line_number;
        const std::string_view line = trim(raw_line);
        if (line.empty() || line.front() == '#' || line.front() == ';') {
            continue;                   /* 空行与整行注释 */
        }

        if (line.front() == '[') {
            const std::size_t close = line.find(']');
            if (close == std::string_view::npos) {
                errors_.push_back({line_number, "段名没有闭合的 ]", std::string(line)});
                continue;
            }
            const std::string_view name = trim(line.substr(1, close - 1));
            if (name.empty()) {
                errors_.push_back({line_number, "段名为空", std::string(line)});
                continue;
            }
            section = std::string(name);
            if (std::find(sections_.begin(), sections_.end(), section) == sections_.end()) {
                sections_.push_back(section);
            }
            continue;
        }

        const std::size_t equals = line.find('=');
        if (equals == std::string_view::npos) {
            errors_.push_back({line_number, "既不是 [段]，也没有 = 号", std::string(line)});
            continue;
        }
        if (section.empty()) {
            errors_.push_back({line_number, "键出现在任何 [段] 之前", std::string(line)});
            continue;
        }
        const std::string_view key = trim(line.substr(0, equals));
        if (key.empty()) {
            errors_.push_back({line_number, "= 左边没有键名", std::string(line)});
            continue;
        }
        put(section, std::string(key), parse_literal(trim(strip_comment(line.substr(equals + 1)))));
    }
}

bool Config::load_file(const std::string &path, std::string &error)
{
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        error = "打不开配置文件：" + path;
        return false;
    }
    std::ostringstream buffer;
    buffer << input.rdbuf();
    parse(buffer.str());
    error.clear();
    return true;
}

const Value *Config::find(const std::string &section, const std::string &key) const
{
    /* 同一个键写两次时 put() 会原地覆盖，因此这里至多命中一条 */
    for (const Entry &entry : entries_) {
        if (entry.section == section && entry.key == key) {
            return &entry.value;
        }
    }
    return nullptr;
}

void Config::put(const std::string &section, const std::string &key, Value value)
{
    for (Entry &entry : entries_) {
        if (entry.section == section && entry.key == key) {
            entry.value = std::move(value);     /* 后写的说了算 */
            return;
        }
    }
    entries_.push_back(Entry{section, key, std::move(value)});
}

std::vector<FlatEntry> Config::flatten() const
{
    std::vector<FlatEntry> flat;
    flat.reserve(entries_.size());
    for (const Entry &entry : entries_) {
        flat.emplace_back(entry.section, entry.key, to_text(entry.value), type_name(entry.value));
    }
    return flat;
}

std::size_t Config::count_in(const std::string &section) const
{
    return static_cast<std::size_t>(std::count_if(
        entries_.begin(), entries_.end(),
        [&section](const Entry &entry) { return entry.section == section; }));
}

std::string CheckResult::summary() const
{
    std::ostringstream os;
    os << lines.size() << " 项中 " << passed << " 项通过";
    if (failed == 0) {
        os << "，全部通过";
    } else {
        os << "，" << failed << " 项失败";
    }
    return os.str();
}

CheckResult run_self_tests()
{
    Checker c;

    const std::string valid_text =
        "# 应用配置\n"
        "; 两种注释都支持，空行同样跳过\n"
        "\n"
        "[app]\n"
        "name = text-analyzer\n"
        "debug = false\n"
        "ratio = 0.75\n"
        "retries = 3\n"
        "window = 1024x768\n"
        "note = \"a # b\"   # 引号里的井号不是注释\n"
        "\n"
        "[run]\n"
        "top = 8\n"
        "top = 12\n"
        "timeout = 30  ; 秒\n"
        "empty =\n";

    Config config;
    config.parse(valid_text);

    /* 1. 段名按首次出现顺序 */
    const std::vector<std::string> &sections = config.sections();
    c.check(sections.size() == 2 && sections[0] == "app" && sections[1] == "run",
            "解析出 app、run 两个段，顺序与文件一致",
            "段数 " + std::to_string(sections.size()));

    /* 2. 摊平成四元组，用结构化绑定解包 */
    const std::vector<FlatEntry> flat = config.flatten();
    bool flat_ok = flat.size() == 9;
    if (flat_ok) {
        const auto &[first_section, first_key, first_text, first_type] = flat.front();
        const auto &[last_section, last_key, last_text, last_type] = flat.back();
        flat_ok = first_section == "app" && first_key == "name" && first_text == "text-analyzer"
                  && first_type == "string"
                  && last_section == "run" && last_key == "empty" && last_text.empty()
                  && last_type == "string";
    }
    c.check(flat_ok, "flatten() 摊出 9 条 (段, 键, 值, 类型) 四元组，结构化绑定取得出首尾两条",
            "条数 " + std::to_string(flat.size()));

    /* 3–6. 四种类型各认一次 */
    const std::optional<long long> retries = config.get<long long>("app", "retries");
    c.check(retries.has_value() && *retries == 3, "retries = 3 认成整数",
            "取到 " + show(retries));

    const std::optional<double> ratio = config.get<double>("app", "ratio");
    c.check(ratio.has_value() && *ratio == 0.75, "ratio = 0.75 认成浮点",
            "取到 " + show(ratio));

    const std::optional<bool> debug = config.get<bool>("app", "debug");
    c.check(debug.has_value() && !*debug, "debug = false 认成布尔",
            "取到 " + show(debug));

    const std::optional<std::string> window = config.get<std::string>("app", "window");
    c.check(window.has_value() && *window == "1024x768",
            "window = 1024x768 整体读不成数，落进字符串", "取到 " + show(window));

    /* 7. 双引号围起来的值去掉引号，里面的 # 不当注释 */
    const std::optional<std::string> note = config.get<std::string>("app", "note");
    c.check(note.has_value() && *note == "a # b",
            "引号里的 # 被保留，引号本身去掉", "取到 " + show(note));

    /* 8. 行尾注释被剥掉 */
    const std::optional<long long> timeout = config.get<long long>("run", "timeout");
    c.check(timeout.has_value() && *timeout == 30,
            "timeout 后面的 `; 秒` 是注释，没有进入值", "取到 " + show(timeout));

    /* 9. 空值是空字符串，不是「没有」 */
    const std::optional<std::string> empty_value = config.get<std::string>("run", "empty");
    c.check(empty_value.has_value() && empty_value->empty(),
            "empty = 取到有值的空字符串，与缺键不是一回事");

    /* 10–12. 三种「取不到」 */
    const std::optional<long long> missing_key = config.get<long long>("app", "missing");
    c.check(!missing_key.has_value(), "缺键返回 nullopt");

    const std::optional<long long> missing_section = config.get<long long>("nope", "top");
    c.check(!missing_section.has_value(), "缺段返回 nullopt");

    const std::optional<std::string> wrong_type = config.get<std::string>("run", "top");
    c.check(!wrong_type.has_value(), "整数键按字符串取返回 nullopt（类型不匹配）",
            "取到 " + show(wrong_type));

    /* 13. 同一个键写两次，后写的说了算；整数可以放宽成浮点 */
    const std::optional<long long> top = config.get<long long>("run", "top");
    const std::optional<double> top_as_double = config.get<double>("run", "top");
    c.check(top.has_value() && *top == 12 && top_as_double.has_value() && *top_as_double == 12.0,
            "重复键以最后一次为准，且整数能放宽成浮点",
            "top = " + show(top) + "，按 double 取 = " + show(top_as_double));

    /* 14. 放宽是单向的：浮点取不成整数 */
    const std::optional<long long> ratio_as_integer = config.get<long long>("app", "ratio");
    c.check(!ratio_as_integer.has_value(), "浮点键按整数取返回 nullopt（放宽是单向的）");

    /* 15. get_or 给默认值 */
    const std::string author = config.get_or<std::string>("app", "author", "(未填写)");
    c.check(author == "(未填写)", "get_or 在缺键时给出默认值", "取到 " + author);

    /* 16. 类型特征判定与取值一致 */
    const Value *retries_value = lookup(config, "app", "retries");
    const Value *debug_value = lookup(config, "app", "debug");
    const Value *ratio_value = lookup(config, "app", "ratio");
    c.check(retries_value != nullptr && debug_value != nullptr && ratio_value != nullptr
                && is_integer_value(*retries_value)
                && !is_integer_value(*debug_value)
                && type_name(*ratio_value) == "double",
            "is_integral_v 判定：整数算数、bool 不算；type_name 与值一致",
            "retries 判为 " + std::string(is_integer_value(*retries_value) ? "整数" : "非整数")
                + "，ratio 的类型名 " + type_name(*ratio_value));

    /* 17–18. 四类错误各记一行，行号准确；出错行之后照常解析 */
    const std::string broken_text =
        "orphan = 1\n"
        "[app]\n"
        "name = demo\n"
        "missing_equals\n"
        "= 5\n"
        "[unclosed\n"
        "ok = 1\n";

    Config broken;
    broken.parse(broken_text);
    const std::vector<ParseError> &errors = broken.errors();
    bool lines_ok = errors.size() == 4;
    if (lines_ok) {
        lines_ok = errors[0].line == 1 && errors[0].message == "键出现在任何 [段] 之前"
                   && errors[1].line == 4 && errors[1].message == "既不是 [段]，也没有 = 号"
                   && errors[2].line == 5 && errors[2].message == "= 左边没有键名"
                   && errors[3].line == 6 && errors[3].message == "段名没有闭合的 ]";
    }
    c.check(lines_ok, "四处笔误各记一行，行号为 1、4、5、6",
            "错误 " + std::to_string(errors.size()) + " 条"
                + (errors.empty() ? std::string()
                                  : "，首条在第 " + std::to_string(errors[0].line) + " 行"));

    const std::optional<std::string> name = broken.get<std::string>("app", "name");
    const std::optional<long long> ok = broken.get<long long>("app", "ok");
    c.check(name.has_value() && *name == "demo" && ok.has_value() && *ok == 1,
            "出错行前后的合法行照常收下", "条目 " + std::to_string(broken.size()) + " 条");

    /* 19. CRLF 行尾一样解析 */
    Config windows;
    windows.parse("[win]\r\nsize = 800\r\nflag = true\r\n");
    const std::optional<long long> size = windows.get<long long>("win", "size");
    const std::optional<bool> flag = windows.get<bool>("win", "flag");
    c.check(windows.errors().empty() && size.has_value() && *size == 800
                && flag.has_value() && *flag,
            "CRLF 行尾解析结果相同，行尾的 \\r 被去掉");

    /* 20. 空文本：没有条目，也没有错误 */
    Config nothing;
    nothing.parse("");
    c.check(nothing.size() == 0 && nothing.errors().empty() && nothing.sections().empty(),
            "空文本解析出 0 条、0 错");

    return c.take();
}

}   /* namespace cfg */
