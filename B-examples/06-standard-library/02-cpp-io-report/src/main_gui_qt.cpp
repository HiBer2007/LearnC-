/**
 * main_gui_qt.cpp —— Qt Widgets 版界面（可选的第二份界面）
 *
 * 与 main_gui_win32.cpp 功能相同：输入文件路径，点「生成报表」出报表，
 * 点「跑自测」显示自测结果。两份界面都只调用 core 里的 sales 命名空间，
 * 业务逻辑一行也不在这里。
 *
 * 这份界面默认【不构建】，因为它需要 Qt 6 Widgets：
 *     $env:QT_ROOT = '<Qt 套件目录>'
 *     cmake --preset mingw-gdb -DWITH_QT=ON
 *     cmake --build --preset mingw-gdb
 * 只用 Win32 那份时 WITH_QT 保持默认的 OFF，不必准备 Qt。
 *
 * ── 与 Win32 版的对应关系 ──────────────────────────────────
 *   窗口      QWidget                            ← CreateWindowExW
 *   路径框    QLineEdit                          ← EDIT（单行）
 *   报表框    QPlainTextEdit（只读）             ← EDIT（多行只读）
 *   按钮      QPushButton                        ← BUTTON
 *   布局      QVBoxLayout / QHBoxLayout          ← 控件坐标写死
 *   状态栏    QLabel                             ← STATIC
 *   点击      connect(...clicked)                ← WM_COMMAND
 *
 * 本文件没有用 Q_OBJECT，因此不依赖 moc；CMakeLists.txt 里仍然打开了
 * CMAKE_AUTOMOC，读者要加自定义信号时不必再改工程。
 *
 * 界面上的固定文字一律写成 QStringLiteral：它以 UTF-16 存进 exe，
 * 与「窄字符串按哪种字符集写进去」无关。这一点在本文件里是必须的 ——
 * 链接了 Qt 的目标不能传字符集选项（Qt 自己会加 /utf-8，再传就报 D8016）。
 */
#include "sales_report.hpp"

#include <QApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QString>
#include <QVBoxLayout>
#include <QWidget>

#include <iomanip>
#include <sstream>
#include <string>

namespace {

/* core 给出的窄字符串是 GBK（那个目标编译时带 -fexec-charset=GBK），
   这里按本地 8 位编码转成 QString —— 与 Win32 版的 CP_ACP 是一回事 */
QString to_qstring(const std::string &text)
{
    return QString::fromLocal8Bit(text.c_str());
}

/* 只读报表框里的换行用 CRLF，与命令行版的 LF 区分开。
   这一步在窄字符串上做：core 给的是 GBK 字节，先按字节插 \r，
   再整体用 fromLocal8Bit 转成 QString，中文才不会被当成 Latin-1。 */
std::string to_crlf(const std::string &text)
{
    std::string result;
    result.reserve(text.size() + 64);
    for (const char ch : text) {
        if (ch == '\n') {
            result += '\r';
        }
        result += ch;
    }
    return result;
}

/* ── 主窗口 ───────────────────────────────────────────── */

class MainWindow : public QWidget {
public:
    MainWindow()
    {
        setWindowTitle(QStringLiteral("示例 06-standard-library/02-cpp-io-report · iostream 报表生成器（Qt 版）"));

        path_ = new QLineEdit(QStringLiteral("data/sales.txt"));
        auto *generate_button = new QPushButton(QStringLiteral("生成报表"));
        auto *self_test_button = new QPushButton(QStringLiteral("跑自测"));

        auto *path_row = new QHBoxLayout;
        path_row->addWidget(new QLabel(QStringLiteral("输入文件（留空则用 data/sales.txt）：")));
        path_row->addWidget(path_, 1);
        path_row->addWidget(generate_button);
        path_row->addWidget(self_test_button);

        output_ = new QPlainTextEdit;
        output_->setReadOnly(true);
        output_->setLineWrapMode(QPlainTextEdit::NoWrap);

        status_ = new QLabel;

        auto *layout = new QVBoxLayout(this);
        layout->addLayout(path_row);
        layout->addWidget(new QLabel(QStringLiteral("报表（只读，与命令行版逐字节相同）：")));
        layout->addWidget(output_, 1);
        layout->addWidget(status_);

        connect(generate_button, &QPushButton::clicked, this, [this] { generate(); });
        connect(self_test_button, &QPushButton::clicked, this, [this] { self_test(); });

        resize(820, 640);
        generate();         /* 启动时先生成一次，窗口里直接有内容 */
    }

private:
    /* 「生成报表」按钮：把输入框里的路径交给 core，报表整段贴出来 */
    void generate()
    {
        std::string path = path_->text().toLocal8Bit().constData();
        if (path.empty()) {
            path = "data/sales.txt";
        }

        const double started = sales::now_ms();

        bool ok = false;
        std::string error;
        const sales::Report report = sales::load(path, ok, error);
        if (!ok) {
            output_->setPlainText(to_qstring("读取失败：" + error));
            status_->setText(QStringLiteral("读取失败"));
            return;
        }
        const std::string text = sales::format_report(report, path);

        const double finished = sales::now_ms();

        output_->setPlainText(to_qstring(to_crlf(text)));

        std::ostringstream status;
        status << "读取 " << report.lines_read << " 行：有效 " << report.lines_valid
               << " 行，跳过 " << report.lines_skipped << " 行，商品 "
               << report.products.size() << " 种，耗时 " << std::fixed
               << std::setprecision(3) << (finished - started) << " 毫秒";
        status_->setText(to_qstring(status.str()));
    }

    /* 「跑自测」按钮：把 core 的自测结果整段贴出来 */
    void self_test()
    {
        const sales::CheckResult result = sales::run_self_tests();
        std::string text;
        for (const std::string &line : result.lines) {
            text += line + "\n";
        }
        output_->setPlainText(to_qstring(to_crlf(text)));
        status_->setText(to_qstring(result.summary()));
    }

    QLineEdit *path_ = nullptr;
    QPlainTextEdit *output_ = nullptr;
    QLabel *status_ = nullptr;
};

}   /* namespace */

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    MainWindow window;
    window.show();
    return QApplication::exec();
}
