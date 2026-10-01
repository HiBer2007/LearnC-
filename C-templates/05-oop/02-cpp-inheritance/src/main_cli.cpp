/**
 * main_cli.cpp —— 空模板 07 的命令行验收程序（C++）
 *
 * 这个文件**不需要改**：它按 4 个阶段使用 Shape / Circle / Rect，
 * 把结果打印成《配置步骤.md》里的期望输出。你的实现写在 src/shape.cpp 里。
 * 界面版（src/main_gui_win32.cpp 与 src/main_gui_qt.cpp）用同一个核心模块，把同样的东西画在窗口上。
 *
 * 构建与运行：
 *     cmake --preset mingw-gdb
 *     cmake --build --preset mingw-gdb
 *     build\mingw\bin\app_cli.exe
 *
 * 阶段的划分：
 *     阶段 1  虚函数：面积与周长
 *     阶段 2  虚析构：通过基类指针 delete
 *     阶段 3  工厂与多态遍历
 *     阶段 4  dynamic_cast：转到指针与转到引用
 */
#include <cstddef>
#include <cstdio>
#include <stdexcept>
#include <typeinfo>
#include <vector>

#include "shape.hpp"

/* ==================================================================
 * 阶段 1 · 面积与周长
 * ================================================================== */
static void stage1()
{
    std::printf("=== 阶段 1：面积与周长 ===\n");

    const Circle circle(2.0);
    const Rect rect(3.0, 4.0);

    /* describe() 是非虚函数，但它内部调用的三个函数都是虚函数，
     * 因此同一个调用点上，圆与矩形各走各的实现。 */
    circle.describe();
    rect.describe();

    std::printf("  只有 Circle 有的操作：半径 = %g\n", circle.radius());
    std::printf("  只有 Rect 有的操作：是正方形吗 = %s\n", rect.is_square() ? "是" : "否");
    std::printf("  live = %d\n", Shape::live_count());
}

/* ==================================================================
 * 阶段 2 · 虚析构
 * ================================================================== */
static void stage2()
{
    std::printf("\n=== 阶段 2：通过基类指针删除对象 ===\n");

    Shape *p = new Circle(1.5);
    std::printf("  delete 之前：live = %d\n", Shape::live_count());

    delete p;   /* 基类析构不是虚函数时，这里只会调用 Shape 的析构 */

    std::printf("  delete 之后：live = %d\n", Shape::live_count());
    std::printf("  析构日志里应当同时出现 Circle 与 Shape 两行\n");
}

/* ==================================================================
 * 阶段 3 · 工厂与多态遍历
 * ================================================================== */
static void stage3()
{
    std::printf("\n=== 阶段 3：工厂与多态遍历 ===\n");

    struct Spec {
        const char *kind;
        double a;
        double b;
    };
    const Spec specs[] = {
        {"circle", 2.0, 0.0},
        {"rect", 3.0, 4.0},
        {"circle", 1.0, 0.0},
        {"rect", 2.0, 2.0},
        {"triangle", 1.0, 0.0},   /* 不认识的名字，应当得到 nullptr */
    };

    std::vector<Shape *> shapes;
    int unknown = 0;
    for (const Spec &spec : specs) {
        Shape *shape = make_shape(spec.kind, spec.a, spec.b);
        if (shape == nullptr) {
            ++unknown;
            continue;
        }
        shapes.push_back(shape);
    }
    std::printf("  造出 %zu 个图形，%d 个名字不认识\n", shapes.size(), unknown);

    double total = 0.0;
    for (const Shape *shape : shapes) {
        shape->describe();
        total += shape->area();
    }
    std::printf("  总面积 = %.4f\n", total);
    std::printf("  live = %d\n", Shape::live_count());

    for (Shape *shape : shapes) {
        delete shape;
    }
    std::printf("  清理后 live = %d\n", Shape::live_count());
}

/* ==================================================================
 * 阶段 4 · dynamic_cast
 * ================================================================== */
static void stage4()
{
    std::printf("\n=== 阶段 4：dynamic_cast ===\n");

    std::vector<Shape *> shapes;
    Shape *made[] = {make_shape("circle", 2.0, 0.0),
                     make_shape("rect", 3.0, 4.0),
                     make_shape("circle", 1.0, 0.0)};
    for (Shape *shape : made) {
        if (shape != nullptr) {
            shapes.push_back(shape);
        }
    }

    if (shapes.size() < 2) {
        std::printf("  图形没造出来，先完成阶段 3-1\n");
        for (Shape *shape : shapes) {
            delete shape;
        }
        return;
    }

    std::vector<const Shape *> view(shapes.begin(), shapes.end());
    std::printf("  图形总数 = %zu，其中圆 = %d\n",
                view.size(), count_circles(view.data(), view.size()));

    const Circle *first = as_circle(view[0]);
    std::printf("  第一个是圆吗：%s\n", first != nullptr ? "是" : "否");
    if (first != nullptr) {
        std::printf("    它的半径 = %g\n", first->radius());
    }

    try {
        const Circle &ref = as_circle_ref(*view[0]);   /* 第一个是圆，应当成功 */
        std::printf("  引用版：成功，半径 = %g\n", ref.radius());
    } catch (const std::bad_cast &) {
        std::printf("  引用版：捕获到 std::bad_cast\n");
    }

    try {
        const Circle &ref = as_circle_ref(*view[1]);   /* 第二个是矩形，应当失败 */
        std::printf("  对矩形用引用版：成功了（不该发生），半径 = %g\n", ref.radius());
    } catch (const std::bad_cast &) {
        std::printf("  对矩形用引用版：捕获到 std::bad_cast\n");
    }

    for (Shape *shape : shapes) {
        delete shape;
    }
    std::printf("  清理后 live = %d\n", Shape::live_count());
}

int main()
{
    stage1();
    stage2();
    stage3();
    stage4();

    std::printf("\n=== 收尾 ===\n");
    std::printf("  live = %d（所有对象都应当被析构）\n", Shape::live_count());
    return 0;
}
