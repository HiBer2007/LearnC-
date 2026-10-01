/**
 * main_gui_win32.cpp —— 练习模板 05 的 Win32 界面版
 *
 * 窗口里画的是同一条正态分布样本的直方图：12 根柱子用 GDI 的 FillRect 画出来，
 * 点「重新抽样」换一个种子重算一遍。核心逻辑全在 stats.cpp 里。
 *
 * 构建与运行（在模板目录下）：
 *     cmake --build --preset mingw-gdb
 *     build\mingw\bin\app_gui_win32.exe
 *
 * 窗口骨架（注册窗口类、消息循环、WM_CREATE / WM_PAINT / WM_SIZE / WM_DESTROY）
 * 已经写好，需要你补的只有两处标了「界面连接」的 TODO。
 */
#define UNICODE
#define _UNICODE
#include <windows.h>

#include "stats.hpp"

#include <cstdio>
#include <cwchar>
#include <vector>

/* 控件编号：GUI冒烟脚本按编号操作控件 */
enum {
    ID_RESAMPLE = 1001,
    ID_STATUS   = 1002
};

/* 柱子的颜色：RGB(70,130,180)，冒烟脚本按这个颜色数像素 */
static const COLORREF kBarColor = RGB(70, 130, 180);

static const double kLo = 20.0;
static const double kHi = 80.0;
static const int    kBuckets = 12;

static unsigned    g_seed = 2026u;
static std::vector<long> g_counts;
static st::Summary g_summary {};

/* 已给出：换一个种子重算一遍直方图与统计量 */
static void recompute()
{
    const std::vector<double> sample = st::normal_sample(g_seed, 1000, 50.0, 10.0);
    g_counts = st::bucket_counts(sample, kLo, kHi, kBuckets);
    g_summary = st::summarize(sample);
}

/* 已给出：UTF-8 的窄字符串转成宽字符串 */
static void to_wide(const char *s, wchar_t *buf, int cap)
{
    MultiByteToWideChar(CP_UTF8, 0, s, -1, buf, cap);
}

/* 已给出：设置默认界面字体 */
static void set_font(HWND control)
{
    SendMessageW(control, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), TRUE);
}

/* TODO（界面连接 1）：把当前状态写到状态行上。
 *
 * 要求：
 *   1. 拼一行文字，至少包含：种子 g_seed、12 个桶的计数之和、均值与标准差；
 *   2. 用 to_wide 转成宽字符串，SetWindowTextW 写到 ID_STATUS 控件上。
 * 提示：
 *     char narrow[256];
 *     wchar_t wide[256];
 *     std::snprintf(narrow, sizeof(narrow), "seed = %u   samples = %ld ...", ...);
 *     to_wide(narrow, wide, 256);
 *     SetWindowTextW(GetDlgItem(hwnd, ID_STATUS), wide);
 * 验收：状态行显示 seed 与样本数（见《配置步骤.md》的界面部分）。 */
static void refresh(HWND hwnd)
{
    /* 占位实现：明确告诉读者这里还没接上。下面几行只是引用一下待用的变量，
     * 让骨架保持「零警告」，补完「界面连接 1」之后可以删掉。 */
    (void)g_seed;
    (void)g_counts;
    (void)g_summary;
    (void)&to_wide;   /* 取地址而不是光写函数名：MSVC 的 /W4 会对「函数名不带参数列表」报 C4551 */

    SetWindowTextW(GetDlgItem(hwnd, ID_STATUS), L"尚未统计（界面连接 1 还没有做）");
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    switch (msg) {
    case WM_CREATE: {
        /* 已给出：建控件，并算一遍初始数据 */
        HWND button = CreateWindowW(L"BUTTON", L"重新抽样",
                                    WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                    16, 12, 100, 28, hwnd,
                                    (HMENU)(INT_PTR)ID_RESAMPLE, NULL, NULL);
        set_font(button);

        HWND status = CreateWindowW(L"STATIC", L"", WS_CHILD | WS_VISIBLE,
                                    128, 18, 420, 22, hwnd,
                                    (HMENU)(INT_PTR)ID_STATUS, NULL, NULL);
        set_font(status);

        recompute();
        refresh(hwnd);
        return 0;
    }

    case WM_COMMAND: {
        if (LOWORD(wparam) == ID_RESAMPLE) {
            /* TODO（界面连接 2）：换一个种子重算并重画。
             *
             * 要求：
             *   1. 把 g_seed 加一（换个种子就换一组样本）；
             *   2. 调用 recompute()；
             *   3. 调用 refresh(hwnd) 更新状态行，再 InvalidateRect(hwnd, NULL, TRUE)
             *      让窗口重画。
             * 验收：点一次按钮，种子数字变化，柱子的高度分布跟着变。 */
            (void)lparam;
            SetWindowTextW(GetDlgItem(hwnd, ID_STATUS),
                           L"（界面连接 2 还没有做：这里应当换种子重算）");
        }
        return 0;
    }

    case WM_PAINT: {
        /* 已给出：把 12 个桶画成柱子，柱子用 kBarColor 填充 */
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        RECT client;
        GetClientRect(hwnd, &client);

        const int left   = 40;
        const int bottom = client.bottom - 30;
        const int top    = 60;
        const int width  = (client.right - left - 20);

        HBRUSH brush = CreateSolidBrush(kBarColor);
        HGDIOBJ old_brush = SelectObject(hdc, brush);
        HGDIOBJ old_pen   = SelectObject(hdc, GetStockObject(NULL_PEN));

        long max_count = 0;
        for (long c : g_counts) {
            if (c > max_count) {
                max_count = c;
            }
        }

        if (g_counts.empty() || max_count <= 0) {
            const wchar_t *note = L"直方图还没有数据（阶段 1 与阶段 3 完成后出现）";
            TextOutW(hdc, left, top, note, static_cast<int>(std::wcslen(note)));
        } else {
            const int slot = width / static_cast<int>(g_counts.size());
            for (std::size_t i = 0; i < g_counts.size(); ++i) {
                const int h = static_cast<int>((bottom - top) * g_counts[i] / max_count);
                const int x = left + static_cast<int>(i) * slot + 4;
                Rectangle(hdc, x, bottom - h, x + slot - 8, bottom);
            }
        }

        SelectObject(hdc, old_pen);
        SelectObject(hdc, old_brush);
        DeleteObject(brush);

        const wchar_t *title = L"normal(50,10) 的 1000 个样本，[20,80) 等分 12 桶";
        TextOutW(hdc, left, 6, title, static_cast<int>(std::wcslen(title)));
        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_SIZE: {
        InvalidateRect(hwnd, NULL, TRUE);
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
    wc.lpszClassName = L"NumericRandomWindow";
    if (RegisterClassExW(&wc) == 0) {
        return 1;
    }

    HWND hwnd = CreateWindowExW(0, wc.lpszClassName,
                                L"标准库配套件 05 · 随机数与直方图（Win32 界面）",
                                WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
                                620, 420, NULL, NULL, hinst, NULL);
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
