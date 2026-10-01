/**
 * main_gui_win32.cpp —— GUI 版（Win32 API，零依赖）
 *
 * 为什么用 Win32：
 *   它随 Windows 一起来，MinGW 的 g++ 加 -mwindows 就能直接编，
 *   读者不需要装 Qt、WinUI 或任何第三方库。
 *
 * 界面与逻辑如何分工：
 *   本文件只管窗口、控件与 GDI 绘制；
 *   抽样、统计、分箱与两段结论文本全在 demo 命名空间（src/stats_demo.cpp），
 *   与命令行版共用同一份实现。GUI 出问题时先跑命令行版确认逻辑。
 *
 * ── 用到的 Win32 部件 ──────────────────────────────────────
 *   窗口类    RegisterClassW / CreateWindowExW / DefWindowProcW
 *   消息      WM_CREATE、WM_COMMAND、WM_PAINT、WM_CLOSE、WM_DESTROY
 *   控件      EDIT（种子输入、只读结果框）、BUTTON（重新生成、跑自测）、STATIC
 *   GDI       BeginPaint/EndPaint、FillRect、Rectangle、CreateSolidBrush、
 *             CreatePen、SelectObject、SetBkMode、SetTextColor、TextOutW
 *   字符集    窗口与控件一律用 W 结尾的宽字符版本；
 *             核心模块返回的窄字符串按 ANSI 代码页转宽（见 to_wide）
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "stats_demo.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace {

/* ── 控件编号：WM_COMMAND 里靠它们区分是哪个控件发的 ───── */
constexpr INT_PTR kIdSeed = 1001;
constexpr INT_PTR kIdRegenerate = 1002;
constexpr INT_PTR kIdSelfTest = 1003;
constexpr INT_PTR kIdOutput = 1004;

/* ── 窗口与控件句柄 ───────────────────────────────────── */
HWND g_seed = nullptr;          /* 种子输入框 */
HWND g_output = nullptr;        /* 结果框（只读多行） */

/* ── 界面状态：当前要画成柱状图的直方图 ───────────────── */
demo::Histogram g_histogram;
std::wstring g_chart_note = L"还没有数据，点「重新生成」";

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

/* 读输入框里的种子，转成窄字符串交给核心模块去解析 */
std::string read_seed_text()
{
    const int length = GetWindowTextLengthW(g_seed);
    std::wstring buffer(static_cast<std::size_t>(length) + 1, L'\0');
    GetWindowTextW(g_seed, &buffer[0], length + 1);
    buffer.resize(static_cast<std::size_t>(length));

    std::string narrow(static_cast<std::size_t>(length), '\0');
    const int written = WideCharToMultiByte(CP_ACP, 0, buffer.c_str(), length,
                                            &narrow[0], length, nullptr, nullptr);
    narrow.resize(static_cast<std::size_t>(written < 0 ? 0 : written));
    return narrow;
}

/* 「重新生成」按钮：解析种子 → 抽样 → 显示文本 → 重画直方图 */
void on_regenerate(HWND hwnd)
{
    demo::Options options;
    std::string error;
    if (!demo::parse_seed(read_seed_text(), options.seed, error)) {
        g_histogram = demo::Histogram{};
        g_chart_note = L"种子有误，没有可画的数据";
        set_output("种子有误：" + error + "\r\n请填一个非负整数，例如 20240601。");
        InvalidateRect(hwnd, nullptr, TRUE);
        return;
    }

    const demo::Report report = demo::make_report(options);
    g_histogram = report.histogram;
    g_chart_note = L"种子 " + std::to_wstring(options.seed) + L"，共 "
        + std::to_wstring(report.uniform.size()) + L" 个样本";
    set_output(report.data_text + "\r\n" + report.stats_text + "\r\n" + report.rand_text);
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

/* ── GDI 绘制：把分箱计数画成柱子 ─────────────────────── */

RECT chart_rect(HWND hwnd)
{
    RECT rc{};
    GetClientRect(hwnd, &rc);
    rc.left += 14;
    rc.right -= 14;
    rc.top = 336;
    rc.bottom -= 14;
    return rc;
}

void paint_chart(HWND hwnd, HDC hdc)
{
    const RECT rc = chart_rect(hwnd);

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

    const int padding = 12;
    const int label_height = 18;
    const int axis_width = 52;                             /* 左边留一条纵轴 */
    const int base = rc.bottom - padding - label_height;   /* 柱子底边 */

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(60, 60, 70));
    HGDIOBJ old_font = SelectObject(hdc, GetStockObject(DEFAULT_GUI_FONT));

    if (!g_histogram.bins.empty() && g_histogram.peak > 0) {
        const std::size_t count = g_histogram.bins.size();
        const int inner_width = (rc.right - rc.left) - 2 * padding - axis_width;
        const int inner_height = base - (rc.top + padding);
        const int slot = inner_width / static_cast<int>(count);
        int bar_width = slot - 6;
        if (bar_width < 3) {
            bar_width = 3;
        }

        for (std::size_t i = 0; i < count; ++i) {
            const int height = static_cast<int>(
                static_cast<long long>(g_histogram.bins[i].count) * inner_height
                / static_cast<long long>(g_histogram.peak));
            RECT bar{};
            bar.left = rc.left + padding + axis_width + static_cast<int>(i) * slot + 3;
            bar.right = bar.left + bar_width;
            bar.bottom = base;
            bar.top = base - height;

            HBRUSH brush = CreateSolidBrush(RGB(70, 130, 180));
            FillRect(hdc, &bar, brush);
            DeleteObject(brush);

            /* 横轴上标出这个箱的下界，柱子太窄时隔一个标一次 */
            const std::wstring tick = std::to_wstring(g_histogram.bins[i].low);
            if (slot >= 24 || i % 2 == 0) {
                TextOutW(hdc, bar.left, base + 2, tick.c_str(),
                         static_cast<int>(tick.size()));
            }
        }

        /* 纵轴：顶端写峰值，底端写 0 */
        const std::wstring peak_text = std::to_wstring(g_histogram.peak);
        TextOutW(hdc, rc.left + padding, rc.top + padding, peak_text.c_str(),
                 static_cast<int>(peak_text.size()));
        TextOutW(hdc, rc.left + padding, base - 14, L"0", 1);
    }

    TextOutW(hdc, rc.left + padding, rc.top + 2, g_chart_note.c_str(),
             static_cast<int>(g_chart_note.size()));

    const std::wstring axis = g_histogram.bins.empty()
        ? std::wstring(L"直方图（GDI 绘制）：等待数据")
        : L"直方图（GDI 绘制）：横轴是取值区间，纵轴是落在该箱里的样本数";
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
        CreateWindowExW(0, L"STATIC", L"种子（非负整数，同一个种子给出同一批样本）：",
                        WS_CHILD | WS_VISIBLE,
                        14, 12, 420, 20, hwnd, nullptr, nullptr, nullptr);

        g_seed = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"20240601",
                                 WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | ES_NUMBER,
                                 14, 34, 200, 26, hwnd,
                                 reinterpret_cast<HMENU>(kIdSeed), nullptr, nullptr);

        CreateWindowExW(0, L"BUTTON", L"重新生成",
                        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                        230, 33, 130, 28, hwnd,
                        reinterpret_cast<HMENU>(kIdRegenerate), nullptr, nullptr);

        CreateWindowExW(0, L"BUTTON", L"跑自测",
                        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                        372, 33, 130, 28, hwnd,
                        reinterpret_cast<HMENU>(kIdSelfTest), nullptr, nullptr);

        CreateWindowExW(0, L"STATIC", L"结果（只读）：", WS_CHILD | WS_VISIBLE,
                        14, 76, 200, 20, hwnd, nullptr, nullptr, nullptr);

        g_output = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
                                   WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE
                                       | ES_AUTOVSCROLL | ES_READONLY,
                                   14, 98, 732, 222, hwnd,
                                   reinterpret_cast<HMENU>(kIdOutput), nullptr, nullptr);

        /* 控件默认字体又粗又大，换成系统界面字体 */
        const HGDIOBJ gui_font = GetStockObject(DEFAULT_GUI_FONT);
        SendMessageW(g_seed, WM_SETFONT, reinterpret_cast<WPARAM>(gui_font), TRUE);
        SendMessageW(g_output, WM_SETFONT, reinterpret_cast<WPARAM>(gui_font), TRUE);

        on_regenerate(hwnd);    /* 启动时先算一次，窗口里直接有内容 */
        return 0;
    }

    case WM_COMMAND: {
        const int id = LOWORD(wparam);
        const int code = HIWORD(wparam);
        if (id == kIdRegenerate && code == BN_CLICKED) {
            on_regenerate(hwnd);
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
    const wchar_t *class_name = L"StatsDemoWnd";

    WNDCLASSW wc{};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = instance;
    wc.lpszClassName = class_name;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);
    RegisterClassW(&wc);

    /* 固定大小：不许拉伸，省掉控件重新布局的代码 */
    const DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
    RECT rect{0, 0, 760, 640};
    AdjustWindowRect(&rect, style, FALSE);

    HWND hwnd = CreateWindowExW(0, class_name, L"示例 05 · 随机数与直方图（Win32 版）",
                                style, CW_USEDEFAULT, CW_USEDEFAULT,
                                rect.right - rect.left, rect.bottom - rect.top,
                                nullptr, nullptr, instance, nullptr);
    if (hwnd == nullptr) {
        MessageBoxW(nullptr, L"窗口创建失败", L"示例 05", MB_ICONERROR);
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
