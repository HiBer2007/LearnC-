/**
 * main_gui_win32.c —— Win32 界面版（用 C 写，零依赖）
 *
 * 为什么用 Win32：
 *   它随 Windows 一起来，MinGW 的 gcc 加 -mwindows 就能直接编，
 *   读者不需要装 Qt、WinUI 或任何第三方库。
 *
 * 界面与逻辑怎么分工：
 *   本文件只管窗口、控件与字符串转换；
 *   读文件、聚合、排序、拼报表全在 src/sales_report.c 里，
 *   与命令行版共用同一份实现。界面里一行业务逻辑都没有。
 *   界面出问题时先跑命令行版：报表对得上，问题就在这一层。
 *
 * 用到的 Win32 部件：
 *   窗口类   RegisterClassW / CreateWindowExW / DefWindowProcW
 *   消息     WM_CREATE、WM_COMMAND、WM_CLOSE、WM_DESTROY
 *   控件     EDIT（路径输入、只读报表框）、BUTTON、STATIC
 *   字符串   MultiByteToWideChar / WideCharToMultiByte
 *
 * 字符串为什么要在两种宽度之间来回转：
 *   核心模块给的是窄字符串，而且编译时用了 -fexec-charset=GBK，
 *   所以它是 GBK 字节流；W 结尾的 API 要 UTF-16。
 *   两边靠 CP_ACP（本机即 936）对接，中文才不会变成乱码。
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "sales_report.h"

#include <stdio.h>
#include <stdlib.h>

/* ── 控件编号：WM_COMMAND 里靠它们区分是哪个控件发的 ───── */
#define IDC_INPUT     1001
#define IDC_GENERATE  1002
#define IDC_SELFTEST  1003
#define IDC_REPORT    1004

/* ── 控件句柄 ─────────────────────────────────────────── */
static HWND g_input = NULL;
static HWND g_report = NULL;
static HWND g_status = NULL;

/* ── 字符串转换 ───────────────────────────────────────── */

/** 把 GBK 窄字符串转成宽字符串。返回的缓冲区要由调用方 free。 */
static WCHAR *to_wide(const char *text)
{
    WCHAR *wide;
    int length;

    if (text == NULL) {
        return NULL;
    }
    length = MultiByteToWideChar(CP_ACP, 0, text, -1, NULL, 0);
    if (length <= 0) {
        return NULL;
    }
    wide = (WCHAR *)malloc((size_t)length * sizeof(WCHAR));
    if (wide == NULL) {
        return NULL;
    }
    if (MultiByteToWideChar(CP_ACP, 0, text, -1, wide, length) <= 0) {
        free(wide);
        return NULL;
    }
    return wide;
}

/** 把宽字符串转回 GBK 窄字符串，写进调用方给的缓冲区。 */
static void to_narrow(const WCHAR *text, char *dst, size_t cap)
{
    if (dst == NULL || cap == 0) {
        return;
    }
    dst[0] = '\0';
    if (text == NULL) {
        return;
    }
    if (WideCharToMultiByte(CP_ACP, 0, text, -1, dst, (int)cap, NULL, NULL) <= 0) {
        dst[0] = '\0';
    }
}

/**
 * EDIT 控件要多行显示，换行必须写成 \r\n，只写 \n 不会换行。
 * 报表文本本身用的是 \n，因此显示之前补一次 \r。
 */
static void expand_newlines(char *dst, size_t cap, const char *src)
{
    size_t used = 0;

    if (dst == NULL || cap == 0) {
        return;
    }
    while (src != NULL && *src != '\0' && used + 3 <= cap) {
        if (*src == '\n') {
            dst[used] = '\r';
            used += 1;
        }
        dst[used] = *src;
        used += 1;
        src += 1;
    }
    dst[used] = '\0';
}

/** 把一段窄文本放进某个控件。 */
static void set_control_text(HWND control, const char *text)
{
    WCHAR *wide = to_wide(text);

    if (wide != NULL) {
        SetWindowTextW(control, wide);
        free(wide);
    }
}

/** 把报表文本放进只读框：先补 \r，再转宽字符。 */
static void set_report_text(const char *text)
{
    char *expanded = (char *)malloc(SR_REPORT_MAX * 2U);

    if (expanded == NULL) {
        return;
    }
    expand_newlines(expanded, SR_REPORT_MAX * 2U, text);
    set_control_text(g_report, expanded);
    free(expanded);
}

/** 读回输入框里的文件路径。 */
static void read_input_path(char *dst, size_t cap)
{
    int length = GetWindowTextLengthW(g_input);
    WCHAR *wide;

    if (dst == NULL || cap == 0) {
        return;
    }
    dst[0] = '\0';
    if (length <= 0) {
        return;
    }
    wide = (WCHAR *)malloc(((size_t)length + 1U) * sizeof(WCHAR));
    if (wide == NULL) {
        return;
    }
    GetWindowTextW(g_input, wide, length + 1);
    to_narrow(wide, dst, cap);
    free(wide);
}

/* ── 两个按钮 ─────────────────────────────────────────── */

/** 「生成报表」：调核心模块，把报表文本原样显示出来。 */
static void on_generate(void)
{
    sr_report_t report;
    char path[512];
    char text[SR_REPORT_MAX];
    char err[256];
    char status[256];
    double started;
    double finished;

    read_input_path(path, sizeof path);
    if (path[0] == '\0') {
        set_report_text("请先填写输入文件路径。");
        set_control_text(g_status, "没有输入文件");
        return;
    }

    started = sr_now_ms();
    if (!sr_load(path, &report, err, sizeof err)) {
        char message[768];
        snprintf(message, sizeof message, "读取失败：%s", err);
        set_report_text(message);
        set_control_text(g_status, "读取失败");
        return;
    }
    (void)sr_format_report(&report, path, text, sizeof text);
    finished = sr_now_ms();

    set_report_text(text);

    snprintf(status, sizeof status,
             "读取 %d 行：有效 %d 行，跳过 %d 行，商品 %d 种，耗时 %.3f 毫秒",
             report.lines_read, report.lines_valid, report.lines_skipped,
             report.product_count, finished - started);
    set_control_text(g_status, status);
}

/** 「跑自测」：把核心模块的自测结果整段显示出来。 */
static void on_selftest(void)
{
    sr_checks_t checks;
    char summary[SR_CHECK_LINE_MAX];
    char text[SR_REPORT_MAX];
    size_t used = 0;
    int i;

    sr_run_selftest(&checks);
    sr_checks_summary(&checks, summary, sizeof summary);

    text[0] = '\0';
    for (i = 0; i < checks.count && used + 160U < sizeof text; ++i) {
        int written = snprintf(text + used, sizeof text - used, "%s\n", checks.lines[i]);
        if (written <= 0) {
            break;
        }
        used += (size_t)written;
    }

    set_report_text(text);
    set_control_text(g_status, summary);
}

/* ── 窗口过程 ─────────────────────────────────────────── */

static LRESULT CALLBACK WndProc(HWND hwnd, UINT message,
                                WPARAM wparam, LPARAM lparam)
{
    switch (message) {
    case WM_CREATE: {
        HGDIOBJ gui_font = GetStockObject(DEFAULT_GUI_FONT);

        CreateWindowExW(0, L"STATIC", L"输入文件（相对本示例目录，或写绝对路径）：",
                        WS_CHILD | WS_VISIBLE,
                        14, 12, 480, 20, hwnd, NULL, NULL, NULL);

        g_input = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"data/sales.txt",
                                  WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
                                  14, 34, 560, 26, hwnd,
                                  (HMENU)(INT_PTR)IDC_INPUT, NULL, NULL);

        CreateWindowExW(0, L"BUTTON", L"生成报表",
                        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                        586, 33, 120, 28, hwnd,
                        (HMENU)(INT_PTR)IDC_GENERATE, NULL, NULL);

        CreateWindowExW(0, L"BUTTON", L"跑自测",
                        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                        586, 67, 120, 28, hwnd,
                        (HMENU)(INT_PTR)IDC_SELFTEST, NULL, NULL);

        CreateWindowExW(0, L"STATIC", L"报表（只读，与命令行版逐字节相同）：",
                        WS_CHILD | WS_VISIBLE,
                        14, 76, 400, 20, hwnd, NULL, NULL, NULL);

        g_report = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
                                   WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL
                                       | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY,
                                   14, 98, 692, 380, hwnd,
                                   (HMENU)(INT_PTR)IDC_REPORT, NULL, NULL);

        g_status = CreateWindowExW(0, L"STATIC", L"",
                                   WS_CHILD | WS_VISIBLE | SS_LEFTNOWORDWRAP,
                                   14, 486, 692, 20, hwnd, NULL, NULL, NULL);

        SendMessageW(g_input, WM_SETFONT, (WPARAM)gui_font, TRUE);
        SendMessageW(g_report, WM_SETFONT, (WPARAM)gui_font, TRUE);
        SendMessageW(g_status, WM_SETFONT, (WPARAM)gui_font, TRUE);

        on_generate();      /* 启动时先生成一次，窗口里直接有内容 */
        return 0;
    }

    case WM_COMMAND: {
        int id = LOWORD(wparam);
        int code = HIWORD(wparam);

        if (id == IDC_GENERATE && code == BN_CLICKED) {
            on_generate();
            return 0;
        }
        if (id == IDC_SELFTEST && code == BN_CLICKED) {
            on_selftest();
            return 0;
        }
        break;
    }

    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;

    default:
        break;
    }
    return DefWindowProcW(hwnd, message, wparam, lparam);
}

int WINAPI WinMain(HINSTANCE instance, HINSTANCE previous, LPSTR command_line,
                   int show_command)
{
    const WCHAR *class_name = L"CStdlibToolboxWnd";
    DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
    RECT rect;
    HWND hwnd;
    MSG message;

    (void)previous;
    (void)command_line;

    {
        WNDCLASSW wc;
        ZeroMemory(&wc, sizeof wc);
        wc.lpfnWndProc = WndProc;
        wc.hInstance = instance;
        wc.lpszClassName = class_name;
        wc.hCursor = LoadCursor(NULL, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
        RegisterClassW(&wc);
    }

    rect.left = 0;
    rect.top = 0;
    rect.right = 720;
    rect.bottom = 520;
    AdjustWindowRect(&rect, style, FALSE);

    hwnd = CreateWindowExW(0, class_name,
                           L"示例 06-standard-library/01-c-stdlib-toolbox · C 标准库报表工具",
                           style, CW_USEDEFAULT, CW_USEDEFAULT,
                           rect.right - rect.left, rect.bottom - rect.top,
                           NULL, NULL, instance, NULL);
    if (hwnd == NULL) {
        MessageBoxW(NULL, L"窗口创建失败", L"01-c-stdlib-toolbox", MB_ICONERROR);
        return 1;
    }

    ShowWindow(hwnd, show_command);
    UpdateWindow(hwnd);

    while (GetMessageW(&message, NULL, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    return 0;
}
