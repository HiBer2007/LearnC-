/**
 * capstone.cpp —— 标准库综合流水线的实现
 *
 * 没有界面代码，也不打印：每一步的结果都交回调用方。
 * 命令行版与自测共用这一份实现。
 */
#include "capstone.hpp"

#include <algorithm>
#include <charconv>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <map>
#include <numeric>
#include <sstream>
#include <system_error>
#include <utility>

namespace capstone {

namespace {

/* ---------- 计时 ---------- */

/* 一段代码的计时器：用 steady_clock，它只往前走，不受系统时间调整影响 */
class PhaseTimer {
public:
    using clock = std::chrono::steady_clock;

    PhaseTimer() : start_(clock::now()) {}

    double elapsed_ms() const
    {
        const std::chrono::duration<double, std::milli> span = clock::now() - start_;
        return span.count();
    }

private:
    clock::time_point start_;
};

/* ---------- 文本小工具 ---------- */

bool is_word_char(char ch)
{
    return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z');
}

char to_lower(char ch)
{
    return (ch >= 'A' && ch <= 'Z') ? static_cast<char>(ch - 'A' + 'a') : ch;
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

/* 注释符前面要有空白才算行尾注释，否则 `a#b` 这种值会被截断 */
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
        if ((ch == '#' || ch == ';') && i > 0 && (text[i - 1] == ' ' || text[i - 1] == '\t')) {
            return text.substr(0, i);
        }
    }
    return text;
}

/* 字面量翻成值：整体读成整数才算整数，整体读成小数才算小数，其余是字符串。
   用 from_chars 而不是 strtod：它不认本地化的小数点，也不看前导空白 */
Value parse_literal(std::string_view text)
{
    if (text == "true" || text == "false") {
        return Value{text == "true"};
    }
    if (text.size() >= 2 && text.front() == '"' && text.back() == '"') {
        return Value{std::string(text.substr(1, text.size() - 2))};
    }

    const char *const begin = text.data();
    const char *const end = begin + text.size();
    long long as_integer = 0;
    std::from_chars_result integer_result = std::from_chars(begin, end, as_integer);
    if (integer_result.ec == std::errc() && integer_result.ptr == end) {
        return Value{as_integer};
    }

    double as_double = 0.0;
    std::from_chars_result double_result = std::from_chars(begin, end, as_double);
    if (double_result.ec == std::errc() && double_result.ptr == end) {
        return Value{as_double};
    }

    return Value{std::string(text)};
}

/* ---------- 段落：每个段落只认自己那一段数据 ---------- */

class OverviewSection : public Section {
public:
    OverviewSection(Settings settings, Stats stats, std::string label, std::size_t line_count)
        : settings_(std::move(settings)), stats_(std::move(stats)),
          label_(std::move(label)), line_count_(line_count)
    {
    }

    std::string name() const override { return "概览"; }

    std::string render() const override
    {
        std::ostringstream os;
        os << "-- 概览 --\n";
        os << "  输入文件  : " << label_ << "（" << line_count_ << " 行）\n";
        os << "  配置      : ";
        for (std::size_t i = 0; i < settings_.entries.size(); ++i) {
            const auto &[section, key, text] = settings_.entries[i];
            if (i > 0) {
                os << "，";
            }
            if (!section.empty()) {
                os << section << ".";
            }
            os << key << " = " << text;
        }
        os << "\n";
        os << "  总词数    : " << stats_.total_words << "\n";
        os << "  不同词数  : " << stats_.distinct_words << "\n";
        os << std::fixed << std::setprecision(3);
        os << "  平均词长  : " << stats_.average_length << "\n";
        os << "  最短/最长 : " << stats_.shortest << " / " << stats_.longest << "\n";
        return os.str();
    }

private:
    Settings settings_;
    Stats stats_;
    std::string label_;
    std::size_t line_count_ = 0;
};

class TopWordsSection : public Section {
public:
    TopWordsSection(Stats stats, long long top) : stats_(std::move(stats)), top_(top) {}

    std::string name() const override { return "词频"; }

    std::string render() const override
    {
        std::ostringstream os;
        os << "-- 词频前 " << (top_ < 0 ? 0 : top_) << "（不同词共 " << stats_.distinct_words
           << " 个）--\n";
        if (stats_.top.empty()) {
            os << "  （没有可统计的词）\n";
            return os.str();
        }
        for (std::size_t i = 0; i < stats_.top.size(); ++i) {
            os << "  " << std::right << std::setw(3) << (i + 1) << ". " << std::left
               << std::setw(16) << stats_.top[i].word << std::right << std::setw(5)
               << stats_.top[i].count << "\n";
        }
        return os.str();
    }

private:
    Stats stats_;
    long long top_ = 0;
};

class HistogramSection : public Section {
public:
    HistogramSection(Stats stats, long long bar_width)
        : stats_(std::move(stats)), bar_width_(bar_width)
    {
    }

    std::string name() const override { return "词长分布"; }

    std::string render() const override
    {
        std::ostringstream os;
        os << "-- 词长分布（" << stats_.bins.size() << " 档，按不同词统计）--\n";
        if (stats_.bins.empty()) {
            os << "  （没有可统计的词）\n";
            return os.str();
        }

        std::size_t max_count = 0;
        for (const LengthBin &bin : stats_.bins) {
            max_count = std::max(max_count, bin.count);
        }
        const long long width = std::max<long long>(1, bar_width_);
        for (const LengthBin &bin : stats_.bins) {
            std::size_t bar = 0;
            if (max_count > 0) {
                bar = static_cast<std::size_t>(static_cast<long long>(bin.count) * width
                                               / static_cast<long long>(max_count));
            }
            if (bin.count > 0 && bar == 0) {
                bar = 1;                    /* 有计数就至少画一个字符，免得看成空档 */
            }
            os << "  [" << std::setw(2) << bin.begin << ", " << std::setw(2) << bin.end << "]"
               << std::setw(6) << bin.count << "  " << std::string(bar, '#') << "\n";
        }
        return os.str();
    }

private:
    Stats stats_;
    long long bar_width_ = 0;
};

class TimingSection : public Section {
public:
    explicit TimingSection(Timings timings) : timings_(timings) {}

    std::string name() const override { return "耗时"; }

    std::string render() const override
    {
        std::ostringstream os;
        os << "-- 耗时（steady_clock，毫秒）--\n";
        os << std::fixed << std::setprecision(3);
        os << "  读文件    : " << timings_.read_ms << "\n";
        os << "  分词      : " << timings_.tokenize_ms << "\n";
        os << "  统计      : " << timings_.stats_ms << "\n";
        os << "  报表      : " << timings_.report_ms << "\n";
        os << "  合计      : " << timings_.total_ms() << "\n";
        return os.str();
    }

private:
    Timings timings_;
};

/* 配置项的范围检查：配置文件是给人改的，写错一个数字不该让程序算错 */
void check_range(long long &value, long long low, long long high, long long fallback,
                 const std::string &name, std::vector<std::string> &problems)
{
    if (value < low || value > high) {
        problems.push_back(name + " 要在 " + std::to_string(low) + " 到 " + std::to_string(high)
                           + " 之间，收到 " + std::to_string(value) + "，改用 "
                           + std::to_string(fallback));
        value = fallback;
    }
}

/* 把 optional 渲染成一行文本，失败时用来说清「期望什么、拿到什么」 */
template <typename T>
std::string show(const std::optional<T> &value)
{
    if (!value.has_value()) {
        return "没有";
    }
    if constexpr (std::is_same_v<T, bool>) {
        return *value ? "true" : "false";
    } else {
        std::ostringstream os;
        os << *value;
        return os.str();
    }
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

}   /* namespace */

/* ---------- 配置 ---------- */

void ConfigReader::parse(std::string_view text)
{
    std::istringstream stream{std::string(text)};
    std::string raw_line;
    std::string section;
    std::size_t line_number = 0;

    while (std::getline(stream, raw_line)) {
        ++line_number;
        const std::string_view line = trim(raw_line);
        if (line.empty() || line.front() == '#' || line.front() == ';') {
            continue;
        }

        if (line.front() == '[') {
            const std::size_t close = line.find(']');
            if (close == std::string_view::npos) {
                problems_.push_back("第 " + std::to_string(line_number) + " 行的段名没有闭合的 ]");
                continue;
            }
            section = std::string(trim(line.substr(1, close - 1)));
            continue;
        }

        const std::size_t equals = line.find('=');
        if (equals == std::string_view::npos) {
            problems_.push_back("第 " + std::to_string(line_number) + " 行没有 = 号："
                                + std::string(line));
            continue;
        }
        const std::string key(trim(line.substr(0, equals)));
        if (key.empty()) {
            problems_.push_back("第 " + std::to_string(line_number) + " 行的 = 左边没有键名");
            continue;
        }

        const std::string_view literal = trim(strip_comment(line.substr(equals + 1)));
        const std::string text_of_value(literal);
        Value value = parse_literal(literal);

        /* 同一个键写两次时后写的说了算：列表里替换，不留重复 */
        bool replaced = false;
        for (std::size_t i = 0; i < items_.size(); ++i) {
            if (items_[i].key == key && items_[i].section == section) {
                items_[i].value = std::move(value);
                std::get<2>(entries_[i]) = text_of_value;   /* 三元组里第 3 项是值文本 */
                replaced = true;
                break;
            }
        }
        if (!replaced) {
            items_.push_back(Item{section, key, std::move(value)});
            entries_.emplace_back(section, key, text_of_value);
        }
    }
}

const Value *ConfigReader::find(const std::string &name) const
{
    const std::size_t dot = name.rfind('.');
    const bool has_section = dot != std::string::npos;
    const std::string want_section = has_section ? name.substr(0, dot) : std::string();
    const std::string want_key = has_section ? name.substr(dot + 1) : name;

    for (const Item &item : items_) {
        if (item.key == want_key && (!has_section || item.section == want_section)) {
            return &item.value;
        }
    }
    return nullptr;
}

/* ---------- 读入与分词 ---------- */

Corpus split_lines(std::string_view text)
{
    Corpus corpus;
    corpus.bytes = text.size();

    std::istringstream stream{std::string(text)};
    std::string line;
    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        corpus.lines.push_back(line);
    }
    return corpus;
}

bool read_text_file(const std::string &path, std::string &text, std::string &error)
{
    text.clear();
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        error = "打不开文件：" + path;
        return false;
    }
    std::ostringstream buffer;
    buffer << input.rdbuf();
    text = buffer.str();
    error.clear();
    return true;
}

std::vector<std::string> tokenize(std::string_view text, const Settings &settings)
{
    const std::size_t min_length =
        static_cast<std::size_t>(settings.min_length < 1 ? 1 : settings.min_length);

    std::vector<std::string> words;
    std::string current;
    const auto flush = [&words, &current, min_length]() {
        if (!current.empty()) {
            if (current.size() >= min_length) {
                words.push_back(current);
            }
            current.clear();
        }
    };

    for (const char ch : text) {
        if (is_word_char(ch)) {
            current.push_back(settings.lowercase ? to_lower(ch) : ch);
        } else {
            flush();
        }
    }
    flush();
    return words;
}

/* ---------- 统计 ---------- */

std::size_t Stats::bin_total() const
{
    return std::accumulate(bins.begin(), bins.end(), std::size_t{0},
                           [](std::size_t sum, const LengthBin &bin) { return sum + bin.count; });
}

Stats compute_stats(const std::vector<std::string> &words, const Settings &settings)
{
    Stats stats;
    stats.total_words = words.size();

    std::map<std::string, std::size_t> counts;
    for (const std::string &word : words) {
        ++counts[word];
    }
    stats.distinct_words = counts.size();
    if (counts.empty()) {
        return stats;                       /* 空输入：分箱与词频榜都留空 */
    }

    /* 平均词长按词次加权：词长乘次数累加起来，再除以词次 */
    const std::size_t total_length = std::accumulate(
        counts.begin(), counts.end(), std::size_t{0},
        [](std::size_t sum, const std::map<std::string, std::size_t>::value_type &item) {
            return sum + item.first.size() * item.second;
        });
    stats.average_length =
        static_cast<double>(total_length) / static_cast<double>(stats.total_words);

    /* 词频榜：次数从多到少；次数相同按词典序，输出因此逐字节稳定 */
    std::vector<WordCount> sorted;
    sorted.reserve(counts.size());
    for (const auto &item : counts) {
        sorted.push_back(WordCount{item.first, item.second});
    }
    std::sort(sorted.begin(), sorted.end(), [](const WordCount &left, const WordCount &right) {
        if (left.count != right.count) {
            return left.count > right.count;
        }
        return left.word < right.word;
    });
    const std::size_t keep =
        std::min(static_cast<std::size_t>(std::max<long long>(0, settings.top)), sorted.size());
    stats.top.assign(sorted.begin(), sorted.begin() + static_cast<std::ptrdiff_t>(keep));

    /* 词长分箱：按不同词统计，因此各档计数之和等于不同词数。
       档数不超过词长跨度，免得出现一排空档 */
    std::size_t shortest = counts.begin()->first.size();
    std::size_t longest = shortest;
    for (const auto &item : counts) {
        shortest = std::min(shortest, item.first.size());
        longest = std::max(longest, item.first.size());
    }
    stats.shortest = shortest;
    stats.longest = longest;

    const std::size_t span = longest - shortest + 1;
    const std::size_t bin_count = std::min(
        static_cast<std::size_t>(std::max<long long>(1, settings.bins)), span);
    stats.bins.assign(bin_count, LengthBin{});
    for (std::size_t i = 0; i < bin_count; ++i) {
        stats.bins[i].begin = shortest + (span * i + bin_count - 1) / bin_count;
        stats.bins[i].end = shortest + (span * (i + 1) + bin_count - 1) / bin_count - 1;
    }
    for (const auto &item : counts) {
        const std::size_t length = item.first.size();
        const std::size_t index = std::min(bin_count - 1, (length - shortest) * bin_count / span);
        stats.bins[index].count += 1;
    }
    return stats;
}

/* ---------- 段落工厂 ---------- */

std::unique_ptr<Section> make_overview_section(const Settings &settings, const Stats &stats,
                                               const std::string &input_label,
                                               std::size_t line_count)
{
    return std::make_unique<OverviewSection>(settings, stats, input_label, line_count);
}

std::unique_ptr<Section> make_top_words_section(const Settings &settings, const Stats &stats)
{
    return std::make_unique<TopWordsSection>(stats, settings.top);
}

std::unique_ptr<Section> make_histogram_section(const Settings &settings, const Stats &stats)
{
    return std::make_unique<HistogramSection>(stats, settings.bar_width);
}

std::unique_ptr<Section> make_timing_section(const Timings &timings)
{
    return std::make_unique<TimingSection>(timings);
}

/* ---------- 段落登记表 ---------- */

void ReportBuilder::add_section(std::unique_ptr<Section> section)
{
    Section *const raw = section.get();
    names_.push_back(raw->name());
    /* 回调只捕获裸指针：段落的所有权就在 owned_ 里，回调不会活得比它长 */
    callbacks_.emplace_back([raw]() { return raw->render(); });
    owned_.push_back(std::move(section));
}

const std::vector<std::string> &ReportBuilder::render()
{
    /* 段落一经登记就不再变化，因此已渲染的结果可以留着；
       后加入的段落接着渲染，重复调用不会重算 */
    while (cache_.size() < callbacks_.size()) {
        cache_.push_back(callbacks_[cache_.size()]());
    }
    return cache_;
}

/* ---------- 流水线 ---------- */

RunResult run_text(std::string_view config_text, const Corpus &corpus,
                   const std::string &input_label, double read_ms)
{
    RunResult result;
    result.timings.read_ms = read_ms;

    /* 读配置：配置很小，耗时不单列 */
    ConfigReader reader;
    reader.parse(config_text);
    result.problems = reader.problems();
    Settings &settings = result.settings;
    settings.entries = reader.entries();

    if (const std::optional<long long> value = reader.get<long long>("analysis.top")) {
        settings.top = *value;
    }
    if (const std::optional<long long> value = reader.get<long long>("analysis.min_length")) {
        settings.min_length = *value;
    }
    if (const std::optional<bool> value = reader.get<bool>("analysis.lowercase")) {
        settings.lowercase = *value;
    }
    if (const std::optional<long long> value = reader.get<long long>("analysis.bins")) {
        settings.bins = *value;
    }
    if (const std::optional<long long> value = reader.get<long long>("report.bar_width")) {
        settings.bar_width = *value;
    }
    check_range(settings.top, 1, 50, 8, "top", result.problems);
    check_range(settings.min_length, 1, 20, 3, "min_length", result.problems);
    check_range(settings.bins, 1, 20, 5, "bins", result.problems);
    check_range(settings.bar_width, 10, 80, 40, "bar_width", result.problems);

    /* 分词：逐行切分，按 min_length 过滤、按 lowercase 规范化 */
    PhaseTimer tokenize_timer;
    std::vector<std::string> words;
    for (const std::string &line : corpus.lines) {
        const std::vector<std::string> from_line = tokenize(line, settings);
        words.insert(words.end(), from_line.begin(), from_line.end());
    }
    result.timings.tokenize_ms = tokenize_timer.elapsed_ms();

    /* 统计：计数、平均词长、词长分箱、词频榜 */
    PhaseTimer stats_timer;
    result.stats = compute_stats(words, settings);
    result.timings.stats_ms = stats_timer.elapsed_ms();

    /* 报表：工厂造出段落（unique_ptr 独占），登记进回调表，按登记顺序渲染。
       计时段落最后登记，它的数字只能来自它登记之前的动作 */
    PhaseTimer report_timer;
    ReportBuilder builder;
    builder.add_section(
        make_overview_section(settings, result.stats, input_label, corpus.lines.size()));
    builder.add_section(make_top_words_section(settings, result.stats));
    builder.add_section(make_histogram_section(settings, result.stats));
    (void)builder.render();                 /* 正文先渲染一次，这一段算进 report_ms */
    result.timings.report_ms = report_timer.elapsed_ms();
    builder.add_section(make_timing_section(result.timings));
    result.report = builder.render();       /* 正文走缓存，只补渲染计时段 */

    if (corpus.empty()) {
        result.problems.push_back("输入文本是空的：" + input_label);
    }
    result.ok = !corpus.empty();
    return result;
}

RunResult run(const std::string &config_path, const std::string &input_path)
{
    RunResult result;
    std::string config_text;
    std::string error;
    if (!read_text_file(config_path, config_text, error)) {
        result.problems.push_back(error);
        return result;                      /* ok 保持 false，报表为空 */
    }

    PhaseTimer read_timer;
    std::string corpus_text;
    const bool corpus_ok = read_text_file(input_path, corpus_text, error);
    const Corpus corpus = split_lines(corpus_text);
    const double read_ms = read_timer.elapsed_ms();

    result = run_text(config_text, corpus, input_path, read_ms);
    if (!corpus_ok) {
        result.problems.insert(result.problems.begin(), error);
        result.ok = false;
    }
    return result;
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

    const std::string sample_config =
        "# 内置样例配置\n"
        "[analysis]\n"
        "top = 3              # 词频榜前三名\n"
        "min_length = 3\n"
        "lowercase = true\n"
        "bins = 5\n"
        "\n"
        "[report]\n"
        "bar_width = 20\n";

    const std::string sample_text =
        "the cat sat on the mat\n"
        "The CAT sat, and the mat sat still.\n";

    const Corpus corpus = split_lines(sample_text);
    const RunResult result = run_text(sample_config, corpus, "（内置样例）", 0.25);
    const Settings &settings = result.settings;

    /* 1–5. 配置读得对不对 */
    ConfigReader reader;
    reader.parse(sample_config);
    const std::optional<long long> top = reader.get<long long>("analysis.top");
    c.check(top.has_value() && *top == 3, "配置 analysis.top 读成整数 3", "取到 " + show(top));

    c.check(settings.min_length == 3 && settings.bins == 5,
            "min_length 与 bins 都读到了",
            "min_length = " + std::to_string(settings.min_length) + "，bins = "
                + std::to_string(settings.bins));

    c.check(settings.lowercase, "lowercase = true 读成布尔真");

    c.check(settings.bar_width == 20, "只写键名 bar_width 也能取到 report 段里的值",
            "取到 " + std::to_string(settings.bar_width));

    bool entry_ok = settings.entries.size() == 5;
    if (entry_ok) {
        const auto &[first_section, first_key, first_text] = settings.entries.front();
        const auto &[last_section, last_key, last_text] = settings.entries.back();
        entry_ok = first_section == "analysis" && first_key == "top" && first_text == "3"
                   && last_section == "report" && last_key == "bar_width" && last_text == "20";
    }
    c.check(entry_ok, "配置条目按 (段, 键, 值) 结构化绑定解包，共 5 条，行尾注释没有进值",
            "条数 " + std::to_string(settings.entries.size()));

    /* 6–7. 两种取不到，以及越界的值被挡下 */
    const std::optional<long long> missing = reader.get<long long>("analysis.missing");
    const std::optional<bool> wrong_type = reader.get<bool>("analysis.top");
    c.check(!missing.has_value() && !wrong_type.has_value(),
            "缺键返回 nullopt，类型不符也返回 nullopt");

    const RunResult clamped =
        run_text("[analysis]\ntop = 999\nbins = 0\n", corpus, "（越界配置）", 0.0);
    c.check(clamped.settings.top == 8 && clamped.settings.bins == 5
                && clamped.problems.size() == 2,
            "越界的 top 与 bins 改回默认值，并各记一条问题",
            "问题 " + std::to_string(clamped.problems.size()) + " 条");

    /* 8–10. 分词 */
    const std::vector<std::string> punctuation = tokenize("Hello, world! 42 times", settings);
    c.check(punctuation.size() == 3 && punctuation[0] == "hello" && punctuation[2] == "times",
            "标点与数字都是分隔符，词统一转成小写",
            "切出 " + std::to_string(punctuation.size()) + " 个词");

    const std::vector<std::string> words = tokenize(sample_text, settings);
    c.check(words.size() == 13, "样例文本按 min_length = 3 过滤后剩 13 个词次",
            "切出 " + std::to_string(words.size()) + " 个词次");

    Settings strict = settings;
    strict.min_length = 5;
    const std::vector<std::string> long_words = tokenize(sample_text, strict);
    c.check(long_words.size() == 1 && long_words[0] == "still",
            "min_length 改成 5 之后只剩 still 一个词，过滤确实生效",
            "剩 " + std::to_string(long_words.size()) + " 个词次");

    /* 11–14. 统计 */
    c.check(result.stats.total_words == 13 && result.stats.distinct_words == 6,
            "总词数 13、不同词数 6",
            "总 " + std::to_string(result.stats.total_words) + "，不同 "
                + std::to_string(result.stats.distinct_words));

    c.check(std::fabs(result.stats.average_length - 41.0 / 13.0) < 1e-9,
            "平均词长等于 41 / 13（按词次加权）",
            "得到 " + std::to_string(result.stats.average_length));

    Settings case_sensitive = settings;
    case_sensitive.lowercase = false;
    const Stats mixed = compute_stats(tokenize(sample_text, case_sensitive), case_sensitive);
    c.check(mixed.distinct_words == 8,
            "关掉 lowercase 后 The 与 the 分开算，不同词从 6 变成 8",
            "得到 " + std::to_string(mixed.distinct_words));

    c.check(!result.stats.top.empty() && result.stats.top[0].word == "the"
                && result.stats.top[0].count == 4,
            "词频第一名是 the，出现 4 次",
            result.stats.top.empty() ? "词频榜为空"
                                     : result.stats.top[0].word + " "
                                           + std::to_string(result.stats.top[0].count));

    bool top_ok = result.stats.top.size() == 3;
    if (top_ok) {
        top_ok = result.stats.top[0].word == "the" && result.stats.top[1].word == "sat"
                 && result.stats.top[2].word == "cat";
    }
    c.check(top_ok, "词频前三名依次是 the、sat、cat（同为 2 次的按词典序）",
            "榜上 " + std::to_string(result.stats.top.size()) + " 条");

    /* 15–16. 分箱 */
    c.check(result.stats.bin_total() == result.stats.distinct_words,
            "各档计数之和等于不同词数",
            std::to_string(result.stats.bin_total()) + " 对 "
                + std::to_string(result.stats.distinct_words));

    bool bins_ok = result.stats.bins.size() == 3;
    if (bins_ok) {
        bins_ok = result.stats.bins.front().begin == 3 && result.stats.bins.back().end == 5;
    }
    c.check(bins_ok, "档数取 min(bins, 词长跨度)，这里是 3 档，覆盖 3 到 5",
            "档数 " + std::to_string(result.stats.bins.size()));

    /* 17–20. 段落工厂与回调表 */
    ReportBuilder builder;
    builder.add_section(
        make_overview_section(settings, result.stats, "（内置样例）", corpus.lines.size()));
    builder.add_section(make_top_words_section(settings, result.stats));
    builder.add_section(make_histogram_section(settings, result.stats));
    const std::size_t before_timing = builder.render().size();
    builder.add_section(make_timing_section(result.timings));
    const std::vector<std::string> &rendered = builder.render();

    c.check(builder.size() == 4 && before_timing == 3 && rendered.size() == 4,
            "工厂造出 4 个段落；render() 按登记进度补渲染，不重算已渲染的段落",
            "登记 " + std::to_string(builder.size()) + " 段，渲染 "
                + std::to_string(rendered.size()) + " 段");

    const std::vector<std::string> &names = builder.section_names();
    c.check(names.size() == 4 && names[0] == "概览" && names[1] == "词频"
                && names[2] == "词长分布" && names[3] == "耗时",
            "段落名按登记顺序为 概览、词频、词长分布、耗时");

    std::string joined;
    for (const std::string &part : rendered) {
        joined += part;
    }
    const std::size_t at_overview = joined.find("-- 概览 --");
    const std::size_t at_top = joined.find("-- 词频前");
    const std::size_t at_histogram = joined.find("-- 词长分布");
    const std::size_t at_timing = joined.find("-- 耗时");
    c.check(at_overview < at_top && at_top < at_histogram && at_histogram < at_timing,
            "正文里四段的出现顺序与登记顺序一致");

    /* 21. 除耗时外逐字节稳定：同一份输入跑两遍，前三段完全一致 */
    const RunResult again = run_text(sample_config, corpus, "（内置样例）", 0.25);
    bool stable = again.report.size() == result.report.size() && !result.report.empty();
    if (stable) {
        for (std::size_t i = 0; i + 1 < result.report.size(); ++i) {
            stable = stable && again.report[i] == result.report[i];
        }
    }
    c.check(stable, "同一份输入跑两遍，概览、词频、词长分布逐字节相同，只有耗时段不同");

    /* 22. 计时 */
    const Timings &timings = result.timings;
    c.check(timings.read_ms == 0.25 && timings.tokenize_ms >= 0.0 && timings.stats_ms >= 0.0
                && timings.report_ms >= 0.0 && timings.total_ms() >= timings.read_ms,
            "四个计时字段都非负，读文件那一段正是传进来的 0.25 毫秒",
            "合计 " + std::to_string(timings.total_ms()) + " 毫秒");

    /* 23–24. 输入的两条边界 */
    std::string text;
    std::string error;
    const bool opened = read_text_file("data/no-such-file.txt", text, error);
    c.check(!opened && !error.empty() && text.empty(),
            "读不到的文件返回 false、给出原因，并把文本清空", error);

    const Corpus nothing;
    const RunResult empty_run = run_text(sample_config, nothing, "（空输入）", 0.0);
    c.check(!empty_run.ok && !empty_run.problems.empty() && empty_run.report.size() == 4
                && empty_run.stats.total_words == 0,
            "空输入仍然出齐四段，ok 为假并给出说明",
            "报表 " + std::to_string(empty_run.report.size()) + " 段");

    return c.take();
}

}   /* namespace capstone */
