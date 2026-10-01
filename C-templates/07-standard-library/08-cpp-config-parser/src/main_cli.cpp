/**
 * main_cli.cpp —— 练习模板 08 的命令行验收程序（C++）
 *
 * 这个文件**不需要改**：它按 4 个阶段调用 config.cpp 里的函数，
 * 把结果打印成《配置步骤.md》里的验收输出。你的实现写在 src/config.cpp 与
 * include/config.hpp 的模板函数里。
 *
 * 构建与运行（**在模板目录下**执行，数据文件是相对路径 data/app.ini）：
 *     cmake --preset mingw-gdb
 *     cmake --build --preset mingw-gdb
 *     build\mingw\bin\app_cli.exe
 *
 * 阶段的划分：
 *     阶段 1  optional：解析失败怎么表达
 *     阶段 2  variant：配置值的多选一与 visit
 *     阶段 3  tuple 与结构化绑定
 *     阶段 4  type_traits 约束：只接受算术类型的取值函数
 */
#include <fstream>
#include <iomanip>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#include "config.hpp"

namespace {

std::string read_file(const std::string &path)
{
    std::ifstream in(path, std::ios::binary);
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

std::string trim(const std::string &s)
{
    const std::size_t first = s.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return std::string();
    }
    const std::size_t last = s.find_last_not_of(" \t\r\n");
    return s.substr(first, last - first + 1);
}

/* 已给出：把 ini 文本拆成 (key, raw value)，只做拆分不做类型判断 */
std::vector<std::pair<std::string, std::string>> split_ini(const std::string &text)
{
    std::vector<std::pair<std::string, std::string>> out;
    std::istringstream in(text);
    std::string      line;

    while (std::getline(in, line)) {
        const std::size_t hash = line.find('#');
        if (hash != std::string::npos) {
            line = line.substr(0, hash);
        }
        const std::size_t eq = line.find('=');
        if (eq == std::string::npos) {
            continue;
        }
        const std::string key = trim(line.substr(0, eq));
        const std::string val = trim(line.substr(eq + 1));
        if (!key.empty()) {
            out.emplace_back(key, val);
        }
    }
    return out;
}

template <class T>
void print_optional(const char *label, const std::optional<T> &v)
{
    std::cout << std::left << std::setw(28) << label << ": ";
    if (!v) {
        std::cout << "(none)\n";
    } else if constexpr (std::is_same_v<T, bool>) {
        std::cout << (*v ? "true" : "false") << "\n";
    } else {
        std::cout << *v << "\n";
    }
}

/* 检测 get_as<T> 能不能被调用：SFINAE 的经典写法，用来证明约束真的生效了 */
template <class T, class = void>
struct has_numeric_get : std::false_type {};

template <class T>
struct has_numeric_get<T,
    std::void_t<decltype(cfg::get_as<T>(std::declval<const cfg::Config &>(),
                                       std::declval<std::string>(), T{}))>>
    : std::true_type {};

/* 编译期断言：算术类型可以，字符串类型不行 */
static_assert(has_numeric_get<int>::value, "get_as<int> 应当可用");
static_assert(!has_numeric_get<std::string>::value, "get_as<std::string> 应当被约束挡住");

} /* namespace */

int main()
{
    /* ---------------------------------------------------------- 阶段 1 */
    std::cout << "=== Stage 1: optional and parsing ===\n";
    print_optional("parse_int(\"8080\")", cfg::parse_int("8080"));
    print_optional("parse_int(\"12abc\")", cfg::parse_int("12abc"));
    print_optional("parse_int(\" 42\")", cfg::parse_int(" 42"));
    print_optional("parse_double(\"0.75\")", cfg::parse_double("0.75"));
    print_optional("parse_bool(\"true\")", cfg::parse_bool("true"));
    print_optional("parse_bool(\"ON\")", cfg::parse_bool("ON"));
    print_optional("parse_bool(\"maybe\")", cfg::parse_bool("maybe"));
    std::cout << std::left << std::setw(28) << "lookup(\"name\")" << ": "
              << cfg::lookup("name").value_or("(none)") << "\n";
    std::cout << std::left << std::setw(28) << "lookup(\"retries\")" << ": "
              << cfg::lookup("retries").value_or("(none)") << "\n";
    std::cout << std::left << std::setw(28) << "port value_or(0)" << ": "
              << cfg::parse_int(cfg::lookup("port").value_or("0")).value_or(0) << "\n";

    /* ---------------------------------------------------------- 阶段 2 */
    std::cout << "\n=== Stage 2: variant and visit ===\n";
    const std::string text = read_file("data/app.ini");
    cfg::Config config;
    for (const auto &kv : split_ini(text)) {
        config.set(kv.first, kv.second);
    }
    std::cout << "items            : " << config.size() << "\n";
    for (const auto &kv : config.all()) {
        std::cout << std::left << std::setw(17) << kv.first << ": "
                  << cfg::dump_value(kv.second) << "\n";
    }
    if (const cfg::Value *v = config.find("debug")) {
        std::cout << "find(\"debug\")     : " << cfg::dump_value(*v) << "\n";
    } else {
        std::cout << "find(\"debug\")     : (none)\n";
    }
    std::cout << "has(\"missing\")    : " << (config.has("missing") ? "yes" : "no") << "\n";

    /* ---------------------------------------------------------- 阶段 3 */
    std::cout << "\n=== Stage 3: tuple, structured binding, apply ===\n";
    const auto items = cfg::parse_lines(text);
    std::cout << "lines parsed     : " << items.size() << "\n";
    std::cout << "ok flags         :";
    for (const auto &[key, value, ok] : items) {
        (void)key;
        (void)value;
        std::cout << " " << (ok ? "yes" : "no");
    }
    std::cout << "\n";

    const auto first = cfg::first_item(text);
    std::cout << "first item       : " << first.first << " / "
              << cfg::dump_value(first.second) << "\n";

    const std::tuple<std::string, int, double> sample{"timeout", 30, 1.5};
    std::apply([](const std::string &k, int n, double d) {
        std::cout << "apply demo       : " << k << " / " << n << " / " << d << "\n";
    }, sample);

    /* ---------------------------------------------------------- 阶段 4 */
    std::cout << "\n=== Stage 4: type_traits constraints ===\n";
    std::cout << "is_arithmetic<int>          : " << (std::is_arithmetic_v<int> ? 1 : 0) << "\n";
    std::cout << "is_arithmetic<std::string>  : "
              << (std::is_arithmetic_v<std::string> ? 1 : 0) << "\n";
    std::cout << "get_as<int> available       : "
              << (has_numeric_get<int>::value ? "yes" : "no") << "\n";
    std::cout << "get_as<std::string>         : "
              << (has_numeric_get<std::string>::value ? "yes" : "no") << "\n";
    std::cout << "get_as<int>(\"port\", -1)     : " << cfg::get_as<int>(config, "port", -1) << "\n";
    std::cout << "get_as<double>(\"ratio\", 0)  : "
              << cfg::get_as<double>(config, "ratio", 0.0) << "\n";
    std::cout << "get_as<int>(\"name\", -1)     : " << cfg::get_as<int>(config, "name", -1) << "\n";

    if (const cfg::Value *v = config.find("port")) {
        std::cout << "type_name_of(port)          : " << cfg::type_name_of(*v) << "\n";
    }

    return 0;
}
