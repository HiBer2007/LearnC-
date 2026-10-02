/**
 * varargs_lab.cpp —— 不定参数：从 printf 到可变模板
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

#include "varargs_lab.hpp"

#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <sstream>
#include <string>
#include <vector>

namespace vlab {

/* ================= 结构量：计数器 ================= */

namespace {

Stats g_stats;
CopyMoveTally g_tally;

}   /* namespace */

Stats &stats()
{
    return g_stats;
}

void reset_stats()
{
    g_stats = Stats{};
}

CopyMoveTally &counter_tally()
{
    return g_tally;
}

void reset_counter_tally()
{
    g_tally = CopyMoveTally{};
}

/* ================= 一、C 的 va_list ================= */

long long sum_ints(int count, ...)
{
    ++stats().va_start_calls;

    va_list ap;
    va_start(ap, count);              /* 三个宏必须配对：start 取地址，end 收拾 */

    long long total = 0;
    for (int i = 0; i < count; ++i) {
        const int value = va_arg(ap, int);   /* 每调用一次取走一个实参 */
        ++stats().va_arg_calls;
        total += value;
    }

    va_end(ap);
    ++stats().va_end_calls;
    return total;
}

FormatRun format_small(const char *fmt, ...)
{
    FormatRun run;
    ++stats().va_start_calls;

    va_list ap;
    va_start(ap, fmt);

    std::string out;
    const char *cursor = fmt;
    while (*cursor != '\0') {
        if (*cursor != '%') {
            out.push_back(*cursor);
            ++cursor;
            continue;
        }
        const char spec = *(cursor + 1);
        if (spec == '\0') {
            out.push_back('%');              /* 结尾一个孤零零的百分号，原样输出 */
            break;
        }
        if (spec == '%') {
            out.push_back('%');              /* %% 输出一个百分号，不消耗实参 */
            cursor += 2;
            continue;
        }
        cursor += 2;                         /* 吃掉 % 与转换字符 */

        switch (spec) {
        case 'd': {
            const int value = va_arg(ap, int);
            ++stats().va_arg_calls;
            ++run.va_arg_calls;
            ++run.specs;
            ++stats().format_specs;
            out += std::to_string(value);
            break;
        }
        case 'f': {
            const double value = va_arg(ap, double);   /* float 已经在调用点提升成 double */
            ++stats().va_arg_calls;
            ++run.va_arg_calls;
            ++run.specs;
            ++stats().format_specs;
            char buffer[64];
            std::snprintf(buffer, sizeof buffer, "%.6f", value);
            out += buffer;
            break;
        }
        case 's': {
            const char *value = va_arg(ap, const char *);
            ++stats().va_arg_calls;
            ++run.va_arg_calls;
            ++run.specs;
            ++stats().format_specs;
            out += (value != nullptr ? value : "(null)");
            break;
        }
        default:
            ++run.unknown_specs;             /* 认不出来：原样输出，不消耗实参 */
            out.push_back('%');
            out.push_back(spec);
            break;
        }
    }

    va_end(ap);
    ++stats().va_end_calls;

    run.text = out;
    return run;
}

/* ================= 二、类型陷阱 ================= */

double read_double_arg(int tag, ...)
{
    ++stats().va_start_calls;
    va_list ap;
    va_start(ap, tag);
    const double value = va_arg(ap, double);
    ++stats().va_arg_calls;
    va_end(ap);
    ++stats().va_end_calls;
    return value;
}

int read_int_arg(int tag, ...)
{
    ++stats().va_start_calls;
    va_list ap;
    va_start(ap, tag);
    const int value = va_arg(ap, int);
    ++stats().va_arg_calls;
    va_end(ap);
    ++stats().va_end_calls;
    return value;
}

long long read_longlong_arg(int tag, ...)
{
    ++stats().va_start_calls;
    va_list ap;
    va_start(ap, tag);
    const long long value = va_arg(ap, long long);
    ++stats().va_arg_calls;
    va_end(ap);
    ++stats().va_end_calls;
    return value;
}

/* 换一个形参表来调用同一份实现：调用点以为形参是 float，就不会做提升。
   用不兼容的函数类型调用是未定义行为，这一步只是为了给下面那行读数一个来源。 */
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-function-type"
#endif

double read_double_bypassing_promotion(float value, int neighbour)
{
    using FloatProto = double (*)(float, int);
    const FloatProto bypass = reinterpret_cast<FloatProto>(&read_double_arg);
    return bypass(value, neighbour);
}

#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

std::string bits_of(double value)
{
    unsigned long long raw = 0;
    std::memcpy(&raw, &value, sizeof raw);
    char buffer[32];
    std::snprintf(buffer, sizeof buffer, "%016llX", raw);
    return buffer;
}

std::string bits_of(long long value)
{
    unsigned long long raw = 0;
    std::memcpy(&raw, &value, sizeof raw);
    char buffer[32];
    std::snprintf(buffer, sizeof buffer, "%016llX", raw);
    return buffer;
}

std::string bits_of_int(int value)
{
    unsigned int raw = 0;
    std::memcpy(&raw, &value, sizeof raw);
    char buffer[16];
    std::snprintf(buffer, sizeof buffer, "%08X", raw);
    return buffer;
}

std::string bits_of_float(float value)
{
    unsigned int raw = 0;
    std::memcpy(&raw, &value, sizeof raw);
    char buffer[16];
    std::snprintf(buffer, sizeof buffer, "%08X", raw);
    return buffer;
}

/* ================= 四、std::initializer_list 对照 ================= */

Counter::Counter(const Counter &other) : value_(other.value_)
{
    ++counter_tally().copies;
}

Counter::Counter(Counter &&other) noexcept : value_(other.value_)
{
    ++counter_tally().moves;
}

long long sum_il(std::initializer_list<long long> values)
{
    long long total = 0;
    for (const long long value : values) {
        total += value;
    }
    return total;
}

long long sum_counter_il(std::initializer_list<Counter> values)
{
    long long total = 0;
    for (const Counter &value : values) {
        total += value.value();
    }
    return total;
}

/* ================= 报告 ================= */

namespace {

/** 按显示宽度算列数：CJK 与全角符号算 2 列，其余算 1 列 */
std::size_t display_width(const std::string &text)
{
    std::size_t width = 0;
    for (std::size_t i = 0; i < text.size();) {
        const unsigned char byte = static_cast<unsigned char>(text[i]);
        if (byte < 0x80) {
            width += 1;
            i += 1;
        } else if ((byte & 0xE0) == 0xC0) {
            width += 1;
            i += 2;
        } else if ((byte & 0xF0) == 0xE0) {
            width += 2;
            i += 3;
        } else {
            width += 2;
            i += 4;
        }
    }
    return width;
}

/** 右边补空格到指定显示宽度 */
std::string pad(const std::string &text, std::size_t width)
{
    const std::size_t used = display_width(text);
    return text + std::string(used < width ? width - used : 0, ' ');
}

const char *yes_no(bool value)
{
    return value ? u8"是" : u8"否";
}

/** 定点读数的文本：17 位有效数字，1.5 这类精确值不会被补成 1.500000 */
std::string decimal(double value)
{
    char buffer[64];
    std::snprintf(buffer, sizeof buffer, "%.17g", value);
    return buffer;
}

void append_va_list_section(std::ostringstream &os)
{
    os << u8"一、C 的 va_list：va_start、va_arg、va_end 三件套\n";
    os << u8"  sum_ints(count, ...)：个数由调用方单独传进来，函数照它读\n";

    const std::size_t before_four = stats().va_arg_calls;
    const long long four = sum_ints(4, 1, 2, 3, 4);
    os << u8"    sum_ints(4, 1, 2, 3, 4) = " << four << u8"，va_arg 调用 "
       << (stats().va_arg_calls - before_four) << u8" 次\n";

    const std::size_t before_none = stats().va_arg_calls;
    const long long none = sum_ints(0);
    os << u8"    sum_ints(0) = " << none << u8"，va_arg 调用 "
       << (stats().va_arg_calls - before_none) << u8" 次，一次也没取用实参，"
       << u8"va_start 与 va_end 照样配对\n";

    os << u8"    同一批实参 1 2 3 4，个数传 2、3、4 各读几个（个数是运行期的一个值）：\n";
    for (int count = 2; count <= 4; ++count) {
        os << u8"      count = " << count << u8" -> " << sum_ints(count, 1, 2, 3, 4) << "\n";
    }
    os << u8"      个数比实参少时多出来的实参被忽略；个数比实参多就是未定义行为，程序里不演示\n";

    const FormatRun point = format_small(u8"点 %s：x = %d，y = %f", u8"A", 3, 1.5);
    os << u8"  format_small(fmt, ...)：个数由格式串里的转换说明决定\n";
    os << u8"    format_small(u8\"点 %s：x = %d，y = %f\", u8\"A\", 3, 1.5)\n";
    os << u8"      -> " << point.text << "\n";
    os << u8"      消耗实参的转换说明 " << point.specs << u8" 个，va_arg 调用 "
       << point.va_arg_calls << u8" 次\n";

    const FormatRun percent = format_small(u8"100%% 完成：%d", 7);
    os << u8"    format_small(u8\"100%% 完成：%d\", 7) -> " << percent.text
       << u8"（%% 输出一个百分号，不消耗实参）\n";

    const FormatRun mixed = format_small(u8"%d 与 %s 与 %d", 1, u8"中间", 2);
    os << u8"    format_small(u8\"%d 与 %s 与 %d\", 1, u8\"中间\", 2) -> " << mixed.text << "\n";

    const FormatRun unknown = format_small(u8"%q 与 %d", 3);
    os << u8"    format_small(u8\"%q 与 %d\", 3) -> " << unknown.text
       << u8"（认不出来的 %q 原样输出，不消耗实参）\n";

    os << u8"    格式串与实参对不上就是未定义行为：%d 遇到 double 会读它的低 4 字节（见第二段），\n";
    os << u8"    %s 遇到整数会把这个整数当成地址去解引用\n";
    os << u8"  本程序累计：va_start " << stats().va_start_calls << u8" 次、va_arg "
       << stats().va_arg_calls << u8" 次、va_end " << stats().va_end_calls << u8" 次，配平："
       << yes_no(stats().va_start_calls == stats().va_end_calls) << "\n";
}

void append_trap_section(std::ostringstream &os)
{
    os << u8"二、类型陷阱：默认实参提升与 va_arg 的宽度\n";
    os << u8"  sizeof(float) = " << sizeof(float) << u8"，sizeof(double) = " << sizeof(double)
       << "\n";
    os << u8"  同一个 1.5 两种宽度：float 的位模式 " << bits_of_float(1.5f) << u8"（4 字节），"
       << u8"double 的位模式 " << bits_of(1.5) << u8"（8 字节）\n";

    os << u8"  (1) 直接传 float，用 va_arg(ap, double) 读\n";
    const double direct = read_double_arg(0, 1.5f);
    const double converted = read_double_arg(0, static_cast<double>(1.5f));
    os << u8"      传 1.5f，按 double 读：" << decimal(direct) << u8"，位模式 " << bits_of(direct)
       << "\n";
    os << u8"      先 static_cast<double> 再传，按 double 读：" << decimal(converted)
       << u8"，位模式 " << bits_of(converted) << "\n";
    os << u8"      两者逐位相同：" << yes_no(bits_of(direct) == bits_of(converted)) << "\n";
    os << u8"      匹配省略号的实参先做默认实参提升，float 在进入函数之前就已经是 double，\n";
    os << u8"      所以这一处不是未定义行为：调用方写不写 static_cast<double>，传进去的都是 8 字节\n";

    os << u8"  (2) 比 int 窄的整型提升为 int，按 int 读是合法的\n";
    os << u8"      传 (char)-1、(short)300、true，按 int 读：" << read_int_arg(0, static_cast<char>(-1))
       << u8"、" << read_int_arg(0, static_cast<short>(300)) << u8"、" << read_int_arg(0, true) << "\n";

    os << u8"  (3) 读的类型与实际传的类型不一致时才是未定义行为。下面三行是未定义行为在本机上的\n";
    os << u8"      样子，换编译器、换平台、换优化等级都可能得到别的数。程序只在自测里把它们\n";
    os << u8"      当成现场记录核对一遍，不靠它们得出任何结论\n";

    const double broken = read_double_bypassing_promotion(1.5f, 7);
    os << u8"      UB 一 绕开提升按 double 读：形参表被声明成 (float, int)，4 字节的 float 才真的\n";
    os << u8"            按 4 字节进参数区。传 1.5f 与 7，按 double 读：" << decimal(broken)
       << u8"，位模式 " << bits_of(broken) << "\n";
    os << u8"            读到的 8 字节既不是 1.5，也不是 float 的 4 字节加 4 字节垃圾，\n";
    os << u8"            而是同一个调用里第二个实参 7 的那 8 字节被当成 double 解释的结果\n";

    const int narrow = read_int_arg(0, 1.1f);
    os << u8"      UB 二 传 1.1f（提升后的位模式 " << bits_of(static_cast<double>(1.1f))
       << u8"），按 int 读：" << narrow << u8"，位模式 " << bits_of_int(narrow) << "\n";
    os << u8"            只取了 8 字节里的低 4 字节\n";

    const long long wide = read_longlong_arg(0, 1);
    os << u8"      UB 三 传 1，按 long long 读：" << wide << u8"，位模式 " << bits_of(wide) << "\n";
    os << u8"            多读了 4 字节，本机那 4 字节恰好是全零，于是读出来「对」：\n";
    os << u8"            未定义行为不保证看得出来，只保证不用保证\n";
}

void append_template_section(std::ostringstream &os)
{
    os << u8"三、可变参数模板与折叠表达式（C++17）\n";
    os << u8"  sum_all(args...) 的写法是 return (0 + ... + args);，二元右折叠，初值 0\n";
    os << u8"    sum_all() = " << sum_all() << u8"（空包也能用，初值就是结果）\n";
    os << u8"    sum_all(1, 2, 3, 4) = " << sum_all(1, 2, 3, 4) << "\n";
    os << u8"    sum_all(1, 2.5, 3) = " << sum_all(1, 2.5, 3)
       << u8"（同一个模板吃下 int 与 double，返回 double）\n";

    /* 只取两次读数之间的增量，全局计数留给第五段汇总 */
    const std::size_t fold_before = stats().fold_entries;
    const long long fold_value = sum_all(1, 2, 3, 4);
    const std::size_t fold_entries = stats().fold_entries - fold_before;
    const std::size_t rec_before = stats().rec_entries;
    const long long rec_value = sum_rec(1, 2, 3, 4);
    const std::size_t rec_entries = stats().rec_entries - rec_before;

    os << u8"  同样 4 个实参，两种写法对照（重载个数读自源码，次数由计数器数出来）\n";
    os << u8"    " << pad(u8"写法", 22) << pad(u8"模板重载个数", 14) << pad(u8"进入函数次数", 14)
       << u8"结果\n";
    os << u8"    " << pad(u8"递归展开（C++11）", 22) << pad("2", 14) << pad(std::to_string(rec_entries), 14)
       << rec_value << "\n";
    os << u8"    " << pad(u8"折叠表达式（C++17）", 22) << pad("1", 14) << pad(std::to_string(fold_entries), 14)
       << fold_value << "\n";
    os << u8"    递归版每取走一个实参就再进一次函数，折叠版整包只进一次\n";

    os << u8"  异构类型一次调用\n";
    os << u8"    print_all(u8\"标签\", 42, 2.5, u8\"文本\", true)\n";
    os << u8"      -> " << print_all(u8"标签", 42, 2.5, u8"文本", true) << "\n";
    os << u8"    每一类的渲染由 if constexpr 在编译期选定：bool 打 true／false，其余交给流\n";
    os << u8"    print_all 不直接输出，返回文本，输出由命令行层负责\n";
}

struct IlTallies {
    CopyMoveTally rvalue;
    CopyMoveTally lvalue;
    CopyMoveTally kept;
};

IlTallies append_il_section(std::ostringstream &os)
{
    IlTallies tallies;

    os << u8"四、std::initializer_list 对照：同一个求和，几种写法\n";
    os << u8"  Counter 每发生一次拷贝构造、一次移动构造都记一笔\n";
    os << u8"  (1) 元素拷贝与移动次数\n";

    reset_counter_tally();
    const long long rvalue_sum = sum_counter_il({Counter(1), Counter(2), Counter(3)});
    tallies.rvalue = counter_tally();

    const Counter a(1);
    const Counter b(2);
    const Counter c(3);
    reset_counter_tally();
    const long long lvalue_sum = sum_counter_il({a, b, c});
    tallies.lvalue = counter_tally();

    reset_counter_tally();
    const std::initializer_list<Counter> kept = {Counter(4), Counter(5)};
    const long long kept_sum = sum_counter_il(kept);
    tallies.kept = counter_tally();

    os << u8"      " << pad(u8"写法", 34) << pad(u8"元素拷贝", 12) << u8"元素移动\n";
    os << u8"      " << pad(u8"va_list 的 sum_ints", 34) << pad("0", 12) << "0\n";
    os << u8"      " << pad(u8"initializer_list，元素是右值", 34)
       << pad(std::to_string(tallies.rvalue.copies), 12) << tallies.rvalue.moves
       << u8"（C++17 起元素就地构造）\n";
    os << u8"      " << pad(u8"initializer_list，元素是左值", 34)
       << pad(std::to_string(tallies.lvalue.copies), 12) << tallies.lvalue.moves
       << u8"（3 个元素各拷一次进底层数组）\n";
    os << u8"      " << pad(u8"传入一个已有的 initializer_list", 34)
       << pad(std::to_string(tallies.kept.copies), 12) << tallies.kept.moves
       << u8"（列表对象只复制指针与长度）\n";
    os << u8"      sum_counter_il({Counter(1), Counter(2), Counter(3)}) = " << rvalue_sum << "\n";
    os << u8"      Counter a(1), b(2), c(3); sum_counter_il({a, b, c}) = " << lvalue_sum << "\n";
    os << u8"      sum_counter_il(kept) = " << kept_sum << u8"（kept 是已有的列表）\n";

    os << u8"  (2) 能不能接受不同类型\n";
    os << u8"      一个 initializer_list 只有一种元素类型。int 放进 long long 列表算加宽，合法\n";
    os << u8"        sum_il({1, 2, 3}) = " << sum_il({1, 2, 3}) << "\n";
    os << u8"      同一个列表里混进 double 就编不过（窄化），命令行版演示不了编不过的代码，\n";
    os << u8"      编译器原话记在 README 里\n";
    os << u8"      可变参数模板没有这条限制：sum_all(1, 2.5, 3) = " << sum_all(1, 2.5, 3) << "\n";

    os << u8"  (3) 个数什么时候定下来\n";
    os << u8"      可变参数模板：编译期。每个实参对应一个形参，个数不同就是不同的实例\n";
    os << u8"        sum_all(1, 2) = " << sum_all(1, 2) << u8"、sum_all(1, 2, 3) = " << sum_all(1, 2, 3)
       << u8"、sum_all(1, 2, 3, 4) = " << sum_all(1, 2, 3, 4) << u8"，三个不同的实例\n";
    os << u8"      initializer_list：调用点。大括号里写几个就是几个，函数体里拿到的 .size() 只是\n";
    os << u8"        把这个数带进来；sum_il({1, 2, 3}) 的元素个数 3 在源码里就写死了\n";
    os << u8"      va_list：运行期。个数是调用方传进去的一个值，第一段的 count = 2、3、4 就是\n";
    os << u8"        同一个调用点按运行期的值读不同个数\n";
    os << u8"      要在运行期决定个数又要类型安全，用的是 vector 一类的容器，不是这三者中任何一个\n";

    return tallies;
}

void append_stats_section(std::ostringstream &os, const IlTallies &tallies)
{
    const std::size_t counter_copies =
        tallies.rvalue.copies + tallies.lvalue.copies + tallies.kept.copies;
    const std::size_t counter_moves =
        tallies.rvalue.moves + tallies.lvalue.moves + tallies.kept.moves;

    os << u8"五、结构量汇总（本次运行，重跑逐位相同）\n";
    os << u8"  va_start 调用 " << stats().va_start_calls << u8" 次\n";
    os << u8"  va_arg 调用 " << stats().va_arg_calls << u8" 次\n";
    os << u8"  va_end 调用 " << stats().va_end_calls << u8" 次，与 va_start 配平："
       << yes_no(stats().va_start_calls == stats().va_end_calls) << "\n";
    os << u8"  格式串里消耗实参的转换说明 " << stats().format_specs << u8" 个，每次取用都对应一次 va_arg\n";
    os << u8"  求和写法的函数模板声明个数：va_list 1、递归展开 2、折叠 1、initializer_list 1\n";
    os << u8"  进入函数的次数（4 个实参）：递归展开 4、折叠 1\n";
    os << u8"  Counter 拷贝构造 " << counter_copies << u8" 次、移动构造 " << counter_moves
       << u8" 次（右值列表 " << tallies.rvalue.copies << u8"、左值列表 " << tallies.lvalue.copies
       << u8"、传入已有列表 " << tallies.kept.copies << u8"）\n";
}

}   /* namespace */

std::string build_report()
{
    reset_stats();

    std::ostringstream os;
    append_va_list_section(os);
    append_trap_section(os);
    append_template_section(os);
    const IlTallies tallies = append_il_section(os);
    append_stats_section(os, tallies);
    return os.str();
}

/* ================= 自测 ================= */

namespace {

class Checks {
public:
    void expect(bool ok, const std::string &what)
    {
        ++total_;
        if (ok) {
            ++passed_;
            lines_.push_back(u8"[通过] " + std::to_string(total_) + ". " + what);
        } else {
            ++failed_;
            lines_.push_back(u8"[失败] " + std::to_string(total_) + ". " + what);
        }
    }

    void expect_eq(long long got, long long want, const std::string &what)
    {
        if (got == want) {
            expect(true, what);
        } else {
            expect(false, what + u8"（得到 " + std::to_string(got) + u8"，应为 " +
                              std::to_string(want) + u8"）");
        }
    }

    void expect_eq_size(std::size_t got, std::size_t want, const std::string &what)
    {
        if (got == want) {
            expect(true, what);
        } else {
            expect(false, what + u8"（得到 " + std::to_string(got) + u8"，应为 " +
                              std::to_string(want) + u8"）");
        }
    }

    CheckResult finish() const
    {
        CheckResult result;
        result.total = total_;
        result.passed = passed_;
        result.failed = failed_;
        result.lines = lines_;
        return result;
    }

private:
    std::size_t total_ = 0;
    std::size_t passed_ = 0;
    std::size_t failed_ = 0;
    std::vector<std::string> lines_;
};

}   /* namespace */

std::string CheckResult::summary() const
{
    std::ostringstream os;
    os << total << u8" 项中 " << passed << u8" 项通过";
    if (failed == 0) {
        os << u8"，全部通过";
    } else {
        os << u8"，" << failed << u8" 项不符";
    }
    return os.str();
}

CheckResult run_self_tests()
{
    Checks checks;

    /* 1—2 个数必须另传 */
    reset_stats();
    const long long four = sum_ints(4, 1, 2, 3, 4);
    checks.expect_eq(four, 10, u8"sum_ints(4, 1, 2, 3, 4) 是 10");
    checks.expect_eq_size(stats().va_arg_calls, 4, u8"这一次取用 4 个实参：va_arg 调用 4 次");

    /* 3 空列表 */
    reset_stats();
    checks.expect(sum_ints(0) == 0 && stats().va_arg_calls == 0 && stats().va_start_calls == 1 &&
                      stats().va_end_calls == 1,
                  u8"sum_ints(0) 是 0，va_arg 一次也没调用，va_start 与 va_end 仍然配对");

    /* 4 个数由运行期决定 */
    checks.expect(sum_ints(2, 1, 2, 3, 4) == 3 && sum_ints(3, 1, 2, 3, 4) == 6 &&
                      sum_ints(4, 1, 2, 3, 4) == 10,
                  u8"同一批实参按运行期的个数读：count 传 2、3、4 分别是 3、6、10");

    /* 5 负数与零 */
    reset_stats();
    checks.expect(sum_ints(3, -5, 0, 5) == 0 && stats().va_arg_calls == 3,
                  u8"负数与零照常相加：sum_ints(3, -5, 0, 5) 是 0");

    /* 6 一次调用里三件套各一次 */
    reset_stats();
    checks.expect(sum_ints(5, 1, 2, 3, 4, 5) == 15 && stats().va_start_calls == 1 &&
                      stats().va_end_calls == 1 && stats().va_arg_calls == 5,
                  u8"一次调用里 va_start 与 va_end 各一次，va_arg 五次，结果 15");

    /* 7 全局配平 */
    reset_stats();
    (void)sum_ints(3, 1, 2, 3);
    (void)format_small(u8"%d", 1);
    checks.expect(stats().va_start_calls == 2 && stats().va_end_calls == 2,
                  u8"va_start 与 va_end 逐次配对：两次调用一共两对");

    /* 8—11 极小的格式化函数 */
    const FormatRun point = format_small(u8"点 %s：x = %d，y = %f", u8"A", 3, 1.5);
    checks.expect(point.text == u8"点 A：x = 3，y = 1.500000" && point.va_arg_calls == 3 &&
                      point.specs == 3,
                  u8"%d、%s、%f 各一个：文本逐字符对得上，va_arg 调用 3 次");

    const FormatRun percent = format_small(u8"100%% 完成：%d", 7);
    checks.expect(percent.text == u8"100% 完成：7" && percent.va_arg_calls == 1 &&
                      percent.specs == 1,
                  u8"%% 输出一个百分号且不消耗实参：文本与 va_arg 次数都对");

    const FormatRun unknown = format_small(u8"%q 与 %d", 3);
    checks.expect(unknown.text == u8"%q 与 3" && unknown.unknown_specs == 1 &&
                      unknown.va_arg_calls == 1,
                  u8"认不出来的 %q 原样输出、不消耗实参，后面的 %d 照常取用");

    bool spec_rule = true;
    {
        const FormatRun r1 = format_small(u8"%d%d%d", 1, 2, 3);
        const FormatRun r2 = format_small(u8"%s", u8"x");
        const FormatRun r3 = format_small(u8"%%%f", 0.5);
        const FormatRun r4 = format_small(u8"%d%%%s", 9, u8"尾");
        spec_rule = r1.text == u8"123" && r2.text == u8"x" && r3.text == u8"%0.500000" &&
                    r4.text == u8"9%尾" && r1.specs == 3 && r2.specs == 1 && r3.specs == 1 &&
                    r4.specs == 2;
    }
    checks.expect(spec_rule, u8"va_arg 的调用次数等于格式串里消耗实参的转换说明个数");

    /* 12—15 默认实参提升 */
    checks.expect(sizeof(float) == 4 && sizeof(double) == 8,
                  u8"sizeof(float) 是 4、sizeof(double) 是 8");

    const double direct = read_double_arg(0, 1.5f);
    const double converted = read_double_arg(0, static_cast<double>(1.5f));
    checks.expect(direct == 1.5 && bits_of(direct) == bits_of(converted) &&
                      bits_of(direct) == std::string("3FF8000000000000"),
                  u8"直接传 1.5f 与先转 double 再传，按 double 读出来逐位相同，都是 1.5");

    checks.expect(bits_of_float(1.5f) == std::string("3FC00000") &&
                      bits_of(1.5) == std::string("3FF8000000000000"),
                  u8"同一个 1.5 两种宽度：float 是 3FC00000，double 是 3FF8000000000000");

    checks.expect(read_int_arg(0, static_cast<char>(-1)) == -1 &&
                      read_int_arg(0, static_cast<short>(300)) == 300 &&
                      read_int_arg(0, true) == 1,
                  u8"比 int 窄的整型提升为 int：char、short、bool 按 int 读都是原值");

    /* 16—18 三处未定义行为在本机上的读数 */
    const double broken = read_double_bypassing_promotion(1.5f, 7);
    checks.expect(bits_of(broken) == std::string("0000000000000007"),
                  u8"UB 观测一：绕开提升按 double 读，本机读到的 8 字节是相邻实参 7，不是 1.5");

    const int narrow = read_int_arg(0, 1.1f);
    checks.expect(narrow == -1610612736 && bits_of_int(narrow) == std::string("A0000000"),
                  u8"UB 观测二：1.1f 按 int 读只取低 4 字节，本机读到 A0000000");

    const long long wide = read_longlong_arg(0, 1);
    checks.expect(wide == 1 && bits_of(wide) == std::string("0000000000000001"),
                  u8"UB 观测三：传 1 按 long long 读多读 4 字节，本机那 4 字节恰好是全零");

    /* 19—21 折叠表达式 */
    checks.expect(sum_all() == 0, u8"空包折叠：sum_all() 是 0，初值就是结果");
    checks.expect_eq(sum_all(1, 2, 3, 4), 10, u8"sum_all(1, 2, 3, 4) 是 10");
    checks.expect(sum_all(1, 2.5, 3) == 6.5 && sum_all(2.5, 2.5) == 5.0,
                  u8"同一个模板吃下 int 与 double：sum_all(1, 2.5, 3) 是 6.5");

    /* 22 两种写法结果一致 */
    bool same_values = sum_all(7) == sum_rec(7) && sum_all(7, 8) == sum_rec(7, 8) &&
                       sum_all(7, 8, 9) == sum_rec(7, 8, 9) &&
                       sum_all(7, 8, 9, 10) == sum_rec(7, 8, 9, 10) &&
                       sum_all(7, 8, 9, 10, 11) == sum_rec(7, 8, 9, 10, 11) &&
                       sum_all(7, 8, 9, 10, 11, 12) == sum_rec(7, 8, 9, 10, 11, 12);
    checks.expect(same_values, u8"递归展开版与折叠版在 1 到 6 个实参上结果逐位相同");

    /* 23 进入函数的次数 */
    reset_stats();
    const long long fold_value = sum_all(1, 2, 3, 4);
    const std::size_t fold_entries = stats().fold_entries;
    reset_stats();
    const long long rec_value = sum_rec(1, 2, 3, 4);
    const std::size_t rec_entries = stats().rec_entries;
    checks.expect(fold_value == 10 && rec_value == 10 && fold_entries == 1 && rec_entries == 4,
                  u8"4 个实参：折叠版进入函数 1 次，递归展开版进入 4 次，结果相同");

    /* 24—25 异构渲染 */
    checks.expect(print_all(u8"标签", 42, 2.5, u8"文本", true) == u8"标签, 42, 2.5, 文本, true",
                  u8"异构实参一次调用：文本、整数、浮点、bool 各按自己的规则渲染");
    checks.expect(print_all().empty() && print_all(1) == std::string("1"),
                  u8"空包渲染出空串，单个实参不加逗号");

    /* 26—30 initializer_list 的三条对照 */
    checks.expect(sum_il({1, 2, 3}) == 6 && sum_il({}) == 0,
                  u8"initializer_list 求和：{1, 2, 3} 是 6，空列表是 0");

    reset_counter_tally();
    const long long rvalue_sum = sum_counter_il({Counter(1), Counter(2), Counter(3)});
    checks.expect(rvalue_sum == 6 && counter_tally().copies == 0 && counter_tally().moves == 0,
                  u8"元素是右值时 0 次拷贝 0 次移动：元素直接构造在列表的底层数组里");

    const Counter ca(1);
    const Counter cb(2);
    const Counter cc(3);
    reset_counter_tally();
    const long long lvalue_sum = sum_counter_il({ca, cb, cc});
    checks.expect(lvalue_sum == 6 && counter_tally().copies == 3 && counter_tally().moves == 0,
                  u8"元素是左值时 3 次拷贝 0 次移动：3 个元素各拷一次");

    reset_counter_tally();
    const std::initializer_list<Counter> kept = {Counter(4), Counter(5)};
    const long long kept_sum = sum_counter_il(kept);
    checks.expect(kept_sum == 9 && counter_tally().copies == 0 && counter_tally().moves == 0,
                  u8"传入已有的列表：列表对象只复制指针与长度，元素一次也不拷");

    checks.expect(sum_il({1, 2, 3}) == sum_all(1, 2, 3),
                  u8"同一份数据两种写法结果相同：sum_il({1, 2, 3}) 与 sum_all(1, 2, 3) 都是 6");

    /* 31 报告本身 */
    const std::string report = build_report();
    checks.expect(report.find(u8"五、结构量汇总") != std::string::npos &&
                      report.find(u8"与 va_start 配平：是") != std::string::npos,
                  u8"报告五段都能生成，整份报告里 va_start 与 va_end 配平");

    return checks.finish();
}

}   /* namespace vlab */
