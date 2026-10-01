/**
 * main_gui_qt.cpp —— Qt Widgets 版界面（可选的第二份界面）
 *
 * 与 main_gui_win32.cpp 功能相同：上方三个面板对应工厂造出的三种导出器，
 * 点面板切换格式，下面显示渲染结果，另有「重绘」「跑自测」与
 * 「换成含转义字符的表格」两项操作。面板上的名字与说明来自虚函数
 * name() 与 hint()，本文件同样不知道有哪些派生类。
 *
 * 这份界面默认【不构建】，因为它需要 Qt 6 Widgets：
 *     cmake --preset mingw-gdb -DWITH_QT=ON
 *     cmake --build --preset mingw-gdb
 * Qt 从哪来见 工具/获取依赖/README.md（自建静态 Qt，耗时较长）。
 * 只用 Win32 那份时 WITH_QT 保持默认的 OFF，不必准备 Qt。
 *
 * ── 与 Win32 版的对应关系 ──────────────────────────────────
 *   窗口      QMainWindow                      ← CreateWindowExW
 *   面板      QWidget 子类 + paintEvent        ← WM_PAINT + GDI
 *   点面板    mousePressEvent                  ← WM_LBUTTONDOWN
 *   结果框    QPlainTextEdit（只读）           ← EDIT（多行只读）
 *   按钮      QPushButton / QCheckBox          ← BUTTON / BS_AUTOCHECKBOX
 *   状态栏    QStatusBar::showMessage          ← STATIC
 *
 * 本文件没有用 Q_OBJECT：面板用 std::function 回调，
 * 因此不依赖 moc。若读者要加自定义信号，请在 CMakeLists.txt 里
 * 打开 CMAKE_AUTOMOC（那里已经写好，只是当前用不到）。
 */
#include "report_demo.hpp"

#include <QApplication>
#include <QByteArray>
#include <QCheckBox>
#include <QColor>
#include <QHBoxLayout>
#include <QLabel>
#include <QMainWindow>
#include <QMouseEvent>
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
#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace {

/* core 返回的是窄字符串（编译时按 GBK 写进 exe），
   这里按本地 8 位编码转成 QString。

   本文件里界面自己的固定文字一律写成 QStringLiteral：它以 UTF-16 存进 exe，
   与窄字符串按哪种字符集写无关。链接了 Qt 的目标不能传字符集选项
   （Qt 自己会加 /utf-8，再传就报 D8016），因此这一点必须靠 QStringLiteral 保证。 */
QString to_qstring(const std::string &text)
{
    return QString::fromLocal8Bit(text.c_str());
}

/* ── 一个可点击的格式面板：对应 Win32 版 GDI 画的那三个矩形 ── */

class FormatPanel : public QWidget {
public:
    FormatPanel(QString title, QString hint, int index, QWidget *parent = nullptr)
        : QWidget(parent), title_(std::move(title)), hint_(std::move(hint)), index_(index)
    {
        setMinimumHeight(104);
        setCursor(Qt::PointingHandCursor);
    }

    void setSelected(bool selected)
    {
        selected_ = selected;
        update();
    }

    /** 点击时调用。用回调而不是自定义信号，省掉 moc */
    std::function<void()> on_click;

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        const QRect area = rect();

        painter.fillRect(area, panel_color(index_, selected_));

        QPen pen(selected_ ? QColor(40, 90, 160) : QColor(180, 180, 185));
        pen.setWidth(selected_ ? 2 : 1);
        painter.setPen(pen);
        painter.drawRect(area.adjusted(1, 1, -1, -1));

        painter.setPen(QColor(30, 60, 100));
        painter.drawText(QPoint(12, 30),
                         selected_ ? title_ + QStringLiteral("（当前）") : title_);
        painter.setPen(QColor(80, 80, 90));
        painter.drawText(QRect(12, 40, area.width() - 24, 40),
                         Qt::TextWordWrap, hint_);
        painter.setPen(QColor(130, 130, 140));
        painter.drawText(QPoint(12, 96), QStringLiteral("点这里切换格式"));
    }

    void mousePressEvent(QMouseEvent *event) override
    {
        if (event->button() == Qt::LeftButton && on_click) {
            on_click();
        }
    }

private:
    static QColor panel_color(int index, bool selected)
    {
        if (selected) {
            switch (index % 3) {
            case 0: return QColor(198, 224, 245);
            case 1: return QColor(205, 240, 213);
            default: return QColor(250, 236, 200);
            }
        }
        switch (index % 3) {
        case 0: return QColor(238, 244, 250);
        case 1: return QColor(240, 248, 242);
        default: return QColor(252, 249, 240);
        }
    }

    QString title_;
    QString hint_;
    int index_ = 0;
    bool selected_ = false;
};

/* ── 主窗口 ───────────────────────────────────────────── */

class MainWindow : public QMainWindow {
public:
    MainWindow()
    {
        setWindowTitle(QStringLiteral("示例 08 · 导出器：抽象基类、工厂与虚析构（Qt 版）"));

        formats_ = demo::format_infos();        /* 工厂 + 虚函数 */

        auto *panel_row = new QHBoxLayout;
        for (std::size_t i = 0; i < formats_.size(); ++i) {
            auto *panel = new FormatPanel(to_qstring(formats_[i].name),
                                          to_qstring(formats_[i].hint),
                                          static_cast<int>(i));
            panel->on_click = [this, i] { select(i); };
            panels_.push_back(panel);
            panel_row->addWidget(panel, 1);
        }

        output_ = new QPlainTextEdit;
        output_->setReadOnly(true);

        auto *repaint = new QPushButton(QStringLiteral("重绘"));
        tricky_ = new QCheckBox(QStringLiteral("换成含转义字符的表格"));
        auto *self_test_button = new QPushButton(QStringLiteral("跑自测"));

        auto *button_row = new QHBoxLayout;
        button_row->addWidget(repaint);
        button_row->addWidget(tricky_);
        button_row->addWidget(self_test_button);
        button_row->addStretch(1);

        auto *central = new QWidget;
        auto *layout = new QVBoxLayout(central);
        layout->addLayout(panel_row);
        layout->addWidget(new QLabel(QStringLiteral(
            "面板上的名字与说明来自虚函数 name() 与 hint()；"
            "本文件并不知道有哪几个派生类，新增格式只改工厂。")));
        layout->addWidget(new QLabel(QStringLiteral("渲染结果（只读）：")));
        layout->addWidget(output_, 1);
        layout->addLayout(button_row);
        setCentralWidget(central);

        connect(repaint, &QPushButton::clicked, this, [this] { refresh(); });
        connect(self_test_button, &QPushButton::clicked, this, [this] { self_test(); });
        connect(tricky_, &QCheckBox::toggled, this, [this](bool checked) {
            use_tricky_ = checked;
            refresh();
        });

        resize(800, 620);
        refresh();
    }

private:
    void select(std::size_t index)
    {
        selected_ = index;
        refresh();
    }

    void refresh()
    {
        if (formats_.empty()) {
            return;
        }
        for (std::size_t i = 0; i < panels_.size(); ++i) {
            panels_[i]->setSelected(i == selected_);
        }

        const demo::FormatInfo &info = formats_[selected_];
        const Table table = use_tricky_ ? demo::tricky_table() : demo::grade_table();

        bool ok = false;
        std::string error;
        const std::string text = demo::render(info.name, table, ok, error);
        if (ok) {
            output_->setPlainText(to_qstring(text));
        } else {
            output_->setPlainText(QStringLiteral("渲染失败：") + to_qstring(error));
        }

        statusBar()->showMessage(QStringLiteral("当前格式：") + to_qstring(info.name)
                                 + QStringLiteral("（") + to_qstring(info.hint)
                                 + QStringLiteral("）"));
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

    std::vector<demo::FormatInfo> formats_;
    std::vector<FormatPanel *> panels_;
    QPlainTextEdit *output_ = nullptr;
    QCheckBox *tricky_ = nullptr;
    std::size_t selected_ = 0;
    bool use_tricky_ = false;
};

}   /* namespace */

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    MainWindow window;
    window.show();
    return QApplication::exec();
}
