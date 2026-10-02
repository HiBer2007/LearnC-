/**
 * varargs_lab.hpp —— 不定参数：从 printf 到可变模板
 *
 * 版权所有 (C) 2026 HiBer2007，保留所有权利。
 *
 * 本程序是《C 与 C++》教材的一部分，采用与教材文档相同的授权：
 * CC BY-NC-ND 4.0 加附加条款。全文见仓库根目录的 LICENSE 与 许可附加条款.md。
 *
 * 允许在保留本声明的前提下查看、编译、运行本程序用于学习；
 * 不允许二次分发，不允许商用，不允许演绎（修改后再分发），不允许移除署名。
 *
 * 本程序不提供任何担保。
 */

/**
 * 报告里出现的每个数字都由这里的计数器数出来，重跑逐位相同：
 *
 *   va_start_calls   va_start 被调用的次数
 *   va_arg_calls     va_arg 被调用的次数，也就是「取出了几个实参」
 *   va_end_calls     va_end 被调用的次数，正常时与 va_start_calls 相等
 *   format_specs     格式串里消耗实参的转换说明个数
 *   fold_entries     折叠版 sum_all 进入函数的次数（每次调用进一次）
 *   rec_entries      递归版 sum_rec 进入函数的次数（每一层递归各进一次）
 *
 * Counter 那一组另有拷贝构造与移动构造的计数，见 counter_tally。
 *
 * 第二段的三行读数（标着 UB 的那三行）是**未定义行为**在本机上的样子：
 * 换编译器、换平台、换优化等级都可能得到别的数，程序里只把它们打印出来，
 * 不用它们做任何判断。
 */
#ifndef VARARGS_LAB_HPP
#define VARARGS_LAB_HPP

#include <cstddef>
#include <initializer_list>
#include <ostream>
#include <sstream>
#include <string>
#include <type_traits>
#include <vector>

namespace vlab {

/* ================= 结构量：计数器 ================= */

struct Stats {
    std::size_t va_start_calls = 0;
    std::size_t va_arg_calls = 0;
    std::size_t va_end_calls = 0;
    std::size_t format_specs = 0;
    std::size_t fold_entries = 0;
    std::size_t rec_entries = 0;
};

/** 全程序共用的一个计数器组。教学示例固定单线程使用 */
Stats &stats();

/** 清空全部计数，便于逐段测量 */
void reset_stats();

/* ================= 一、C 的 va_list ================= */

/** 把 count 个 int 加起来。个数由调用方单独传进来，函数照它读。
 *  count 比实际实参多就是未定义行为；比实际实参少时多出来的实参被忽略。 */
long long sum_ints(int count, ...);

/** 极小格式化函数的结果：文本与本次取用实参的次数 */
struct FormatRun {
    std::string text;
    std::size_t va_arg_calls = 0;   /**< 本次 va_arg 的调用次数 */
    std::size_t specs = 0;          /**< 消耗实参的转换说明个数 */
    std::size_t unknown_specs = 0;  /**< 认不出来的 %x，原样输出，不消耗实参 */
};

/** 支持 %d、%s、%f 与 %% 的极小格式化函数。
 *  %d 取 int，%f 取 double（float 会在进入 ... 之前先提升为 double），
 *  %s 取 const char *，%% 输出一个百分号且不消耗实参。 */
FormatRun format_small(const char *fmt, ...);

/* ================= 二、类型陷阱 ================= */

/** 按 double 读第一个变参槽。直接传 float 时，提升已经让实参是 double */
double read_double_arg(int tag, ...);

/** 按 int 读第一个变参槽。实参比 int 宽时只取低 4 字节，是未定义行为 */
int read_int_arg(int tag, ...);

/** 按 long long 读第一个变参槽。实参只有 4 字节时会多读 4 字节，是未定义行为 */
long long read_longlong_arg(int tag, ...);

/** 绕开默认实参提升之后再按 double 读。
 *  匹配省略号的实参一律先做默认实参提升，float 提升为 double，
 *  因此只有让调用点「以为形参是 float」时，4 字节的 float 才会真的按 4 字节进参数区。
 *  这里换一个函数指针类型来做到这一点，这一步本身也是未定义行为。 */
double read_double_bypassing_promotion(float value, int neighbour);

/** 8 字节值的位模式，16 位十六进制，大写 */
std::string bits_of(double value);
std::string bits_of(long long value);

/** 4 字节值的位模式，8 位十六进制，大写 */
std::string bits_of_float(float value);
std::string bits_of_int(int value);

/* ================= 三、可变参数模板与折叠表达式 ================= */

/** 折叠表达式版求和：二元右折叠，初值 0，空包也能用 */
template <typename... Args>
auto sum_all(Args... args)
{
    ++stats().fold_entries;
    return (0 + ... + args);
}

/** 递归展开版的基线：只剩一个实参时它自己就是结果 */
template <typename T>
auto sum_rec(T value)
{
    ++stats().rec_entries;
    return value;
}

/** 递归展开版：取走第一个实参，剩下的交给同一名字的另一个重载 */
template <typename T, typename... Rest>
auto sum_rec(T first, Rest... rest)
{
    ++stats().rec_entries;
    return first + sum_rec(rest...);
}

/** 渲染一个实参：bool 打 true／false，浮点按 %g，其余交给流 */
template <typename T>
void render_one(std::ostream &os, const T &value)
{
    using Plain = std::decay_t<T>;
    if constexpr (std::is_same_v<Plain, bool>) {
        os << (value ? "true" : "false");
    } else if constexpr (std::is_floating_point_v<Plain>) {
        os << value;                  /* ostream 的默认精度是 6 位有效数字，2.5 就是 2.5 */
    } else {
        os << value;
    }
}

/** 一次调用打印一串异构实参，用逗号隔开，返回文本（核心库不直接输出） */
template <typename... Args>
std::string print_all(const Args &... args)
{
    std::ostringstream os;
    std::size_t index = 0;
    ((os << (index++ == 0 ? "" : ", "), render_one(os, args)), ...);
    return os.str();
}

/* ================= 四、std::initializer_list 对照 ================= */

struct CopyMoveTally {
    std::size_t copies = 0;
    std::size_t moves = 0;
};

/** 元素拷贝与移动的全局计数，用法与 stats() 相同 */
CopyMoveTally &counter_tally();
void reset_counter_tally();

/** 会记账的元素类型：拷贝构造与移动构造各记一笔 */
class Counter {
public:
    explicit Counter(long long value = 0) : value_(value) {}

    Counter(const Counter &other);
    Counter(Counter &&other) noexcept;

    Counter &operator=(const Counter &other) = default;
    Counter &operator=(Counter &&other) noexcept = default;
    ~Counter() = default;

    long long value() const { return value_; }

private:
    long long value_;
};

/** 用 initializer_list 求一串数的和，元素是纯数值 */
long long sum_il(std::initializer_list<long long> values);

/** 同一个求和，元素换成会记账的 Counter */
long long sum_counter_il(std::initializer_list<Counter> values);

/* ================= 报告与自测 ================= */

/** 自测结果。不打印，交给界面决定怎么显示 */
struct CheckResult {
    std::size_t total = 0;
    std::size_t passed = 0;
    std::size_t failed = 0;
    std::vector<std::string> lines;   /**< 每项一行，例如 "[通过] 3. ……" */

    bool all_passed() const { return failed == 0; }
    std::string summary() const;      /**< 例如 "31 项中 31 项通过，全部通过" */
};

/** 项目输出：va_list、类型陷阱、可变参数模板、initializer_list 对照、结构量汇总五段。
 *  返回多行 UTF-8 文本，末尾带一个换行；由界面层决定怎么显示 */
std::string build_report();

/** 逐项核对 va_list 的用法、提升规则、UB 观测值、模板写法与列表拷贝次数 */
CheckResult run_self_tests();

}   /* namespace vlab */

#endif /* VARARGS_LAB_HPP */
