/**
 * main_gui.cpp —— GUI 版（Win32 API，零依赖）
 *
 * 为什么用 Win32：
 *   它随 Windows 一起来，MinGW 的 g++ 加 -mwindows 就能直接编，
 *   读者不需要装 Qt、WinUI 或任何第三方库。
 *   换到 Qt 的做法见本目录 README「想换成 Qt 版」一节。
 *
 * 界面与逻辑如何分工：
 *   本文件只管窗口、控件与 GDI 绘制；
 *   算的部分全在 demo 命名空间（src/vector_demo.cpp），与命令行版共用。
 *   GUI 出问题时，先用命令行版确认核心逻辑，再看这里。
 *
 * ── 用到的 Win32 部件 ──────────────────────────────────────
 *   窗口类    RegisterClassW / CreateWindowExW / DefWindowProcW
 *   消息      WM_CREATE、WM_COMMAND、WM_PAINT、WM_CLOSE、WM_DESTROY
 *   控件      EDIT（输入、只读结果框）、BUTTON（计算、跑自测）、STATIC（标签）
 *   GDI       BeginPaint/EndPaint、FillRect、Rectangle、CreateSolidBrush、
 *             CreatePen、SelectObject、SetBkMode、SetTextColor、TextOutW
 *   字符集    窗口与控件一律用 W 结尾的宽字符版本，
 *             核心模块返回的窄字符串按 ANSI 代码页转宽（见 to_wide）
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "vector_demo.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace {

/* ── 控件编号：WM_COMMAND 里靠它们区分是哪个控件发的 ───── */
constexpr INT_PTR kIdInput = 1001;
constexpr INT_PTR kIdCompute = 1002;
constexpr INT_PTR kIdSelfTest = 1003;
constexpr INT_PTR kIdOutput = 1004;

/* ── 窗口与控件句柄 ───────────────────────────────────── */
HWND g_input = nullptr;         /* 输入框 */
HWND g_output = nullptr;        /* 结果框（只读多行） */

/* ── 界面状态：当前要画成柱状图的数列 ─────────────────── */
IntVector g_values;
std::wstring g_chart_note = L"还没有数据，点「计算」";

/* 核心模块给出的窄字符串是 GBK（编译时 -fexec-charset=GBK），
   这里按 ANSI 代码页转成宽字符，交给 W 结尾的 API。
   若改成按 UTF-8 转，界面上的中文会是乱码。 */
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

void set_output(const std::string &text)
{
    SetWindowTextW(g_output, to_wide(text).c_str());
}

/* 把一段算式与结果拼成多行文本，显示在只读结果框里 */
std::string build_report(const IntVector &values)
{
    const IntVector sums = demo::prefix_sum(values);
    const IntVector scaled = values * 3;
    const IntVector shifted = demo::add_scalar(values, 1);

    std::string text;
    text += "输入数列 : " + demo::to_text(values) + "\r\n";
    text += "前缀和   : " + demo::to_text(sums) + "\r\n";
    text += "每项乘 3 : " + demo::to_text(scaled) + "\r\n";
    text += "每项加 1 : " + demo::to_text(shifted) + "\r\n";
    text += "长度 " + std::to_string(values.size())
          + "，容量 " + std::to_string(values.capacity())
          + "，首项 " + std::to_string(values[0])
          + "，末项 " + std::to_string(values.at(values.size() - 1))
          + "，总和 " + std::to_string(sums.at(sums.size() - 1)) + "\r\n";
    text += "以上都是 IntVector 算出来的：+ 与 * 走的是自己写的运算符";
    return text;
}

/* 「计算」按钮：解析输入 → 算 → 显示 → 重画柱状图 */
void on_compute(HWND hwnd)
{
    const int length = GetWindowTextLengthW(g_input);
    std::wstring buffer(static_cast<std::size_t>(length) + 1, L'\0');
    GetWindowTextW(g_input, &buffer[0], length + 1);
    buffer.resize(static_cast<std::size_t>(length));

    /* 界面上的输入是宽字符，核心模块要窄字符串，按 ANSI 代码页转回去 */
    std::string narrow(static_cast<std::size_t>(length), '\0');
    const int written = WideCharToMultiByte(CP_ACP, 0, buffer.c_str(), length,
                                            &narrow[0], length, nullptr, nullptr);
    narrow.resize(static_cast<std::size_t>(written < 0 ? 0 : written));

    bool ok = false;
    std::string error;
    const IntVector values = demo::parse_numbers(narrow, ok, error);
    if (!ok) {
        g_values = IntVector{};
        g_chart_note = L"输入有误，没有可画的数据";
        set_output("输入有误：" + error + "\r\n请用空格、逗号或分号分隔整数。");
        InvalidateRect(hwnd, nullptr, TRUE);
        return;
    }

    g_values = values;
    std::wstring note = L"共 " + std::to_wstring(values.size()) + L" 项";
    note += L"，最大 " + std::to_wstring(values.at(values.size() - 1));
    g_chart_note = note;

    set_output(build_report(values));
    InvalidateRect(hwnd, nullptr, TRUE);     /* 让画图区重画 */
}

/* 「跑自测」按钮：把核心模块的自测结果整段显示出来 */
void on_self_test(HWND hwnd)
{
    const demo::CheckResult result = demo::run_self_tests();
    std::string text;
    for (const std::string &line : result.lines) {
        text += line + "\r\n";
    }
    text += "\r\n自测结果：" + result.summary();
    set_output(text);

    g_chart_note = L"自测：" + to_wide(result.summary());
    InvalidateRect(hwnd, nullptr, TRUE);
}

/* ── GDI 绘制：把数列画成柱状图 ───────────────────────── */

RECT chart_rect(HWND hwnd)
{
    RECT rc{};
    GetClientRect(hwnd, &rc);
    rc.left += 14;
    rc.right -= 14;
    rc.top = 316;
    rc.bottom -= 14;
    return rc;
}

COLORREF bar_color(std::size_t index)
{
    /* 按序号轮换几种颜色，纯 GDI 的 RGB 宏 */
    switch (index % 4U) {
    case 0: return RGB(70, 130, 180);
    case 1: return RGB(60, 179, 113);
    case 2: return RGB(218, 165, 32);
    default: return RGB(205, 92, 92);
    }
}

void paint_chart(HWND hwnd, HDC hdc)
{
    const RECT rc = chart_rect(hwnd);

    /* 底色 */
    HBRUSH background = CreateSolidBrush(RGB(250, 250, 252));
    FillRect(hdc, &rc, background);
    DeleteObject(background);

    /* 边框：Rectangle 用的是当前画笔与画刷，因此先把画刷换成空画刷 */
    HPEN border = CreatePen(PS_SOLID, 1, RGB(170, 170, 175));
    HGDIOBJ old_pen = SelectObject(hdc, border);
    HGDIOBJ old_brush = SelectObject(hdc, GetStockObject(NULL_BRUSH));
    Rectangle(hdc, rc.left, rc.top, rc.right, rc.bottom);
    SelectObject(hdc, old_brush);
    SelectObject(hdc, old_pen);
    DeleteObject(border);

    const int padding = 10;
    const int label_height = 20;
    const int base = rc.bottom - padding - label_height;   /* 柱子底边 */

    if (!g_values.empty()) {
        int maximum = 1;
        for (std::size_t i = 0; i < g_values.size(); ++i) {
            if (g_values[i] > maximum) {
                maximum = g_values[i];
            }
        }
        const int inner_width = (rc.right - rc.left) - 2 * padding;
        const int inner_height = base - (rc.top + padding + label_height);
        const int slot = inner_width / static_cast<int>(g_values.size());
        int bar_width = slot - 6;
        if (bar_width < 3) {
            bar_width = 3;
        }

        for (std::size_t i = 0; i < g_values.size(); ++i) {
            const int height = static_cast<int>(
                static_cast<long long>(g_values[i]) * inner_height / maximum);
            RECT bar{};
            bar.left = rc.left + padding + static_cast<int>(i) * slot + 3;
            bar.right = bar.left + bar_width;
            bar.bottom = base;
            bar.top = base - height;

            HBRUSH brush = CreateSolidBrush(bar_color(i));
            FillRect(hdc, &bar, brush);
            DeleteObject(brush);
        }
    }

    /* 文字：先设成透明背景，否则文字底下会带一块底色 */
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(60, 60, 70));
    HGDIOBJ old_font = SelectObject(hdc, GetStockObject(DEFAULT_GUI_FONT));

    TextOutW(hdc, rc.left + padding, rc.top + 4, g_chart_note.c_str(),
             static_cast<int>(g_chart_note.size()));

    const std::wstring axis = g_values.empty()
        ? std::wstring(L"柱状图（GDI 绘制）：等待数据")
        : L"柱状图（GDI 绘制）：共 " + std::to_wstring(g_values.size())
              + L" 根柱子，高度与数值成正比";
    TextOutW(hdc, rc.left + padding, rc.bottom - label_height + 2, axis.c_str(),
             static_cast<int>(axis.size()));

    SelectObject(hdc, old_font);
}

/* ── 窗口过程 ─────────────────────────────────────────── */

LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
    switch (message) {
    case WM_CREATE: {
        /* 控件一律用 W 结尾的版本，标题里的中文才不会乱码 */
        CreateWindowExW(0, L"STATIC", L"输入（空格、逗号或分号分隔的整数，最多 64 个）：",
                        WS_CHILD | WS_VISIBLE,
                        14, 12, 480, 20, hwnd, nullptr, nullptr, nullptr);

        g_input = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT",
                                  L"1 1 2 3 5 8 13 21 34 55",
                                  WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
                                  14, 34, 560, 26, hwnd,
                                  reinterpret_cast<HMENU>(kIdInput), nullptr, nullptr);

        CreateWindowExW(0, L"BUTTON", L"计算",
                        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                        586, 33, 120, 28, hwnd,
                        reinterpret_cast<HMENU>(kIdCompute), nullptr, nullptr);

        CreateWindowExW(0, L"BUTTON", L"跑自测",
                        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                        586, 67, 120, 28, hwnd,
                        reinterpret_cast<HMENU>(kIdSelfTest), nullptr, nullptr);

        CreateWindowExW(0, L"STATIC", L"结果（只读）：", WS_CHILD | WS_VISIBLE,
                        14, 76, 200, 20, hwnd, nullptr, nullptr, nullptr);

        g_output = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
                                   WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE
                                       | ES_AUTOVSCROLL | ES_READONLY,
                                   14, 98, 692, 200, hwnd,
                                   reinterpret_cast<HMENU>(kIdOutput), nullptr, nullptr);

        /* 控件默认字体又粗又大，换成系统界面字体 */
        const HGDIOBJ gui_font = GetStockObject(DEFAULT_GUI_FONT);
        SendMessageW(g_input, WM_SETFONT, reinterpret_cast<WPARAM>(gui_font), TRUE);
        SendMessageW(g_output, WM_SETFONT, reinterpret_cast<WPARAM>(gui_font), TRUE);

        on_compute(hwnd);       /* 启动时先算一次，窗口里直接有内容 */
        return 0;
    }

    case WM_COMMAND: {
        const int id = LOWORD(wparam);
        const int code = HIWORD(wparam);
        if (id == kIdCompute && code == BN_CLICKED) {
            on_compute(hwnd);
            return 0;
        }
        if (id == kIdSelfTest && code == BN_CLICKED) {
            on_self_test(hwnd);
            return 0;
        }
        break;
    }

    case WM_PAINT: {
        PAINTSTRUCT ps{};
        HDC hdc = BeginPaint(hwnd, &ps);
        paint_chart(hwnd, hdc);
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
    const wchar_t *class_name = L"IntVectorDemoWnd";

    WNDCLASSW wc{};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = instance;
    wc.lpszClassName = class_name;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);
    RegisterClassW(&wc);

    /* 固定大小：不许拉伸，省掉控件重新布局的代码 */
    const DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
    RECT rect{0, 0, 720, 620};
    AdjustWindowRect(&rect, style, FALSE);

    HWND hwnd = CreateWindowExW(0, class_name, L"示例 07 · IntVector 值类型演示",
                                style, CW_USEDEFAULT, CW_USEDEFAULT,
                                rect.right - rect.left, rect.bottom - rect.top,
                                nullptr, nullptr, instance, nullptr);
    if (hwnd == nullptr) {
        MessageBoxW(nullptr, L"窗口创建失败", L"示例 07", MB_ICONERROR);
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
