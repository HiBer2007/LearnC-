/**
 * capstone.hpp —— 标准库综合流水线，不依赖任何界面
 *
 * 一条流水线跑到底：读配置 → 读文本 → 分词计数 → 统计 → 计时 → 出报表。
 * 每一步只做一件事，步骤之间靠普通数据结构传递结果，
 * 因此任何一段都能从流水线里单独拿出来测。
 *
 * 所有权与回调：报表的每个段落由工厂函数造成 std::unique_ptr，
 * 交给 ReportBuilder 保管；ReportBuilder 为每段登记一个 std::function，
 * 渲染时按登记顺序调用，段落本体不重复渲染。
 */
#ifndef CAPSTONE_HPP
#define CAPSTONE_HPP

#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <variant>
#include <vector>

namespace capstone {

/** 一个配置值：四种类型之一，与配置解析那一套思路相同 */
using Value = std::variant<bool, long long, double, std::string>;

/** 摊平后的配置项：(段, 键, 值文本) */
using ConfigEntry = std::tuple<std::string, std::string, std::string>;

/** 从配置文件里读出来的设置，每一项都有可用的默认值 */
struct Settings {
    long long top = 8;              /**< 词频榜取前几名 */
    long long min_length = 3;       /**< 短于这个长度的词丢掉 */
    bool lowercase = true;          /**< 统计前是否统一转小写 */
    long long bins = 5;             /**< 词长分箱的档数 */
    long long bar_width = 40;       /**< 直方图条形的最大宽度，单位是字符 */
    std::vector<ConfigEntry> entries;   /**< 配置文件里的全部条目，按出现顺序 */
};

class ConfigReader {
public:
    /** 逐行解析 INI 风格的文本；有问题的行记进 problems()，其余照常收下 */
    void parse(std::string_view text);

    /** 按键取值。名字可以写「段.键」，也可以只写「键」；
     *  缺键与类型不符都返回 nullopt */
    template <typename T>
    std::optional<T> get(const std::string &name) const;

    /** 按键取值，缺省时用 fallback */
    template <typename T>
    T get_or(const std::string &name, T fallback) const;

    const std::vector<ConfigEntry> &entries() const { return entries_; }
    const std::vector<std::string> &problems() const { return problems_; }

private:
    struct Item {
        std::string section;
        std::string key;
        Value value;
    };

    const Value *find(const std::string &name) const;

    std::vector<Item> items_;
    std::vector<ConfigEntry> entries_;
    std::vector<std::string> problems_;
};

template <typename T>
std::optional<T> ConfigReader::get(const std::string &name) const
{
    const Value *found = find(name);
    if (found == nullptr) {
        return std::nullopt;
    }
    if (const T *exact = std::get_if<T>(found)) {
        return *exact;
    }
    if constexpr (std::is_floating_point_v<T>) {
        if (const long long *as_integer = std::get_if<long long>(found)) {
            return static_cast<T>(*as_integer);
        }
    }
    return std::nullopt;
}

template <typename T>
T ConfigReader::get_or(const std::string &name, T fallback) const
{
    const std::optional<T> found = get<T>(name);
    return found.has_value() ? *found : fallback;
}

/** 文本按行切开之后的样子：分词与统计都从它出发 */
struct Corpus {
    std::vector<std::string> lines;
    std::size_t bytes = 0;

    bool empty() const { return lines.empty(); }
};

/** 读文件这一段：把一段文本按行切开，去掉行尾的 \r，并记下字节数 */
Corpus split_lines(std::string_view text);

/** 一次读入整个文件。失败时返回 false，把原因写进 error，并把 text 清空 */
bool read_text_file(const std::string &path, std::string &text, std::string &error);

/** 分词：按 ASCII 字母切，数字与标点都是分隔符；
 *  再按 min_length 过滤、按 lowercase 决定是否统一小写 */
std::vector<std::string> tokenize(std::string_view text, const Settings &settings);

/** 词长分箱里的一档 */
struct LengthBin {
    std::size_t begin = 0;      /**< 这一档覆盖的最短词长 */
    std::size_t end = 0;        /**< 这一档覆盖的最长词长 */
    std::size_t count = 0;      /**< 落在这一档的不同词个数 */
};

/** 一个词与它的出现次数 */
struct WordCount {
    std::string word;
    std::size_t count = 0;
};

/** 统计结果 */
struct Stats {
    std::size_t total_words = 0;        /**< 全部词次 */
    std::size_t distinct_words = 0;     /**< 不同词个数 */
    std::size_t shortest = 0;           /**< 不同词里最短的长度 */
    std::size_t longest = 0;            /**< 不同词里最长的长度 */
    double average_length = 0.0;        /**< 平均词长，按词次加权 */
    std::vector<LengthBin> bins;        /**< 词长分箱，按不同词统计 */
    std::vector<WordCount> top;         /**< 词频榜，次数从多到少 */

    /** 各档计数之和：它应当等于不同词数 */
    std::size_t bin_total() const;
};

/** 统计：总词数、不同词数、平均词长、绝长、词长分箱、词频榜 */
Stats compute_stats(const std::vector<std::string> &words, const Settings &settings);

/** 四段耗时，单位毫秒 */
struct Timings {
    double read_ms = 0.0;       /**< 读文件：打开、读完、按行切开 */
    double tokenize_ms = 0.0;   /**< 分词：切分、过滤、规范化 */
    double stats_ms = 0.0;      /**< 统计：计数、排序、分箱 */
    double report_ms = 0.0;     /**< 报表：造段落与渲染正文 */

    double total_ms() const { return read_ms + tokenize_ms + stats_ms + report_ms; }
};

/** 报表里的一段：只有名字与渲染两件事 */
class Section {
public:
    virtual ~Section() = default;
    virtual std::string name() const = 0;
    virtual std::string render() const = 0;
};

/** 段落工厂：返回独占所有权的段落，调用方拿到的是 unique_ptr */
std::unique_ptr<Section> make_overview_section(const Settings &settings, const Stats &stats,
                                               const std::string &input_label,
                                               std::size_t line_count);
std::unique_ptr<Section> make_top_words_section(const Settings &settings, const Stats &stats);
std::unique_ptr<Section> make_histogram_section(const Settings &settings, const Stats &stats);
std::unique_ptr<Section> make_timing_section(const Timings &timings);

/** 段落登记表：接管段落的所有权，为每段登记一个 std::function，按登记顺序渲染 */
class ReportBuilder {
public:
    /** 段落一经登记就不再变化，渲染结果因此可以缓存 */
    void add_section(std::unique_ptr<Section> section);

    /** 渲染全部已登记的段落。每段只渲染一次，后加入的段落接着渲染 */
    const std::vector<std::string> &render();

    /** 段落名，按登记顺序 */
    const std::vector<std::string> &section_names() const { return names_; }

    std::size_t size() const { return callbacks_.size(); }

private:
    std::vector<std::unique_ptr<Section>> owned_;
    std::vector<std::function<std::string()>> callbacks_;
    std::vector<std::string> names_;
    std::vector<std::string> cache_;
};

/** 整条流水线的结果 */
struct RunResult {
    Settings settings;
    Stats stats;
    Timings timings;
    std::vector<std::string> report;        /**< 报表正文，每段一个元素 */
    std::vector<std::string> problems;      /**< 配置与输入的问题 */
    bool ok = false;                        /**< 输入读到了、报表出来了才算 true */
};

/** 从已经在内存里的文本跑完整条流水线。
 *  read_ms 是「把文本拿进来」那一段的耗时：run() 里真的读文件时由它计量，
 *  文本本来就现成的时候传 0。 */
RunResult run_text(std::string_view config_text, const Corpus &corpus,
                   const std::string &input_label, double read_ms);

/** 从两个文件跑完整条流水线 */
RunResult run(const std::string &config_path, const std::string &input_path);

/** 自测结果。不打印，交给调用方决定怎么显示 */
struct CheckResult {
    std::vector<std::string> lines;     /**< 每项一行，已含「[通过] N. …」 */
    std::size_t passed = 0;
    std::size_t failed = 0;

    bool all_passed() const { return failed == 0; }
    std::string summary() const;
};

/** 逐项核对配置、分词、统计、分箱、工厂、回调顺序与计时 */
CheckResult run_self_tests();

}   /* namespace capstone */

#endif /* CAPSTONE_HPP */
