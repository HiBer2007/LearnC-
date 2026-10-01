/**
 * shape.cpp —— Shape / Circle / Rect 的实现（空模板 07）
 *
 * 骨架里的虚函数都是返回 0 的占位版本，因此面积与周长全是 0；
 * 虚析构、工厂、dynamic_cast 也还没有实现。
 * 把每个 TODO 换成真正的实现，逐阶段对照《配置步骤.md》的验收标准。
 */
#include "shape.hpp"

#include <cstdio>
#include <cstring>
#include <typeinfo>

/* ==================================================================
 * Shape：基类里已经写好的部分
 * ================================================================== */

int Shape::live_count_ = 0;

Shape::Shape(const char *name) : name_(name)
{
    ++live_count_;
}

/* TODO（阶段 2-1）
 * 要求：在头文件里给这个析构函数加上 virtual（函数体不用改）。
 * 验收：阶段 2 通过基类指针 delete 时，能看到派生类与基类两行析构日志；
 *       不加 virtual 时只有 "析构 Shape(circle)" 一行，派生类那一行不会出现。
 */
Shape::~Shape()
{
    std::printf("  析构 Shape(%s)\n", name_);
    --live_count_;
}

const char *Shape::name() const
{
    return name_;
}

/* 非虚函数，内部调用的是虚函数，因此运行期仍然会选到派生类那一份 */
void Shape::describe() const
{
    std::printf("  %-7s 面积 = %.4f，周长 = %.4f\n", name(), area(), perimeter());
}

int Shape::live_count()
{
    return live_count_;
}

/* ==================================================================
 * Circle
 * ================================================================== */

Circle::Circle(double r) : Shape("circle"), r_(r)
{
}

/* TODO（阶段 1-1）
 * 要求：面积 = 圆周率 × 半径²。圆周率用 3.14159265358979323846 一类的常量。
 * 验收：阶段 1 里半径 2 的圆，面积打印 12.5664。
 */
double Circle::area() const
{
    (void)r_;
    return 0.0;
}

/* TODO（阶段 1-2）
 * 要求：周长 = 2 × 圆周率 × 半径。
 * 验收：阶段 1 里半径 2 的圆，周长打印 12.5664。
 */
double Circle::perimeter() const
{
    return 0.0;
}

/* TODO（阶段 2-2）
 * 要求：在头文件里给这个析构函数加上 override，函数体打印一行日志：
 *           std::printf("  析构 Circle(半径 %g)\n", r_);
 * 验收：阶段 2 里通过基类指针 delete 一个圆，能看到这一行。
 */
Circle::~Circle()
{
    /* TODO */
}

/* ==================================================================
 * Rect
 * ================================================================== */

Rect::Rect(double w, double h) : Shape("rect"), w_(w), h_(h)
{
}

/* TODO（阶段 1-3）
 * 要求：面积 = 宽 × 高。
 * 验收：阶段 1 里 3 × 4 的矩形，面积打印 12.0000。
 */
double Rect::area() const
{
    return 0.0;
}

/* TODO（阶段 1-4）
 * 要求：周长 = 2 × (宽 + 高)。
 * 验收：阶段 1 里 3 × 4 的矩形，周长打印 14.0000。
 */
double Rect::perimeter() const
{
    return 0.0;
}

/* TODO（阶段 2-3）
 * 要求：在头文件里给这个析构函数加上 override，函数体打印一行日志：
 *           std::printf("  析构 Rect(%g x %g)\n", w_, h_);
 */
Rect::~Rect()
{
    /* TODO */
}

/* ==================================================================
 * 工厂
 * ================================================================== */

/* TODO（阶段 3-1）
 * 要求：
 *     "circle" → new Circle(a)
 *     "rect"   → new Rect(a, b)
 *     其它名字 → 返回 nullptr
 * 提示：用 std::strcmp 比较字符串。
 * 验收：阶段 3 里 make_shape("circle", 2.0) 造出圆，
 *       make_shape("triangle", 1.0) 得到 nullptr（程序不会崩）。
 */
Shape *make_shape(const char *kind, double a, double b)
{
    (void)kind;
    (void)a;
    (void)b;
    return nullptr;
}

/* ==================================================================
 * 运行期类型识别
 * ================================================================== */

/* TODO（阶段 4-1）
 * 要求：用 dynamic_cast<const Circle *>(s)，转不过去时得到 nullptr。
 * 验收：阶段 4 里对圆得到非空指针、对矩形得到 nullptr。
 */
const Circle *as_circle(const Shape *s)
{
    (void)s;
    return nullptr;
}

/* TODO（阶段 4-2）
 * 要求：遍历数组，用 as_circle 判断每一项是不是圆，返回圆的个数。
 * 验收：阶段 4 里 5 个图形中有 2 个圆，打印 2。
 */
int count_circles(const Shape *const *shapes, std::size_t n)
{
    (void)shapes;
    (void)n;
    return 0;
}

/* TODO（阶段 4-3）
 * 要求：用 dynamic_cast<const Circle &>(s) 转换并返回。
 *       转不过去时 dynamic_cast 自己会抛 std::bad_cast，不用手写 throw。
 * 验收：阶段 4 里对圆调用成功并打印半径，对矩形抛出 std::bad_cast 被 main 接住。
 *       现在的占位版本对任何对象都抛 std::bad_cast。
 */
const Circle &as_circle_ref(const Shape &s)
{
    (void)s;
    throw std::bad_cast();
}
