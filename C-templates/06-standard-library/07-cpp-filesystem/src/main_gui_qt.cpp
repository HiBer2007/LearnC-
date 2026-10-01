/**
 * main_gui_qt.cpp —— 练习模板 07 的 Qt 界面版
 *
 * 与 Win32 版功能相同：给一个目录，点「扫描」，下面显示统计结果。
 * Qt 版**默认不构建**，打开方式见同目录《配置步骤.md》的「界面部分怎么用」一节：
 *     cmake --preset mingw-gdb -DWITH_QT=ON -DQT_ROOT=<Qt 的套件目录>
 *
 * 骨架（窗口与控件、布局、信号槽的接线）已经写好，
 * 需要你补的只有两处标了「界面连接」的 TODO，编号与 Win32 版一致，做一份即可。
 */
#include <QApplication>
#include <QFont>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QString>
#include <QVBoxLayout>
#include <QWidget>

#include "dirscan.hpp"

namespace {

ds::DirStats g_stats;
int          g_scanned = 0;

/* 已给出：把统计结果拼成一段文本 */
std::string compose_report(const ds::DirStats &s)
{
    std::string out;

    out += "files = " + std::to_string(s.files) + "\n";
    out += "dirs  = " + std::to_string(s.dirs) + "\n";
    out += "bytes = " + std::to_string(s.bytes) + "\n";
    out += "by extension:\n";
    for (const auto &kv : s.by_extension) {
        out += "  " + kv.first + " : " + std::to_string(kv.second) + "\n";
    }
    return out;
}

class ScanWindow : public QWidget {
public:
    ScanWindow()
    {
        setWindowTitle(QStringLiteral("标准库配套件 07 · 目录扫描工具（Qt 界面）"));

        path_edit_ = new QLineEdit(QStringLiteral("data/tree"), this);
        scan_button_ = new QPushButton(QStringLiteral("扫描"), this);
        status_label_ = new QLabel(QString(), this);
        result_view_ = new QPlainTextEdit(this);
        result_view_->setReadOnly(true);

        auto *top = new QHBoxLayout;
        top->addWidget(new QLabel(QStringLiteral("目录"), this));
        top->addWidget(path_edit_);
        top->addWidget(scan_button_);

        auto *layout = new QVBoxLayout(this);
        layout->addLayout(top);
        layout->addWidget(status_label_);
        layout->addWidget(result_view_);

        connect(scan_button_, &QPushButton::clicked, this, &ScanWindow::on_scan);
        refresh();
    }

private:
    /* TODO（界面连接 1）：把统计结果写到状态标签上。
     *
     * 要求：
     *   1. g_scanned 为 0 时显示「尚未扫描」，否则显示 files / dirs / bytes；
     *   2. 用 QString::fromStdString 或 QStringLiteral 拼出文字，
     *      status_label_->setText(...)。
     * 验收：点「扫描」之后状态行显示 files = 3   dirs = 1   bytes = 20（对 data/tree）。 */
    void refresh()
    {
        /* 占位实现：明确告诉读者这里还没接上。下面几行只是引用一下待用的成员，
         * 让骨架保持「零警告」，补完「界面连接 1」之后可以删掉。 */
        (void)g_stats;
        (void)g_scanned;

        status_label_->setText(QStringLiteral("尚未扫描（界面连接 1 还没有做）"));
    }

    /* TODO（界面连接 2）：扫描并把结果显示出来。
     *
     * 要求：
     *   1. 用 path_edit_->text().toStdString() 取目录；
     *   2. g_stats = ds::scan(dir, true);（递归统计）
     *   3. g_scanned 置 1；
     *   4. result_view_->setPlainText(QString::fromStdString(compose_report(g_stats)));
     *   5. 最后调用 refresh()。
     * 验收：点「扫描」后结果框里出现 files / dirs / bytes 与按扩展名的分类。 */
    void on_scan()
    {
        result_view_->setPlainText(
            QStringLiteral("（界面连接 2 还没有做：这里应当调用 ds::scan 并显示结果）"));
    }

    QLineEdit      *path_edit_ = nullptr;
    QPushButton    *scan_button_ = nullptr;
    QLabel         *status_label_ = nullptr;
    QPlainTextEdit *result_view_ = nullptr;
};

} /* namespace */

int main(int argc, char **argv)
{
    QApplication app(argc, argv);

    ScanWindow window;
    window.resize(560, 420);
    window.show();

    return app.exec();
}
