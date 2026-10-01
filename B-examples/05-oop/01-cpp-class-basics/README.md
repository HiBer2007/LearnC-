# 示例 `05-oop/01-cpp-class-basics` · 值类型项目：自己写一个 `IntVector`（CMake 多文件 + GUI）

一个能编辑、能实时看到结果的窗口，加一个命令行版，两者共用同一份核心逻辑：

```
build\mingw\bin\app_cli.exe 8      命令行版：装 8 项斐波那契数，做前缀和与缩放
build\mingw\bin\app_gui_win32.exe        GUI 版：输入框里改数列，下面实时出结果与柱状图
```

项目本体是自己写的一个整型动态数组 `IntVector`：构造与析构、拷贝与移动、
`const` 成员函数、类内初始化器、静态成员、运算符一应俱全，
然后用它做一段真实的计算（斐波那契数列的前缀和与缩放）。

## 先读教材

| 章节 | 讲的是什么 | 本示例用在哪里 |
|---|---|---|
| 《05-类与面向对象/02-类是一种类型.md》第 1、4、5 节 | 类与结构体的差别、定义与实现分开放 | `include/int_vector.hpp` 只放声明，长函数在 `src/int_vector.cpp` |
| 《05-类与面向对象/04-构造与析构.md》第 1、2、3、6 节 | 构造、初始化列表、析构、自动生成的成员 | 四个构造函数、一个析构函数，都手写 |
| 《05-类与面向对象/05-拷贝与移动.md》第 2、3、4、5 节 | 拷贝构造、拷贝赋值、移动、六个特殊成员 | 拷贝并交换、移动只搬指针；自测第 3 至第 8 项 |
| 《05-类与面向对象/09-运算符重载.md》第 2、3 节 | 写成成员还是非成员函数 | `+=` `*=` 写成成员，`+` `*` `==` `<<` 写成非成员 |
| 《05-类与面向对象/03-成员与细节.md》第 1、2、4 节 | 静态成员、`const` 成员函数、成员的初值 | `live_count()` / `allocations()`；`data_ = nullptr` 等类内初始化器 |

## 这个项目要解决什么问题

标准库已经有 `std::vector`，为什么还要自己写一个：

- 值类型该有的六件事（构造、析构、拷贝构造、拷贝赋值、移动构造、移动赋值）
  只有自己写一遍，才知道编译器默认生成的那几个够不够用
- 「移动之后源对象里还剩什么」「拷贝赋值要不要先销毁自己」「自赋值会不会把自己拆了」
  这些问题在标准库上看不到，在自己写的类上必须回答

算出来的结果也不只是看看：程序带一组自测，
逐项核对拷贝出来的两份是不是独立、移动之后源对象是不是空了、
移动有没有偷偷复制元素、对象是不是都析构了。

## 做完能掌握什么

- 会判断一个类该不该允许拷贝：管资源的类型只能移动，不能拷贝
- 会写「拷贝并交换」式的赋值运算符，并说明它为什么对自赋值安全
- 会用静态计数器给自己写的类做体检：对象数、分配次数都是可观察的证据
- 会把「界面」与「逻辑」分开：`IntVector` 与 `demo` 命名空间不认识窗口，也不认识 `std::cout`
- 会用 Win32 API 写一个零依赖的小窗口，并知道为什么不用 Qt

## 文件

```
include/int_vector.hpp     类定义：声明与一行式的短函数
include/vector_demo.hpp    项目逻辑的接口（不依赖界面）
src/int_vector.cpp         类的实现：内存管理、拷贝、移动、运算符
src/vector_demo.cpp        项目逻辑的实现：数列、解析、格式化、自测
src/main_cli.cpp           命令行版：只负责读参数与打印
src/main_gui_win32.cpp           GUI 版（Win32 API）：只负责窗口、控件与 GDI 绘制
CMakeLists.txt             目标：core（静态库）、app_cli、app_gui_win32，加 WITH_QT=ON 后多一个 app_gui_qt
CMakePresets.json          mingw-gdb 与 msvc 两套预设
.vscode/                   调试与任务配置（四个调试配置，见下）
```

`core` 是纯逻辑的静态库，`app_cli` 与两个 GUI 版都链接它。
界面出问题时先跑命令行版：逻辑对了，问题就在界面那一层。

## 阶段推进

| 阶段 | 目标 | 做什么 | 怎么验收 |
|---|---|---|---|
| **阶段 1** | 跑通命令行版 | 配置、编译，运行 `app_cli.exe 8` | 看到数列、前缀和与「20 项中 20 项通过，全部通过」 |
| **阶段 2** | 跑通 GUI 版 | 运行 `app_gui_win32.exe`，改输入框，点「计算」 | 结果框里换成新数列；下方柱状图跟着变；点「跑自测」显示 20 项通过 |
| **阶段 3** | 给类加一个成员 | 加 `push_front` 或 `pop_back`，并补一条自测；注意 `reserve` 与 `size_` 的配合 | 新自测项通过；`live_count()` 在自测结束时仍回到基线 |
| **阶段 4**（选做） | 加一种界面 | 仿照 `main_gui_win32.cpp` 再写一个只读的柱状图窗口，或把 `fibonacci` 换成别的数列 | 不改 `core` 的任何一行就能换界面，这就是分层的目的 |

阶段 3 是重点：一旦动了内存管理，自测里的对象计数与分配次数会立刻告诉你有没有写错。

## 构建与运行

`PowerShell`

```powershell
# 在 05-oop/01-cpp-class-basics 目录下：配置 + 编译
cmake --preset mingw-gdb
cmake --build --preset mingw-gdb

# 运行
build\mingw\bin\app_cli.exe 8
build\mingw\bin\app_gui_win32.exe
```

不想用预设时，等价的手写命令是：

`PowerShell`

```powershell
cmake -S . -B build/mingw -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_COMPILER=g++
cmake --build build/mingw
```

用 VS Code 打开本文件夹后按 `F5`，有四个配置可选：
`GDB · 命令行版`、`MSVC · 命令行版`、`GDB · GUI 版 Win32`、`MSVC · GUI 版 Win32`。
调试路线的选择、`.vscode` 的用法与产物位置见 [`../../README.md`](../../README.md)。

## 运行后应当看到什么

命令行版 `build\mingw\bin\app_cli.exe 8`：

`实测数据`
`Text`

```text
示例 07 · 值类型 IntVector（命令行版）

== 项目输出 ==
  斐波那契数列 : [1, 1, 2, 3, 5, 8, 13, 21]
  前缀和       : [1, 2, 4, 7, 12, 20, 33, 54]
  每项乘 3     : [3, 3, 6, 9, 15, 24, 39, 63]
  每项加 1     : [2, 2, 3, 4, 6, 9, 14, 22]
  与 std::vector 对照：结果一致（共 8 项）
  长度 8，首项 1，末项 21，容量 8
  故意越界一次：IntVector::at 下标越界

== 自测 ==
  [通过] 1. 初始化列表构造 {1, 2, 3}
  [通过] 2. IntVector(4, 7) 造出 4 个 7
  [通过] 3. 拷贝构造出的对象是独立的一份
  ……（自测共 20 项，此处省略中间几行）
  [通过] 18. 自己写的容器与 std::vector 算出的数列一致
  [通过] 19. 作用域内 6 个对象都活着
  [通过] 20. 离开作用域后全部析构，没有对象泄漏

  自测结果：20 项中 20 项通过，全部通过
```

GUI 版没有输出，看到的是一个标题为「示例 07 · IntVector 值类型演示」的窗口：

| 位置 | 内容 |
|---|---|
| 顶部输入框 | `1 1 2 3 5 8 13 21 34 55`，可以随便改成 `3 1 4 1 5 9 2 6` |
| 结果框 | 输入数列、前缀和、每项乘 3、每项加 1、长度与容量、总和 |
| 下方柱状图 | 按当前数列画的柱子，高度与数值成正比，颜色循环使用四种 |
| 两个按钮 | 「计算」重新算并重画；「跑自测」把 20 项自测结果显示在结果框里 |

改输入框后点「计算」，结果框与柱状图会一起变。

## Win32 版用的是哪些部件

| 部件 | 用法 |
|---|---|
| 窗口 | `RegisterClassW` / `CreateWindowExW` / `DefWindowProcW`，类名 `IntVectorDemoWnd` |
| 消息 | `WM_CREATE`（建控件）、`WM_COMMAND`（按钮）、`WM_PAINT`（画图）、`WM_CLOSE`、`WM_DESTROY` |
| 控件 | `EDIT`（输入框、只读多行结果框）、`BUTTON`（计算、跑自测）、`STATIC`（标签） |
| GDI | `BeginPaint`/`EndPaint`、`FillRect`、`Rectangle`、`CreateSolidBrush`、`CreatePen`、`SelectObject`、`SetBkMode`、`SetTextColor`、`TextOutW` |
| 字符集 | 窗口与控件一律用 `W` 结尾的宽字符版本；核心模块返回的窄字符串按 ANSI 代码页转宽（`MultiByteToWideChar(CP_ACP, ...)`），否则界面上的中文是乱码 |

## 两份界面

界面做了两份，功能相同，都只调用 `core`：

| | Win32 版 | Qt 版 |
|---|---|---|
| 源文件 | `src/main_gui_win32.cpp` | `src/main_gui_qt.cpp` |
| 目标名 | `app_gui_win32` | `app_gui_qt` |
| 依赖 | **零**：`user32` 与 `gdi32` 随 Windows 提供 | Qt 6 Widgets |
| 平台 | 仅 Windows | 跨平台 |
| 构建 | **默认构建** | `-DWITH_QT=ON` 才构建（默认 `OFF`） |
| 产物 | `build/mingw/bin/app_gui_win32.exe` | `build/qt/bin/app_gui_qt.exe`（Ninja；VS 生成器多一层 `Release\`），旁边是部署出来的 Qt 与 MinGW 运行时 DLL |

**为什么要两份**：Win32 那份零依赖，读者克隆仓库后不必下载任何东西就能编译运行，
这是默认路线的全部理由；Qt 那份界面代码短、跨平台，适合要移植到别的系统的读者。
两份都不含业务逻辑，逻辑只在 `core` 里，因此换界面不必碰逻辑，反之也一样。

默认路线不需要 Qt：

`PowerShell`

```powershell
cmake --preset mingw-gdb
cmake --build --preset mingw-gdb
```

### Qt 版怎么构建

Qt 版只在 `WITH_QT=ON` 时构建。与套件有关的规定有两条：

| 规定 | 说明 |
|---|---|
| **编译器必须与套件一致** | MinGW 套件（`mingw_64`）必须用 MinGW 编，而且要用**套件配套的那个 MinGW**：这套 Qt 是 GCC 13.1 编的，就用 Qt 自带的 13.1.0。MSVC 套件必须用 MSVC 编。混用会在配置阶段被 `qt-dynamic.cmake` 拦下，或在链接期报一片未解析符号 |
| **构建配置** | **动态版**套件（本仓库的默认路线）Debug 与 Release 都能配；**静态版**套件才要求与 Qt 一致，否则链接期报 `LNK2038: "_ITERATOR_DEBUG_LEVEL" 值"2"不匹配值"0"` |

`PowerShell`

```powershell
# 1. 指向 Qt 的套件目录（编译器那一层，例如 <Qt>/6.11.1/mingw_64）
$env:QT_ROOT = '<Qt 套件目录>'

# 2. 配置：编译器与生成器都要与套件一致
#    MinGW 套件，用 Qt 自带的那个 MinGW
cmake -S . -B build/qt -G Ninja -DCMAKE_BUILD_TYPE=Release `
      -DCMAKE_CXX_COMPILER=<Qt>\Tools\mingw1310_64\bin\g++.exe -DWITH_QT=ON
#    MSVC 套件
cmake -S . -B build/qt -G "Visual Studio 17 2022" -A x64 -DWITH_QT=ON

# 3. 构建，完成后自动部署运行时
cmake --build build/qt                     # Ninja
cmake --build build/qt --config Release    # Visual Studio 生成器

# 4. 运行（Qt 与 MinGW 的 DLL 已经在 exe 旁边）
build\qt\bin\app_gui_qt.exe              # Ninja；VS 生成器是 build\qt\bin\Release\
```

生成器名与 `--config` 不是固定答案：换成读者自己那套 Qt 时，按它的编译器与配置来。
`qt-dynamic.cmake` 读 `-DQT_ROOT` 或环境变量 `QT_ROOT`，并在配置阶段检查编译器是否匹配；
两者都没给时它直接返回，由 `find_package` 到系统里找。
Qt 从哪来、怎么装、许可义务，见 [`工具/获取依赖/README.md`](../../../工具/获取依赖/README.md)。

### 构建后自动部署

动态版 Qt 的程序启动时要能找到 `Qt6Core.dll`、`Qt6Gui.dll`、`Qt6Widgets.dll`
与 `platforms\qwindows.dll`，否则双击就报缺 DLL。构建完成后 CMake 会调用
`windeployqt` 把它们拷到 exe 旁边（开关 `WITH_QT_DEPLOY`，默认 `ON`）。

MinGW 目标还多一步：`libgcc_s_seh-1.dll`、`libstdc++-6.dll`、`libwinpthread-1.dll`
不在 Qt 的部署范围内，CMake 会**从编译这个 exe 的那套 MinGW 的 `bin` 目录**取它们
（依据是 `CMAKE_CXX_COMPILER` 的位置，不写死任何本机路径）——
必须与编译器同源，不同 GCC 版本的 `libstdc++-6.dll` 混用会出怪问题。
文件已存在且内容相同时自动跳过。

MinGW 目标的 exe 还用 `-static-libgcc -static-libstdc++` 把 GCC 运行时静态链进去，
于是 **exe 自己**不再依赖 `libgcc_s_seh-1.dll` 与 `libstdc++-6.dll`；
但 Qt 自己的 DLL 仍然依赖它们，所以这三个文件照样要随程序分发。

目标机器上还需要的运行库：MSVC 套件要装 VC++ 运行库；
MinGW 套件就是上面那三个 DLL，已经随部署拷到 exe 旁边。

**部署完成的判据**：把 exe 连同旁边的 DLL 与插件目录拷到一个
**没有 Qt、`PATH` 里也没有 Qt 与 MinGW** 的目录，双击能起来。

`实测数据`
`Text`

```text
> objdump -p app_gui_qt.exe | Select-String "DLL Name"      # MinGW 套件，07
  DLL Name: Qt6Core.dll
  DLL Name: Qt6Gui.dll
  DLL Name: Qt6Widgets.dll
  DLL Name: KERNEL32.dll
  DLL Name: msvcrt.dll
  DLL Name: libwinpthread-1.dll
  DLL Name: SHELL32.dll

> objdump -p Qt6Core.dll | Select-String "DLL Name" | Select-String "lib"
  DLL Name: libgcc_s_seh-1.dll
  DLL Name: libwinpthread-1.dll
  DLL Name: libstdc++-6.dll
```

`实测数据`

| 项 | 值 |
|---|---|
| 构建 | `cmake --build build/qt`（Ninja，GNU 13.1.0 + Qt 6.11.1 `mingw_64`），退出码 0，**无警告** |
| 部署目录 | `Qt6Core.dll` 11,309,920 / `Qt6Gui.dll` 11,544,920 / `Qt6Widgets.dll` 7,199,064 / `platforms\qwindows.dll` 1,272,168 / `libgcc_s_seh-1.dll` 109,056 / `libstdc++-6.dll` 2,243,072 / `libwinpthread-1.dll` 53,248 字节，一个不缺 |
| exe 大小 | 3,967,787 字节（GCC 运行时已静态链入） |
| 干净目录运行 | 整个 `bin` 拷到没有 Qt、`PATH` 里也没有 Qt 与 MinGW 的目录后启动，窗口标题 `示例 07 · IntVector 值类型演示（Qt 版）`，客户区 760x640，截图里柱子色 `70,130,180` 占 13,794 个像素 |
| MSVC 套件 | `-G "Visual Studio 17 2022"` 配 `msvc2022_64` 套件同样构建成功并部署（`app_gui_qt.exe` 90,112 字节，配一整套 Qt 的 DLL） |

> [!WARNING]
> **`WITH_QT=ON` 时不要给 Qt 目标传字符集选项。**
> Qt 6 在 MSVC 下会自动给目标加 `/utf-8`，而 `/utf-8` 已经同时定下源字符集与执行字符集；
> 再传 `/source-charset:utf-8` 或 `/execution-charset:gbk` 属于重复指定，编译期直接报
> `D8016: “/source-charset:utf-8”和“/utf-8”命令行选项不兼容`。
> Qt 的字符串走 `QString`，界面上的固定文字一律写成 `QStringLiteral`，
> 本来也不依赖窄字符串的执行字符集。`CMakeLists.txt` 里因此刻意没有这两项。

`待确认`

**静态版套件另有一条约束**：Debug 与 Release 必须与 Qt 一致。
本机那套静态 Qt 由 MSVC 19.51 构建，而预设用的 cl 是 19.44，
两者能否链接尚未再实测；静态路线已不是本仓库的主路线，改用它时请自行验证。

**`WITH_QT` 为默认的 `OFF` 时，找不到 Qt 也不影响构建**：
命令行版与 Win32 版照常配置、照常编译，不会报错。

## 代码里哪几处是要点

| 位置 | 要点 |
|---|---|
| `int_vector.hpp` 私有段 | 三个成员全带类内初始化器：默认构造出来的对象是合法的空数组 |
| `int_vector.cpp` 的 `IntVector(const IntVector &)` | 自己分配、自己逐个复制：这是「深拷贝」，不是复制指针 |
| `int_vector.cpp` 的移动构造 | 只搬三个成员并把源对象置空，因此可以标 `noexcept`，也不产生分配 |
| `int_vector.cpp` 的拷贝赋值 | 拷贝并交换：先造副本再换进来，中途抛异常也不会破坏原对象；`this != &other` 挡住自赋值 |
| `int_vector.cpp` 的 `operator<<` | 非成员函数，收 `const IntVector&`，因此 `const` 对象也能打印 |
| `vector_demo.cpp` 的 `parse_numbers` | 解析输入并给出可读错误，界面与命令行共用同一份 |
| `vector_demo.cpp` 的 `run_self_tests` | 20 项自测全部在这里，界面只负责把结果摆出来 |
| `main_gui_win32.cpp` 的 `paint_chart` | 纯 GDI 绘制：底色、边框、四色柱子、两行文字 |
| `main_gui_qt.cpp` 的 `ChartWidget::paintEvent` | 同样一版柱状图，换成 `QPainter`；两份界面的几何算法一致 |

## 已知问题

- Win32 版的窗口是固定大小的（`WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX`），
  **窗口不会跟着拉伸**，没有写控件重排；Qt 版可以拉伸，控件按布局自动排
- 柱状图的颜色按序号轮换，不代表任何含义，只为看清相邻柱子的边界
- `IntVector` 只支持 `int`；要放别的类型，见 `05-oop/03-cpp-raii-and-templates` 的类模板写法
- Qt 版需要自建静态 Qt 才能编译，见「两份界面」一节
