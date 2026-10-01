/**
 * main_gui_win32.cpp —— 空模板 09 的 Win32 界面版（Win32 API，零依赖）
 *
 * 窗口上有一个「当前打开的文件」句柄，四个按钮分别做四件事：
 * 打开、写入、制造一次失败、关闭。每次操作都会往下面的日志框里写一行，
 * 状态栏则显示 live（活着的句柄数）与 close（关闭次数）两个计数器。
 * 「制造失败」那一个按钮是重点：故意打开一个不存在的文件，看资源有没有被收干净。
 *
 * 构建与运行：
 *     cmake --preset mingw-gdb
 *     cmake --build --preset mingw-gdb
 *     build\mingw\bin\app_gui_win32.exe
 *
 * 窗口骨架（注册窗口类、消息循环、WM_CREATE / WM_COMMAND / WM_PAINT / WM_DESTROY）
 * 已经写好并且能编译；需要你补的是标了「界面连接」的 TODO。
 * 同一套 TODO 在 src/main_gui_qt.cpp（Qt Widgets 版）里一一对应，两份做一份即可。
 * 详细说明与自查方法见同目录《配置步骤.md》的「界面部分怎么用」一节。
 */
#define UNICODE
#define _UNICODE
#include <windows.h>

#include <cstdio>
#include <cwchar>
#include <exception>

#include "file_guard.hpp"

/* 控件 ID */
enum {
    ID_STATUS = 3001,
    ID_LOG,
    ID_OPEN,
    ID_WRITE,
    ID_FAIL,
    ID_CLOSE
};

/* 窗口当前持有的句柄：没有打开时为 nullptr */
static FileGuard *g_guard = nullptr;

/* 已给出：UTF-8 的窄字符串转成宽字符串（W 版 API 需要宽字符串） */
static void to_wide(const char *s, wchar_t *buf, int cap)
{
    MultiByteToWideChar(CP_UTF8, 0, s, -1, buf, cap);
}

/* 已给出：给控件换上默认界面字体 */
static void set_font(HWND control)
{
    SendMessageW(control, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), TRUE);
}

/* 已给出：往日志框里追加一行 */
static void append_log(HWND hwnd, const char *text)
{
    wchar_t wide[512];
    to_wide(text, wide, 512);
    HWND list = GetDlgItem(hwnd, ID_LOG);
    if (list != nullptr) {
        SendMessageW(list, LB_ADDSTRING, 0, (LPARAM)wide);
        SendMessageW(list, LB_SETTOPINDEX, (WPARAM)SendMessageW(list, LB_GETCOUNT, 0, 0) - 1, 0);
    }
}

/* TODO（界面连接 1）：把当前状态写到状态栏上
 * 要求：一行文字里至少包含三项：
 *       1. 当前是否打开了文件（g_guard != nullptr，以及 g_guard->path()）；
 *       2. FileGuard::live_count()；
 *       3. FileGuard::close_count()。
 * 提示：
 *     char narrow[512];
 *     wchar_t wide[512];
 *     std::snprintf(narrow, sizeof(narrow), "...", ...);
 *     to_wide(narrow, wide, 512);
 *     SetWindowTextW(GetDlgItem(hwnd, ID_STATUS), wide);
 * 验收：点「打开」之后状态栏显示文件名，live 变成 1；
 *       点「关闭」之后文件名消失，live 回到 0、close 加一。
 */
static void refresh(HWND hwnd)
{
    (void)hwnd;
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    switch (msg) {
    case WM_CREATE: {
        /* 已给出：建好状态栏、日志框与四个按钮 */
        (void)lparam;
        HWND status = CreateWindowW(L"STATIC", L"", WS_CHILD | WS_VISIBLE,
                                    20, 16, 520, 24, hwnd, (HMENU)(INT_PTR)ID_STATUS,
                                    nullptr, nullptr);
        set_font(status);

        HWND list = CreateWindowExW(WS_EX_CLIENTEDGE, L"LISTBOX", L"",
                                    WS_CHILD | WS_VISIBLE | WS_VSCROLL | LBS_NOINTEGRALHEIGHT,
                                    20, 48, 520, 150, hwnd, (HMENU)(INT_PTR)ID_LOG,
                                    nullptr, nullptr);
        set_font(list);

        const wchar_t *labels[] = {L"打开", L"写入一行", L"制造失败", L"关闭"};
        const int ids[] = {ID_OPEN, ID_WRITE, ID_FAIL, ID_CLOSE};
        for (int i = 0; i < 4; ++i) {
            HWND button = CreateWindowW(L"BUTTON", labels[i],
                                        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                        20 + i * 134, 210, 124, 30, hwnd,
                                        (HMENU)(INT_PTR)ids[i], nullptr, nullptr);
            set_font(button);
        }

        append_log(hwnd, "窗口已就绪。先点「打开」，再点「写入一行」，最后点「制造失败」。");
        refresh(hwnd);
        return 0;
    }

    case WM_COMMAND: {
        switch (LOWORD(wparam)) {
        case ID_OPEN:
            /* TODO（界面连接 2a）：打开一个文件
             * 要求：
             *   1. 已经打开时先不要重复打开（可以在日志里写一行提示）；
             *   2. 用 new 造一个 FileGuard，路径用 "raii_gui.tmp"，模式 Mode::Write；
             *   3. 用 try / catch (const std::exception &) 接住构造失败的情况，
             *      失败时把 e.what() 写进日志；
             *   4. 成功后把指针存进 g_guard，并调用 refresh(hwnd)。
             * 验收：日志里出现「打开 raii_gui.tmp」，状态栏 live = 1。 */
            break;

        case ID_WRITE:
            /* TODO（界面连接 2b）：往当前打开的文件写一行
             * 要求：g_guard 为空时在日志里提示「还没有打开文件」；
             *       否则调用 write 写 "hello from gui\n"，并在日志里写一行结果。
             * 提示：write 会抛异常，记得 try / catch。
             * 验收：点几次「写入一行」，日志每次多一行，句柄状态不变。 */
            break;

        case ID_FAIL:
            /* TODO（界面连接 2c）：制造一次失败，验证资源被收干净
             * 要求：
             *   1. 在 try 里用 new FileGuard("raii_no_such_dir/x.tmp", Mode::Write)；
             *      这个路径打不开，构造函数会抛 std::runtime_error；
             *   2. catch (const std::exception &) 里把 e.what() 写进日志；
             *   3. 在日志里再写一行当前的 live_count()，它**不应该**变大——
             *      构造失败的对象根本不存在，也就没有资源需要回收。
             * 验收：点「制造失败」之后日志里出现异常消息，live 与点击前一样。 */
            break;

        case ID_CLOSE:
            /* TODO（界面连接 2d）：关闭并释放当前句柄
             * 要求：delete g_guard 并把指针置空（没有打开时写一行提示），然后刷新。
             * 验收：状态栏 live 回到 0、close 加一；日志里出现「关闭 raii_gui.tmp」。 */
            break;

        default:
            break;
        }
        return 0;
    }

    case WM_PAINT: {
        /* 已给出：一行说明文字 */
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        const char *hint_utf8 = "「制造失败」故意打开一个不存在的路径，观察 live 有没有变大。";
        wchar_t hint[256];
        to_wide(hint_utf8, hint, 256);
        TextOutW(hdc, 20, 252, hint, (int)std::wcslen(hint));
        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_DESTROY:
        delete g_guard;      /* 窗口关闭时也要把句柄还回去 */
        g_guard = nullptr;
        PostQuitMessage(0);
        return 0;

    default:
        break;
    }
    return DefWindowProcW(hwnd, msg, wparam, lparam);
}

int WINAPI WinMain(HINSTANCE hinst, HINSTANCE hprev, LPSTR cmdline, int show)
{
    (void)hprev;
    (void)cmdline;

    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hinst;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = L"FileGuardGuiWindow";
    if (RegisterClassExW(&wc) == 0) {
        return 1;
    }

    HWND hwnd = CreateWindowExW(0, wc.lpszClassName,
                                L"空模板 09 · RAII 与异常（Win32 界面）",
                                WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
                                600, 360, nullptr, nullptr, hinst, nullptr);
    if (hwnd == nullptr) {
        return 1;
    }

    ShowWindow(hwnd, show);
    UpdateWindow(hwnd);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return (int)msg.wParam;
}
