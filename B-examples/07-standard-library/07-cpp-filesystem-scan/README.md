# 示例 `07-standard-library/07-cpp-filesystem-scan` · 目录扫描与属性统计（命令行 + Win32 GUI + Qt GUI）

一个能扫出报表的窗口，加一个命令行版，两者共用同一份核心逻辑：

`Text`

```text
build\mingw\bin\app_cli.exe             命令行版：扫描自带的 data/ 目录，出报表与自测
build\mingw\bin\app_cli.exe <路径>      命令行版：扫描指定目录
build\mingw\bin\app_cli.exe --selftest  命令行版：只跑自测
build\mingw\bin\app_gui_win32.exe       GUI 版（Win32 API）：改路径、点「扫描」、点「跑自测」
build\qt\bin\app_gui_qt.exe             GUI 版（Qt Widgets）：同一份报表，-DWITH_QT=ON 才构建
```

项目本体是一段 `<filesystem>` 的目录遍历：递归与非递归两种走法，
按扩展名分类统计文件数、总字节、最大文件与最深层级，收集最近修改的几个文件，
并把 `last_write_time` 换算成人能读的时间。同一个不存在的目录，
一条路线用带 `std::error_code` 的重载走，一条路线用会抛异常的重载走，
报表把两条路线的结论并排摆出来。

三份界面（命令行、Win32、Qt）都不含业务逻辑：遍历、统计、排版全在 `core` 静态库里。
把两个 GUI 文件删掉，`core` 照样能编译、能自测通过。

## 先读教材

| 章节 | 讲的是什么 | 本示例用在哪里 |
|---|---|---|
| 《07-标准库/B-07-文件系统：filesystem.md》第 1 节 | `path` 是一个类型：分段、拼接、`lexically_relative` | `join_paths()` 与 `relative_of()`；演示输出里的「path 拼装」一段 |
| 《07-标准库/B-07-文件系统：filesystem.md》第 3 节 | `directory_iterator` 与 `recursive_directory_iterator`、`depth()`、每查一次属性就是一次系统调用 | `walk_with_error_code()` 的递归与非递归两个分支；报表里的文件数与总字节 |
| 《07-标准库/B-07-文件系统：filesystem.md》第 4 节 | 建目录、删目录、`file_size`、改 `last_write_time` | 自测里造临时目录树与清理；报表里的大小与时间 |
| 《07-标准库/B-07-文件系统：filesystem.md》第 5 节 | 抛异常与收 `error_code` 两条路线，报的是同一批错误码 | `scan_or_throw()` 与 `scan_with_error_code()`；自测第 16、17 项；演示输出末尾的「取舍」一段 |

## 这个项目要解决什么问题

遍历一个目录并统计，是文件工具里最常写的一段代码。写起来不难，
难的是三件事：**顺序不确定**、**属性要一条条去查**、**失败到底怎么报**。

遍历顺序不由标准规定，因此报表里凡是并列的名次都要自己定规则：
按总字节从多到少、同一层取名字小的那个、最近修改的按时间从新到旧再按名字。
不写死规则，同一棵树两次扫出来的报表就可能不一样。

属性的代价也是真实的：`directory_entry` 只在被问到时才去查，
每问一次就是一次系统调用，因此「数条目」与「条目带大小」是两件事。

最后是失败。`<filesystem>` 的每个操作都有两条路线，
一条抛 `filesystem_error`，一条把错误码写进 `std::error_code` 参数。
两条路线报的是同一批错误码，区别只在失败怎么交出来。
本示例对同一个不存在的目录各走一次，把两次的结论都摆进报表，
读者可以直接比出该选哪条。

## 做完能掌握什么

- 会写递归与非递归两种遍历，并说清 `depth()` 为什么写在迭代器上而不在条目上
- 会用 `lexically_relative` 把绝对路径压成报表里好看的相对路径，
  并知道它是纯文本运算、不查磁盘、不化简 `..`
- 会做 `last_write_time` 到可读时间的换算，并知道 C++17 里
  `file_time_type` 与 `system_clock` 不是同一个时钟
- 会在同一个操作上写两条错误处理路线，并说清「预期结果用错误码、意外用异常」的判断
- 会把界面与逻辑分开：`dirscan` 命名空间不认识窗口，也不认识 `std::cout`

## 文件

`Text`

```text
include/dir_scan.hpp       核心库的接口：报告结构、两条扫描路线、自测入口
src/dir_scan.cpp           核心库的实现：遍历、统计、排版、自测（不依赖界面）
src/main_cli.cpp           命令行版：只负责读参数与打印
src/main_gui_win32.cpp     GUI 版之一（Win32 API）：只负责窗口、控件与字符串转换
src/main_gui_qt.cpp        GUI 版之二（Qt Widgets）：同一份功能，-DWITH_QT=ON 才构建
data/                      示例数据：不同扩展名、不同大小、不同层级的小文件
data/sub/deep/note.txt     整棵树里最深的一层，非递归扫描看不到它
CMakeLists.txt             目标：core（静态库）、app_cli、app_gui_win32，加 WITH_QT=ON 后多一个 app_gui_qt
CMakePresets.json          mingw-gdb 与 msvc 两套预设
.vscode/                   调试与任务配置（四个调试配置）
```

`core` 是纯逻辑的静态库，`app_cli` 与两份 GUI 都链接它。
界面出问题时先跑命令行版：逻辑对了，问题就在界面那一层。

## 阶段推进

| 阶段 | 目标 | 做什么 | 怎么验收 |
|---|---|---|---|
| **阶段 1** | 跑通命令行版 | 配置、编译，运行 `app_cli.exe` | 看到 8 个文件、2 个目录、1899 字节，末行是「18 项中 18 项通过，全部通过」 |
| **阶段 2** | 跑通两份界面 | 运行 `app_gui_win32.exe`，点「扫描」与「跑自测」；再按「Qt 版怎么构建」出一份 Qt 版 | 两份界面里读回的报表与命令行版逐字相同 |
| **阶段 3** | 改一条统计规则 | 把「最近修改的 5 个」改成 `ScanOptions::recent_limit` 控制，并补一条自测 | 新自测项通过；把 `recent_limit` 设成 1 时报表里只剩一条 |
| **阶段 4**（选做） | 加一个过滤 | 给 `ScanOptions` 加一个「只看某个扩展名」，扫描时跳过其余文件 | 报表里只剩那一种扩展名；两种遍历方式的结果都不受影响 |

阶段 3 是重点：统计规则一旦可配，自测就要覆盖边界值（0 条、超过文件总数）。

## 构建与运行

`PowerShell`

```powershell
# 在 07-standard-library/07-cpp-filesystem-scan 目录下：配置 + 编译
cmake --preset mingw-gdb
cmake --build --preset mingw-gdb

# 运行：不带参数时扫自带的 data/，也可以给一个目录
build\mingw\bin\app_cli.exe
build\mingw\bin\app_cli.exe <路径>
build\mingw\bin\app_gui_win32.exe

# 自测
build\mingw\bin\app_cli.exe --selftest
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

## 两份界面

界面做了两份，功能相同，都只调用 `core`：

| | Win32 版 | Qt 版 |
|---|---|---|
| 源文件 | `src/main_gui_win32.cpp` | `src/main_gui_qt.cpp` |
| 目标名 | `app_gui_win32` | `app_gui_qt` |
| 依赖 | **零**：`user32` 与 `gdi32` 随 Windows 提供 | Qt 6 Widgets |
| 平台 | 仅 Windows | 跨平台 |
| 构建 | **默认构建** | `-DWITH_QT=ON` 才构建（默认 `OFF`） |
| 产物 | `build/mingw/bin/app_gui_win32.exe` | `build/qt/bin/app_gui_qt.exe` |

两份界面的部件一一对应：

| 部件 | Win32 版 | Qt 版 |
|---|---|---|
| 窗口 | `RegisterClassW` / `CreateWindowExW` | `QWidget` 派生类 |
| 路径输入 | `EDIT`（编号 1001） | `QLineEdit` |
| 报表框 | `EDIT`（编号 1004，`ES_MULTILINE \| ES_READONLY`） | `QPlainTextEdit`（`setReadOnly(true)`） |
| 两个按钮 | `BUTTON`（编号 1002「扫描」、1003「跑自测」） | `QPushButton` |
| 点击 | `WM_COMMAND` 里的 `BN_CLICKED` | `connect(...clicked)` |
| 布局 | 控件坐标写死，窗口固定大小 | `QVBoxLayout` / `QHBoxLayout`，窗口可拉伸 |
| 窄字符串转文本 | `MultiByteToWideChar(CP_ACP, ...)` | `QString::fromLocal8Bit` |

**为什么要两份**：Win32 那份零依赖，读者克隆仓库后不必下载任何东西就能编译运行，
这是默认路线的全部理由；Qt 那份界面代码短、跨平台，适合要移植到别的系统的读者。
两份都不含业务逻辑，逻辑只在 `core` 里，因此换界面不必碰逻辑，反之也一样。

`core` 返回的是窄字符串（那个目标编译时带 `-fexec-charset=GBK`），
两份界面各自按本地 8 位编码把它转成自己那一套文本：Win32 用 `CP_ACP`，Qt 用 `fromLocal8Bit`。
两边都不写死代码页，跟着系统的 ANSI 代码页走。

### Qt 版怎么构建

Qt 版只在 `WITH_QT=ON` 时构建，默认的 `OFF` 下配置与编译都不需要 Qt。

与套件有关的规定有两条：

| 规定 | 说明 |
|---|---|
| **编译器必须与套件同源** | MinGW 套件（`mingw_64`）必须用 MinGW 编，而且要用**套件配套的那个 MinGW**：那套 Qt 由 GCC 13.1 编，就用 Qt 自带的 13.1.0。MSVC 套件必须用 MSVC 编 |
| **不传任何字符集选项** | Qt 6 在 MSVC 下自己会加 `/utf-8`，再传 `/source-charset:utf-8` 或 `-fexec-charset` 就是重复指定，编译期直接报 `D8016`。界面上的固定文字一律写成 `QStringLiteral`，本来也不依赖窄字符串的执行字符集 |

`PowerShell`

```powershell
# 1. 指向 Qt 的套件目录（编译器那一层，例如 <Qt>\6.11.1\mingw_64）
$env:QT_ROOT = '<Qt 套件目录>'

# 2. 配置：编译器必须用套件自带的那个
cmake -S . -B build/qt -G Ninja -DCMAKE_BUILD_TYPE=Release `
      -DCMAKE_CXX_COMPILER=<Qt>\Tools\mingw1310_64\bin\g++.exe -DWITH_QT=ON

# 3. 构建，完成后自动部署 Qt 运行时与编译器运行时
cmake --build build/qt

# 4. 运行（DLL 已经在 exe 旁边）
build\qt\bin\app_gui_qt.exe
```

`build/qt` 不在预设里，用上面的命令行方式配置：凡是链接了 Qt 的目标，
编译器与生成器都要与那套 Qt 一致，写进预设反而容易与别的读者那套对不上。

工程里的 `CMakeLists.txt` 只做两件事：`include` 仓库里的
`工具/获取依赖/qt-dynamic.cmake`，然后在 `find_package(Qt6 ...)` 之前核对编译器版本。

`qt-dynamic.cmake` 负责从 `-DQT_ROOT` 或环境变量 `QT_ROOT` 取套件目录、
核对「MSVC 套件 vs MinGW 套件」这一层，并把 `CMAKE_PREFIX_PATH` 与 `Qt6_DIR` 指过去。
它按设计只做这一层判断，因此**同一个工具链家族里的两个 GCC 版本它拦不住**：
`mingw_64` 套件配系统里的 g++ 也能配置成功、也能链上，问题要到运行期才显现。
本示例因此在 `CMakeLists.txt` 里补了一道 GCC 版本核对——读套件自带的
`mkspecs/qconfig.pri` 里的 `QT_GCC_MAJOR_VERSION` 等三项，与 `CMAKE_CXX_COMPILER_VERSION` 比对，
不符就用 `message(FATAL_ERROR)` 拒绝配置。没有给 `QT_ROOT`（走 `find_package` 自己找）时这一段跳过。

`qt_enable_deploy(app_gui_qt)` 是构建后的自动部署：它调用 `windeployqt`
把 `Qt6Core.dll`、`Qt6Gui.dll`、`Qt6Widgets.dll` 与 `platforms\qwindows.dll` 拷到 exe 旁边，
再显式从 `CMAKE_CXX_COMPILER` 所在目录拷 `libgcc_s_seh-1.dll`、`libstdc++-6.dll`、
`libwinpthread-1.dll`——这三个是编译器的东西，不在 `windeployqt` 的范围内。
运行时必须与编这个 exe 的编译器同源，因此来源取的是 `CMAKE_CXX_COMPILER` 的位置，不写死任何路径。
平台插件那一个最容易漏，漏了会报「no Qt platform plugin could be initialized」。

Qt 从哪来、怎么装、许可义务，见 [`工具/获取依赖/README.md`](../../../工具/获取依赖/README.md)。

## 运行后应当看到什么

下面各段输出都是在本机真跑出来的，只把其中的本机绝对路径换成了
`<工作区>`、`<Qt>`、`<MinGW>` 这三个占位符，其余逐字未改。

配置与构建：

`实测数据`
`Text`

```text
> cmake --preset mingw-gdb
-- The CXX compiler identification is GNU 15.2.0
-- Detecting CXX compiler ABI info
-- Detecting CXX compiler ABI info - done
-- Check for working CXX compiler: <MinGW>/bin/g++.exe - skipped
-- Detecting CXX compile features
-- Detecting CXX compile features - done
-- Configuring done (0.9s)
-- Generating done (0.0s)
-- Build files have been written to: <工作区>/B-examples/07-standard-library/07-cpp-filesystem-scan/build/mingw
（退出码 0）

> cmake --build --preset mingw-gdb
[1/6] Building CXX object CMakeFiles/app_cli.dir/src/main_cli.cpp.obj
[2/6] Building CXX object CMakeFiles/core.dir/src/dir_scan.cpp.obj
[3/6] Building CXX object CMakeFiles/app_gui_win32.dir/src/main_gui_win32.cpp.obj
[4/6] Linking CXX static library libcore.a
[5/6] Linking CXX executable bin\app_cli.exe
[6/6] Linking CXX executable bin\app_gui_win32.exe
（退出码 0，-Wall -Wextra 零警告）
```

命令行版 `build\mingw\bin\app_cli.exe`（不带参数，默认扫 `data/`）：

`实测数据`
`Text`

```text
示例 07-standard-library/07-cpp-filesystem-scan · 目录扫描与属性统计（命令行版）

== 项目输出 ==
扫描根目录：data（递归，含子目录）
  文件 8 个，目录 2 个，合计 1899 字节
  最深层级：第 2 层，例如 sub\deep\note.txt
  按扩展名分类（总字节多的在前）：
    .txt       3 个     1168 字节   最大 readme.txt（531 字节）   最深 第 2 层
    .md        1 个      302 字节   最大 notes.md（302 字节）   最深 第 0 层
    .png       1 个      191 字节   最大 logo.png（191 字节）   最深 第 0 层
    .ini       1 个       98 字节   最大 config.ini（98 字节）   最深 第 0 层
    .csv       1 个       71 字节   最大 sub\table.csv（71 字节）   最深 第 1 层
    (none)     1 个       69 字节   最大 LICENSE（69 字节）   最深 第 0 层
  最近修改的 5 个文件：
    2026-10-01 15:38:45      144 字节   sub\deep\note.txt
    2026-10-01 15:38:44       71 字节   sub\table.csv
    2026-10-01 15:38:38       69 字节   LICENSE
    2026-10-01 15:38:38      191 字节   logo.png
    2026-10-01 15:38:33       98 字节   config.ini

两种遍历对照：
  directory_iterator            只走一层，看到 6 个文件、1 个目录、1684 字节
  recursive_directory_iterator  连子目录，看到 8 个文件、2 个目录、1899 字节

path 拼装：
  root / "sub" / "deep"            = data\sub\deep
  root += "sub/deep"（不补分隔符）  = datasub/deep
  lexically_relative(data\sub\deep\note.txt, data) = sub\deep\note.txt
  lexically_relative("a", "a/b/../c") = b\..\c（纯文本，不化简 ..）

不存在的路径：data\no_such_subdir
  error_code 版本：不抛异常，报告里 0 个文件；ec = 2（generic），消息：No such file or directory
  异常版本：抛出 filesystem_error
    what()         = filesystem error: recursive directory iterator cannot open directory: No such file or directory [data\no_such_subdir]
    code().value() = 2
    path1()        = data\no_such_subdir
    path2()        = 
  取舍：两条路线报的是同一批错误码，区别在于失败怎么交出来。
        「文件不存在」属于预期结果，用 error_code 版，调用方按返回值分支；
        权限不足、磁盘故障这类意外用异常版，错误信息自带路径，不会被忽略。
        带 error_code 的重载只把文件系统错误转成错误码，别的异常照样抛，
        因此外层该有的 try 不能因为用了 error_code 版就省掉。

== 自测 ==
  [通过] 1. 取得系统临时目录
  [通过] 2. 在临时目录下建起三层目录树，写进 6 个文件：四种扩展名归类、大小与层级都不同
  [通过] 3. 非递归扫描只走一层
  [通过] 4. 递归扫描把子目录里的文件也算进来
  [通过] 5. 总字节数与写进去的一致
  [通过] 6. .txt 一类合在一起，.TXT 也归进来（大小写不敏感）
  [通过] 7. 没有扩展名的文件归到 (none) 一类
  [通过] 8. 最大文件落在 .bin 这一类里，是 1000 字节的 delta.bin
  [通过] 9. 最深层级是第 2 层，最深文件是 sub/deep/delta.bin
  [通过] 10. 最近修改的三个文件按时间从新到旧，最新的是 5 秒前的 delta.bin
  [通过] 11. last_write_time 换算成了 YYYY-MM-DD HH:MM:SS 形状的可读时间
  [通过] 12. file_size 读回来的大小与写进去的字节数一致
  [通过] 13. operator/ 会补分隔符，+= 直接把字符接上去
  [通过] 14. lexically_relative 把子路径变回相对路径
  [通过] 15. lexically_relative 是纯文本运算，不化简 ..
  [通过] 16. 不存在的路径：error_code 版本给出错误码，不抛异常
  [通过] 17. 不存在的路径：异常版本抛出 filesystem_error，错误码与上一条相同
  [通过] 18. 自测建的临时目录树清理干净

  自测结果：18 项中 18 项通过，全部通过
```

命令行版 `build\mingw\bin\app_cli.exe --selftest`（只跑自测，没有项目输出）：

`实测数据`
`Text`

```text
示例 07-standard-library/07-cpp-filesystem-scan · 目录扫描与属性统计（命令行版）

== 自测 ==
  [通过] 1. 取得系统临时目录
  ……（第 2 至第 17 项与上面逐字相同，此处省略）
  [通过] 18. 自测建的临时目录树清理干净

  自测结果：18 项中 18 项通过，全部通过
（退出码 0）
```

自测的临时目录树建在 `std::filesystem::temp_directory_path()` 下面，
名字是 `dirscan_selftest`，跑完由自测自己删掉——它不读仓库里的任何文件。

`app_gui_win32.exe` 没有标准输出，看到的是一个标题为
「目录扫描 · filesystem 遍历与属性统计」的窗口，客户区 712x548：

| 位置 | 内容 |
|---|---|
| 顶部标签与输入框（编号 1001） | 「要扫描的目录（留空则用 data）：」，默认填着 `data` |
| 「扫描」按钮（编号 1002） | 把输入框里的路径交给 `core`，报表整段贴进只读框 |
| 「跑自测」按钮（编号 1003） | 把自测结果整段贴进只读框 |
| 只读报表框（编号 1004） | 多行 `EDIT`，带 `ES_READONLY`，内容与命令行版的项目输出逐字相同 |

窗口是固定大小的（`WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX`），
没有写控件重排；Qt 版可以拉伸，控件按布局自动排。

`app_gui_qt.exe` 的窗口标题是「目录扫描 · filesystem 遍历与属性统计（Qt 版）」，
客户区 760x620，控件与 Win32 版一一对应，报表内容也相同。
Qt 的控件不是 Win32 控件，`EnumChildWindows` 数出来是 0 个，
用冒烟脚本能确认的只有窗口与标题，报表内容要肉眼看或另写 Qt 侧的测试。

Qt 版构建、部署与干净目录启动的结果：

`实测数据`

| 项 | 值 |
|---|---|
| 配置（`-DCMAKE_CXX_COMPILER=<Qt>\Tools\mingw1310_64\bin\g++.exe -DWITH_QT=ON`） | 退出码 0，识别为 GNU 13.1.0，套件 `mingw_64` |
| 构建（`cmake --build build/qt`） | 退出码 0，**零警告**，末尾自动执行 `windeployqt` 与三个编译器运行时的拷贝 |
| `objdump -p build\qt\bin\app_gui_qt.exe` 的 `DLL Name` | `Qt6Core.dll`、`Qt6Widgets.dll`、`libgcc_s_seh-1.dll`、`libstdc++-6.dll`、`KERNEL32.dll`、`msvcrt.dll`、`SHELL32.dll` |
| 部署目录 | `Qt6Core.dll` 11,309,920 / `Qt6Gui.dll` 11,544,920 / `Qt6Widgets.dll` 7,199,064 / `platforms\qwindows.dll` 1,272,168 / `libgcc_s_seh-1.dll` 109,056 / `libstdc++-6.dll` 2,243,072 / `libwinpthread-1.dll` 53,248 字节，一个不缺 |
| `app_gui_qt.exe` 大小 | 2,611,325 字节 |
| 干净目录启动 | 整个 `bin` 拷到没有 Qt、`PATH` 里也没有 Qt 与 MinGW 的目录后启动，窗口标题 `目录扫描 · filesystem 遍历与属性统计（Qt 版）`，客户区 760x620，进程正常退出 |

**部署完成的判据**就是最后一行：把 exe 连同旁边的 DLL 与插件目录拷到一个
没有 Qt、`PATH` 里也没有 Qt 与 MinGW 的目录，双击能起来。

`待确认`

`objdump` 列的是**直接**依赖。`Qt6Core.dll` 自己还依赖 `libstdc++-6.dll` 等几个，
因此那三个编译器运行时即使 exe 不直接引用也要随程序分发。
本示例的部署目录里它们都在，但这份清单没有逐个 DLL 展开验证。

## GUI 怎么无人值守验证

GUI 没有标准输出，结果只在窗口里。仓库里的 `工具/GUI冒烟/GUI冒烟.ps1`
能绕开「看」这一步：启动程序、按标题找窗口、列出子控件、点按钮、把控件文本读回来。
在示例目录下（工作目录必须是示例目录，程序按 `data/` 这个相对路径找数据）：

`PowerShell`

```powershell
pwsh -File "<工作区>\工具\GUI冒烟\GUI冒烟.ps1" -Exe .\build\mingw\bin\app_gui_win32.exe `
     -WindowTitle 目录扫描 -ClickId 1002 -ReadId 1004
```

`实测数据`
`Text`

```text
[进程] pid=48320  程序=<工作区>\B-examples\07-standard-library\07-cpp-filesystem-scan\build\mingw\bin\app_gui_win32.exe
[窗口] 标题=[目录扫描 · filesystem 遍历与属性统计]  类名=DirScanDemoWnd  客户区=712x548
[控件] 共 6 个
        id=0     类名=Static   文本=[要扫描的目录（留空则用 data）：]
        id=1001  类名=Edit     文本=[data]
        id=1002  类名=Button   文本=[扫描]
        id=1003  类名=Button   文本=[跑自测]
        id=0     类名=Static   文本=[报表（只读）：]
        id=1004  类名=Edit     文本=[]
[操作] 点击控件 1002（BM_CLICK）
[读回] 控件 1004 的文本：
        扫描根目录：data（递归，含子目录）
          文件 8 个，目录 2 个，合计 1899 字节
          最深层级：第 2 层，例如 sub\deep\note.txt
          按扩展名分类（总字节多的在前）：
            .txt       3 个     1168 字节   最大 readme.txt（531 字节）   最深 第 2 层
            （中间 4 行与命令行版逐字相同，此处省略）
            (none)     1 个       69 字节   最大 LICENSE（69 字节）   最深 第 0 层
          最近修改的 5 个文件：
            2026-10-01 15:38:45      144 字节   sub\deep\note.txt
            2026-10-01 15:38:44       71 字节   sub\table.csv
            2026-10-01 15:38:38       69 字节   LICENSE
            2026-10-01 15:38:38      191 字节   logo.png
            2026-10-01 15:38:33       98 字节   config.ini

        两种遍历对照：
          directory_iterator            只走一层，看到 6 个文件、1 个目录、1684 字节
          recursive_directory_iterator  连子目录，看到 8 个文件、2 个目录、1899 字节

        path 拼装：
          root / "sub" / "deep"            = data\sub\deep
          root += "sub/deep"（不补分隔符）  = datasub/deep
          lexically_relative(data\sub\deep\note.txt, data) = sub\deep\note.txt
          lexically_relative("a", "a/b/../c") = b\..\c（纯文本，不化简 ..）

        不存在的路径：data\no_such_subdir
          error_code 版本：不抛异常，报告里 0 个文件；ec = 2（generic），消息：No such file or directory
          异常版本：抛出 filesystem_error
            what()         = filesystem error: recursive directory iterator cannot open directory: No such file or directory [data\no_such_subdir]
            code().value() = 2
            path1()        = data\no_such_subdir
            path2()        = 
          取舍：两条路线报的是同一批错误码，区别在于失败怎么交出来。
                「文件不存在」属于预期结果，用 error_code 版，调用方按返回值分支；
                权限不足、磁盘故障这类意外用异常版，错误信息自带路径，不会被忽略。
                带 error_code 的重载只把文件系统错误转成错误码，别的异常照样抛，
                因此外层该有的 try 不能因为用了 error_code 版就省掉。

[收尾] 窗口还在吗 = False；进程已退出 = True
```

读回的报表与命令行版的项目输出逐字相同，包括换行。
把 `-ClickId` 换成 `1003` 再跑一次，读回的就是 18 项自测与末行
「自测结果：18 项中 18 项通过，全部通过」。

Qt 版同样能这样验证，但读不到控件文本——Qt 的控件不是 Win32 控件：

`实测数据`
`Text`

```text
> pwsh -File <工作区>\工具\GUI冒烟\GUI冒烟.ps1 -Exe <干净目录>\app_gui_qt.exe -WindowTitle 目录扫描
[进程] pid=48500  程序=<干净目录>\app_gui_qt.exe
[窗口] 标题=[目录扫描 · filesystem 遍历与属性统计（Qt 版）]  类名=Qt6111QWindowIcon  客户区=760x620
[控件] 共 0 个
[收尾] 窗口还在吗 = False；进程已退出 = True
```

窗口与标题能对上，就说明 Qt 的运行时装齐了、平台插件也加载成功——
缺任何一样，进程根本不会出现这个窗口。

## 代码里哪几处是要点

| 位置 | 要点 |
|---|---|
| `dir_scan.cpp` 的 `Accumulator::add` | 并列名次都写死规则：同层取名字小的、同时间取路径小的，报表才不随遍历顺序变化 |
| `dir_scan.cpp` 的 `read_entry_with_error_code` | 一连串带 `error_code` 的重载，任一步出错都把错误码交出来，不抛 |
| `dir_scan.cpp` 的 `entry_info_or_throw` | 同样的四步，换成会抛异常的重载；两条路线只在这里分岔 |
| `dir_scan.cpp` 的 `walk_with_error_code` | `it.increment(ec)` 与 `it != last` 的配合：失败时迭代器变成尾后，循环退出后还要再看一眼 `ec` |
| `dir_scan.cpp` 的 `walk_or_throw` | 非递归用范围 `for`，递归用 `it.depth()`——`depth()` 在迭代器上，不在条目上 |
| `dir_scan.cpp` 的 `to_system_time` | C++17 没有 `clock_cast`，只能取两个时钟当前值的差把文件时间平移过去 |
| `dir_scan.cpp` 的 `extension_key` | `(none)` 与 `.TXT` 归并：没有扩展名的单列一类，大小写统一 |
| `dir_scan.cpp` 的 `run_self_tests` | 自建临时目录树、改文件时间让次序确定、跑完删掉，不依赖仓库里的任何文件 |
| `main_gui_win32.cpp` 的 `to_wide` / `to_crlf` | 窄字符串按 `CP_ACP` 转宽；换行必须补成 CRLF，否则多行 `EDIT` 里不换行 |
| `main_gui_qt.cpp` 的 `to_crlf` | 同样的补 CRLF，但要在窄字符串上做——先按字节插 `\r` 再整体 `fromLocal8Bit`，中文才不会被当成 Latin-1 |

## 已知问题

- `data/` 里各文件的修改时间来自本机写入的那一刻，因此读者跑出来的
  「最近修改的」一段，日期与文件名顺序与本文不一定相同；
  自测那 18 项不依赖具体日期，只核对时间格式与先后次序，因此在任何机器上都能过
- 报表里的「最大」一列按文件名字节数补齐，中文文件名与英文文件名混在一起时列宽对不齐
- 扫描巨型目录时会把所有文件的修改时间先收进 `std::vector` 再排序取前几名，
  内存随文件数增长；要扫整块磁盘时应改成定长容器
- Win32 版的窗口固定大小，没有写控件重排；Qt 版可以拉伸
- `data/logo.png` 不是真正的图片，只是用那个扩展名占一个分类
- Qt 目标只加 `-Wall -Wextra`，不带任何字符集选项；
  因此 `main_gui_qt.cpp` 里的固定文字一律写成 `QStringLiteral`，不依赖执行字符集
- `qt-dynamic.cmake` 只核对「MSVC 套件 vs MinGW 套件」，
  同家族的两个 GCC 版本它拦不住；本示例在 `CMakeLists.txt` 里补了一道版本核对，
  别的示例若也要这道保护，需要各自加
