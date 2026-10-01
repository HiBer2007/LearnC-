/**
 * main_gui_qt.cpp —— Qt Widgets 版界面（可选的第二份界面）
 *
 * 与 main_gui_win32.cpp 功能相同：输入一串数，点「计算」出结果与柱状图，
 * 点「跑自测」显示自测结果。两份界面都只调用 core 里的 demo 命名空间，
 * 业务逻辑一行也不在这里。
 *
 * 这份界面默认【不构建】，因为它需要 Qt 6 Widgets：
 *     cmake --preset mingw-gdb -DWITH_QT=ON
 *     cmake --build --preset mingw-gdb
 * Qt 从哪来见 工具/获取依赖/README.md（自建静态 Qt，耗时较长）。
 * 只用 Win32 那份时 WITH_QT 保持默认的 OFF，不必准备 Qt。
 *
 * ── 与 Win32 版的对应关系 ──────────────────────────────────
 *   窗口      QMainWindow                      ← CreateWindowExW
 *   输入框    QLineEdit                        ← EDIT（单行）
 *   结果框    QPlainTextEdit（只读）           ← EDIT（多行只读）
 *   按钮      QPushButton                      ← BUTTON
 *   状态栏    QStatusBar::showMessage          ← STATIC
 *   柱状图    QWidget::paintEvent + QPainter   ← WM_PAINT + GDI
 *   点击      connect(...clicked)              ← WM_COMMAND
 *
 * 本文件没有用 Q_OBJECT：自绘控件用 std::function 回调，
 * 因此不依赖 moc。若读者要加自定义信号，请在 CMakeLists.txt 里
 * 打开 CMAKE_AUTOMOC（那里已经写好，只是当前用不到）。
 */
#include "vector_demo.hpp"

#include <QApplication>
#include <QByteArray>
#include <QColor>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMainWindow>
#include <QPaintEvent>
#include <QPainter>
#include <QPen>
#include <QPlainTextEdit>
#include <QPoint>
#include <QPushButton>
#include <QRect>
#include <QStatusBar>
#include <QString>
#include <QVBoxLayout>
#include <QWidget>

#include <cstddef>
#include <string>

namespace {

/* core 返回的是窄字符串（编译时 -fexec-charset=GBK），
   这里按本地 8 位编码转成 QString —— 与 Win32 版的 CP_ACP 是一回事 */
QString to_qstring(const std::string &text)
{
    return QString::fromLocal8Bit(text.c_str());
}

/* 界面上的文本转回窄字符串，交给 core */
std::string from_qstring(const QString &text)
{
    const QByteArray bytes = text.toLocal8Bit();
    return std::string(bytes.constData());
}

/* 拼「标签：值」这样一行。
   标签用 QStringLiteral：它以 UTF-16 存进 exe，与编译器把窄字符串
   按哪种字符集写进去无关。这一点在本文件里是必须的 —— 链接了 Qt 的
   目标不能传字符集选项（Qt 自己会加 /utf-8，再传就报 D8016），
   因此不能指望「窄字符串按 GBK 写」这条约定。
   值来自 core，是 GBK 窄字符串，仍由 to_qstring 按本地 8 位编码转。 */
QString labeled_line(const QString &label, const std::string &value)
{
    return label + to_qstring(value) + QStringLiteral("\r\n");
}

/* ── 柱状图：对应 Win32 版的 paint_chart ──────────────── */

class ChartWidget : public QWidget {
public:
    explicit ChartWidget(QWidget *parent = nullptr) : QWidget(parent)
    {
        setMinimumHeight(220);
    }

    void setValues(const IntVector &values, const QString &note)
    {
        values_ = values;
        note_ = note;
        update();                       /* 请求重画，等价于 InvalidateRect */
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        const QRect area = rect();

        painter.fillRect(area, QColor(250, 250, 252));
        painter.setPen(QColor(170, 170, 175));
        painter.drawRect(area.adjusted(0, 0, -1, -1));

        const int padding = 10;
        const int label_height = 20;
        const int base = area.height() - padding - label_height;

        if (!values_.empty()) {
            int maximum = 1;
            for (std::size_t i = 0; i < values_.size(); ++i) {
                if (values_[i] > maximum) {
                    maximum = values_[i];
                }
            }
            const int inner_width = area.width() - 2 * padding;
            const int inner_height = base - padding - label_height;
            const int slot = inner_width / static_cast<int>(values_.size());
            int bar_width = slot - 6;
            if (bar_width < 3) {
                bar_width = 3;
            }

            for (std::size_t i = 0; i < values_.size(); ++i) {
                const int height = static_cast<int>(
                    static_cast<long long>(values_[i]) * inner_height / maximum);
                const QRect bar(padding + static_cast<int>(i) * slot + 3,
                                base - height, bar_width, height);
                painter.fillRect(bar, bar_color(i));
            }
        }

        painter.setPen(QColor(60, 60, 70));
        painter.drawText(QPoint(padding, padding + 14), note_);

        const QString axis = values_.empty()
            ? QStringLiteral("柱状图（QPainter 绘制）：等待数据")
            : QStringLiteral("柱状图（QPainter 绘制）：共 %1 根柱子，高度与数值成正比")
                  .arg(values_.size());
        painter.drawText(QPoint(padding, area.height() - 6), axis);
    }

private:
    static QColor bar_color(std::size_t index)
    {
        switch (index % 4U) {
        case 0: return QColor(70, 130, 180);
        case 1: return QColor(60, 179, 113);
        case 2: return QColor(218, 165, 32);
        default: return QColor(205, 92, 92);
        }
    }

    IntVector values_;
    QString note_;
};

/* ── 主窗口 ───────────────────────────────────────────── */

class MainWindow : public QMainWindow {
public:
    MainWindow()
    {
        setWindowTitle(QStringLiteral("示例 07 · IntVector 值类型演示（Qt 版）"));

        input_ = new QLineEdit(QStringLiteral("1 1 2 3 5 8 13 21 34 55"));
        auto *compute_button = new QPushButton(QStringLiteral("计算"));
        auto *self_test_button = new QPushButton(QStringLiteral("跑自测"));

        auto *input_row = new QHBoxLayout;
        input_row->addWidget(new QLabel(QStringLiteral("输入（空格、逗号或分号分隔的整数）：")));
        input_row->addWidget(input_, 1);
        input_row->addWidget(compute_button);
        input_row->addWidget(self_test_button);

        output_ = new QPlainTextEdit;
        output_->setReadOnly(true);

        chart_ = new ChartWidget;

        auto *central = new QWidget;
        auto *layout = new QVBoxLayout(central);
        layout->addLayout(input_row);
        layout->addWidget(new QLabel(QStringLiteral("结果（只读）：")));
        layout->addWidget(output_, 1);
        layout->addWidget(new QLabel(QStringLiteral("柱状图：")));
        layout->addWidget(chart_, 2);
        setCentralWidget(central);

        connect(compute_button, &QPushButton::clicked, this, [this] { compute(); });
        connect(self_test_button, &QPushButton::clicked, this, [this] { self_test(); });

        resize(760, 640);
        compute();                      /* 启动时先算一次，窗口里直接有内容 */
    }

private:
    void compute()
    {
        bool ok = false;
        std::string error;
        const IntVector values = demo::parse_numbers(from_qstring(input_->text()), ok, error);
        if (!ok) {
            chart_->setValues(IntVector{}, QStringLiteral("输入有误，没有可画的数据"));
            output_->setPlainText(QStringLiteral("输入有误：") + to_qstring(error)
                                  + QStringLiteral("\r\n请用空格、逗号或分号分隔整数。"));
            statusBar()->showMessage(QStringLiteral("输入有误"));
            return;
        }

        const IntVector sums = demo::prefix_sum(values);
        QString shown;
        shown += labeled_line(QStringLiteral("输入数列 : "), demo::to_text(values));
        shown += labeled_line(QStringLiteral("前缀和   : "), demo::to_text(sums));
        shown += labeled_line(QStringLiteral("每项乘 3 : "), demo::to_text(values * 3));
        shown += labeled_line(QStringLiteral("每项加 1 : "),
                              demo::to_text(demo::add_scalar(values, 1)));
        output_->setPlainText(shown);

        chart_->setValues(values, QStringLiteral("共 %1 项，总和 %2")
                                      .arg(values.size())
                                      .arg(sums.at(sums.size() - 1)));
        statusBar()->showMessage(QStringLiteral("长度 %1，容量 %2，首项 %3，末项 %4")
                                     .arg(values.size())
                                     .arg(values.capacity())
                                     .arg(values[0])
                                     .arg(values.at(values.size() - 1)));
    }

    void self_test()
    {
        const demo::CheckResult result = demo::run_self_tests();
        QString shown;
        for (const std::string &line : result.lines) {
            shown += to_qstring(line) + QStringLiteral("\r\n");
        }
        shown += QStringLiteral("\r\n自测结果：") + to_qstring(result.summary());
        output_->setPlainText(shown);
        statusBar()->showMessage(to_qstring(result.summary()));
    }

    QLineEdit *input_ = nullptr;
    QPlainTextEdit *output_ = nullptr;
    ChartWidget *chart_ = nullptr;
};

}   /* namespace */

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    MainWindow window;
    window.show();
    return QApplication::exec();
}
