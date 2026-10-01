/**
 * main_gui_qt.cpp —— 空模板 06 的 Qt 界面版（Qt Widgets）
 *
 * 与 src/main_gui_win32.cpp 是同一个窗口的两份实现：控件、按钮、状态栏，
 * 以及标了「界面连接」的 TODO 都一一对应。业务逻辑仍然只在 core 里，
 * 界面文件只负责调用它并把结果显示出来。
 *
 * 构建（需要先准备好仓库内的静态 Qt，见《配置步骤.md》的「两份界面」一节）：
 *     cmake --preset mingw-gdb -DWITH_QT=ON
 *     cmake --build --preset mingw-gdb
 *     build\mingw\bin\app_gui_qt.exe
 *
 * 需要你补的与 Win32 版完全相同：refresh 与 5 个按钮的处理函数。
 */
#include <QApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>

#include <cstdio>
#include <utility>

#include "mystring.hpp"

/* 窗口里正在编辑的对象，以及用来演示拷贝与移动的第二个对象 */
static MyString g_text;
static MyString g_clip;

/* 已给出：UTF-8 的窄字符串转成 Qt 字符串 */
static QString to_qstring(const char *s)
{
    return QString::fromUtf8(s);
}

class MainWindow : public QWidget {
public:
    MainWindow();

private:
    /* TODO（界面连接 1）：把 g_text 的当前状态显示到窗口上
     * 要求：拼一行文字，至少包含四项：内容 g_text.c_str()、长度 g_text.size()、
     *       活着的对象数 MyString::live_count()、累计分配次数 MyString::allocation_count()，
     *       然后用 status_->setText(...) 显示。
     * 提示：
     *     char narrow[512];
     *     std::snprintf(narrow, sizeof(narrow), "...", ...);
     *     status_->setText(to_qstring(narrow));
     * 验收：与 Win32 版一致——显示 内容 = "hello"，长度 = 5；
     *       点「清空」之后长度回到 0、内容变成 ""。
     */
    void refresh();

    void onSet();      /* TODO（界面连接 2a）：设置内容 */
    void onAppend();   /* TODO（界面连接 2b）：追加内容 */
    void onCopy();     /* TODO（界面连接 2c）：深拷贝给 g_clip */
    void onMove();     /* TODO（界面连接 2d）：移动给 g_clip */
    void onClear();    /* TODO（界面连接 2e）：清空 */

    /* 已给出：刷新下面那一行「拷贝/移动的目标」 */
    void refreshClip();

    /* 已给出：两个显示位置一起刷新，处理函数末尾调用它即可 */
    void updateViews()
    {
        refresh();
        refreshClip();
    }

    QLineEdit *edit_;
    QLabel    *status_;
    QLabel    *clip_;
};

MainWindow::MainWindow()
{
    /* 已给出：控件、布局与信号槽连接。
     * 这里的每个 connect 与 Win32 版 WM_COMMAND 里的一个 case 一一对应。 */
    setWindowTitle(to_qstring("空模板 06 · 自己写的字符串类（Qt 界面）"));

    edit_ = new QLineEdit(to_qstring("hello"), this);
    status_ = new QLabel(this);
    clip_ = new QLabel(this);

    QPushButton *set_button = new QPushButton(to_qstring("设置"), this);
    QPushButton *append_button = new QPushButton(to_qstring("追加"), this);
    QPushButton *copy_button = new QPushButton(to_qstring("拷贝"), this);
    QPushButton *move_button = new QPushButton(to_qstring("移动"), this);
    QPushButton *clear_button = new QPushButton(to_qstring("清空"), this);

    connect(set_button, &QPushButton::clicked, this, &MainWindow::onSet);
    connect(append_button, &QPushButton::clicked, this, &MainWindow::onAppend);
    connect(copy_button, &QPushButton::clicked, this, &MainWindow::onCopy);
    connect(move_button, &QPushButton::clicked, this, &MainWindow::onMove);
    connect(clear_button, &QPushButton::clicked, this, &MainWindow::onClear);

    QHBoxLayout *buttons = new QHBoxLayout;
    buttons->addWidget(set_button);
    buttons->addWidget(append_button);
    buttons->addWidget(copy_button);
    buttons->addWidget(move_button);
    buttons->addWidget(clear_button);

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->addWidget(edit_);
    layout->addWidget(status_);
    layout->addLayout(buttons);
    layout->addWidget(clip_);
    layout->addStretch();

    updateViews();
}

/* TODO（界面连接 1）：见类内的说明 */
void MainWindow::refresh()
{
}

void MainWindow::refreshClip()
{
    char narrow[512];
    std::snprintf(narrow, sizeof(narrow), "拷贝/移动的目标：内容 = \"%s\"，长度 = %zu",
                  g_clip.c_str(), g_clip.size());
    clip_->setText(to_qstring(narrow));
}

/* TODO（界面连接 2a）
 * 要求：把编辑框里的内容设成 g_text 的内容，然后调用 updateViews()。
 * 提示：编辑框给出的是 QString，core 只认 UTF-8 的窄字符串：
 *           g_text = MyString(edit_->text().toUtf8().constData());
 *       这一行会同时用到拷贝赋值与移动赋值。
 * 验收：状态栏的内容与长度随编辑框变化；live 不变。 */
void MainWindow::onSet()
{
}

/* TODO（界面连接 2b）
 * 要求：把编辑框里的内容追加到 g_text 末尾，然后调用 updateViews()。
 * 提示：g_text.append(edit_->text().toUtf8().constData());
 * 验收：长度变成「原长度 + 新内容长度」，alloc 增加一次。 */
void MainWindow::onAppend()
{
}

/* TODO（界面连接 2c）
 * 要求：g_clip = g_text;（深拷贝），然后调用 updateViews()。
 * 验收：alloc 增加一次，live 不变；接着改 g_text，g_clip 不受影响。 */
void MainWindow::onCopy()
{
}

/* TODO（界面连接 2d）
 * 要求：g_clip = std::move(g_text);（所有权转移），然后调用 updateViews()。
 * 验收：alloc 不增加，g_text 的长度变成 0。 */
void MainWindow::onMove()
{
}

/* TODO（界面连接 2e）
 * 要求：清空 g_text，然后调用 updateViews()。
 * 验收：长度变成 0、内容变成 ""。 */
void MainWindow::onClear()
{
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    MainWindow window;
    window.resize(480, 200);
    window.show();
    return app.exec();
}
