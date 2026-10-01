/**
 * main_gui_win32.cpp —— 空模板 07 的 Win32 界面版（Win32 API，零依赖）
 *
 * 窗口里放着一组图形对象（都通过基类指针持有）。点一次「加圆」「加矩形」
 * 就是让工厂造一个派生类对象；窗口重画时对每个对象调用同一行代码，
 * 却会因为虚函数而画出不同的名字、面积与横条长度——这就是多态最直观的样子。
 *
 * 构建与运行：
 *     cmake --preset mingw-gdb
 *     cmake --build --preset mingw-gdb
 *     build\mingw\bin\app_gui_win32.exe
 *
 * 窗口骨架（注册窗口类、消息循环、WM_CREATE / WM_COMMAND / WM_PAINT / WM_DESTROY）
 * 已经写好并且能编译；需要你补的是标了「界面连接」的 TODO。
 * 同一套 TODO 在 src/main_gui_qt.cpp（Qt Widgets 版）里一一对应，两份做一份即可。
 * 详细说明与自查方法见同目录《配置步骤.md》的「界面部分怎么用」一节。
 */
#define UNICODE
#define _UNICODE
#include <windows.h>

#include <cstdio>
#include <cwchar>
#include <vector>

#include "shape.hpp"

/* 控件 ID */
enum {
    ID_STATUS = 2001,
    ID_ADD_CIRCLE,
    ID_ADD_RECT,
    ID_DEL_LAST,
    ID_COUNT_CIRCLE,
    ID_CLEAR_ALL
};

/* 窗口持有的图形对象：全部通过基类指针访问 */
static std::vector<Shape *> g_shapes;

/* 已给出：UTF-8 的窄字符串转成宽字符串（W 版 API 需要宽字符串） */
static void to_wide(const char *s, wchar_t *buf, int cap)
{
    MultiByteToWideChar(CP_UTF8, 0, s, -1, buf, cap);
}

/* 已给出：给控件换上默认界面字体 */
static void set_font(HWND control)
{
    SendMessageW(control, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), TRUE);
}

/* 已给出：把 g_shapes 里非空的元素收进一个只读视图，供 count_circles 使用 */
std::vector<const Shape *> shape_view()
{
    std::vector<const Shape *> view;
    for (std::size_t i = 0; i < g_shapes.size(); ++i) {
        if (g_shapes[i] != nullptr) {
            view.push_back(g_shapes[i]);
        }
    }
    return view;
}

/* TODO（界面连接 1）：把当前状态写到状态栏控件上
 * 要求：一行文字里至少包含四项：图形个数、总面积、圆的个数、活着的对象数
 *       （Shape::live_count()）。圆的个数用 count_circles(shape_view().data(), ...) 取。
 * 提示：
 *     char narrow[512];
 *     wchar_t wide[512];
 *     std::snprintf(narrow, sizeof(narrow), "...", ...);
 *     to_wide(narrow, wide, 512);
 *     SetWindowTextW(GetDlgItem(hwnd, ID_STATUS), wide);
 * 验收：加两个图形后状态栏的个数与总面积跟着变；
 *       删除之后个数与 live 一起下降。
 */
static void refresh(HWND hwnd)
{
    (void)hwnd;
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    switch (msg) {
    case WM_CREATE: {
        /* 已给出：建好状态栏与按钮 */
        (void)lparam;
        HWND status = CreateWindowW(L"STATIC", L"", WS_CHILD | WS_VISIBLE,
                                    20, 20, 520, 24, hwnd, (HMENU)(INT_PTR)ID_STATUS,
                                    nullptr, nullptr);
        set_font(status);

        const wchar_t *labels[] = {L"加圆", L"加矩形", L"删除最后一个", L"统计圆", L"清空"};
        const int ids[] = {ID_ADD_CIRCLE, ID_ADD_RECT, ID_DEL_LAST, ID_COUNT_CIRCLE, ID_CLEAR_ALL};
        for (int i = 0; i < 5; ++i) {
            HWND button = CreateWindowW(L"BUTTON", labels[i],
                                        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                        20 + i * 108, 56, 100, 28, hwnd,
                                        (HMENU)(INT_PTR)ids[i], nullptr, nullptr);
            set_font(button);
        }

        refresh(hwnd);
        return 0;
    }

    case WM_COMMAND: {
        switch (LOWORD(wparam)) {
        case ID_ADD_CIRCLE:
            /* TODO（界面连接 2a）：让工厂造一个圆（半径 2），放进 g_shapes，然后刷新。
             * 提示：Shape *s = make_shape("circle", 2.0);
             *       造出非空指针才 push_back，最后调用 refresh(hwnd) 与 InvalidateRect。
             * 验收：窗口上多出一行 "circle"，状态栏的个数加一。 */
            break;

        case ID_ADD_RECT:
            /* TODO（界面连接 2b）：让工厂造一个 3 x 4 的矩形，放进 g_shapes，然后刷新。
             * 验收：窗口上多出一行 "rect"，它的面积是 12.0000。 */
            break;

        case ID_DEL_LAST:
            /* TODO（界面连接 2c）：删除最后一个图形，然后刷新。
             * 提示：g_shapes 里存的是基类指针，delete 时派生类的析构函数是否被调用，
             *       取决于基类析构是不是虚函数（阶段 2 的验收点）。
             * 验收：窗口上少一行，live 少一；控制台版的阶段 2 会打印析构日志。 */
            break;

        case ID_COUNT_CIRCLE:
            /* TODO（界面连接 2d）：统计当前有几个圆，并把结果显示到状态栏。
             * 提示：用 count_circles 与 shape_view()；结果可以拼进 refresh 的那行文字，
             *       也可以单独 SetWindowTextW 到 ID_STATUS 上。
             * 验收：加两个圆一个矩形后，状态栏显示圆的个数为 2。 */
            break;

        case ID_CLEAR_ALL:
            /* TODO（界面连接 2e）：删除全部图形并清空 g_shapes，然后刷新。
             * 验收：窗口上不再有图形行，live 回到 0。 */
            break;

        default:
            break;
        }
        return 0;
    }

    case WM_PAINT: {
        /* 已给出：框架与循环，只差把每个图形画出来 */
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        const char *title_utf8 = "图形列表（同一行代码，靠虚函数画出不同结果）：";
        wchar_t title[128];
        to_wide(title_utf8, title, 128);
        TextOutW(hdc, 20, 100, title, (int)std::wcslen(title));

        int y = 130;
        for (std::size_t i = 0; i < g_shapes.size(); ++i) {
            const Shape *shape = g_shapes[i];
            if (shape == nullptr) {
                continue;
            }
            /* TODO（界面连接 3）：把这一行画出来
             *   1. 取 shape->name() 与 shape->area()——两者都是虚函数调用；
             *   2. 用 as_circle(shape) 判断它是不是圆，是圆就在行首加一个 "O "；
             *   3. 用 Rectangle 画一根横条，宽度与面积成正比，直观看大小差别。
             * 提示：
             *   char narrow[256];
             *   wchar_t wide[256];
             *   std::snprintf(narrow, sizeof(narrow), "%s  面积 = %.4f", shape->name(), shape->area());
             *   to_wide(narrow, wide, 256);
             *   TextOutW(hdc, 40, y, wide, (int)std::wcslen(wide));
             *   Rectangle(hdc, 300, y + 2, 300 + (int)(shape->area() * 4), y + 18);
             * 验收：圆那一行的面积是 12.5664、横条最长；矩形是 12.0000；
             *       若所有行的面积都是 0.0000，说明阶段 1 的虚函数还没实现。 */
            y += 26;
        }

        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_DESTROY:
        for (std::size_t i = 0; i < g_shapes.size(); ++i) {
            delete g_shapes[i];
        }
        g_shapes.clear();
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
    wc.lpszClassName = L"ShapeGuiWindow";
    if (RegisterClassExW(&wc) == 0) {
        return 1;
    }

    HWND hwnd = CreateWindowExW(0, wc.lpszClassName,
                                L"空模板 07 · 继承与多态（Win32 界面）",
                                WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
                                620, 400, nullptr, nullptr, hinst, nullptr);
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
