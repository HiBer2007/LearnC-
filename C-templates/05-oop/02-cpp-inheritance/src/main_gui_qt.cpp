/**
 * main_gui_qt.cpp —— 空模板 07 的 Qt 界面版（Qt Widgets）
 *
 * 与 src/main_gui_win32.cpp 是同一个窗口的两份实现：状态栏、图形列表、
 * 五个按钮，以及标了「界面连接」的 TODO 都一一对应。
 * 业务逻辑仍然只在 core 里，界面文件只负责调用它并把结果画出来。
 *
 * 构建（需要先准备好仓库内的静态 Qt，见《配置步骤.md》的「两份界面」一节）：
 *     cmake --preset mingw-gdb -DWITH_QT=ON
 *     cmake --build --preset mingw-gdb
 *     build\mingw\bin\app_gui_qt.exe
 *
 * 需要你补的与 Win32 版完全相同：refresh、5 个按钮的处理函数，以及绘制循环。
 */
#include <QApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>

#include <cstdio>
#include <vector>

#include "shape.hpp"

/* 窗口持有的图形对象：全部通过基类指针访问 */
static std::vector<Shape *> g_shapes;

/* 已给出：UTF-8 的窄字符串转成 Qt 字符串 */
static QString to_qstring(const char *s)
{
    return QString::fromUtf8(s);
}

/* 已给出：把 g_shapes 里非空的元素收进一个只读视图，供 count_circles 使用 */
std::vector<const Shape *> shape_view()
{
    std::vector<const Shape *> view;
    for (std::size_t i = 0; i < g_shapes.size(); ++i) {
        if (g_shapes[i] != nullptr) {
            view.push_back(g_shapes[i]);
        }
    }
    return view;
}

/* ------------------------------------------------------------------
 * 画布：图形列表画在这里
 * ------------------------------------------------------------------ */
class Canvas : public QWidget {
public:
    explicit Canvas(QWidget *parent = nullptr) : QWidget(parent)
    {
        setMinimumHeight(180);
    }

protected:
    /* TODO（界面连接 3）：把图形列表画出来
     * 要求（与 Win32 版的 WM_PAINT 循环相同）：
     *   1. 先画一行标题；
     *   2. 遍历 g_shapes，对每个非空对象取 shape->name() 与 shape->area()
     *      ——两者都是虚函数调用，同一个循环里对不同派生对象会取到不同实现；
     *   3. 用 as_circle(shape) 判断它是不是圆，是圆就在行首加一个 "O "；
     *   4. 在文字右侧画一根横条，宽度与面积成正比。
     * 提示：
     *     QPainter painter(this);
     *     painter.drawText(10, 20, to_qstring("图形列表（同一行代码，靠虚函数画出不同结果）："));
     *     char narrow[256];
     *     std::snprintf(narrow, sizeof(narrow), "%s  面积 = %.4f", shape->name(), shape->area());
     *     painter.drawText(30, y, to_qstring(narrow));
     *     painter.fillRect(300, y - 12, static_cast<int>(shape->area() * 4), 14, Qt::darkGray);
     * 验收：圆那一行的面积是 12.5664、横条最长；矩形是 12.0000；
     *       若所有行的面积都是 0.0000，说明阶段 1 的虚函数还没实现。 */
    void paintEvent(QPaintEvent *) override
    {
    }
};

class MainWindow : public QWidget {
public:
    MainWindow();
    ~MainWindow() override;   /* 退出前释放 g_shapes 里的对象 */

private:
    /* TODO（界面连接 1）：把当前状态显示到状态栏上
     * 要求：一行文字里至少包含四项：图形个数、总面积、圆的个数、活着的对象数
     *       （Shape::live_count()）。圆的个数用 count_circles(view.data(), view.size()) 取。
     * 提示：
     *     std::vector<const Shape *> view = shape_view();
     *     char narrow[512];
     *     std::snprintf(narrow, sizeof(narrow), "...", ...);
     *     status_->setText(to_qstring(narrow));
     * 验收：与 Win32 版一致——加两个图形后个数与总面积跟着变；
     *       删除之后个数与 live 一起下降。
     */
    void refresh();

    void onAddCircle();     /* TODO（界面连接 2a）：加一个圆 */
    void onAddRect();       /* TODO（界面连接 2b）：加一个矩形 */
    void onDelLast();       /* TODO（界面连接 2c）：删除最后一个 */
    void onCountCircle();   /* TODO（界面连接 2d）：统计圆的个数 */
    void onClearAll();      /* TODO（界面连接 2e）：清空 */

    /* 已给出：状态栏与画布一起刷新，处理函数末尾调用它即可 */
    void updateViews()
    {
        refresh();
        canvas_->update();
    }

    QLabel *status_;
    Canvas *canvas_;
};

MainWindow::MainWindow()
{
    /* 已给出：控件、布局与信号槽连接。
     * 这里的每个 connect 与 Win32 版 WM_COMMAND 里的一个 case 一一对应。 */
    setWindowTitle(to_qstring("空模板 07 · 继承与多态（Qt 界面）"));

    status_ = new QLabel(this);
    canvas_ = new Canvas(this);

    QPushButton *add_circle = new QPushButton(to_qstring("加圆"), this);
    QPushButton *add_rect = new QPushButton(to_qstring("加矩形"), this);
    QPushButton *del_last = new QPushButton(to_qstring("删除最后一个"), this);
    QPushButton *count_circle = new QPushButton(to_qstring("统计圆"), this);
    QPushButton *clear_all = new QPushButton(to_qstring("清空"), this);

    connect(add_circle, &QPushButton::clicked, this, &MainWindow::onAddCircle);
    connect(add_rect, &QPushButton::clicked, this, &MainWindow::onAddRect);
    connect(del_last, &QPushButton::clicked, this, &MainWindow::onDelLast);
    connect(count_circle, &QPushButton::clicked, this, &MainWindow::onCountCircle);
    connect(clear_all, &QPushButton::clicked, this, &MainWindow::onClearAll);

    QHBoxLayout *buttons = new QHBoxLayout;
    buttons->addWidget(add_circle);
    buttons->addWidget(add_rect);
    buttons->addWidget(del_last);
    buttons->addWidget(count_circle);
    buttons->addWidget(clear_all);

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->addWidget(status_);
    layout->addLayout(buttons);
    layout->addWidget(canvas_);

    updateViews();
}

MainWindow::~MainWindow()
{
    /* 已给出：退出前把 g_shapes 里的对象全部 delete（与 Win32 版的 WM_DESTROY 相同） */
    for (std::size_t i = 0; i < g_shapes.size(); ++i) {
        delete g_shapes[i];
    }
    g_shapes.clear();
}

/* TODO（界面连接 1）：见类内的说明 */
void MainWindow::refresh()
{
}

/* TODO（界面连接 2a）
 * 要求：让工厂造一个圆（半径 2），非空则放进 g_shapes，然后调用 updateViews()。
 * 提示：Shape *shape = make_shape("circle", 2.0);
 * 验收：画布上多出一行 "circle"，状态栏的个数加一。 */
void MainWindow::onAddCircle()
{
}

/* TODO（界面连接 2b）
 * 要求：让工厂造一个 3 x 4 的矩形，放进 g_shapes，然后调用 updateViews()。
 * 验收：画布上多出一行 "rect"，它的面积是 12.0000。 */
void MainWindow::onAddRect()
{
}

/* TODO（界面连接 2c）
 * 要求：删除最后一个图形，然后调用 updateViews()。
 * 提示：g_shapes 里存的是基类指针，delete 时派生类的析构函数是否被调用，
 *       取决于基类析构是不是虚函数（阶段 2 的验收点）。
 * 验收：画布上少一行，live 少一。 */
void MainWindow::onDelLast()
{
}

/* TODO（界面连接 2d）
 * 要求：统计当前有几个圆并显示到状态栏。
 * 提示：用 count_circles 与 shape_view()；可以拼进 refresh 的那行文字里单独显示。
 * 验收：加两个圆一个矩形后，状态栏显示圆的个数为 2。 */
void MainWindow::onCountCircle()
{
}

/* TODO（界面连接 2e）
 * 要求：删除全部图形并清空 g_shapes，然后调用 updateViews()。
 * 验收：画布上不再有图形行，live 回到 0。 */
void MainWindow::onClearAll()
{
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    MainWindow window;
    window.resize(660, 380);
    window.show();
    return app.exec();
}
