/**
 * shape.hpp —— 图形抽象基类与两个派生类（空模板 07）
 *
 * 接口已经定好，main_cli.cpp 与 main_gui.cpp 都按这份接口写好了驱动。
 * 你要做的是在 src/shape.cpp 里把标了 TODO 的成员函数实现出来。
 *
 * 约定（两个驱动都依赖这些约定）：
 *   1. Shape 是抽象类：area() 与 perimeter() 是纯虚函数，不能直接造 Shape 对象
 *   2. 通过基类指针 delete 派生类对象时，派生类的析构函数必须被调用
 *   3. Shape::live_count() 统计当前活着的图形对象个数（含派生类对象），最终回到 0
 */
#ifndef SHAPE_HPP
#define SHAPE_HPP

#include <cstddef>

class Shape {
public:
    explicit Shape(const char *name);
    ~Shape();                                  /* TODO（阶段 2-1）：给它加上 virtual */

    virtual double area() const = 0;           /* 纯虚：只留接口 */
    virtual double perimeter() const = 0;      /* 纯虚：只留接口 */
    virtual const char *name() const;          /* 非纯虚：默认返回构造时记下的名字 */

    void describe() const;                     /* 非虚函数，内部调用上面三个虚函数 */

    static int live_count();                   /* 当前活着的图形个数 */

protected:
    const char *name_;

private:
    static int live_count_;
};

/* ------------------------------------------------------------------
 * 圆：半径
 * ------------------------------------------------------------------ */
class Circle : public Shape {
public:
    explicit Circle(double r);
    ~Circle();                                 /* TODO（阶段 2-2）：加 override 并打印析构日志 */

    double area() const override;              /* TODO（阶段 1-1） */
    double perimeter() const override;         /* TODO（阶段 1-2） */

    double radius() const { return r_; }       /* 只有 Circle 有的操作 */

private:
    double r_;
};

/* ------------------------------------------------------------------
 * 矩形：宽与高
 * ------------------------------------------------------------------ */
class Rect : public Shape {
public:
    Rect(double w, double h);
    ~Rect();                                   /* TODO（阶段 2-3）：加 override 并打印析构日志 */

    double area() const override;              /* TODO（阶段 1-3） */
    double perimeter() const override;         /* TODO（阶段 1-4） */

    bool is_square() const { return w_ == h_; }  /* 只有 Rect 有的操作 */

private:
    double w_;
    double h_;
};

/* ------------------------------------------------------------------
 * 工厂：按名字造对象，不认识的种类返回 nullptr
 * ------------------------------------------------------------------ */
Shape *make_shape(const char *kind, double a, double b = 0.0);   /* TODO（阶段 3-1） */

/* ------------------------------------------------------------------
 * 运行期类型识别（RTTI）
 * ------------------------------------------------------------------ */

/* 是圆就返回指向它的指针，否则返回 nullptr */
const Circle *as_circle(const Shape *s);                         /* TODO（阶段 4-1） */

/* 统计数组里有多少个圆 */
int count_circles(const Shape *const *shapes, std::size_t n);    /* TODO（阶段 4-2） */

/* 引用版：确定它一定是圆时使用；不是圆则抛 std::bad_cast */
const Circle &as_circle_ref(const Shape &s);                     /* TODO（阶段 4-3） */

#endif /* SHAPE_HPP */
