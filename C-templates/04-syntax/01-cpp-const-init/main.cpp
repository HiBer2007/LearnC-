/**
 * 空模板 05 · const 与初始化（单文件）
 *
 * 成品形态
 *     一个「单位换算小工具」：内置一张只读的换算表，能按名字查找单位、
 *     在两个单位之间换算，并按设定的小数位打印结果。
 *     程序一次跑完 4 个阶段，每阶段打印一段固定格式的结果。
 *
 * 用到哪几章
 *     《04-语法/03-常量与 const.md》   const 变量、const 指针、const 引用、const 成员函数
 *     《04-语法/05-初始化.md》         初始化列表、类内初始化
 *     《04-语法/12-编译期能力.md》     constexpr、static_assert、if constexpr
 *
 * 编译与运行
 *     g++ -std=c++17 -g -O0 -Wall -Wextra main.cpp -o unit_tool.exe
 *     .\unit_tool.exe
 *
 * 程序的输出刻意写成 ASCII（注释仍是中文），避免控制台代码页带来的干扰，
 * 这样自查时可以把输出与《配置步骤.md》里的期望值逐字对照。
 *
 * 本文件只留骨架与 TODO，分 4 个阶段。每个阶段做什么、验收标准是什么，
 * 见同目录《配置步骤.md》。除 TODO 标记处外，其余代码用来固定输出格式，不要改动。
 */
#include <cstddef>
#include <cstdio>
#include <type_traits>

/* ==================================================================
 * 阶段 1 · 换算表：const 与 constexpr 常量
 * ================================================================== */

struct Unit {
    const char *name;    /* 单位名，例如 "km" */
    double      factor;  /* 1 个该单位等于多少个基准单位（长度以米为基准） */
};

/* 这张表只读：kUnits 是 constexpr 对象，本身隐含 const。
 * 表写死在这里，运行期不允许任何代码改动它。 */
constexpr Unit kUnits[] = {
    {"mm", 0.001},
    {"cm", 0.01},
    {"m", 1.0},
    {"km", 1000.0},
    {"inch", 0.0254},
    {"ft", 0.3048},
    {"mile", 1609.344},
};

/* TODO（阶段 1-1）
 * 要求：算出上表有多少个单位，把 kUnitCount 改成正确的常量表达式。
 * 提示：sizeof(kUnits) / sizeof(kUnits[0])，结果是 std::size_t。
 * 验收：阶段 1 的表头打印 count = 7。
 *       写成 sizeof(kUnits) 会得到整个数组的字节数，是一眼能看出的错误值。
 */
constexpr std::size_t kUnitCount = 0;

/* TODO（阶段 1-2）
 * 要求：打印整张表，每行形如 "  mm     x 0.001"。
 * 提示：形参保持 const Unit *units（声明表是只读的）；行内用 %.6s 一类的宽度
 *       控制对齐，小数点后用 %g。
 * 验收：输出 7 行；随后把形参改成 Unit *units，main 里传 kUnits 会编译失败
 *       （kUnits 是 const 对象，这正是 const 在起作用），试完记得改回来。
 */
void print_table(const Unit *units, std::size_t count)
{
    (void)units;
    (void)count;
    std::printf("  (TODO 阶段 1-2：换算表还没有打印)\n");
}

/* ==================================================================
 * 阶段 2 · 查找与换算：const 在指针与引用上
 * ================================================================== */

/* TODO（阶段 2-1）
 * 要求：按名字查找单位，找到返回指向表内元素的指针，找不到返回 nullptr。
 *       返回类型保持 const Unit *：查找的结果不允许被调用方改写。
 * 提示：逐个比较名字用 std::strcmp（需要 #include <cstring>，自己在文件顶部补）。
 * 验收：find_unit(kUnits, kUnitCount, "km")   指向表里的 "km"；
 *       find_unit(kUnits, kUnitCount, "kg")   得到 nullptr。
 */
const Unit *find_unit(const Unit *units, std::size_t count, const char *name)
{
    (void)units;
    (void)count;
    (void)name;
    return nullptr;
}

/* TODO（阶段 2-2）
 * 要求：把 v 从 from 单位换算到 to 单位，两个形参都用 const Unit &。
 *       公式：v * from.factor / to.factor。
 * 验收：3 km 换算成 mile 约为 1.86411；3 km 换算成 m 为 3000。
 *       把形参改成 Unit & 之后，main 里传表内元素会编译失败
 *       （constexpr 表的元素是 const 对象），试完记得改回来。
 */
double convert(const Unit &from, const Unit &to, double v)
{
    (void)from;
    (void)to;
    (void)v;
    return 0.0;
}

/* ==================================================================
 * 阶段 3 · Converter 类：初始化列表、类内初始化、const 成员函数
 * ================================================================== */

class Converter {
public:
    /* TODO（阶段 3-1）
     * 要求：用【成员初始化列表】把 name_ 与 factor_ 初始化，不要写在函数体里赋值。
     * 验收：main 里打印出的名字与倍数正确。
     * 提示：初始化列表里两个成员的书写顺序若与声明顺序相反，-Wall -Wextra 会给出
     *       "will be initialized after" 警告，这属于故意可复现的现象，见《配置步骤.md》。
     */
    Converter(const char *name, double factor)
    {
        (void)name;
        (void)factor;
    }

    /* TODO（阶段 3-2）
     * 要求：让 alive_ 在对象消失时自减。
     * 验收：阶段 3 在作用域内打印 alive = 1，出作用域后打印 alive = 0。
     */
    ~Converter() { }

    /* 下面三个是 const 成员函数：它们承诺不改动对象。
     * 漏写 const，main 里对 const Converter 对象的调用就会编译失败。 */
    const char *name() const { return name_; }
    double factor() const { return factor_; }

    /* TODO（阶段 3-3）
     * 要求：把 v 个本单位换算成基准单位（v * factor_）。
     * 验收：print_value(1.0) 打印 "1 mile = 1609.344 base"。
     */
    double to_base(double v) const
    {
        (void)v;
        return 0.0;
    }

    void print_value(double v) const
    {
        std::printf("  %g %s = %.*f base\n", v, name_, precision_, to_base(v));
    }

    static int alive() { return alive_; }

private:
    /* TODO（阶段 3-4）
     * 要求：类内初始化，默认保留 3 位小数。
     * 验收：print_value 打印出三位小数；改成 0 会看到 1609。
     */
    int precision_ = 0;

    const char *name_ = "?";
    double      factor_ = 0.0;

    static int alive_;   /* 静态成员的声明，定义在类外，已给出 */
};

int Converter::alive_ = 0;

/* ==================================================================
 * 阶段 4 · 编译期求值：constexpr 函数、static_assert、if constexpr
 * ================================================================== */

/* TODO（阶段 4-1）
 * 要求：把 to_base_ct 声明成 constexpr（其余不动）。
 * 验收：下面阶段 4-2 的两条 static_assert 取消注释后能编过；
 *       去掉 constexpr 会报 "non-constant condition for static assertion"。
 */
double to_base_ct(double v, double factor)
{
    return v * factor;
}

/* TODO（阶段 4-2）
 * 要求：阶段 1-1 与阶段 4-1 做完之后，把下面两行取消注释。
 * 验收：取消注释后仍能编过。条件写错时，编译器给出的正是引号里那句话。
 *
 * static_assert(kUnitCount == 7, "换算表应当有 7 个单位");
 * static_assert(to_base_ct(1.0, kUnits[6].factor) == 1609.344, "mile 的倍数不对");
 */

/* TODO（阶段 4-3，进阶）
 * 要求：用 if constexpr 让整数与浮点走不同分支：
 *       整数直接打印，浮点保留 3 位小数。
 * 提示：std::is_integral<T>::value（C++17 也可写 std::is_integral_v<T>）。
 *       两个分支在编译期只保留一个，因此分支里可以写只对某一种类型成立的代码。
 * 验收：print_number(5) 打印 "5"；print_number(1.5) 打印 "1.500"。
 *       现在这一版对两者都走 %g，因此第二行会打印 "1.5"。
 */
template <class T>
void print_number(const T &v)
{
    std::printf("  %g\n", static_cast<double>(v));
}

int main()
{
    std::printf("=== 阶段 1：换算表 ===\n");
    std::printf("count = %zu\n", kUnitCount);
    print_table(kUnits, kUnitCount);

    std::printf("\n=== 阶段 2：查找与换算 ===\n");
    const Unit *km = find_unit(kUnits, kUnitCount, "km");
    const Unit *mile = find_unit(kUnits, kUnitCount, "mile");
    const Unit *meter = find_unit(kUnits, kUnitCount, "m");
    const Unit *kg = find_unit(kUnits, kUnitCount, "kg");
    std::printf("  find km   -> %s\n", km ? km->name : "(nullptr)");
    std::printf("  find mile -> %s\n", mile ? mile->name : "(nullptr)");
    std::printf("  find kg   -> %s\n", kg ? kg->name : "(nullptr)");
    if (km && mile) {
        std::printf("  3 km = %.5f mile\n", convert(*km, *mile, 3.0));
    } else {
        std::printf("  3 km = ?（阶段 2-1 还没有做完）\n");
    }
    if (km && meter) {
        std::printf("  3 km = %.1f m\n", convert(*km, *meter, 3.0));
    }

    std::printf("\n=== 阶段 3：Converter 类 ===\n");
    {
        const Converter mile_c("mile", 1609.344);
        std::printf("  name = %s, factor = %g\n", mile_c.name(), mile_c.factor());
        mile_c.print_value(1.0);
        std::printf("  alive = %d\n", Converter::alive());
    }
    std::printf("  alive = %d\n", Converter::alive());

    std::printf("\n=== 阶段 4：编译期求值 ===\n");
    std::printf("  to_base_ct(1.0, 1000.0) = %g\n", to_base_ct(1.0, 1000.0));
    print_number(5);
    print_number(1.5);

    return 0;
}
