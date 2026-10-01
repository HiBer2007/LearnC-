# 示例 `07-standard-library/02-cpp-io-report` · iostream 报表生成器（命令行 + Win32 + Qt）

一份用 C++ 写的销售记录统计工具。它读一份文本，逐行拆字段、把坏行挑出来、
按商品聚合、排序，最后用 `<iomanip>` 拼出一张固定版式的报表；
命令行版、Win32 界面版与 Qt 界面版共用同一个 `core`，
三边输出的报表逐字节相同。

项目的重点是 `<iostream>` 这一套的四个部件各管什么：
`<fstream>` 读文件、`<sstream>` 拆一行与拼一段文本、`<iomanip>` 定列宽与对齐、
流的状态位告诉调用方「读到尾了」还是「读坏了」。核心库另带 20 项自测。

本示例与 [`01-c-stdlib-toolbox`](../01-c-stdlib-toolbox/) 是**一对**：读同一份输入，出同一份报表。
两边的差别只在写法，对照点见下面「与 01 的对照」一节。

`Text`

```text
02-cpp-io-report/
  include/sales_report.hpp   接口：记录、聚合结果、解析、排序、读文件、拼报表、自测
  src/sales_report.cpp       实现，核心逻辑全在这里，三份界面只调它
  src/main_cli.cpp           命令行版：认参数、调核心、把报表写到标准输出
  src/main_gui_win32.cpp     Win32 界面版（默认构建，零依赖）
  src/main_gui_qt.cpp        Qt Widgets 界面版（-DWITH_QT=ON 才构建）
  data/sales.txt             示例数据，与 01 的那一份逐字节相同
  CMakeLists.txt             目标：core（静态库）、app_cli、app_gui_win32、app_gui_qt
  CMakePresets.json          mingw-gdb（Ninja + g++）与 msvc（Visual Studio 17 2022）两套预设
  .vscode/                   五个调试配置与六个构建任务
```

`core` 是纯逻辑的静态库，删掉三个 `main` 文件它照样能编译、能自测通过。
两份界面里一行业务逻辑都没有：它们只负责把 `core` 给出的报表文本摆到只读框里。

## 先读教材

| 章节 | 讲的是什么 | 本示例用在哪里 |
|---|---|---|
| 《07-标准库/B-00-导读：C++ 标准库与 C 的关系.md》第 3.4 小节 | 写错了会怎样：混用两套输出，顺序错乱 | 计时为什么走 `std::cerr`，不混进 `std::cout` |
| 《07-标准库/B-01-输入输出：iostream.md》第 3.1 小节 | 常用件与两个细读点 | `setw`、`left` / `right`、`fixed`、`setprecision` 的配合 |
| 《07-标准库/B-01-输入输出：iostream.md》第 5.2 小节 | 逐行读一个文件 | `std::getline` 的读循环 |
| 《07-标准库/B-01-输入输出：iostream.md》第 6.1 小节 | 解析一行 | `std::istringstream` 的 `>>` 当分词用 |
| 《07-标准库/B-01-输入输出：iostream.md》第 6.2 小节 | 拼一段文本 | `std::ostringstream` 拼整张报表 |
| 《07-标准库/B-01-输入输出：iostream.md》第 7.1 小节 | 三个类型与打开模式 | `std::ifstream` 读输入，`std::ofstream` 加 `binary` 写输出 |
| 《07-标准库/B-01-输入输出：iostream.md》第 7.3 小节 | 两条错误处理路线 | `is_open()` 与 `bad()` 各管什么 |
| 《07-标准库/B-01-输入输出：iostream.md》第 8.3 小节 | 怎么选 | 与 01 的 `printf` 版逐项对照 |
| 《07-标准库/B-02-std-string 与 string_view.md》第 1.3 小节 | 它仍然是一串字节 | `setw` 按字符数补齐，GBK 下一个汉字正好 2 个 |
| 《07-标准库/B-02-std-string 与 string_view.md》第 7.1、7.2 小节 | 两个方向各有一族函数；`stoi` 的几个细节 | `std::stol` / `std::stod` 的 `used` 与异常 |
| 《07-标准库/B-06-时间：chrono.md》第 2.2 小节 | 三种 clock 的区别 | 计时用 `steady_clock` 而不是 `system_clock` |
| 《07-标准库/B-06-时间：chrono.md》第 3.1 小节 | 正确写法与多次测量 | 这里只取一次测量，为什么够用 |

## 这个项目要解决什么问题

报表类程序在 C++ 里有一套固定的骨架：读文件、拆字段、算、排序、按列宽输出。
骨架不长，但每一步都有一个容易写错的地方。

**读文件**。`std::getline` 一次给一行，到文件尾或读出错时流变成「假」。
问题在于这两种情况都会让 `while (std::getline(...))` 停下来，
只看循环退不出来分辨是哪种。标准给的答案是循环结束后查 `bad()`：
它只在真的读坏时置位，文件正常读完不会。项目按这条来写。

**拆字段**。`std::istringstream` 加 `operator>>` 是现成的分词器：
它自动跳过任意多个空白（空格、制表符、回车都算），
于是 C 版里手工写的 `strspn` / `strcspn` 在这里变成两行。
代价是它不告诉你「一共有几个字段」，得自己数。

**转数字**。`std::stol` / `std::stod` 在解析失败时抛异常，
而 `atoi` 只是悄悄返回 0。项目把它们各自包一层：捕获异常、
再用 `used` 参数确认整个串都被吃掉了 —— `"12x"` 必须判为失败，
这一点光靠异常抓不住。

**排序**。`std::sort` 的比较函数返回 `bool`，语义是「a 该不该排在 b 前面」，
不像 `qsort` 那样返回 `int` 的符号。项目里三个 lambda 都写成
「金额不等就按金额降序，相等再按名称升序」，让同样的输入永远得到同样的输出。

**拼文本**。报表每一列的宽度是固定的，`std::setw` 正好做这件事，
但它只作用于紧随其后的那一次输出，而且 `left` / `right` 是粘住的状态。
项目在每一行里显式写出对齐方式，读代码时不必回头找上一次设的是什么。

**测耗时**。`std::chrono::steady_clock` 在标准里就规定了只往前走，
不受系统对时影响，这一条比 C 那边简洁：C 版只能落到 `QueryPerformanceCounter`。

## 做完能掌握什么

- 会用 `std::getline` + `bad()` 写出正确的读文件循环
- 会用 `std::istringstream` 拆一行，用 `std::ostringstream` 拼一整段文本
- 会用 `setw` / `left` / `right` / `fixed` / `setprecision` 排出一张对齐的表
- 会用 `std::stol` / `std::stod` 做「能报错的解析」，并知道光靠异常不够
- 会用 `std::sort` 的 lambda 比较函数，并写出稳定的并列规则
- 会区分 `steady_clock` 与 `system_clock`，知道测耗时该用哪一个
- 会把核心逻辑与界面彻底分开：`core` 里不认识 `HWND`，也不认识 `QString`

## 与 01 的对照

[`01-c-stdlib-toolbox`](../01-c-stdlib-toolbox/) 做的是同一件事：读同一份 `data/sales.txt`，
出一份逐字节相同的报表。两边都编译成 `core` 静态库 + `app_cli` + `app_gui_win32`，
目录结构也一样。同一个任务的两种写法如下。

| 环节 | [`01-c-stdlib-toolbox`](../01-c-stdlib-toolbox/)（C） | 本示例（C++） |
|---|---|---|
| 读文件 | `fopen` + `fgets` + `ferror` | `std::ifstream` + `std::getline` + `bad` |
| 分字段 | `strspn` / `strcspn` 手工切 | `std::istringstream` 的 `>>` |
| 数字转换 | `strtol` / `strtod` 三件套 | `std::stol` / `std::stod` 加 `try` |
| 存数据 | 固定长度数组 + 计数 | `std::vector` |
| 排序 | `qsort` + 三个比较函数 | `std::sort` + lambda |
| 拼报表 | `snprintf` / `vsnprintf` | `std::ostringstream` + `<iomanip>` |
| 测耗时 | `QueryPerformanceCounter` | `std::chrono::steady_clock` |
| 界面 | Win32（C 写） | Win32 与 Qt 两份 |

**报表本身完全相同**。两份 `data/sales.txt` 逐字节相同（1498 字节），
用 `--out` 导出的报表文件都是 1883 字节，逐字节相同：

`实测数据`
`PowerShell`

```powershell
# 在 01 目录下
.\build\mingw\bin\app_cli.exe data\sales.txt --out "$env:TEMP\r01.txt"
# 在 02 目录下
.\build\mingw\bin\app_cli.exe data\sales.txt --out "$env:TEMP\r02.txt"
```

`实测数据`
`Text`

```text
r01.txt  1883 字节
r02.txt  1883 字节
逐字节比较：完全相同

两份的完整标准输出都是 45 行；去掉第 1 行的程序横幅之后，
其余 44 行 0 处差异。
```

列宽能对上的原因是一条编译选项：两个工程都用 `-fexec-charset=GBK`，
一个汉字在 GBK 里是 2 个字节、也是 2 个字符，
于是 `std::setw(16)` 与 C 版的 `%-16s` 补出来一样宽。

代码量上的差别也是对照点：本示例的 `core` 比 01 短，
因为数组、排序、字符串拼接都交给了标准库；代价是运行时多一层抽象，
具体取舍见《07-标准库/B-01-输入输出：iostream.md》第 8 节。

## 阶段推进

| 阶段 | 目标 | 做什么 | 怎么验收 |
|---|---|---|---|
| **阶段 1** | 跑通命令行版 | 配置、编译，运行 `build\mingw\bin\app_cli.exe` | 看到报表与「20 项中 20 项通过，全部通过」 |
| **阶段 2** | 看懂拼表 | 读 `format_report`，把每一处 `setw` 与 C 版的格式串对上 | 能说清 `left` / `right` 各粘到哪一次输出为止 |
| **阶段 3** | 与 C 版对照 | 建好 `01-c-stdlib-toolbox`，把两份 `--out` 的结果比一次 | 两个文件字节数相同、内容相同 |
| **阶段 4** | 换一份数据 | 把自己的 `.txt` 交给 `app_cli <文件路径>`，观察「跳过行数」 | 坏行被算进跳过，有效行数与商品种类对得上 |

阶段 4 是重点：换数据之后 `读取行数`、`有效数据行`、`跳过行数` 三个数
应当满足「注释与空行之外，有效加跳过等于总行数减注释减空行」，
对不上就说明解析规则与预期不同。

## 构建与运行

`PowerShell`

```powershell
# 在 07-standard-library/02-cpp-io-report 目录下：配置 + 编译
cmake --preset mingw-gdb
cmake --build --preset mingw-gdb

# 运行：默认读 data/sales.txt（工作目录必须是示例目录）
build\mingw\bin\app_cli.exe

# 换一份输入
build\mingw\bin\app_cli.exe data\sales.txt

# 顺便把报表写进文件，用来与 01 的输出比对
build\mingw\bin\app_cli.exe data\sales.txt --out report-out.txt

# 只跑自测
build\mingw\bin\app_cli.exe --selftest

# 界面版（Win32，默认构建）
build\mingw\bin\app_gui_win32.exe

# 界面版（Qt，默认不构建；需要自备 Qt 6 Widgets）
cmake --preset mingw-gdb -DWITH_QT=ON
cmake --build --preset mingw-gdb
build\mingw\bin\app_gui_qt.exe
```

不想用预设时，等价的手写命令是：

`PowerShell`

```powershell
cmake -S . -B build/mingw -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_COMPILER=g++
cmake --build build/mingw
```

`WITH_QT` 默认为 `OFF`，**不装 Qt 也能配置、也能构建命令行版与 Win32 版**。
要构建 Qt 版时先准备一套 Qt 6 Widgets，用法见
[`07-cpp-filesystem-scan` 的「Qt 版怎么构建」](../07-cpp-filesystem-scan/) 与
[`工具/获取依赖/README.md`](../../../工具/获取依赖/README.md)。
Qt 目标刻意不传任何字符集选项：Qt 6 在 MSVC 下自己会加 `/utf-8`，
再传 `/source-charset:utf-8` 或 `/execution-charset:gbk` 会报 `D8016`；
界面上的固定文字一律写成 `QStringLiteral`。

用 VS Code 打开本文件夹后按 `F5`，有五个配置可选：
`GDB · 命令行版`、`GDB · 界面版 (Win32)`、`GDB · 界面版 (Qt)`、
`MSVC · 命令行版`、`MSVC · 界面版 (Win32)`。
调试路线的选择、`.vscode` 的用法与产物位置见 [`../../README.md`](../../README.md)。

## 运行后应当看到什么

`build\mingw\bin\app_cli.exe`（不带参数，工作目录为示例目录）：

`实测数据`
`Text`

```text
示例 07-standard-library/02-cpp-io-report · iostream 报表生成器（命令行版）

==================== 销售记录报表 ====================
输入文件        : data/sales.txt
读取行数        : 37
有效数据行      : 26
跳过行数        : 3
商品种类        : 8
总数量          : 107
总金额          : 28830.50
平均单价        : 269.444

[1] 按金额降序
排名  商品              订单数    数量         金额      占比
----  ----------------  ------  ------  -----------  --------
   1  ssd-1tb                3       9      9731.00    33.75%
   2  monitor                4       8      7232.00    25.08%
   3  keyboard               4      15      2945.00    10.21%
   4  headset                3      11      2729.00     9.47%
   5  webcam                 3       6      1894.00     6.57%
   6  laptop-stand           3      11      1709.00     5.93%
   7  mouse                  3      27      1624.00     5.63%
   8  usb-hub                3      20       966.50     3.35%
      合计                  26     107     28830.50   100.00%

[2] 按商品名升序
商品              订单数    数量         金额      占比
----------------  ------  ------  -----------  --------
headset                3      11      2729.00     9.47%
keyboard               4      15      2945.00    10.21%
laptop-stand           3      11      1709.00     5.93%
monitor                4       8      7232.00    25.08%
mouse                  3      27      1624.00     5.63%
ssd-1tb                3       9      9731.00    33.75%
usb-hub                3      20       966.50     3.35%
webcam                 3       6      1894.00     6.57%

[3] 单笔金额最高的前三笔
排名  日期        商品                数量         金额
----  ----------  ----------------  ------  -----------
   1  2026-01-15  ssd-1tb                4      4236.00
   2  2026-01-10  ssd-1tb                3      3297.00
   3  2026-01-18  monitor                3      2637.00

====================== 报表结束 ======================
```

这一段与 [`01-c-stdlib-toolbox`](../01-c-stdlib-toolbox/) 的对应段落逐字节相同。

报表之外，标准错误上还有两行（数值每次运行都不同）：

`实测数据`
`Text`

```text
[输出] 报表已写入 report-out.txt        ← 只有给了 --out 才有这一行
[计时] 读文件并生成报表 0.332 毫秒
```

`build\mingw\bin\app_cli.exe --selftest`：

`实测数据`
`Text`

```text
== 自测 ==
  [通过] 1. split_fields 把一行拆成 4 个字段
  [通过] 2. split_fields 把制表符与连续空白都当作分隔
  [通过] 3. split_fields 拆出 5 个字段，交给调用方判长度
  [通过] 4. parse_line 解析正常行，数量与单价都对
  [通过] 5. parse_line 同时算出金额 3 × 199.00 = 597.00
  [通过] 6. parse_line 拒绝只有 3 个字段的行
  [通过] 7. parse_line 拒绝数量不是数字的行
  [通过] 8. parse_line 拒绝单价不是数字的行
  [通过] 9. parse_line 拒绝数量为 0 或负数的行
  [通过] 10. parse_line 只认 YYYY-MM-DD 形式的日期
  [通过] 11. add_record 把 5 条记录归成 4 种商品，同名商品并成一行
  [通过] 12. add_record 累加订单数（keyboard 2 笔）与数量（7 件）
  [通过] 13. add_record 累加金额（keyboard 1393.00）
  [通过] 14. add_record 累加总量 28 与总额 5298.00
  [通过] 15. sort_products_by_amount 把金额最大的排到最前
  [通过] 16. 金额并列时按名称升序，输出顺序稳定
  [通过] 17. 按名称升序排出来是 keyboard、monitor、mouse、usb-hub
  [通过] 18. format_report 的头一行是报表标题
  [通过] 19. format_report 里有商品名、合计行与第三段标题
  [通过] 20. format_report 以「报表结束」收尾且末尾有换行

  自测结果：20 项中 20 项通过，全部通过
```

`data/sales.txt` 一共 37 行，其中 26 行是有效数据、3 行被跳过：
一行数量写成 `-`、一行单价写成 `abc`、一行只有 3 个字段；
另外 7 行注释与 1 行空行由读循环直接放过，不计入任何一边。

## GUI 怎么无人值守验证

两份界面与命令行版共用 `core`，报表框里的内容应当与上面那一份逐字相同。
Win32 版可以用仓库里的 [`工具/GUI冒烟/`](../../../工具/GUI冒烟/) 驱动：
启动程序、给输入框写路径、点「生成报表」、把只读框的文本读回来。

`PowerShell`

```powershell
cd <工作区>\B-examples\07-standard-library\02-cpp-io-report
pwsh -File <工作区>\工具\GUI冒烟\GUI冒烟.ps1 `
  -Exe .\build\mingw\bin\app_gui_win32.exe `
  -WindowTitle "02-cpp-io-report" -InputId 1001 -Text "data/sales.txt" -ClickId 1002 -ReadId 1004
```

`实测数据`
`Text`

```text
[进程] pid=24084  程序=<工作区>\B-examples\07-standard-library\02-cpp-io-report\build\mingw\bin\app_gui_win32.exe
[窗口] 标题=[示例 07-standard-library/02-cpp-io-report · iostream 报表生成器]  类名=CppIoReportWnd  客户区=720x520
[控件] 共 7 个
        id=0     类名=Static   文本=[输入文件（相对本示例目录，或写绝对路径）：]
        id=1001  类名=Edit     文本=[data/sales.txt]
        id=1002  类名=Button   文本=[生成报表]
        id=1003  类名=Button   文本=[跑自测]
        id=0     类名=Static   文本=[报表（只读，与命令行版、与 01 的 C 版逐字节相同）：]
        id=1004  类名=Edit     文本=[]
        id=0     类名=Static   文本=[读取 37 行：有效 26 行，跳过 3 行，商品 8 种，耗时 0.587 毫秒]
[操作] 给控件 1001 写入「data/sales.txt」
[操作] 点击控件 1002（BM_CLICK）
[读回] 控件 1004 的文本：
        ==================== 销售记录报表 ====================
        输入文件        : data/sales.txt
        读取行数        : 37
        有效数据行      : 26
        跳过行数        : 3
        商品种类        : 8
        总数量          : 107
        总金额          : 28830.50
        平均单价        : 269.444
        ...
        ====================== 报表结束 ======================
[收尾] 窗口还在吗 = False；进程已退出 = True
```

Qt 版的控件不是 Win32 控件，脚本读不到它们的文本，只能确认窗口存在并截图比对；
本机没有安装 Qt，这一份没有在本机构建验证。

## 代码里哪几处是要点

| 位置 | 要点 |
|---|---|
| `src/sales_report.cpp` 的 `split_fields` | `std::istringstream` 加 `operator>>` 就是现成的分词器；它替你跳过任意空白，但不告诉你字段总数 |
| `src/sales_report.cpp` 的 `to_long` / `to_double` | 捕获 `std::exception` 只解决「不是数字」；`"12x"` 要靠 `used != text.size()` 才拦得住 |
| `src/sales_report.cpp` 的 `add_record` | 用 `for (Product &item : report.products)` 找同名商品，找不到才 `push_back`；注意 `push_back` 之后要重新取 `back()` 的地址 |
| `src/sales_report.cpp` 的三个排序 lambda | `std::sort` 的比较函数返回 `bool`；金额相等时补一条按名称升序，输出才稳定 |
| `src/sales_report.cpp` 的 `load` | 循环结束后查 `source.bad()` 而不是 `eof()`；空行与 `#` 注释行先筛掉再解析 |
| `src/sales_report.cpp` 的 `format_report` | `label` 这个 lambda 把带 `setw(16)` 的标签统一起来；`left` / `right` 是粘住的状态，每一行都重新写明 |
| `src/sales_report.cpp` 的 `now_ms` | `steady_clock` 只往前走，测耗时用它；`system_clock` 会被对时改动 |
| `src/main_cli.cpp` 的 `main` | `--out` 用 `std::ofstream(output, std::ios::binary)` 写，换行原样保留成 LF |
| `src/main_gui_win32.cpp` 的 `to_wide` | 核心给的是 GBK 窄字符串，按 `CP_ACP` 转宽字符；按 UTF-8 转会让中文变乱码 |
| `src/main_gui_win32.cpp` 的 `set_report` | `EDIT` 控件认 `\r\n`；报表本身是 `\n`，显示之前补一次 `\r` |
| `src/main_gui_qt.cpp` 的 `to_qstring` / `to_crlf` | 先按字节补 `\r`，再整体用 `fromLocal8Bit` 转 `QString`，中文才不会被当成 Latin-1 |

## 已知问题

- **Qt 版未在本机构建验证。** 本机没有安装 Qt 6 Widgets（`QT_ROOT` 为空，
  `第三方/qt-static` 不存在），因此只实测了 `WITH_QT=OFF` 的完整构建，
  以及 Qt 目标的四项工程约定（默认关、不传字符集选项、用 `qt_enable_deploy()`、
  不装 Qt 也能配置）。Qt 版源码照 Win32 版一一对应写成，标记为待确认。
- **报表列宽依赖执行字符集。** GBK 下一个汉字 2 个字节、也是 2 个字符，
  `std::setw(16)` 才既按字符补齐、又与显示列数一致。
  把 `-fexec-charset=GBK` 去掉，中文列会错位。
- **商品名只能是 ASCII。** 数据文件里的商品名会原样进入报表并按字节比较、排序，
  写成 UTF-8 的中文名会与列宽假设冲突。示例数据因此全部用英文名。
- **明细最多留 256 条。** 这是为了与 C 版行为一致而刻意保留的上限；
  纯 C++ 的写法本可以让 `std::vector` 自己长。
- **`msvc` 预设未实测。** 本机只用 `mingw-gdb` 预设跑过配置、编译与自测；
  MSVC 那一套沿用了全板块统一的写法，没有在本机验证。
