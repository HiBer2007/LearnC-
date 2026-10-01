/**
 * 示例 06 · 命令行统计小工具（单文件）
 *
 * ── 先读教材 ────────────────────────────────────────────────
 *   《04-语法/05-初始化.md》第 2、5 节          写法并不等价、类内初始化器
 *   《04-语法/03-常量与 const.md》第 2、3、5 节   const 限制的是哪条路径
 *   《04-语法/08-数组、指针与引用.md》第 3 节     引用是别名
 *   《04-语法/11-作用域、生存期与链接.md》第 3 节 生存期延长到哪里
 *   《04-语法/12-编译期能力.md》第 1、2、3 节     constexpr 与 static_assert
 *
 * ── 这个项目要解决什么问题 ──────────────────────────────────
 *   把命令行上给的一串数读进来，按指定单位换算，打印一份统计报告：
 *   个数、总和、平均、最小、最大、中位数、总体标准差。
 *   程序末尾跑一遍内置自测，自己核对每一段逻辑是否正确。
 *
 * ── 怎么用 ──────────────────────────────────────────────────
 *   用 VS Code 打开【本文件夹】，按 F5 选一条调试配置；
 *   也可以直接在命令行编译运行：
 *     g++ -std=c++17 -g -O0 -Wall -Wextra -finput-charset=UTF-8
 *         -fexec-charset=GBK main.cpp -o build\gcc\main.exe
 *     build\gcc\main.exe --unit cm 100 250 30
 *     build\gcc\main.exe --help
 *
 * ── 四类语言点各自落在哪里 ──────────────────────────────────
 *   constexpr      单位表、平方、近似比较、静态断言，全在文件开头
 *   初始化         每个结构体都用类内初始化器，SampleView 与选项结构体
 *   const 正确性   SampleView 只持 const double*，统计函数收 const&
 *   引用与生存期  const SampleView& 绑定临时对象、探针对象数进出配对
 */
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace {

/* ══════════════════════════════════════════════════════════════
 *  1. constexpr：能在编译期算的，就不要留到运行期
 * ══════════════════════════════════════════════════════════════ */

/* 近似比较的容差。浮点数不能直接用 == 比，见自测第 2 项 */
constexpr double kEpsilon = 1e-9;

/* 编译期函数：既能在编译期求值，也能在运行期调用 */
constexpr int square(int x) { return x * x; }

/* 编译期断言：条件不成立就编译不过，不产生任何运行期开销 */
static_assert(square(3) == 9, "square(3) 应当是 9");
static_assert(square(4) == 16, "square(4) 应当是 16");
static_assert(square(-5) == 25, "负数平方应当为正");

/* 单位表：整张表在编译期就定下来，查表也是 constexpr 函数 */
struct UnitDef {
    std::string_view name;      /* 单位名 */
    double to_meter;            /* 换算成米的系数 */
};

constexpr std::array<UnitDef, 4> kUnits{{
    {"mm", 0.001},
    {"cm", 0.01},
    {"m", 1.0},
    {"km", 1000.0},
}};

constexpr double unit_to_meter(std::string_view name)
{
    for (const UnitDef &unit : kUnits) {
        if (unit.name == name) {
            return unit.to_meter;
        }
    }
    return 0.0;                 /* 0 表示未知单位 */
}

/* 查表在编译期就完成了，写错立刻编译不过 */
static_assert(unit_to_meter("cm") == 0.01, "厘米的系数应当是 0.01");
static_assert(unit_to_meter("km") == 1000.0, "千米的系数应当是 1000");
static_assert(unit_to_meter("尺") == 0.0, "未知单位应当返回 0");

/* 近似相等：两个浮点数之差小于容差就算相等 */
constexpr bool nearly_equal(double a, double b, double eps = kEpsilon)
{
    const double diff = a > b ? a - b : b - a;
    return diff < eps;
}

/* ══════════════════════════════════════════════════════════════
 *  2. 初始化：每个成员都有确定的初值
 * ══════════════════════════════════════════════════════════════ */

/* 统计结果。不写构造函数，用类内初始化器给出默认值。
   这样即使某个分支忘了赋值，读到的也是 0 而不是未定义值。 */
struct Summary {
    std::size_t count = 0;
    double sum = 0.0;
    double mean = 0.0;
    double min = 0.0;
    double max = 0.0;
    double median = 0.0;
    double stddev = 0.0;        /* 总体标准差：除以 N */
};

/* 命令行选项，同样用类内初始化器 */
struct Options {
    std::string_view unit_name = "m";
    std::vector<double> numbers;
    bool want_help = false;
    bool ok = true;
    std::string error;
};

/* ══════════════════════════════════════════════════════════════
 *  3. const 正确性：只读视图不拥有数据
 * ══════════════════════════════════════════════════════════════ */

/* 一段只读视图。成员是 const double*，因此经视图改不了原数据；
   视图本身很小（一个指针加一个长度），按值传递也不心疼。 */
class SampleView {
public:
    SampleView(const double *data, std::size_t size) noexcept
        : data_(data), size_(size) {}

    std::size_t size() const noexcept { return size_; }
    bool empty() const noexcept { return size_ == 0; }

    const double &operator[](std::size_t i) const noexcept { return data_[i]; }
    const double *begin() const noexcept { return data_; }
    const double *end() const noexcept { return data_ + size_; }

private:
    const double *data_;        /* 底层 const：不能经它写 */
    std::size_t size_;
};

/* 统计函数收 const 引用：不拷贝数据，也不允许改调用者的数据 */
Summary summarize(const SampleView &view)
{
    Summary result;
    result.count = view.size();
    if (view.empty()) {
        return result;
    }

    result.min = view[0];
    result.max = view[0];
    for (const double value : view) {
        result.sum += value;
        if (value < result.min) { result.min = value; }
        if (value > result.max) { result.max = value; }
    }
    result.mean = result.sum / static_cast<double>(result.count);

    /* 中位数要把数据排序，这里拷贝一份，不动调用者的数据 */
    std::vector<double> sorted(view.begin(), view.end());
    std::sort(sorted.begin(), sorted.end());
    const std::size_t mid = sorted.size() / 2;
    if (sorted.size() % 2 != 0) {
        result.median = sorted[mid];
    } else {
        result.median = (sorted[mid - 1] + sorted[mid]) / 2.0;
    }

    /* 总体标准差：各数与平均值之差的平方和，除以 N 再开方 */
    double acc = 0.0;
    for (const double value : view) {
        const double d = value - result.mean;
        acc += d * d;
    }
    result.stddev = std::sqrt(acc / static_cast<double>(result.count));
    return result;
}

/* ══════════════════════════════════════════════════════════════
 *  4. 引用与生存期：探针记下构造与析构的次数
 * ══════════════════════════════════════════════════════════════ */

class LifetimeProbe {
public:
    LifetimeProbe() { ++alive_; ++created_; }
    LifetimeProbe(const LifetimeProbe &) { ++alive_; ++created_; }
    ~LifetimeProbe() { --alive_; ++destroyed_; }

    static std::size_t alive() { return alive_; }
    static std::size_t created() { return created_; }
    static std::size_t destroyed() { return destroyed_; }
    static void reset_counters() { created_ = 0; destroyed_ = 0; }

private:
    static std::size_t alive_;
    static std::size_t created_;
    static std::size_t destroyed_;
};

/* 静态成员属于类，不属于对象，因此要在类外给出定义 */
std::size_t LifetimeProbe::alive_ = 0;
std::size_t LifetimeProbe::created_ = 0;
std::size_t LifetimeProbe::destroyed_ = 0;

/* ══════════════════════════════════════════════════════════════
 *  5. 命令行解析
 * ══════════════════════════════════════════════════════════════ */

/* 第二个参数写成 const char *const *：
   既不能改指针数组里的指针，也不能经它改字符串内容。
   这正是 C++ 里 main 的 argv 的准确类型。 */
Options parse_args(int argc, const char *const *argv)
{
    Options out;
    for (int i = 1; i < argc; ++i) {
        const std::string_view arg = argv[i];

        if (arg == "--help" || arg == "-h") {
            out.want_help = true;
            continue;
        }

        if (arg == "--unit") {
            if (i + 1 >= argc) {
                out.ok = false;
                out.error = "--unit 后面要跟一个单位名";
                return out;
            }
            out.unit_name = argv[++i];
            if (unit_to_meter(out.unit_name) == 0.0) {
                out.ok = false;
                out.error = "不支持的单位：" + std::string(out.unit_name);
                return out;
            }
            continue;
        }

        /* 数字：strtod 解析，并检查整个参数都被吃掉 */
        const char *begin = argv[i];
        char *end = nullptr;
        const double value = std::strtod(begin, &end);
        if (end == begin || *end != '\0') {
            out.ok = false;
            out.error = "不是数字：" + std::string(begin);
            return out;
        }
        out.numbers.push_back(value);
    }
    return out;
}

void print_usage()
{
    std::cout << "用法：stats [--unit mm|cm|m|km] [数值 ...]\n"
                 "  --unit <单位>  输入数值的单位，默认 m\n"
                 "  --help         显示这段说明\n"
                 "  不给数值时使用内置样本 {3, 1, 4, 1, 5, 9, 2, 6}\n";
}

/* ══════════════════════════════════════════════════════════════
 *  6. 报告
 * ══════════════════════════════════════════════════════════════ */

void print_report(const Summary &s, std::string_view unit_name, double factor)
{
    std::cout << std::fixed << std::setprecision(3);
    std::cout << "\n== 统计报告 ==\n";
    std::cout << "  个数      : " << s.count << "\n";
    std::cout << "  总和      : " << s.sum << " " << unit_name << "\n";
    std::cout << "  平均      : " << s.mean << " " << unit_name << "\n";
    std::cout << "  最小      : " << s.min << " " << unit_name << "\n";
    std::cout << "  最大      : " << s.max << " " << unit_name << "\n";
    std::cout << "  中位数    : " << s.median << " " << unit_name << "\n";
    std::cout << "  标准差    : " << s.stddev << " " << unit_name << "\n";
    std::cout << "  换成米    : 平均 " << s.mean * factor
              << " m，最小 " << s.min * factor
              << " m，最大 " << s.max * factor << " m\n";
}

/* ══════════════════════════════════════════════════════════════
 *  7. 自测：程序自己核对每一段逻辑
 * ══════════════════════════════════════════════════════════════ */

class SelfTest {
public:
    void check(bool ok, std::string_view what, const std::string &detail = std::string())
    {
        ++total_;
        if (ok) {
            ++passed_;
            std::cout << "  [通过] " << total_ << ". " << what << "\n";
        } else {
            ++failed_;
            std::cout << "  [不符] " << total_ << ". " << what;
            if (!detail.empty()) {
                std::cout << "（" << detail << "）";
            }
            std::cout << "\n";
        }
    }

    int report() const
    {
        std::cout << "\n  自测结果：" << total_ << " 项中 " << passed_ << " 项通过";
        if (failed_ == 0) {
            std::cout << "，全部通过\n";
        } else {
            std::cout << "，" << failed_ << " 项不符\n";
        }
        return failed_ == 0 ? 0 : 1;
    }

private:
    int total_ = 0;
    int passed_ = 0;
    int failed_ = 0;
};

/* 把 double 转成便于比对的字符串，避免打印精度干扰判断 */
std::string num(double v)
{
    std::ostringstream os;
    os << std::fixed << std::setprecision(6) << v;
    return os.str();
}

/* 返回 0 表示全部通过，返回 1 表示有不符的项 —— 可直接用作进程退出码 */
int run_self_tests()
{
    std::cout << "\n== 自测 ==\n";
    SelfTest t;

    /* 1. 单位表：constexpr 函数在运行期也能调用 */
    t.check(nearly_equal(unit_to_meter("cm"), 0.01), "单位表里 cm 的系数是 0.01");

    /* 2. 浮点数不能直接用 == 比 */
    const double third = 0.1 + 0.2;
    t.check(third != 0.3 && nearly_equal(third, 0.3),
            "0.1 + 0.2 与 0.3 不相等，但近似相等",
            "差 " + num(third - 0.3));

    /* 3. 奇数个样本的中位数 */
    const std::array<double, 5> odd{{5.0, 1.0, 3.0, 2.0, 4.0}};
    const Summary s_odd = summarize(SampleView(odd.data(), odd.size()));
    t.check(nearly_equal(s_odd.median, 3.0), "奇数个样本的中位数是中间那个",
            "得到 " + num(s_odd.median));

    /* 4. 偶数个样本的中位数取中间两个的平均 */
    const std::array<double, 4> even{{1.0, 2.0, 3.0, 4.0}};
    const Summary s_even = summarize(SampleView(even.data(), even.size()));
    t.check(nearly_equal(s_even.median, 2.5), "偶数个样本的中位数取中间两个的平均",
            "得到 " + num(s_even.median));

    /* 5. 平均值、极值与总体标准差 */
    t.check(nearly_equal(s_even.mean, 2.5) && nearly_equal(s_even.min, 1.0)
                && nearly_equal(s_even.max, 4.0),
            "平均值 2.5，最小 1，最大 4");
    t.check(nearly_equal(s_even.stddev, 1.118033988749895),
            "总体标准差用 N 而不是 N-1", "得到 " + num(s_even.stddev));

    /* 6. 空样本：不能除以零，返回全 0 的结果 */
    const Summary s_empty = summarize(SampleView(nullptr, 0));
    t.check(s_empty.count == 0 && nearly_equal(s_empty.mean, 0.0),
            "空样本返回个数 0，平均值 0，不崩");

    /* 7. 单元素样本：标准差为 0 */
    const std::array<double, 1> one{{42.0}};
    const Summary s_one = summarize(SampleView(one.data(), one.size()));
    t.check(nearly_equal(s_one.stddev, 0.0) && nearly_equal(s_one.median, 42.0),
            "单元素样本的标准差是 0，中位数是它自己");

    /* 8. 视图不拷贝数据：同一份数据两种容器，结果一致 */
    const std::vector<double> vec{7.0, 8.0, 9.0};
    const std::array<double, 3> arr{{7.0, 8.0, 9.0}};
    const Summary s_vec = summarize(SampleView(vec.data(), vec.size()));
    const Summary s_arr = summarize(SampleView(arr.data(), arr.size()));
    t.check(nearly_equal(s_vec.mean, s_arr.mean) && nearly_equal(s_vec.sum, s_arr.sum),
            "vector 与 array 交给同一个视图，统计结果一致");

    /* 9. 引用就是别名：地址相同 */
    int n = 5;
    int &r = n;
    t.check(static_cast<const void *>(&n) == static_cast<const void *>(&r),
            "引用与它绑定的对象地址相同");

    /* 10. 临时对象绑定到 const 引用：生存期延长到引用的作用域末尾 */
    {
        const SampleView &extended = SampleView(vec.data(), vec.size());
        t.check(extended.size() == 3, "const 引用绑定临时视图，出了这一行仍然可用",
                "size = " + std::to_string(extended.size()));
    }

    /* 11. 初始化：空花括号保证清零 */
    std::array<int, 4> zeros{};
    t.check(zeros[0] == 0 && zeros[1] == 0 && zeros[2] == 0 && zeros[3] == 0,
            "std::array<int,4> a{} 的四个元素都是 0");

    /* 12. 生存期：构造多少次就析构多少次 */
    LifetimeProbe::reset_counters();
    const std::size_t alive_before = LifetimeProbe::alive();
    {
        LifetimeProbe a;
        LifetimeProbe b(a);            /* 拷贝构造 */
        std::vector<LifetimeProbe> pool(3);
        t.check(LifetimeProbe::alive() == alive_before + 5,
                "作用域内 5 个探针对象都活着",
                "alive = " + std::to_string(LifetimeProbe::alive()));
    }
    t.check(LifetimeProbe::alive() == alive_before
                && LifetimeProbe::created() == LifetimeProbe::destroyed(),
            "离开作用域后全部析构，构造数与析构数相等",
            "构造 " + std::to_string(LifetimeProbe::created())
                + " 次，析构 " + std::to_string(LifetimeProbe::destroyed()) + " 次");

    return t.report();
}

}   /* namespace */

int main(int argc, char *argv[])
{
    std::cout << "示例 06 · 命令行统计小工具\n";

    const Options options = parse_args(argc, argv);
    if (!options.ok) {
        std::cout << "参数有误：" << options.error << "\n";
        print_usage();
        return 2;
    }
    if (options.want_help) {
        print_usage();
        return 0;
    }

    /* 没给数值就用内置样本 */
    std::vector<double> builtin{3.0, 1.0, 4.0, 1.0, 5.0, 9.0, 2.0, 6.0};
    const bool use_builtin = options.numbers.empty();
    const std::vector<double> &data = use_builtin ? builtin : options.numbers;

    std::cout << "样本来源  : " << (use_builtin ? "内置样本" : "命令行") << "\n";
    std::cout << "输入单位  : " << options.unit_name << "\n";

    /* const 引用绑定到 summarize 的返回值（临时对象），生存期延长到这里 */
    const Summary &summary = summarize(SampleView(data.data(), data.size()));
    print_report(summary, options.unit_name, unit_to_meter(options.unit_name));

    return run_self_tests();
}
