# 示例 08 · 导出器系统：抽象基类 + 工厂 + 虚析构（CMake 多文件 + GUI）

同一张表格，导出成 CSV、Markdown、JSON 三种文本。
命令行版按格式名选一种输出；GUI 版把三种格式画成三个可点击的面板，
点哪个就出哪种结果 —— 面板上的名字与说明都来自虚函数。

```
build\mingw\bin\app_cli.exe markdown           学生成绩表导出成 Markdown
build\mingw\bin\app_cli.exe csv --tricky      换成含逗号、引号、竖线、换行的表格
build\mingw\bin\app_gui_win32.exe                    窗口里点面板切换格式
```

## 先读教材

| 章节 | 讲的是什么 | 本示例用在哪里 |
|---|---|---|
| 《05-类与面向对象/07-继承.md》第 1、3、7 节 | 继承解决什么问题、构造与析构顺序、经基类指针删除的陷阱 | 三个派生类；`Exporter` 的虚析构；自测第 16 项 |
| 《05-类与面向对象/08-多态：重载、虚函数与它们的分工.md》第 3、4、5 节 | 虚函数、纯虚函数与抽象类、覆盖与隐藏 | `name` / `hint` / `render` 三个纯虚函数；派生类全部写 `override` |
| 《05-类与面向对象/06-RAII 与资源管理.md》第 5 节 | 智能指针 | 工厂返回 `std::unique_ptr<Exporter>`，调用方不必写 `delete` |
| 《05-类与面向对象/02-类是一种类型.md》第 1、4 节 | 类是类型、定义与实现分开 | `Table` 是纯数据，导出器只负责「怎么写」 |

## 这个项目要解决什么问题

「同一份数据，多种输出格式」是实际项目里反复出现的需求。
最直接的写法是一个大 `switch`，每加一种格式就要改所有分支。
这里换成另一种写法：

- 抽象基类 `Exporter` 规定「一种格式要能做哪三件事」：报名字、给说明、渲染
- 每种格式一个派生类，只写自己那部分
- 工厂 `make_exporter("csv")` 是唯一的创建入口，调用方不必知道有几个派生类
- 调用方一律用 `std::unique_ptr<Exporter>` 持有，经基类指针调用虚函数

于是**新增一种格式，只加一个派生类并在工厂里加一行**，
调用方、命令行版、GUI 版都不用改。GUI 里那三个面板正是这么画出来的：
界面代码从头到尾没有出现 `CsvExporter` 这些类型名。

## 做完能掌握什么

- 会设计一个抽象基类：哪些函数该是纯虚的，哪些该是非虚的公共步骤
- 会解释为什么多态基类的析构函数必须是虚的，并用计数器证明它生效
- 会用工厂把「创建」与「使用」分开，返回智能指针而不是裸指针
- 会为文本格式写转义：同一份数据在三种格式里各有各的转义规则
- 会把多态的创建与调用搬到界面上：面板来自工厂，文字来自虚函数

## 文件

```
include/table.hpp          表格数据（纯数据，不认识导出器）
include/exporter.hpp       抽象基类 Exporter 与工厂声明
include/report_demo.hpp    项目逻辑的接口（不依赖界面）
src/exporter.cpp           三个派生类、工厂、基类的公共步骤
src/report_demo.cpp        演示用表格、格式信息、渲染入口、自测
src/main_cli.cpp           命令行版
src/main_gui_win32.cpp           GUI 版（Win32 API）
CMakeLists.txt             目标：core（静态库）、app_cli、app_gui_win32，加 WITH_QT=ON 后多一个 app_gui_qt
CMakePresets.json          mingw-gdb 与 msvc 两套预设
.vscode/                   调试与任务配置（四个调试配置，见下）
```

## 阶段推进

| 阶段 | 目标 | 做什么 | 怎么验收 |
|---|---|---|---|
| **阶段 1** | 跑通 | 配置、编译，运行 `app_cli.exe markdown` 与 `app_cli.exe csv --tricky` | 看到表格文本与「16 项中 16 项通过，全部通过」 |
| **阶段 2** | 跑通 GUI | 运行 `app_gui_win32.exe`，点三个面板，勾选「换成含转义字符的表格」 | 面板边框移到被选中的那个，结果框内容随之改变 |
| **阶段 3** | 加一种格式 | 加一个 `TsvExporter`（制表符分隔）与它的 `render`，在工厂里加一行，再补一条自测 | 命令行 `app_cli.exe tsv` 直接可用；GUI 上多出第四个面板，界面代码一行未改 |
| **阶段 4**（选做） | 给基类加公共步骤 | 在 `render_checked` 里加「单元格个数与表头不一致时报错」之外的检查，例如表头为空 | 命令行与 GUI 同时生效，派生类不受影响 |

阶段 3 是这个示例的核心：改动的范围就是「一个派生类 + 工厂一行 + 一条自测」。

## 构建与运行

`PowerShell`

```powershell
# 在示例 08 目录下：配置 + 编译
cmake --preset mingw-gdb
cmake --build --preset mingw-gdb

# 运行
build\mingw\bin\app_cli.exe markdown
build\mingw\bin\app_cli.exe csv --tricky
build\mingw\bin\app_gui_win32.exe
```

不想用预设时的等价写法：

`PowerShell`

```powershell
cmake -S . -B build/mingw -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_COMPILER=g++
cmake --build build/mingw
```

用 VS Code 打开本文件夹后按 `F5`，四个配置分别是命令行版与 GUI 版各两条调试路线（都是 Win32 版；Qt 版没有预置调试配置，构建后直接运行）。
调试路线的选择与 `.vscode` 的用法见 [`../README.md`](../README.md)。

## 运行后应当看到什么

`build\mingw\bin\app_cli.exe markdown`：

`实测数据`
`Text`

```text
示例 08 · 导出器：抽象基类、工厂与虚析构（命令行版）

可选格式（来自虚函数 name() 与 hint()）：
  csv：逗号分隔，字段含逗号或引号时加双引号
  markdown：竖线表格，竖线转义、换行写成 <br>
  json：带缩进的对象数组，值一律是字符串

== 项目输出 ==
--- markdown ---
| 姓名 | 语文 | 数学 |
| --- | --- | --- |
| 小明 | 78 | 92 |
| 小红 | 95 | 88 |
| 小刚 | 65 | 71 |

（加上 --tricky 换成含转义字符的表格）

== 自测 ==
  [通过] 1. 工厂一共支持 3 种格式
  [通过] 2. 每种格式的 name() 都与工厂认的名字一致
  [通过] 3. 工厂对不认识的格式返回空指针
  ……（自测共 16 项，此处省略中间几行）
  [通过] 14. 工厂造出的三个对象都还活着
  [通过] 15. unique_ptr 释放后基类对象计数回到原值
  [通过] 16. 派生类里的探针成员也析构了，说明虚析构生效

  自测结果：16 项中 16 项通过，全部通过
```

`build\mingw\bin\app_cli.exe csv --tricky` 的项目输出部分（看转义）：

`实测数据`
`Text`

```text
--- csv ---
名称,备注
逗号,"甲,乙"
引号,"说""你好"""
竖线,a|b
多行,"第一行
第二行"
```

同一张表换成 Markdown 时，竖线写成 `\|`、换行写成 `<br>`；
换成 JSON 时引号写成 `\"`、换行写成 `\n`。三种规则都在自测里逐字比对。

GUI 版没有输出，看到的是一个标题为「示例 08 · 导出器：抽象基类、工厂与虚析构」的窗口：

| 位置 | 内容 |
|---|---|
| 三个面板 | 从上到下依次是 csv / markdown / json，标题是 `name()`，说明是 `hint()`，当前选中的那个边框加粗 |
| 结果框 | 当前格式渲染出的完整文本 |
| 按钮 | 「重绘」「换成含转义字符的表格」（复选框）「跑自测」 |
| 状态栏 | 「当前格式：json（带缩进的对象数组，值一律是字符串）；表格：学生成绩」 |

点第二个面板，「当前格式」变成 `markdown`，结果框立刻换成竖线表格。

## Win32 版用的是哪些部件

| 部件 | 用法 |
|---|---|
| 窗口 | `RegisterClassW` / `CreateWindowExW` / `DefWindowProcW`，类名 `ExporterDemoWnd` |
| 消息 | `WM_CREATE`、`WM_COMMAND`（按钮与复选框）、`WM_PAINT`（画面板）、`WM_LBUTTONDOWN`（点面板）、`WM_CLOSE`、`WM_DESTROY` |
| 控件 | `EDIT`（只读多行结果框）、`BUTTON`（普通按钮与 `BS_AUTOCHECKBOX` 复选框）、`STATIC`（状态栏） |
| GDI | `FillRect`、`Rectangle`、`CreateSolidBrush`、`CreatePen`、`SelectObject`、`SetBkMode`、`SetTextColor`、`TextOutW` |
| 点击判定 | `WM_LBUTTONDOWN` 的 `lparam` 里取出 x、y，按面板宽度换算成第几个面板 |

## 两份界面

界面做了两份，功能相同，都只调用 `core`：

| | Win32 版 | Qt 版 |
|---|---|---|
| 源文件 | `src/main_gui_win32.cpp` | `src/main_gui_qt.cpp` |
| 目标名 | `app_gui_win32` | `app_gui_qt` |
| 依赖 | **零**：`user32` 与 `gdi32` 随 Windows 提供 | Qt 6 Widgets |
| 平台 | 仅 Windows | 跨平台 |
| 构建 | **默认构建** | `-DWITH_QT=ON` 才构建（默认 `OFF`） |
| 面板 | GDI 画的矩形 + `WM_LBUTTONDOWN` 命中判定 | `QWidget` 子类 + `paintEvent` + `mousePressEvent` |
| 产物 | `build/mingw/bin/app_gui_win32.exe` | `build/qt/bin/app_gui_qt.exe`（Ninja；VS 生成器多一层 `Release\`），旁边是部署出来的 Qt 与 MinGW 运行时 DLL |

**为什么要两份**：Win32 那份零依赖，读者克隆仓库后不必下载任何东西就能编译运行，
这是默认路线的全部理由；Qt 那份界面代码短、跨平台，适合要移植到别的系统的读者。
两份拿到的都是 `name()`、`hint()` 与 `render()` 的返回值，
界面代码里没有一处 `switch (format)`，也没有任何业务逻辑。

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
Qt 从哪来、怎么装、许可义务，见 [`工具/获取依赖/README.md`](../../工具/获取依赖/README.md)。

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
| exe 大小 | 4,688,594 字节（GCC 运行时已静态链入） |
| 干净目录运行 | 整个 `bin` 拷到没有 Qt、`PATH` 里也没有 Qt 与 MinGW 的目录后启动，窗口标题 `示例 08 · 导出器：抽象基类、工厂与虚析构（Qt 版）`，客户区 800x620，截图里选中面板底色 `198,224,245` 占 23,059 个像素 |
| MSVC 套件 | `-G "Visual Studio 17 2022"` 配 `msvc2022_64` 套件同样构建成功并部署 |

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
| `exporter.hpp` | 虚析构 + 三个纯虚函数；拷贝构造与拷贝赋值用 `= delete` 显式删掉 |
| `exporter.hpp` 的 `render_checked` | 非虚函数，先做公共检查再调用虚函数 `render`：公共步骤只写一遍 |
| `exporter.cpp` 的 `csv_field` | 需要加引号的三种情况与引号双写：转义规则集中在函数里 |
| `exporter.cpp` 的 `LifetimeTag` | 派生类里放一个探针成员：基类析构不是虚的，它就归不了零 |
| `exporter.cpp` 的 `make_exporter` | 工厂返回 `std::unique_ptr<Exporter>`；不认识的名字返回空指针 |
| `report_demo.cpp` 的 `format_infos` | 界面只知道 `Exporter`，`name()` 与 `hint()` 是虚函数分派出来的 |
| `report_demo.cpp` 的自测 | 三种格式的期望输出写成多行文本逐字比对，转义写错立刻能看出来 |
| `main_gui_win32.cpp` 的 `on_click` | 把点击坐标换算成面板序号，再调 `refresh` —— 界面不做任何格式判断 |
| `main_gui_qt.cpp` 的 `FormatPanel` | 同一套面板改成 Qt 控件：`paintEvent` 画、`mousePressEvent` 响应点击 |

## 已知问题

- 三种格式都把单元格当字符串处理，数字不另做类型推断
- CSV 字段里的换行按标准原样保留在引号内，用 Excel 打开时该单元格会占两行
- Win32 版的面板数量是按 `supported_formats()` 的顺序画的，
  格式多于四个时需要改布局；**窗口固定大小，不会跟着拉伸**
- Qt 版需要自建静态 Qt 才能编译，见「两份界面」一节
