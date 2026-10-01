/**
 * main_gui_win32.cpp —— GUI 版之一（Win32 API，零依赖）
 *
 * 为什么用 Win32：它随 Windows 一起来，MinGW 的 g++ 加 -mwindows 就能直接编，
 * 读者不需要装 Qt 或任何第三方库。换成 Qt 的写法见 src/main_gui_qt.cpp。
 *
 * 界面与逻辑如何分工：
 *   本文件只管窗口、控件与字符串转换；
 *   遍历、统计、排版全在 dirscan 命名空间（src/dir_scan.cpp），与命令行版共用。
 *
 * ── 用到的 Win32 部件 ──────────────────────────────────────
 *   窗口类    RegisterClassW / CreateWindowExW / DefWindowProcW
 *   消息      WM_CREATE、WM_COMMAND、WM_CLOSE、WM_DESTROY
 *   控件      EDIT（路径输入、只读报表框）、BUTTON（扫描、跑自测）、STATIC（标签）
 *   字符集    窗口与控件一律用 W 结尾的宽字符版本，
 *             核心模块返回的窄字符串按 ANSI 代码页转宽（见 to_wide）
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "dir_scan.hpp"

#include <cstddef>
#include <filesystem>
#include <string>

namespace {

/* ── 控件编号：WM_COMMAND 里靠它们区分是哪个控件发的 ───── */
constexpr INT_PTR kIdPath = 1001;       /* 路径输入框 */
constexpr INT_PTR kIdScan = 1002;       /* 「扫描」按钮 */
constexpr INT_PTR kIdSelfTest = 1003;   /* 「跑自测」按钮 */
constexpr INT_PTR kIdOutput = 1004;     /* 只读报表框 */

HWND g_path = nullptr;
HWND g_output = nullptr;

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

/* 界面上的输入是宽字符，核心模块要窄字符串，按 ANSI 代码页转回去 */
std::string to_narrow(const std::wstring &text)
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

/* 只读报表框里的换行必须是 CRLF，单个 LF 在 EDIT 里不换行 */
std::string to_crlf(const std::string &text)
{
    std::string result;
    result.reserve(text.size() + 16);
    for (std::size_t i = 0; i < text.size(); ++i) {
        if (text[i] == '\n' && (i == 0 || text[i - 1] != '\r')) {
            result += '\r';
        }
        result += text[i];
    }
    return result;
}

void set_output(const std::string &text)
{
    SetWindowTextW(g_output, to_wide(to_crlf(text)).c_str());
}

std::wstring path_text()
{
    const int length = GetWindowTextLengthW(g_path);
    std::wstring buffer(static_cast<std::size_t>(length) + 1, L'\0');
    GetWindowTextW(g_path, &buffer[0], length + 1);
    buffer.resize(static_cast<std::size_t>(length));
    return buffer;
}

/* 「扫描」按钮：把输入框里的路径交给核心模块，报表整段贴出来 */
void on_scan()
{
    std::string path = to_narrow(path_text());
    if (path.empty()) {
        path = "data";
    }
    set_output(dirscan::build_demo_output(std::filesystem::path(path)));
}

/* 「跑自测」按钮：把核心模块的自测结果整段贴出来 */
void on_self_test()
{
    const dirscan::CheckResult result = dirscan::run_self_tests();
    std::string text;
    for (const std::string &line : result.lines) {
        text += line + "\n";
    }
    text += "\n自测结果：" + result.summary() + "\n";
    set_output(text);
}

/* ── 窗口过程 ─────────────────────────────────────────── */

LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
    switch (message) {
    case WM_CREATE: {
        /* 控件一律用 W 结尾的版本，标题里的中文才不会乱码 */
        CreateWindowExW(0, L"STATIC", L"要扫描的目录（留空则用 data）：",
                        WS_CHILD | WS_VISIBLE,
                        14, 12, 300, 20, hwnd, nullptr, nullptr, nullptr);

        g_path = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"data",
                                 WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
                                 14, 34, 420, 26, hwnd,
                                 reinterpret_cast<HMENU>(kIdPath), nullptr, nullptr);

        CreateWindowExW(0, L"BUTTON", L"扫描",
                        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                        448, 33, 120, 28, hwnd,
                        reinterpret_cast<HMENU>(kIdScan), nullptr, nullptr);

        CreateWindowExW(0, L"BUTTON", L"跑自测",
                        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                        578, 33, 120, 28, hwnd,
                        reinterpret_cast<HMENU>(kIdSelfTest), nullptr, nullptr);

        CreateWindowExW(0, L"STATIC", L"报表（只读）：", WS_CHILD | WS_VISIBLE,
                        14, 72, 200, 20, hwnd, nullptr, nullptr, nullptr);

        g_output = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
                                   WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL
                                       | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY,
                                   14, 94, 684, 440, hwnd,
                                   reinterpret_cast<HMENU>(kIdOutput), nullptr, nullptr);

        /* 控件默认字体又粗又大，换成系统界面字体 */
        const HGDIOBJ gui_font = GetStockObject(DEFAULT_GUI_FONT);
        SendMessageW(g_path, WM_SETFONT, reinterpret_cast<WPARAM>(gui_font), TRUE);
        SendMessageW(g_output, WM_SETFONT, reinterpret_cast<WPARAM>(gui_font), TRUE);

        on_scan();      /* 启动时先扫一次，窗口里直接有内容 */
        return 0;
    }

    case WM_COMMAND: {
        const int id = LOWORD(wparam);
        const int code = HIWORD(wparam);
        if (id == kIdScan && code == BN_CLICKED) {
            on_scan();
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
    const wchar_t *class_name = L"DirScanDemoWnd";

    WNDCLASSW wc{};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = instance;
    wc.lpszClassName = class_name;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);
    RegisterClassW(&wc);

    /* 固定大小：不许拉伸，省掉控件重新布局的代码 */
    const DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
    RECT rect{0, 0, 712, 548};
    AdjustWindowRect(&rect, style, FALSE);

    HWND hwnd = CreateWindowExW(0, class_name, L"目录扫描 · filesystem 遍历与属性统计",
                                style, CW_USEDEFAULT, CW_USEDEFAULT,
                                rect.right - rect.left, rect.bottom - rect.top,
                                nullptr, nullptr, instance, nullptr);
    if (hwnd == nullptr) {
        MessageBoxW(nullptr, L"窗口创建失败", L"目录扫描", MB_ICONERROR);
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
