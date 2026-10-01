/* main.cpp    Qt 界面最小示例（动态版 Qt）
 *
 * 构建：见同目录 README.md
 *   cmake -S . -B build-qt -G Ninja -DWITH_QT=ON
 *   cmake --build build-qt
 *
 * 运行前要让程序找得到 Qt 的 DLL，两种做法见 README 第四节。
 *
 * 这个文件只做一件事：把 Qt 界面路线跑通。
 * 它没有自定义 Q_OBJECT，因此不需要 moc，也就不必开 AUTOMOC。
 */

#include <QApplication>
#include <QLabel>
#include <QPushButton>
#include <QString>
#include <QVBoxLayout>
#include <QWidget>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    QWidget window;
    window.setWindowTitle(QString::fromUtf8("示例 10 · Qt 界面最小示例"));

    // Qt 的字符串是 QString，与窄字符串的执行字符集无关，
    // 因此这里不依赖任何字符集编译选项。
    auto *label  = new QLabel(QString::fromUtf8("还没有点过"), &window);
    auto *button = new QPushButton(QString::fromUtf8("点我"), &window);

    auto *layout = new QVBoxLayout(&window);
    layout->addWidget(label);
    layout->addWidget(button);

    // 用 lambda 连接信号与槽，避免引入自定义 Q_OBJECT 类。
    // 这也是 Qt 5 之后推荐的写法：连接在编译期检查，写错签名编译就过不去。
    int clicks = 0;
    QObject::connect(button, &QPushButton::clicked, [&clicks, label]() {
        ++clicks;
        label->setText(QString::fromUtf8("已点击 %1 次").arg(clicks));
    });

    window.resize(360, 160);
    window.show();

    return app.exec();
}
