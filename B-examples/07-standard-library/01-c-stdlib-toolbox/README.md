# 示例 `07-standard-library/01-c-stdlib-toolbox` · C 标准库报表工具（命令行 + Win32 GUI）

一份用纯 C 写的销售记录统计工具。它读一份文本，逐行拆字段、把坏行挑出来、
按商品聚合、排序，最后拼出一张固定版式的报表；命令行版与 Win32 界面版共用同一个 `core`，
两边输出的报表逐字节相同。

项目的重点不是「做一个报表工具」，而是把 C 标准库里最常用的那几件串起来用一遍：
`<stdio.h>` 逐行读文件、`<string.h>` 分词与复制、`<stdlib.h>` 的 `strtol` 与 `qsort`、
`<stdarg.h>` 拼格式化文本、`<time.h>` 与平台单调钟测耗时。核心库另带 20 项自测。

本示例与 [`02-cpp-io-report`](../02-cpp-io-report/) 是**一对**：读同一份输入，出同一份报表。
两边的差别只在写法，对照点见下面「与 02 的对照」一节。

`Text`

```text
01-c-stdlib-toolbox/
  include/sales_report.h     接口：记录、聚合结果、解析、排序、读文件、拼报表、自测
  src/sales_report.c         实现，核心逻辑全在这里，界面与命令行只调它
  src/main_cli.c             命令行版：认参数、调核心、把报表写到标准输出
  src/main_gui_win32.c       Win32 界面版（C 写）：窗口、控件与字符串转换
  data/sales.txt             示例数据：中文注释行 + 纯 ASCII 数据行，UTF-8 无 BOM、LF
  CMakeLists.txt             目标：core（静态库）、app_cli、app_gui_win32
  CMakePresets.json          mingw-gdb（Ninja + gcc）与 msvc（Visual Studio 17 2022）两套预设
  .vscode/                   四个调试配置与六个构建任务
```

`core` 是纯逻辑的静态库，删掉两个 `main` 文件它照样能编译、能自测通过。
界面里一行业务逻辑都没有：它只负责把 `core` 给出的报表文本摆到只读框里。

## 先读教材

| 章节 | 讲的是什么 | 本示例用在哪里 |
|---|---|---|
| 《07-标准库/A-00-导读：C 标准库.md》第 2.3、2.4 小节 | 这套划分今天的问题、按用途重新分组 | 为什么这个项目一次用到五个头文件 |
| 《07-标准库/A-01-输入输出：stdio.md》第 4.3 小节 | `fgetc` 与 `fgets`：逐个与逐行 | `sr_load` 里 `fgets` 的读循环 |
| 《07-标准库/A-01-输入输出：stdio.md》第 5.2 小节 | `feof` 与 `ferror`：把「结束」和「出错」分开 | 读完之后的 `ferror` 判断，而不是拿 `feof` 当循环条件 |
| 《07-标准库/A-01-输入输出：stdio.md》第 1.4 小节 | `stderr` 在两个平台上不一样 | 计时为什么写标准错误，不混进报表 |
| 《07-标准库/A-02-字符串与内存：string.h.md》第 2.3 小节 | `strncpy` 的经典坑与 `snprintf` 的正确用法 | `sr_copy_string` 与 `sr_appendf` |
| 《07-标准库/A-02-字符串与内存：string.h.md》第 3.2 小节 | `strspn`、`strcspn`、`strpbrk` | `sr_split_fields` 全靠这一对函数 |
| 《07-标准库/A-02-字符串与内存：string.h.md》第 3.3、3.4 小节 | `strtok` 会改写原串，而且不可重入；不改原串的切分写法 | 为什么这里不写 `strtok` |
| 《07-标准库/A-02-字符串与内存：string.h.md》第 5.2 小节 | `strtol` 的三件套 | 数量与单价的解析：看返回值、看串尾、看 `errno` |
| 《07-标准库/A-04-时间与日期：time.h.md》第 5.2 小节 | 本机的 MinGW 没有 `timespec_get` | 计时为什么不能写 `timespec_get` |
| 《07-标准库/A-04-时间与日期：time.h.md》第 6.3 小节 | 该用什么 | Windows 上的单调钟是 `QueryPerformanceCounter` |
| 《07-标准库/A-05-工具与其它：stdlib 与杂项.md》第 2.1、2.2 小节 | `qsort` 与 `bsearch` 都靠回调；排序与查找实测 | 三个比较函数，以及「并列时怎么定序」 |
| 《07-标准库/A-05-工具与其它：stdlib 与杂项.md》第 11 节 | `<stdarg.h>` | `sr_appendf` 与 `sr_check` 的可变参数写法 |

## 这个项目要解决什么问题

报表类程序在 C 里有一套固定的骨架：读文件、拆字段、算、排序、按列宽输出。
骨架不长，但每一步都有一个容易写错的地方。

**读文件**。`fgets` 一次给一行，读到结尾返回 `NULL`。问题在于 `NULL` 同时意味着
「读完了」与「读错了」，只看返回值分不出来。标准给的答案是在循环结束后查 `ferror`，
`feof` 只回答「是不是撞到了文件尾」。项目按这条来写。

**拆字段**。教科书里常见的写法是 `strtok`，但它有两个硬伤：它会往原串里写 `'\0'`，
而且内部用一个静态变量记住上次的位置，两个线程同时用就乱了。
项目改用 `strspn` 跳过一段空白、`strcspn` 走过一段非空白，
自己在副本上切：不碰调用方的原串，也没有隐藏状态。

**转数字**。`atoi` 在出错时返回 0，调用方无从分辨「真的是 0」还是「根本不是数字」。
项目用 `strtol` 的三件套：看 `end` 有没有停在串尾、看 `errno` 有没有被改、
再看值本身在不在合理范围。单价用 `strtod` 同理。

**排序**。`qsort` 的比较函数返回 `int`，只拿符号。金额是 `double`，
直接写 `return a->amount - b->amount` 会先截断成 `int`，
金额差不到 1 元时返回 0，排序结果随机。项目里三个比较函数都返回 -1 / 0 / 1，
并且在金额并列时补一条「按名称升序」，让同样的输入永远得到同样的输出。

**拼文本**。报表每一列的宽度是固定的，`snprintf` 的宽度说明符正好做这件事。
项目把 `vsnprintf` 包成一个 `sr_appendf`，带上 GCC 的 `format(printf, ...)` 属性，
参数写错在编译期就报出来。

**测耗时**。C 标准库在 Windows 上没有可用的单调钟：`clock()` 在本机的 MinGW 上
返回的是墙上时间（见 A-04 第 6 节），`timespec_get` 是 UCRT 才有的函数，
本机走 msvcrt，头文件里根本没有它的声明（见 A-04 第 5 节）。
项目落到 Windows 自己的 `QueryPerformanceCounter`，这正是 A-04 第 6.3 小节给出的做法。

## 做完能掌握什么

- 会用 `fgets` + `ferror` 写出正确的读文件循环，而不是拿 `feof` 当条件
- 会用 `strspn` / `strcspn` 自己写分词，并说清 `strtok` 为什么不合适
- 会用 `strtol` / `strtod` 的三件套做「能报错的解析」，而不是 `atoi`
- 会写 `qsort` 的比较函数，并知道返回值为什么不能写 `a - b`
- 会用 `vsnprintf` 包一个带格式检查的字符串拼接函数
- 会区分「日历时间」与「单调时间」，并知道 Windows 上该用哪一个
- 会把核心逻辑与界面彻底分开：`core` 里不认识 `HWND`，界面里不认识 `sales.txt`

## 与 02 的对照

[`02-cpp-io-report`](../02-cpp-io-report/) 做的是同一件事：读同一份 `data/sales.txt`，
出一份逐字节相同的报表。两边都编译成 `core` 静态库 + `app_cli` + `app_gui_win32`，
目录结构也一样。同一个任务的两种写法如下。

| 环节 | 本示例（C） | [`02-cpp-io-report`](../02-cpp-io-report/)（C++） |
|---|---|---|
| 读文件 | `fopen` + `fgets` + `ferror` | `std::ifstream` + `std::getline` |
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
一个汉字在 GBK 里是 2 个字节、在控制台上也占 2 列，
于是 C 的 `%-16s` 与 C++ 的 `std::setw(16)` 补出来一样宽。

## 阶段推进

| 阶段 | 目标 | 做什么 | 怎么验收 |
|---|---|---|---|
| **阶段 1** | 跑通命令行版 | 配置、编译，运行 `build\mingw\bin\app_cli.exe` | 看到报表与「20 项中 20 项通过，全部通过」 |
| **阶段 2** | 看懂分词 | 读 `sr_split_fields`，对照 `data/sales.txt` 里被跳过的那 3 行 | 能说清每一行为什么被算作「跳过」 |
| **阶段 3** | 与 C++ 版对照 | 建好 `02-cpp-io-report`，把两份 `--out` 的结果比一次 | 两个文件字节数相同、内容相同 |
| **阶段 4** | 自己加一列 | 在报表第 1 段加一列「平均单价」，同步改 `SR_FMT_HEAD1` / `SR_FMT_ROW1` / `SR_RULE1` 与 02 的那一份 | 自测仍全通过，且两份 `--out` 依旧逐字节相同 |

阶段 4 是重点：格式串的列宽必须三处同时改，改漏一处表格就歪；
改完还要让 C 与 C++ 两边保持一致，这正是本项目想要练的手感。

## 构建与运行

`PowerShell`

```powershell
# 在 07-standard-library/01-c-stdlib-toolbox 目录下：配置 + 编译
cmake --preset mingw-gdb
cmake --build --preset mingw-gdb

# 运行：默认读 data/sales.txt（工作目录必须是示例目录）
build\mingw\bin\app_cli.exe

# 换一份输入
build\mingw\bin\app_cli.exe data\sales.txt

# 顺便把报表写进文件，用来与 02 的输出比对
build\mingw\bin\app_cli.exe data\sales.txt --out report-out.txt

# 只跑自测
build\mingw\bin\app_cli.exe --selftest

# 界面版
build\mingw\bin\app_gui_win32.exe
```

不想用预设时，等价的手写命令是：

`PowerShell`

```powershell
cmake -S . -B build/mingw -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=gcc
cmake --build build/mingw
```

用 VS Code 打开本文件夹后按 `F5`，有四个配置可选：
`GDB · 命令行版`、`GDB · 界面版`、`MSVC · 命令行版`、`MSVC · 界面版`。
调试路线的选择、`.vscode` 的用法与产物位置见 [`../../README.md`](../../README.md)。

## 运行后应当看到什么

`build\mingw\bin\app_cli.exe`（不带参数，工作目录为示例目录）：

`实测数据`
`Text`

```text
示例 07-standard-library/01-c-stdlib-toolbox · C 标准库报表工具（命令行版）

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

报表之外，标准错误上还有两行（数值每次运行都不同）：

`实测数据`
`Text`

```text
[输出] 报表已写入 report-out.txt        ← 只有给了 --out 才有这一行
[计时] 读文件并生成报表 0.309 毫秒
```

`build\mingw\bin\app_cli.exe --selftest`：

`实测数据`
`Text`

```text
== 自测 ==
  [通过] 1. split_fields 把一行拆成 4 个字段
  [通过] 2. split_fields 把制表符与连续空白都当作分隔
  [通过] 3. split_fields 见到第 5 个字段返回 -1
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
  [通过] 20. format_report 以「报表结束」收尾且没有把缓冲区写满

  自测结果：20 项中 20 项通过，全部通过
```

`data/sales.txt` 一共 37 行，其中 26 行是有效数据、3 行被跳过：
一行数量写成 `-`、一行单价写成 `abc`、一行只有 3 个字段；
另外 7 行注释与 1 行空行由读循环直接放过，不计入任何一边。

## GUI 怎么无人值守验证

界面版与命令行版共用 `core`，报表框里的内容应当与上面那一份逐字相同。
验证方式是用仓库里的 [`工具/GUI冒烟/`](../../../工具/GUI冒烟/) 驱动一次：
启动程序、给输入框写路径、点「生成报表」、把只读框的文本读回来。

`PowerShell`

```powershell
cd <工作区>\B-examples\07-standard-library\01-c-stdlib-toolbox
pwsh -File <工作区>\工具\GUI冒烟\GUI冒烟.ps1 `
  -Exe .\build\mingw\bin\app_gui_win32.exe `
  -WindowTitle "c-stdlib-toolbox" -InputId 1001 -Text "data/sales.txt" -ClickId 1002 -ReadId 1004
```

`实测数据`
`Text`

```text
[进程] pid=45864  程序=<工作区>\B-examples\07-standard-library\01-c-stdlib-toolbox\build\mingw\bin\app_gui_win32.exe
[窗口] 标题=[示例 07-standard-library/01-c-stdlib-toolbox · C 标准库报表工具]  类名=CStdlibToolboxWnd  客户区=720x520
[控件] 共 7 个
        id=0     类名=Static   文本=[输入文件（相对本示例目录，或写绝对路径）：]
        id=1001  类名=Edit     文本=[data/sales.txt]
        id=1002  类名=Button   文本=[生成报表]
        id=1003  类名=Button   文本=[跑自测]
        id=0     类名=Static   文本=[报表（只读，与命令行版逐字节相同）：]
        id=1004  类名=Edit     文本=[]
        id=0     类名=Static   文本=[读取 37 行：有效 26 行，跳过 3 行，商品 8 种，耗时 0.789 毫秒]
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

界面里没有一行业务逻辑，因此读回来的报表与命令行版一致：
界面把路径交给 `sr_load`，把 `sr_format_report` 拼出来的文本塞进只读框，如此而已。

## 代码里哪几处是要点

| 位置 | 要点 |
|---|---|
| `src/sales_report.c` 的 `sr_split_fields` | 用 `strspn` 跳过空白、`strcspn` 走过字段，就地写 `'\0'`；超过字段数返回 -1 而不是悄悄截断 |
| `src/sales_report.c` 的 `sr_parse_line` | 先 `memcpy` 一份副本再切，不碰调用方的原串；`strtol` / `strtod` 都查串尾与 `errno` |
| `src/sales_report.c` 的 `sr_add_record` | 线性查找同名商品；商品种类或明细条数到上限时返回 0，由调用方计入「跳过」 |
| `src/sales_report.c` 的三个比较函数 | 返回 -1 / 0 / 1，绝不写 `a->amount - b->amount`；并列时补一条按名称升序 |
| `src/sales_report.c` 的 `sr_appendf` | `vsnprintf` 的返回值要分三种情况看：负数、写不下、正常；带上 `format(printf, 4, 5)` 属性让编译器检查实参 |
| `src/sales_report.c` 的 `sr_load` | 空行与 `#` 注释行直接放过；循环结束后查 `ferror` 而不是拿 `feof` 当条件 |
| `src/sales_report.c` 的 `sr_format_report` | 第二段要按名称排，因此复制一份商品数组来排，不动调用方传进来的数据 |
| `src/sales_report.c` 的 `sr_now_ms` | `#if defined(_WIN32)` 走 `QueryPerformanceCounter`，其余平台退回 `timespec_get` |
| `src/main_cli.c` 的 `main` | `--out` 用 `fopen(..., "wb")` 写，换行原样保留成 LF，方便与 02 逐字节比对 |
| `src/main_gui_win32.c` 的 `expand_newlines` | `EDIT` 控件认 `\r\n`；报表本身是 `\n`，显示之前补一次 `\r` |
| `src/main_gui_win32.c` 的 `to_wide` | 核心给的是 GBK 窄字符串，按 `CP_ACP` 转宽字符；按 UTF-8 转会让中文变乱码 |

## 已知问题

- **计时用的是平台 API，不是标准库。** Windows 上没有可用的 C 标准库单调钟，
  因此 `sr_now_ms` 在 Windows 上调用 `QueryPerformanceCounter`。
  非 Windows 分支走 `timespec_get`，本机未验证。
- **报表列宽依赖执行字符集。** GBK 下一个汉字 2 字节，`%-16s` 才既按字节补齐、又与显示列数一致。
  把 `-fexec-charset=GBK` 去掉，中文列会错位。
- **商品名只能是 ASCII。** 数据文件里的商品名会原样进入报表并按字节比较、排序，
  写成 UTF-8 的中文名会与列宽假设冲突。示例数据因此全部用英文名。
- **明细最多留 256 条。** 超过上限的记录仍会参与聚合，但不再进入「单笔金额最高的前三笔」的候选。
  真实的报表工具应当用动态数组，`02-cpp-io-report` 就是用 `std::vector` 写的。
- **`msvc` 预设未实测。** 本机只用 `mingw-gdb` 预设跑过配置、编译与自测；
  MSVC 那一套沿用了全板块统一的写法，没有在本机验证。
