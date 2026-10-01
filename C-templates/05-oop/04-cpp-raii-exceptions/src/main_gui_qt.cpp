/**
 * main_gui_qt.cpp —— 空模板 09 的 Qt 界面版（Qt Widgets）
 *
 * 与 src/main_gui_win32.cpp 是同一个窗口的两份实现：状态栏、日志框、
 * 四个按钮，以及标了「界面连接」的 TODO 都一一对应。
 * 业务逻辑仍然只在 core 里，界面文件只负责调用它并把日志显示出来。
 *
 * 构建（需要先准备好仓库内的静态 Qt，见《配置步骤.md》的「两份界面」一节）：
 *     cmake --preset mingw-gdb -DWITH_QT=ON
 *     cmake --build --preset mingw-gdb
 *     build\mingw\bin\app_gui_qt.exe
 *
 * 需要你补的与 Win32 版完全相同：refresh 与 4 个按钮的处理函数。
 */
#include <QApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>

#include <cstdio>
#include <exception>

#include "file_guard.hpp"

/* 窗口当前持有的句柄：没有打开时为 nullptr */
static FileGuard *g_guard = nullptr;

/* 已给出：UTF-8 的窄字符串转成 Qt 字符串 */
static QString to_qstring(const char *s)
{
    return QString::fromUtf8(s);
}

class MainWindow : public QWidget {
public:
    MainWindow();
    ~MainWindow() override;   /* 退出前把句柄还回去 */

private:
    /* TODO（界面连接 1）：把当前状态显示到状态栏上
     * 要求：一行文字里至少包含三项：
     *       1. 当前是否打开了文件（g_guard != nullptr，以及 g_guard->path()）；
     *       2. FileGuard::live_count()；
     *       3. FileGuard::close_count()。
     * 提示：
     *     char narrow[512];
     *     std::snprintf(narrow, sizeof(narrow), "...", ...);
     *     status_->setText(to_qstring(narrow));
     * 验收：与 Win32 版一致——点「打开」之后显示文件名、live 变成 1；
     *       点「关闭」之后文件名消失、live 回到 0、close 加一。
     */
    void refresh();

    void onOpen();    /* TODO（界面连接 2a）：打开文件 */
    void onWrite();   /* TODO（界面连接 2b）：写一行 */
    void onFail();    /* TODO（界面连接 2c）：制造一次失败 */
    void onClose();   /* TODO（界面连接 2d）：关闭并释放 */

    /* 已给出：往日志框追加一行（对应 Win32 版的 append_log） */
    void appendLog(const char *text)
    {
        log_->addItem(to_qstring(text));
        log_->scrollToBottom();
    }

    /* 已给出：状态栏刷新，处理函数末尾调用它即可 */
    void updateViews()
    {
        refresh();
    }

    QLabel      *status_;
    QListWidget *log_;
};

MainWindow::MainWindow()
{
    /* 已给出：控件、布局与信号槽连接。
     * 这里的每个 connect 与 Win32 版 WM_COMMAND 里的一个 case 一一对应。 */
    setWindowTitle(to_qstring("空模板 09 · RAII 与异常（Qt 界面）"));

    status_ = new QLabel(this);
    log_ = new QListWidget(this);

    QPushButton *open_button = new QPushButton(to_qstring("打开"), this);
    QPushButton *write_button = new QPushButton(to_qstring("写入一行"), this);
    QPushButton *fail_button = new QPushButton(to_qstring("制造失败"), this);
    QPushButton *close_button = new QPushButton(to_qstring("关闭"), this);

    connect(open_button, &QPushButton::clicked, this, &MainWindow::onOpen);
    connect(write_button, &QPushButton::clicked, this, &MainWindow::onWrite);
    connect(fail_button, &QPushButton::clicked, this, &MainWindow::onFail);
    connect(close_button, &QPushButton::clicked, this, &MainWindow::onClose);

    QHBoxLayout *buttons = new QHBoxLayout;
    buttons->addWidget(open_button);
    buttons->addWidget(write_button);
    buttons->addWidget(fail_button);
    buttons->addWidget(close_button);

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->addWidget(status_);
    layout->addLayout(buttons);
    layout->addWidget(log_);
    layout->addWidget(new QLabel(to_qstring("「制造失败」故意打开一个不存在的路径，观察 live 有没有变大。"), this));

    appendLog("窗口已就绪。先点「打开」，再点「写入一行」，最后点「制造失败」。");
    updateViews();
}

MainWindow::~MainWindow()
{
    /* 已给出：退出前把句柄还回去（与 Win32 版的 WM_DESTROY 相同） */
    delete g_guard;
    g_guard = nullptr;
}

/* TODO（界面连接 1）：见类内的说明 */
void MainWindow::refresh()
{
}

/* TODO（界面连接 2a）
 * 要求：打开一个文件
 *   1. 已经打开时先不要重复打开（可以在日志里写一行提示）；
 *   2. 用 new 造一个 FileGuard，路径用 "raii_gui.tmp"，模式 Mode::Write；
 *   3. 用 try / catch (const std::exception &) 接住构造失败的情况，
 *      失败时把 e.what() 写进日志（appendLog）；
 *   4. 成功后把指针存进 g_guard，并调用 updateViews()。
 * 验收：日志里出现「打开 raii_gui.tmp」，状态栏 live = 1。 */
void MainWindow::onOpen()
{
}

/* TODO（界面连接 2b）
 * 要求：往当前打开的文件写一行
 *        g_guard 为空时在日志里提示「还没有打开文件」；
 *        否则调用 write 写 "hello from qt\n"，并在日志里写一行结果。
 * 提示：write 会抛异常，记得 try / catch。
 * 验收：点几次「写入一行」，日志每次多一行，句柄状态不变。 */
void MainWindow::onWrite()
{
}

/* TODO（界面连接 2c）
 * 要求：制造一次失败，验证资源被收干净
 *   1. 在 try 里用 new FileGuard("raii_no_such_dir/x.tmp", Mode::Write)；
 *      这个路径打不开，构造函数会抛 std::runtime_error；
 *   2. catch (const std::exception &) 里把 e.what() 写进日志；
 *   3. 再写一行当前的 live_count()，它**不应该**变大——
 *      构造失败的对象根本不存在，也就没有资源需要回收。
 * 验收：点「制造失败」之后日志里出现异常消息，live 与点击前一样。 */
void MainWindow::onFail()
{
}

/* TODO（界面连接 2d）
 * 要求：关闭并释放当前句柄
 *        delete g_guard 并把指针置空（没有打开时写一行提示），然后调用 updateViews()。
 * 验收：状态栏 live 回到 0、close 加一；日志里出现「关闭 raii_gui.tmp」。 */
void MainWindow::onClose()
{
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    MainWindow window;
    window.resize(600, 340);
    window.show();
    return app.exec();
}
