/**
 * main_gui_win32.cpp —— 练习模板 07 的 Win32 界面版
 *
 * 窗口里做的是同一件事：给一个目录，点「扫描」，下面显示统计结果。
 * 界面用的是系统自带的控件，零第三方依赖。
 *
 * 构建与运行（在模板目录下）：
 *     cmake --build --preset mingw-gdb
 *     build\mingw\bin\app_gui_win32.exe
 *
 * 窗口骨架（注册窗口类、消息循环、WM_CREATE / WM_SIZE / WM_DESTROY）已经写好，
 * 需要你补的只有两处标了「界面连接」的 TODO。
 * 同一批 TODO 在 src/main_gui_qt.cpp 里一一对应，两份做一份即可。
 */
#define UNICODE
#define _UNICODE
#include <windows.h>

#include "dirscan.hpp"

#include <cstdio>
#include <cwchar>
#include <string>

/* 控件编号：GUI冒烟脚本按编号操作控件 */
enum {
    ID_PATH   = 1001,
    ID_SCAN   = 1002,
    ID_RESULT = 1003,
    ID_STATUS = 1004
};

static ds::DirStats g_stats;
static wchar_t      g_text[16384];
static int          g_scanned = 0;

/* 已给出：UTF-8 的窄字符串转成宽字符串 */
static void to_wide(const char *s, wchar_t *buf, int cap)
{
    MultiByteToWideChar(CP_UTF8, 0, s, -1, buf, cap);
}

/* 已给出：把编辑框里的内容按 UTF-8 读进 buf */
static void read_edit(HWND hwnd, int id, char *buf, int cap)
{
    wchar_t wide[512] = L"";
    buf[0] = '\0';
    GetWindowTextW(GetDlgItem(hwnd, id), wide, 512);
    WideCharToMultiByte(CP_UTF8, 0, wide, -1, buf, cap, NULL, NULL);
}

/* 已给出：设置默认界面字体 */
static void set_font(HWND control)
{
    SendMessageW(control, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), TRUE);
}

/* 已给出：把统计结果拼成一段多行文本（'\n' 换行，写进编辑框之前要换成 "\r\n"） */
static std::string compose_report(const ds::DirStats &s)
{
    char line[256];
    std::string out;

    std::snprintf(line, sizeof(line), "files = %ld\ndirs  = %ld\nbytes = %lld\n",
                  s.files, s.dirs, s.bytes);
    out += line;
    out += "by extension:\n";
    for (const auto &kv : s.by_extension) {
        std::snprintf(line, sizeof(line), "  %s : %ld\n", kv.first.c_str(), kv.second);
        out += line;
    }
    return out;
}

/* TODO（界面连接 1）：把统计结果写到状态行上。
 *
 * 要求：
 *   1. g_scanned 为 0 时显示一句「尚未扫描」，否则显示 files / dirs / bytes 三项；
 *   2. 用 to_wide 转成宽字符串，SetWindowTextW 写到 ID_STATUS 控件上。
 * 验收：点「扫描」之后状态行显示 files = 3   dirs = 1   bytes = 20（对 data/tree）。 */
static void refresh(HWND hwnd)
{
    /* 占位实现：明确告诉读者这里还没接上。下面几行只是引用一下待用的变量，
     * 让骨架保持「零警告」，补完「界面连接 1」之后可以删掉。 */
    (void)g_stats;
    (void)g_text;
    (void)g_scanned;
    (void)&to_wide;    /* 取地址而不是光写函数名：MSVC 的 /W4 会对「函数名不带参数列表」报 C4551 */
    (void)&compose_report;

    SetWindowTextW(GetDlgItem(hwnd, ID_STATUS), L"尚未扫描（界面连接 1 还没有做）");
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    switch (msg) {
    case WM_CREATE: {
        /* 已给出：建好界面控件 */
        HWND label = CreateWindowW(L"STATIC", L"目录", WS_CHILD | WS_VISIBLE,
                                   16, 14, 40, 20, hwnd, NULL, NULL, NULL);
        set_font(label);

        HWND path = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"data/tree",
                                    WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
                                    60, 12, 340, 24, hwnd, (HMENU)(INT_PTR)ID_PATH,
                                    NULL, NULL);
        set_font(path);

        HWND scan = CreateWindowW(L"BUTTON", L"扫描", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                  412, 12, 80, 26, hwnd, (HMENU)(INT_PTR)ID_SCAN, NULL, NULL);
        set_font(scan);

        HWND status = CreateWindowW(L"STATIC", L"", WS_CHILD | WS_VISIBLE,
                                    16, 46, 476, 22, hwnd, (HMENU)(INT_PTR)ID_STATUS,
                                    NULL, NULL);
        set_font(status);

        HWND result = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
                                      WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE |
                                      ES_READONLY | ES_AUTOVSCROLL,
                                      16, 74, 476, 260, hwnd, (HMENU)(INT_PTR)ID_RESULT,
                                      NULL, NULL);
        set_font(result);

        refresh(hwnd);
        return 0;
    }

    case WM_SIZE: {
        /* 已给出：窗口变大时，结果框跟着变大 */
        RECT rc;
        GetClientRect(hwnd, &rc);
        HWND result = GetDlgItem(hwnd, ID_RESULT);
        if (result != NULL) {
            SetWindowPos(result, NULL, 0, 0,
                         rc.right - 32 < 120 ? 120 : rc.right - 32,
                         rc.bottom - 90 < 80 ? 80 : rc.bottom - 90,
                         SWP_NOMOVE | SWP_NOZORDER);
        }
        return 0;
    }

    case WM_COMMAND: {
        if (LOWORD(wparam) == ID_SCAN) {
            char path[512];
            char narrow[16384];
            std::size_t i;
            std::size_t j = 0;

            read_edit(hwnd, ID_PATH, path, sizeof(path));

            /* TODO（界面连接 2）：扫描并把结果显示出来。
             *
             * 要求：
             *   1. g_stats = ds::scan(path, true);（递归统计）
             *   2. g_scanned 置 1；
             *   3. 用 compose_report(g_stats) 拼出多行文本（已给出的函数），
             *      把 '\n' 换成 "\r\n" 之后 to_wide 到 g_text，
             *      SetWindowTextW(GetDlgItem(hwnd, ID_RESULT), g_text)；
             *   4. 最后调用 refresh(hwnd)。
             * 验收：点「扫描」后结果框里出现 files / dirs / bytes 与按扩展名的分类。 */
            (void)path;
            (void)narrow;
            (void)i;
            (void)j;

            SetWindowTextW(GetDlgItem(hwnd, ID_RESULT),
                           L"（界面连接 2 还没有做：这里应当调用 ds::scan 并显示结果）");
        }
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
    wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = L"FilesystemScanWindow";
    if (RegisterClassExW(&wc) == 0) {
        return 1;
    }

    HWND hwnd = CreateWindowExW(0, wc.lpszClassName,
                                L"标准库配套件 07 · 目录扫描工具（Win32 界面）",
                                WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
                                540, 400, NULL, NULL, hinst, NULL);
    if (hwnd == NULL) {
        return 1;
    }

    ShowWindow(hwnd, show);
    UpdateWindow(hwnd);

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return (int)msg.wParam;
}
