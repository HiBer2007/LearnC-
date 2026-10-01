/**
 * main_gui_win32.cpp —— 空模板 06 的 Win32 界面版（Win32 API，零依赖）
 *
 * 窗口里编辑的是一个真实的 MyString 对象：点一次按钮就是调用它的一次成员函数，
 * 状态栏显示的是 c_str()、size() 与两个静态计数的实时值。
 * 界面用的是 Win32 API 自带的控件，MinGW 的 g++ 加 -mwindows 就能编译，
 * 不需要安装任何第三方库。
 *
 * 构建与运行：
 *     cmake --preset mingw-gdb
 *     cmake --build --preset mingw-gdb
 *     build\mingw\bin\app_gui_win32.exe
 *
 * 窗口骨架（注册窗口类、消息循环、WM_CREATE / WM_COMMAND / WM_PAINT / WM_DESTROY）
 * 已经写好并且能编译；需要你补的只有 3 处标了「界面连接」的 TODO。
 * 同一套 TODO 在 src/main_gui_qt.cpp（Qt Widgets 版）里一一对应，两份做一份即可。
 * 详细说明与自查方法见同目录《配置步骤.md》的「界面部分怎么用」一节。
 */
#define UNICODE
#define _UNICODE
#include <windows.h>

#include <cstdio>
#include <cwchar>
#include <utility>

#include "mystring.hpp"

/* 控件 ID */
enum {
    ID_EDIT = 1001,
    ID_STATUS,
    ID_SET,
    ID_APPEND,
    ID_COPY,
    ID_MOVE,
    ID_CLEAR
};

/* 窗口里正在编辑的对象，以及用来演示拷贝与移动的第二个对象 */
static MyString g_text;
static MyString g_clip;

/* 已给出：把编辑框里的内容按 UTF-8 读进 buf */
static void read_edit(HWND hwnd, char *buf, int cap)
{
    buf[0] = '\0';
    HWND edit = GetDlgItem(hwnd, ID_EDIT);
    if (edit == nullptr) {
        return;
    }
    wchar_t wide[512] = L"";
    GetWindowTextW(edit, wide, 512);
    WideCharToMultiByte(CP_UTF8, 0, wide, -1, buf, cap, nullptr, nullptr);
}

/* 已给出：UTF-8 的窄字符串转成宽字符串（W 版 API 需要宽字符串） */
static void to_wide(const char *s, wchar_t *buf, int cap)
{
    MultiByteToWideChar(CP_UTF8, 0, s, -1, buf, cap);
}

/* 已给出：给控件换上默认界面字体，免得中文显示成点阵字 */
static void set_font(HWND control)
{
    SendMessageW(control, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), TRUE);
}

/* TODO（界面连接 1）：把 g_text 的当前状态显示到窗口上
 * 要求：
 *   1. 拼一行文字，至少包含四项：内容 g_text.c_str()、长度 g_text.size()、
 *      活着的对象数 MyString::live_count()、累计分配次数 MyString::allocation_count()；
 *   2. 用 to_wide 转成宽字符串，SetWindowTextW 写到 ID_STATUS 控件上；
 *   3. 末尾调用 InvalidateRect(hwnd, nullptr, TRUE)，让窗口重画。
 * 提示：
 *     char narrow[512];
 *     wchar_t wide[512];
 *     std::snprintf(narrow, sizeof(narrow), "...", ...);
 *     to_wide(narrow, wide, 512);
 *     SetWindowTextW(GetDlgItem(hwnd, ID_STATUS), wide);
 * 验收：窗口状态栏显示 内容 = "hello"，长度 = 5；
 *       点「清空」之后长度回到 0、内容变成 ""。
 */
static void refresh(HWND hwnd)
{
    (void)hwnd;
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    switch (msg) {
    case WM_CREATE: {
        /* 已给出：建好界面控件 */
        (void)lparam;
        HWND edit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"hello",
                                    WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
                                    20, 20, 360, 26, hwnd, (HMENU)(INT_PTR)ID_EDIT,
                                    nullptr, nullptr);
        set_font(edit);

        HWND status = CreateWindowW(L"STATIC", L"", WS_CHILD | WS_VISIBLE,
                                    20, 58, 360, 24, hwnd, (HMENU)(INT_PTR)ID_STATUS,
                                    nullptr, nullptr);
        set_font(status);

        const wchar_t *labels[] = {L"设置", L"追加", L"拷贝", L"移动", L"清空"};
        const int ids[] = {ID_SET, ID_APPEND, ID_COPY, ID_MOVE, ID_CLEAR};
        for (int i = 0; i < 5; ++i) {
            HWND button = CreateWindowW(L"BUTTON", labels[i],
                                        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                        20 + i * 74, 96, 68, 28, hwnd,
                                        (HMENU)(INT_PTR)ids[i], nullptr, nullptr);
            set_font(button);
        }

        refresh(hwnd);
        return 0;
    }

    case WM_COMMAND: {
        char buf[512];
        switch (LOWORD(wparam)) {
        case ID_SET:
            read_edit(hwnd, buf, sizeof(buf));
            /* TODO（界面连接 2a）：把编辑框里的内容设成 g_text 的内容，然后刷新显示。
             * 提示：g_text = MyString(buf);  这一行会同时用到拷贝赋值与移动赋值。
             * 验收：状态栏的内容与长度随编辑框变化；live 不变（旧缓冲被释放，不新增对象）。 */
            break;

        case ID_APPEND:
            read_edit(hwnd, buf, sizeof(buf));
            /* TODO（界面连接 2b）：把 buf 追加到 g_text 末尾，然后刷新显示。
             * 提示：g_text.append(buf);
             * 验收：长度变成「原长度 + buf 长度」，alloc 增加一次（append 重新分配了内存）。 */
            break;

        case ID_COPY:
            /* TODO（界面连接 2c）：把 g_text 深拷贝给 g_clip，然后刷新显示。
             * 提示：g_clip = g_text;
             * 验收：alloc 增加一次，live 不变；两份内容各自独立，
             *       接着点「追加」改 g_text，g_clip 的内容不受影响。 */
            break;

        case ID_MOVE:
            /* TODO（界面连接 2d）：把 g_text 的内容移动给 g_clip，然后刷新显示。
             * 提示：g_clip = std::move(g_text);
             * 验收：alloc 不增加，g_text 的长度变成 0（内容被搬走了）；
             *       与「拷贝」对比，这一步没有新的内存分配。 */
            break;

        case ID_CLEAR:
            /* TODO（界面连接 2e）：清空 g_text，然后刷新显示。
             * 验收：长度变成 0、内容变成 ""。 */
            break;

        default:
            break;
        }
        return 0;
    }

    case WM_PAINT: {
        /* 已给出：窗口左上角的两行说明文字，以及 g_clip 的状态 */
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        const wchar_t *line1 = L"窗口里的对象就是 src/mystring.cpp 里的 MyString。";
        TextOutW(hdc, 20, 140, line1, (int)std::wcslen(line1));

        char narrow[512];
        wchar_t wide[512];
        std::snprintf(narrow, sizeof(narrow), "拷贝/移动的目标：内容 = \"%s\"，长度 = %zu",
                      g_clip.c_str(), g_clip.size());
        to_wide(narrow, wide, 512);
        TextOutW(hdc, 20, 166, wide, (int)std::wcslen(wide));

        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_DESTROY:
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
    wc.lpszClassName = L"MyStringGuiWindow";
    if (RegisterClassExW(&wc) == 0) {
        return 1;
    }

    HWND hwnd = CreateWindowExW(0, wc.lpszClassName,
                                L"空模板 06 · 自己写的字符串类（Win32 界面）",
                                WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
                                420, 260, nullptr, nullptr, hinst, nullptr);
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
