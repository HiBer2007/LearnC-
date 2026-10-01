/**
 * main_gui_win32.cpp —— Win32 界面版（零依赖，默认构建）
 *
 * 与 main_gui_qt.cpp 功能相同：输入文件路径，点「生成报表」出报表，
 * 点「跑自测」显示自测结果。两份界面都只调用 core 里的 sales 命名空间，
 * 业务逻辑一行也不在这里。
 *
 * ── 与命令行版的对应关系 ──────────────────────────────────
 *   路径输入框   EDIT（单行）           ← argv[1]
 *   「生成报表」 BUTTON + WM_COMMAND    ← sales::load + sales::format_report
 *   「跑自测」   BUTTON + WM_COMMAND    ← sales::run_self_tests
 *   报表框       EDIT（多行只读）        ← std::cout
 *   底部状态栏   STATIC                 ← 标准错误上那一行计时
 *
 * ── 用到的 Win32 部件 ──────────────────────────────────────
 *   窗口类   RegisterClassW / CreateWindowExW / DefWindowProcW
 *   消息     WM_CREATE、WM_COMMAND、WM_CLOSE、WM_DESTROY
 *   控件     EDIT、BUTTON、STATIC
 *   字符串   MultiByteToWideChar / WideCharToMultiByte
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "sales_report.hpp"

#include <iomanip>
#include <sstream>
#include <string>

namespace {

/* ── 控件编号：WM_COMMAND 里靠它们区分是哪个控件发的 ───── */
constexpr INT_PTR kIdInput = 1001;
constexpr INT_PTR kIdGenerate = 1002;
constexpr INT_PTR kIdSelfTest = 1003;
constexpr INT_PTR kIdReport = 1004;

/* ── 控件句柄 ─────────────────────────────────────────── */
HWND g_input = nullptr;
HWND g_report = nullptr;
HWND g_status = nullptr;

/* core 给出的窄字符串是 GBK（编译时 -fexec-charset=GBK），
   这里按 ANSI 代码页转成宽字符，交给 W 结尾的 API。
   若改成按 UTF-8 转，界面上的中文会变成乱码。 */
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

std::string from_wide(const std::wstring &text)
{
    if (text.empty()) {
        return std::string();
    }
    const int length = WideCharToMultiByte(CP_ACP, 0, text.c_str(),
                                           static_cast<int>(text.size()),
                                           nullptr, 0, nullptr, nullptr);
    std::string result(static_cast<std::size_t>(length), '\0');
    WideCharToMultiByte(CP_ACP, 0, text.c_str(), static_cast<int>(text.size()),
                        &result[0], length, nullptr, nullptr);
    return result;
}

void set_status(const std::string &text)
{
    SetWindowTextW(g_status, to_wide(text).c_str());
}

/* 只读报表框认 \r\n 才换行；core 给的是 \n，显示之前补一次 \r */
void set_report(const std::string &text)
{
    std::string expanded;
    expanded.reserve(text.size() + 64);
    for (const char ch : text) {
        if (ch == '\n') {
            expanded += '\r';
        }
        expanded += ch;
    }
    SetWindowTextW(g_report, to_wide(expanded).c_str());
}

std::string read_input_path()
{
    const int length = GetWindowTextLengthW(g_input);
    if (length <= 0) {
        return std::string();
    }
    std::wstring buffer(static_cast<std::size_t>(length) + 1, L'\0');
    GetWindowTextW(g_input, &buffer[0], length + 1);
    buffer.resize(static_cast<std::size_t>(length));
    return from_wide(buffer);
}

/* 「生成报表」：调核心模块，把报表文本原样显示出来 */
void on_generate()
{
    const std::string path = read_input_path();
    if (path.empty()) {
        set_report("请先填写输入文件路径。");
        set_status("没有输入文件");
        return;
    }

    const double started = sales::now_ms();

    bool ok = false;
    std::string error;
    const sales::Report report = sales::load(path, ok, error);
    if (!ok) {
        set_report("读取失败：" + error);
        set_status("读取失败");
        return;
    }
    const std::string text = sales::format_report(report, path);

    const double finished = sales::now_ms();

    set_report(text);

    std::ostringstream status;
    status << "读取 " << report.lines_read << " 行：有效 " << report.lines_valid
           << " 行，跳过 " << report.lines_skipped << " 行，商品 "
           << report.products.size() << " 种，耗时 " << std::fixed
           << std::setprecision(3) << (finished - started) << " 毫秒";
    set_status(status.str());
}

/* 「跑自测」：把核心模块的自测结果整段显示出来 */
void on_self_test()
{
    const sales::CheckResult result = sales::run_self_tests();
    std::string text;
    for (const std::string &line : result.lines) {
        text += line + "\n";
    }
    set_report(text);
    set_status(result.summary());
}

/* ── 窗口过程 ─────────────────────────────────────────── */

LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
    switch (message) {
    case WM_CREATE: {
        /* 控件一律用 W 结尾的版本，标题里的中文才不会乱码 */
        CreateWindowExW(0, L"STATIC", L"输入文件（相对本示例目录，或写绝对路径）：",
                        WS_CHILD | WS_VISIBLE,
                        14, 12, 480, 20, hwnd, nullptr, nullptr, nullptr);

        g_input = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"data/sales.txt",
                                  WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
                                  14, 34, 560, 26, hwnd,
                                  reinterpret_cast<HMENU>(kIdInput), nullptr, nullptr);

        CreateWindowExW(0, L"BUTTON", L"生成报表",
                        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                        586, 33, 120, 28, hwnd,
                        reinterpret_cast<HMENU>(kIdGenerate), nullptr, nullptr);

        CreateWindowExW(0, L"BUTTON", L"跑自测",
                        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                        586, 67, 120, 28, hwnd,
                        reinterpret_cast<HMENU>(kIdSelfTest), nullptr, nullptr);

        CreateWindowExW(0, L"STATIC", L"报表（只读，与命令行版、与 01 的 C 版逐字节相同）：",
                        WS_CHILD | WS_VISIBLE,
                        14, 76, 620, 20, hwnd, nullptr, nullptr, nullptr);

        g_report = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
                                   WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL
                                       | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY,
                                   14, 98, 692, 380, hwnd,
                                   reinterpret_cast<HMENU>(kIdReport), nullptr, nullptr);

        g_status = CreateWindowExW(0, L"STATIC", L"",
                                   WS_CHILD | WS_VISIBLE | SS_LEFTNOWORDWRAP,
                                   14, 486, 692, 20, hwnd, nullptr, nullptr, nullptr);

        /* 控件默认字体又粗又大，换成系统界面字体 */
        const HGDIOBJ gui_font = GetStockObject(DEFAULT_GUI_FONT);
        SendMessageW(g_input, WM_SETFONT, reinterpret_cast<WPARAM>(gui_font), TRUE);
        SendMessageW(g_report, WM_SETFONT, reinterpret_cast<WPARAM>(gui_font), TRUE);
        SendMessageW(g_status, WM_SETFONT, reinterpret_cast<WPARAM>(gui_font), TRUE);

        on_generate();      /* 启动时先生成一次，窗口里直接有内容 */
        return 0;
    }

    case WM_COMMAND: {
        const int id = LOWORD(wparam);
        const int code = HIWORD(wparam);
        if (id == kIdGenerate && code == BN_CLICKED) {
            on_generate();
            return 0;
        }
        if (id == kIdSelfTest && code == BN_CLICKED) {
            on_self_test();
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

}   /* namespace */

int WINAPI WinMain(HINSTANCE instance, HINSTANCE, LPSTR, int show_command)
{
    const wchar_t *class_name = L"CppIoReportWnd";

    WNDCLASSW wc{};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = instance;
    wc.lpszClassName = class_name;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);
    RegisterClassW(&wc);

    /* 固定大小：不许拉伸，省掉控件重新布局的代码 */
    const DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
    RECT rect{0, 0, 720, 520};
    AdjustWindowRect(&rect, style, FALSE);

    HWND hwnd = CreateWindowExW(0, class_name,
                                L"示例 07-standard-library/02-cpp-io-report · iostream 报表生成器",
                                style, CW_USEDEFAULT, CW_USEDEFAULT,
                                rect.right - rect.left, rect.bottom - rect.top,
                                nullptr, nullptr, instance, nullptr);
    if (hwnd == nullptr) {
        MessageBoxW(nullptr, L"窗口创建失败", L"02-cpp-io-report", MB_ICONERROR);
        return 1;
    }

    ShowWindow(hwnd, show_command);
    UpdateWindow(hwnd);

    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    return 0;
}
