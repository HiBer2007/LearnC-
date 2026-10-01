/**
 * main_gui_win32.c —— 练习模板 01 的 Win32 界面版
 *
 * 窗口里做的是同一件事：给一个文件路径，点「统计」，下面显示 core 生成的报表。
 * 界面用的是系统自带的控件，gcc 加 -mwindows 就能编译，不需要安装第三方库。
 *
 * 构建与运行（在模板目录下）：
 *     cmake --build --preset mingw-gdb
 *     build\mingw\bin\app_gui_win32.exe
 *
 * 窗口骨架（注册窗口类、消息循环、WM_CREATE / WM_SIZE / WM_DESTROY）已经写好，
 * 需要你补的只有两处标了「界面连接」的 TODO。详细说明见《配置步骤.md》。
 */
#define UNICODE
#define _UNICODE
#include <windows.h>

#include "textstats.h"

#include <stdio.h>

/* 控件编号：GUI冒烟脚本按编号操作控件 */
enum {
    ID_PATH = 1001,
    ID_RUN  = 1002,
    ID_REPORT = 1003,
    ID_STATUS = 1004
};

#define REPORT_CAP (16 * 1024)

static TsStats g_stats;
static char    g_report[REPORT_CAP];
static int     g_analyzed = 0;

/* 已给出：UTF-8 的窄字符串转成宽字符串（W 版 API 需要宽字符串） */
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

/* 已给出：给控件换上默认界面字体 */
static void set_font(HWND control)
{
    SendMessageW(control, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), TRUE);
}

/* TODO（界面连接 1）：把统计结果写到状态行上。
 *
 * 要求：
 *   1. 拼一行文字，至少包含四项：bytes、lines、words、unique；
 *      g_analyzed 为 0 时（还没点过「统计」）显示一句「尚未统计」；
 *   2. 用 to_wide 转成宽字符串，SetWindowTextW 写到 ID_STATUS 控件上。
 * 提示：
 *     char narrow[256];
 *     wchar_t wide[256];
 *     snprintf(narrow, sizeof(narrow), "...", ...);
 *     to_wide(narrow, wide, 256);
 *     SetWindowTextW(GetDlgItem(hwnd, ID_STATUS), wide);
 * 验收：点「统计」之后状态行显示 bytes = 963 一类的内容（数字见《配置步骤.md》）。 */
static void refresh(HWND hwnd)
{
    /* 占位实现：明确告诉读者这里还没接上。
     * 下面几行只是引用一下待用的变量与函数，让骨架保持「零警告」，
     * 补完「界面连接 1」之后可以删掉。 */
    (void)g_stats;
    (void)g_report;
    (void)g_analyzed;
    (void)to_wide;

    SetWindowTextW(GetDlgItem(hwnd, ID_STATUS), L"尚未统计（界面连接 1 还没有做）");
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    switch (msg) {
    case WM_CREATE: {
        /* 已给出：建好界面控件 */
        HWND label = CreateWindowW(L"STATIC", L"文件路径", WS_CHILD | WS_VISIBLE,
                                   16, 14, 70, 20, hwnd, NULL, NULL, NULL);
        set_font(label);

        HWND path = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"data/sample.txt",
                                    WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
                                    92, 12, 300, 24, hwnd, (HMENU)(INT_PTR)ID_PATH,
                                    NULL, NULL);
        set_font(path);

        HWND run = CreateWindowW(L"BUTTON", L"统计", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                 404, 12, 80, 26, hwnd, (HMENU)(INT_PTR)ID_RUN, NULL, NULL);
        set_font(run);

        HWND status = CreateWindowW(L"STATIC", L"", WS_CHILD | WS_VISIBLE,
                                    16, 46, 468, 22, hwnd, (HMENU)(INT_PTR)ID_STATUS,
                                    NULL, NULL);
        set_font(status);

        HWND report = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
                                      WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL |
                                      ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL,
                                      16, 74, 468, 260, hwnd, (HMENU)(INT_PTR)ID_REPORT,
                                      NULL, NULL);
        set_font(report);

        refresh(hwnd);
        return 0;
    }

    case WM_SIZE: {
        /* 已给出：窗口变大时，报表框跟着变大 */
        RECT rc;
        GetClientRect(hwnd, &rc);
        HWND report = GetDlgItem(hwnd, ID_REPORT);
        if (report != NULL) {
            SetWindowPos(report, NULL, 0, 0,
                         rc.right - 32 < 120 ? 120 : rc.right - 32,
                         rc.bottom - 90 < 80 ? 80 : rc.bottom - 90,
                         SWP_NOMOVE | SWP_NOZORDER);
        }
        return 0;
    }

    case WM_COMMAND: {
        if (LOWORD(wparam) == ID_RUN) {
            char path[512];
            char err[256] = "";

            read_edit(hwnd, ID_PATH, path, sizeof(path));

            /* TODO（界面连接 2）：调用 core，把报表写进 ID_REPORT。
             *
             * 要求：
             *   1. ts_analyze(path, &g_stats, err, sizeof(err)) 返回 0 时把
             *      g_analyzed 置 1，否则置 0，并把 err 的内容也写进报表框（让失败可见）；
             *   2. 成功时用 ts_write_report(&g_stats, g_report, sizeof(g_report))
             *      生成报表，再 to_wide + SetWindowTextW 写到 ID_REPORT；
             *   3. 最后调用一次 refresh(hwnd) 更新状态行。
             * 提示（多行文本用 \r\n 换行，编辑框才认）：
             *     ts_write_report 生成的报表用 '\n'，写进编辑框之前要把 '\n'
             *     换成 "\r\n"，否则整段会挤成一行。
             * 验收：点「统计」后报表框里出现与命令行版阶段 5 相同的报表。 */
            (void)err;
            SetWindowTextW(GetDlgItem(hwnd, ID_REPORT),
                           L"（界面连接 2 还没有做：这里应当调用 ts_analyze 并显示报表）");
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

    WNDCLASSEXW wc = {0};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hinst;
    wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = L"CStdlibToolboxWindow";
    if (RegisterClassExW(&wc) == 0) {
        return 1;
    }

    HWND hwnd = CreateWindowExW(0, wc.lpszClassName,
                                L"标准库配套件 01 · C 文本统计工具（Win32 界面）",
                                WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
                                520, 400, NULL, NULL, hinst, NULL);
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
