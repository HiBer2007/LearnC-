/**
 * main_gui_qt.cpp —— Qt Widgets 版界面（可选的第二份界面）
 *
 * 与 main_gui_win32.cpp 功能相同：输入一个目录，点「扫描」出报表，
 * 点「跑自测」显示自测结果。两份界面都只调用 core 里的 dirscan 命名空间，
 * 业务逻辑一行也不在这里。
 *
 * 这份界面默认【不构建】，因为它需要 Qt 6 Widgets：
 *     $env:QT_ROOT = '<Qt 套件目录>'
 *     cmake -S . -B build/qt -G Ninja -DCMAKE_BUILD_TYPE=Release `
 *           -DCMAKE_CXX_COMPILER=<Qt>\Tools\mingw1310_64\bin\g++.exe -DWITH_QT=ON
 *     cmake --build build/qt
 * 只用 Win32 那份时 WITH_QT 保持默认的 OFF，不必准备 Qt。
 *
 * ── 与 Win32 版的对应关系 ──────────────────────────────────
 *   窗口      QWidget                            ← CreateWindowExW
 *   路径框    QLineEdit                          ← EDIT（单行）
 *   报表框    QPlainTextEdit（只读）             ← EDIT（多行只读）
 *   按钮      QPushButton                        ← BUTTON
 *   布局      QVBoxLayout / QHBoxLayout          ← 控件坐标写死
 *   点击      connect(...clicked)                ← WM_COMMAND
 *
 * 本文件没有用 Q_OBJECT，因此不依赖 moc；CMakeLists.txt 里仍然打开了
 * CMAKE_AUTOMOC，读者要加自定义信号时不必再改工程。
 */
#include "dir_scan.hpp"

#include <QApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QString>
#include <QVBoxLayout>
#include <QWidget>

#include <filesystem>
#include <string>

namespace {

/* core 返回的是窄字符串（那个目标编译时带 -fexec-charset=GBK），
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
    result.reserve(text.size() + 16);
    for (std::size_t i = 0; i < text.size(); ++i) {
        if (text[i] == '\n' && (i == 0 || text[i - 1] != '\r')) {
            result += '\r';
        }
        result += text[i];
    }
    return result;
}

/* ── 主窗口 ───────────────────────────────────────────── */

class MainWindow : public QWidget {
public:
    MainWindow()
    {
        setWindowTitle(QStringLiteral("目录扫描 · filesystem 遍历与属性统计（Qt 版）"));

        path_ = new QLineEdit(QStringLiteral("data"));
        auto *scan_button = new QPushButton(QStringLiteral("扫描"));
        auto *self_test_button = new QPushButton(QStringLiteral("跑自测"));

        auto *path_row = new QHBoxLayout;
        path_row->addWidget(new QLabel(QStringLiteral("要扫描的目录（留空则用 data）：")));
        path_row->addWidget(path_, 1);
        path_row->addWidget(scan_button);
        path_row->addWidget(self_test_button);

        output_ = new QPlainTextEdit;
        output_->setReadOnly(true);
        output_->setLineWrapMode(QPlainTextEdit::NoWrap);

        auto *layout = new QVBoxLayout(this);
        layout->addLayout(path_row);
        layout->addWidget(new QLabel(QStringLiteral("报表（只读）：")));
        layout->addWidget(output_, 1);

        connect(scan_button, &QPushButton::clicked, this, [this] { scan(); });
        connect(self_test_button, &QPushButton::clicked, this, [this] { self_test(); });

        resize(760, 620);
        scan();             /* 启动时先扫一次，窗口里直接有内容 */
    }

private:
    /* 「扫描」按钮：把输入框里的路径交给 core，报表整段贴出来 */
    void scan()
    {
        std::string path = path_->text().toLocal8Bit().constData();
        if (path.empty()) {
            path = "data";
        }
        output_->setPlainText(to_qstring(to_crlf(dirscan::build_demo_output(
            std::filesystem::path(path)))));
    }

    /* 「跑自测」按钮：把 core 的自测结果整段贴出来 */
    void self_test()
    {
        const dirscan::CheckResult result = dirscan::run_self_tests();
        std::string text;
        for (const std::string &line : result.lines) {
            text += line + "\n";
        }
        text += "\n自测结果：" + result.summary() + "\n";
        output_->setPlainText(to_qstring(to_crlf(text)));
    }

    QLineEdit *path_ = nullptr;
    QPlainTextEdit *output_ = nullptr;
};

}   /* namespace */

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    MainWindow window;
    window.show();
    return QApplication::exec();
}
