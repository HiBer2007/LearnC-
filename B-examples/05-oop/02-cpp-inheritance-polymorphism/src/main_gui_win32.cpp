/**
 * main_gui.cpp —— GUI 版（Win32 API，零依赖）
 *
 * 界面上方是三个面板，每个面板对应工厂造出的一种导出器：
 *   面板上的名字与说明分别是 name() 与 hint() 的返回值，
 *   本文件并不知道 CsvExporter / JsonExporter 这些类型存在，
 *   这就是虚函数的效果 —— 界面上直接看得见。
 * 点面板即切换格式，结果立刻显示在下面的只读框里。
 *
 * 为什么用 Win32：随 Windows 一起来，g++ 加 -mwindows 直接能编，
 * 读者不需要装任何第三方库。换 Qt 的做法见本目录 README。
 *
 * ── 用到的 Win32 部件 ──────────────────────────────────────
 *   窗口类    RegisterClassW / CreateWindowExW / DefWindowProcW
 *   消息      WM_CREATE、WM_COMMAND、WM_PAINT、WM_LBUTTONDOWN、
 *             WM_CLOSE、WM_DESTROY
 *   控件      EDIT（只读结果框）、BUTTON（重绘、复选框、跑自测）、STATIC
 *   GDI       FillRect、Rectangle、CreateSolidBrush、CreatePen、
 *             SelectObject、SetBkMode、SetTextColor、TextOutW
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "report_demo.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace {

/* ── 控件编号 ─────────────────────────────────────────── */
constexpr INT_PTR kIdRepaint = 3001;
constexpr INT_PTR kIdTricky = 3002;
constexpr INT_PTR kIdSelfTest = 3003;
constexpr INT_PTR kIdOutput = 3004;

/* ── 面板布局：三个格式并排 ───────────────────────────── */
constexpr int kPanelLeft = 14;
constexpr int kPanelTop = 12;
constexpr int kPanelWidth = 244;
constexpr int kPanelHeight = 104;
constexpr int kPanelGap = 10;

HWND g_output = nullptr;
HWND g_status = nullptr;
HWND g_tricky = nullptr;

std::vector<demo::FormatInfo> g_formats;    /* 来自工厂 + 虚函数 */
std::size_t g_selected = 0;
bool g_use_tricky = false;

std::wstring to_wide(const std::string &text)
{
    if (text.empty()) {
        return std::wstring();
    }
    const int length = MultiByteToWideChar(CP_ACP, 0, text.c_str(),
                                           static_cast<int>(text.size()), nullptr, 0);
    std::wstring result(static_cast<std::size_t>(length), L'\0');
    MultiByteToWideChar(CP_ACP, 0, text.c_str(), static_cast<int>(text.size()),
                        &result[0], length);
    return result;
}

Table current_table()
{
    return g_use_tricky ? demo::tricky_table() : demo::grade_table();
}

/* 重新渲染并刷新界面。这是整个界面的「刷新」入口 */
void refresh(HWND hwnd)
{
    if (g_formats.empty()) {
        return;
    }
    const demo::FormatInfo &info = g_formats[g_selected];

    bool ok = false;
    std::string error;
    const std::string text = demo::render(info.name, current_table(), ok, error);
    SetWindowTextW(g_output, to_wide(ok ? text : "渲染失败：" + error).c_str());

    std::string status = "当前格式：" + info.name + "（" + info.hint + "）";
    status += g_use_tricky ? "；表格：含转义字符" : "；表格：学生成绩";
    SetWindowTextW(g_status, to_wide(status).c_str());

    InvalidateRect(hwnd, nullptr, TRUE);
}

void on_self_test()
{
    const demo::CheckResult result = demo::run_self_tests();
    std::string text;
    for (const std::string &line : result.lines) {
        text += line + "\r\n";
    }
    text += "\r\n自测结果：" + result.summary();
    SetWindowTextW(g_output, to_wide(text).c_str());
}

/* ── GDI：画三个可点击的格式面板 ──────────────────────── */

COLORREF panel_color(std::size_t index, bool selected)
{
    if (selected) {
        switch (index % 3U) {
        case 0: return RGB(198, 224, 245);
        case 1: return RGB(205, 240, 213);
        default: return RGB(250, 236, 200);
        }
    }
    switch (index % 3U) {
    case 0: return RGB(238, 244, 250);
    case 1: return RGB(240, 248, 242);
    default: return RGB(252, 249, 240);
    }
}

void paint_panels(HDC hdc)
{
    SetBkMode(hdc, TRANSPARENT);
    HGDIOBJ old_font = SelectObject(hdc, GetStockObject(DEFAULT_GUI_FONT));

    for (std::size_t i = 0; i < g_formats.size(); ++i) {
        const int left = kPanelLeft + static_cast<int>(i) * (kPanelWidth + kPanelGap);
        RECT panel{left, kPanelTop, left + kPanelWidth, kPanelTop + kPanelHeight};
        const bool selected = (i == g_selected);

        HBRUSH brush = CreateSolidBrush(panel_color(i, selected));
        FillRect(hdc, &panel, brush);
        DeleteObject(brush);

        /* 选中的面板用两像素边框，一眼能看出当前选的是哪种格式 */
        HPEN pen = CreatePen(PS_SOLID, selected ? 2 : 1,
                             selected ? RGB(40, 90, 160) : RGB(180, 180, 185));
        HGDIOBJ old_pen = SelectObject(hdc, pen);
        HGDIOBJ old_brush = SelectObject(hdc, GetStockObject(NULL_BRUSH));
        Rectangle(hdc, panel.left, panel.top, panel.right, panel.bottom);
        SelectObject(hdc, old_brush);
        SelectObject(hdc, old_pen);
        DeleteObject(pen);

        /* 面板上的两行字都来自虚函数 */
        std::wstring title = to_wide(g_formats[i].name);
        if (selected) {
            title += L"（当前）";
        }
        SetTextColor(hdc, RGB(30, 60, 100));
        TextOutW(hdc, panel.left + 12, panel.top + 14, title.c_str(),
                 static_cast<int>(title.size()));

        const std::wstring hint = to_wide(g_formats[i].hint);
        SetTextColor(hdc, RGB(80, 80, 90));
        TextOutW(hdc, panel.left + 12, panel.top + 40, hint.c_str(),
                 static_cast<int>(hint.size()));

        const std::wstring tip = L"点这里切换格式";
        SetTextColor(hdc, RGB(130, 130, 140));
        TextOutW(hdc, panel.left + 12, panel.top + 68, tip.c_str(),
                 static_cast<int>(tip.size()));
    }

    /* 面板下方的说明：讲清这些字是从哪来的 */
    const std::wstring note =
        L"面板上的格式名与说明都来自虚函数 name() 与 hint()；"
        L"本文件并不知道有哪几个派生类，新增格式只改工厂。";
    SetTextColor(hdc, RGB(70, 70, 80));
    TextOutW(hdc, kPanelLeft, kPanelTop + kPanelHeight + 6, note.c_str(),
             static_cast<int>(note.size()));

    SelectObject(hdc, old_font);
}

/* 点面板：把 x 坐标换算成第几个面板 */
void on_click(int x, int y, HWND hwnd)
{
    if (y < kPanelTop || y > kPanelTop + kPanelHeight) {
        return;
    }
    if (x < kPanelLeft) {
        return;
    }
    const int index = (x - kPanelLeft) / (kPanelWidth + kPanelGap);
    if (index < 0 || static_cast<std::size_t>(index) >= g_formats.size()) {
        return;
    }
    g_selected = static_cast<std::size_t>(index);
    refresh(hwnd);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
    switch (message) {
    case WM_CREATE: {
        g_formats = demo::format_infos();       /* 工厂 + 虚函数 */

        CreateWindowExW(0, L"STATIC", L"渲染结果（只读）：", WS_CHILD | WS_VISIBLE,
                        14, 146, 200, 20, hwnd, nullptr, nullptr, nullptr);

        g_output = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
                                   WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE
                                       | ES_AUTOVSCROLL | ES_READONLY,
                                   14, 168, 752, 320, hwnd,
                                   reinterpret_cast<HMENU>(kIdOutput), nullptr, nullptr);

        CreateWindowExW(0, L"BUTTON", L"重绘",
                        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                        14, 500, 110, 30, hwnd,
                        reinterpret_cast<HMENU>(kIdRepaint), nullptr, nullptr);

        g_tricky = CreateWindowExW(0, L"BUTTON", L"换成含转义字符的表格",
                                   WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                                   140, 504, 260, 24, hwnd,
                                   reinterpret_cast<HMENU>(kIdTricky), nullptr, nullptr);

        CreateWindowExW(0, L"BUTTON", L"跑自测",
                        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                        420, 500, 110, 30, hwnd,
                        reinterpret_cast<HMENU>(kIdSelfTest), nullptr, nullptr);

        g_status = CreateWindowExW(0, L"STATIC", L"", WS_CHILD | WS_VISIBLE,
                                   14, 542, 752, 40, hwnd, nullptr, nullptr, nullptr);

        const HGDIOBJ gui_font = GetStockObject(DEFAULT_GUI_FONT);
        SendMessageW(g_output, WM_SETFONT, reinterpret_cast<WPARAM>(gui_font), TRUE);
        SendMessageW(g_status, WM_SETFONT, reinterpret_cast<WPARAM>(gui_font), TRUE);
        SendMessageW(g_tricky, WM_SETFONT, reinterpret_cast<WPARAM>(gui_font), TRUE);

        refresh(hwnd);
        return 0;
    }

    case WM_COMMAND: {
        const int id = LOWORD(wparam);
        const int code = HIWORD(wparam);
        if (id == kIdRepaint && code == BN_CLICKED) {
            refresh(hwnd);
            return 0;
        }
        if (id == kIdTricky && code == BN_CLICKED) {
            g_use_tricky = (SendMessageW(g_tricky, BM_GETCHECK, 0, 0) == BST_CHECKED);
            refresh(hwnd);
            return 0;
        }
        if (id == kIdSelfTest && code == BN_CLICKED) {
            on_self_test();
            return 0;
        }
        break;
    }

    case WM_LBUTTONDOWN: {
        on_click(static_cast<int>(LOWORD(lparam)), static_cast<int>(HIWORD(lparam)), hwnd);
        return 0;
    }

    case WM_PAINT: {
        PAINTSTRUCT ps{};
        HDC hdc = BeginPaint(hwnd, &ps);
        paint_panels(hdc);
        EndPaint(hwnd, &ps);
        return 0;
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

}   /* namespace */

int WINAPI WinMain(HINSTANCE instance, HINSTANCE, LPSTR, int show_command)
{
    const wchar_t *class_name = L"ExporterDemoWnd";

    WNDCLASSW wc{};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = instance;
    wc.lpszClassName = class_name;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);
    RegisterClassW(&wc);

    const DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
    RECT rect{0, 0, 780, 600};
    AdjustWindowRect(&rect, style, FALSE);

    HWND hwnd = CreateWindowExW(0, class_name, L"示例 08 · 导出器：抽象基类、工厂与虚析构",
                                style, CW_USEDEFAULT, CW_USEDEFAULT,
                                rect.right - rect.left, rect.bottom - rect.top,
                                nullptr, nullptr, instance, nullptr);
    if (hwnd == nullptr) {
        MessageBoxW(nullptr, L"窗口创建失败", L"示例 08", MB_ICONERROR);
        return 1;
    }

    ShowWindow(hwnd, show_command);
    UpdateWindow(hwnd);

    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return 0;
}
