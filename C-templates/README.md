# 练习模板（`C-templates`）

**本目录只包含源码，不包含 `.vscode` 目录**，用于练习「从零开始配置调试器」，
以及按阶段完成一个完整的小工程。

每个模板中均提供一份 `配置步骤.md`，即一份操作核对清单，按清单逐步操作一遍即可完成练习。

---

## 怎么用这些模板

公共说明集中在本节，各个模板的 `配置步骤.md` 里不再重复。

### 前置条件

| 项目 | 要求 | 说明 |
|---|---|---|
| **PowerShell 7** | 命令 `pwsh` 可用 | 本目录的练习全部以 PowerShell 7 为准。它用于执行构建任务脚本（`.ps1`）。安装方法见《01-编译器/02-环境配置.md》第 3 章步骤 1 |
| 编译器 | `gcc` / `g++` / `gdb` 可用 | 或在 `01-compiler/02-cpp-single-file` 等目录中验证 MSVC 路线 |
| CMake 与 Ninja | `cmake` / `ninja` 可用 | 仅 `03-build-toolchain/`、`05-oop/`、`06-standard-library/` 与 `07-lower-level/` 下的 CMake 工程需要 |

> **说明**：Windows 自带的 `cmd.exe` 与 Windows PowerShell 5.1 在中文路径下均存在
> 实际限制（代码页解析、参数转义），不建议用于本目录的练习。原因与实测记录见
> 《01-编译器/02-环境配置.md》第 3 章步骤 1 与第 9 章第 4 节。

### 三步走

1. **先将模板复制到其他位置**（请勿在原目录中练习，否则模板将被修改而无法重复使用）

   `PowerShell`

   ```powershell
   Copy-Item -Recurse "C-templates\01-compiler\01-c-single-file" "我的练习\01"
   ```

2. 使用 VS Code **打开该副本文件夹**（菜单「文件」中的「打开文件夹」命令）
3. 按照其中的 `配置步骤.md` 逐步操作

> **说明**：建议先自行尝试，再对照参考答案。`B-examples` 中的配置均带有逐字段注释，
> 但直接复制会跳过「为什么这样写」这一环节。

### 两类模板的构建方式

| 形态 | 模板 | 怎么构建 |
|---|---|---|
| 单文件 | `01-compiler/01-c-single-file`、`01-compiler/02-cpp-single-file`、`04-syntax/01-cpp-const-init` | 直接调用编译器，例如 `g++ -std=c++17 -g -O0 -Wall -Wextra main.cpp -o app.exe` |
| CMake 工程 | `03-build-toolchain/` 下的两个、`05-oop/` 下的四个、`06-standard-library/` 下的九个、`07-lower-level/` 下的四个，共十九个模板 | `cmake --preset mingw-gdb` 然后 `cmake --build --preset mingw-gdb`；`05-oop/01-cpp-class`、`05-oop/02-cpp-inheritance`、`05-oop/04-cpp-raii-exceptions`、`06-standard-library/07-cpp-filesystem` 的 Qt 界面版另加 `-DWITH_QT=ON` |

`03-build-toolchain/` 下两个模板的预设文件**需要自己编写**（这是那两道练习的一部分）；
`05-oop/`、`06-standard-library/` 与 `07-lower-level/` 下十七个模板的 `CMakePresets.json` 已经给出，可以直接用。

### 怎么自查

每个 `配置步骤.md` 都按阶段或按步骤列出**验收标准**与**自查表**：

- 应当输出什么，逐字给出；
- 若输出不同，说明漏了哪一步，附常见错误与对应的原因；
- 标记 `实测数据` 的段落是本机真实跑出来的结果，可直接对照。

### 对照答案

| 类型 | 参考答案位置 |
|---|---|
| 调试配置（`.vscode`） | `B-examples\` 下对应示例 |
| `CMakePresets.json` | `B-examples\03-build-toolchain\01-cmake-c\` 与 `B-examples\03-build-toolchain\02-cmake-cpp\` |
| `04-syntax/` 与 `05-oop/` 下各模板的核心逻辑 | 本目录不给答案，验收标准就是答案的判据 |

### 中文输出与控制台代码页

> [!TIP]
> 程序里写中文输出时，先看当前控制台的代码页：`chcp` 显示 `65001` 表示 UTF-8，
> 此时 gcc 与 g++ 的默认执行字符集正好匹配，**不需要**额外参数；
> 若显示 `936`，才需要 `-fexec-charset=GBK`（MSVC 对应 `/execution-charset:gbk`）。

`实测数据`
`Text`

```text
本机控制台代码页 65001，同一份打印中文的源码：
  默认编译                 输出正常
  -fexec-charset=GBK       输出为乱码
```

原理与两种控制台下的完整对照见《01-编译器/02-环境配置.md》第 9.6 小节。
本目录 `04-syntax/` 与 `05-oop/` 下各模板的程序输出刻意写成 ASCII，正是为了避开这一层干扰；
中文只出现在注释与界面文字里。

---

## 模板索引

难度按「入门 → 进阶 → 综合」递增；阶段数指 `配置步骤.md` 里划分的练习阶段。

| 模板 | 形态 | 服务板块 / 对应章节 / 练习难度 | 练什么 | 起点与需要自行完成的操作 |
|---|---|---|---|---|
| [`01-compiler/01-c-single-file`](01-compiler/01-c-single-file/) | 单文件（C） | `01-编译器`、`02-调试器` · 《01-编译器/02-环境配置.md》第 3、5、7 章 · 入门 | 从零写出 `tasks.json` 与 `launch.json`，让 F5 能调试一个 C 文件 | 一个 `main.c`；自行创建 `.vscode` 下的两个配置文件 |
| [`01-compiler/02-cpp-single-file`](01-compiler/02-cpp-single-file/) | 单文件（C++） | `01-编译器`、`02-调试器` · 《01-编译器/02-环境配置.md》第 3、5、7 章 · 入门 | 同上，并把编译器从 `gcc` 换成 `g++`，看清「不链接 libstdc++」的报错 | 一个 `main.cpp`；自行创建 `.vscode` 下的两个配置文件 |
| [`03-build-toolchain/01-cmake-c`](03-build-toolchain/01-cmake-c/) | CMake 多文件（C） | `03-构建工具链` · 《03-构建工具链/01-构建工具链.md》 · 入门 | 为 CMake 工程写预设，并把「配置 + 编译」串成一个任务供 F5 调用 | `CMakeLists.txt` + `include/` + `src/`；自行编写 `CMakePresets.json` 与 `.vscode` |
| [`03-build-toolchain/02-cmake-cpp`](03-build-toolchain/02-cmake-cpp/) | CMake 多文件（C++） | `03-构建工具链` · 《03-构建工具链/01-构建工具链.md》 · 入门 | 同上（C++ 版），并处理 MSVC 与 MinGW 两条路线的产物路径差异 | 同上；自行编写 `CMakePresets.json` 与 `.vscode` |
| [`04-syntax/01-cpp-const-init`](04-syntax/01-cpp-const-init/) | 单文件（C++） | `04-语法` · 第 03 章《常量与 const》、第 05 章《初始化》、第 12 章《编译期能力》 · 入门 | `const` 用在变量、指针、引用、成员函数上；初始化列表与类内初始化；`constexpr` 与 `static_assert` | 一个 `main.cpp` 骨架（4 个阶段）；按 TODO 实现单位换算小工具 |
| [`05-oop/01-cpp-class`](05-oop/01-cpp-class/) | CMake 多文件（核心 + 命令行 + 两份界面） | `05-类与面向对象` · 第 02 章《类是一种类型》、第 03 章《构造与析构》、第 04 章《拷贝与移动》 · 进阶 | 写一个自己的字符串类：构造与析构、深拷贝、自赋值、移动与 `noexcept`、`static` 计数、`const` 成员函数 | `include/mystring.hpp` + `src/mystring.cpp` 骨架（4 个阶段）、`src/main_cli.cpp`、`src/main_gui_win32.cpp`、`src/main_gui_qt.cpp`；按 TODO 实现核心逻辑与界面连接 |
| [`05-oop/02-cpp-inheritance`](05-oop/02-cpp-inheritance/) | CMake 多文件（核心 + 命令行 + 两份界面） | `05-类与面向对象` · 第 06 章《继承》、第 07 章《多态：重载、虚函数与它们的分工》 · 进阶 | 抽象基类 + 虚函数 + `override` + 虚析构；工厂与多态遍历；`dynamic_cast` 转到指针与引用 | `include/shape.hpp` + `src/shape.cpp` 骨架（4 个阶段）、命令行版与两份界面；按 TODO 实现 |
| [`05-oop/03-cpp-templates`](05-oop/03-cpp-templates/) | CMake 多文件（纯命令行） | `05-类与面向对象` · 第 10 章《模板》 · 进阶 | 函数模板与非类型模板参数、一次全特化、类模板与成员模板、显式实例化、模板为什么写在头文件里 | `include/algo.hpp`、`include/stack.hpp`、`src/instantiations.cpp` 骨架（4 个阶段）；按 TODO 实现 |
| [`05-oop/04-cpp-raii-exceptions`](05-oop/04-cpp-raii-exceptions/) | CMake 多文件（核心 + 命令行 + 两份界面） | `05-类与面向对象` · 第 05 章《RAII 与资源管理》；`04-语法` · 第 13 章《异常》 · 综合 | 自己写 RAII 包装、拷贝禁用、移动转移所有权、异常安全（构造失败不留半成品、失败不留半成品文件、析构不抛） | `include/file_guard.hpp` + `src/file_guard.cpp` 骨架（4 个阶段）、命令行版与两份界面；按 TODO 实现 |
| [`06-standard-library/01-c-stdlib-toolbox`](06-standard-library/01-c-stdlib-toolbox/) | CMake 多文件（核心 + 命令行 + Win32 界面） | `06-标准库` · 《06-标准库/A-01-输入输出：stdio.md》第 1、2、4 节、《06-标准库/A-02-字符串与内存：string.h.md》第 1 节、《06-标准库/A-04-时间与日期：time.h.md》第 1、6 节、《06-标准库/A-05-工具与其它：stdlib 与杂项.md》第 1、2、6、7 节 · 综合 | C 段综合：`fopen`/`fread` 整份读入、`isalpha` 分词与词频、`qsort` 排序、`clock` 与 `time` 两种计时、`snprintf` 拼报表 | `include/textstats.h` + `src/textstats.c` 骨架（5 个阶段）、`src/main_cli.c`、`src/main_gui_win32.c`、`data/sample.txt`；按 TODO 实现核心逻辑与界面连接 |
| [`06-standard-library/02-cpp-io-format`](06-standard-library/02-cpp-io-format/) | CMake 多文件（核心 + 命令行） | `06-标准库` · 《06-标准库/B-01-输入输出：iostream.md》第 1、2、3、4、5、6、7、8 节、《06-标准库/B-00-导读：C++ 标准库与 C 的关系.md》第 2、4 节 · 进阶 | 用 `ifstream`/`istringstream` 读同一份样本、`iomanip` 出与 01 逐字节相同的报表、`ofstream` 写文件、与 `printf` 对照 | `include/report.hpp` + `src/report.cpp` 骨架（4 个阶段）、`src/main_cli.cpp`、`data/sample.txt`；按 TODO 实现 |
| [`06-standard-library/03-cpp-string-text`](06-standard-library/03-cpp-string-text/) | CMake 多文件（核心 + 命令行） | `06-标准库` · 《06-标准库/B-02-std-string 与 string_view.md》第 1、2、3、4、5、6、7、8 节、《06-标准库/B-05-数值.md》第 3 节 · 进阶 | `std::string` 的容量与短串优化、`find`/`replace`/`split`/`trim`、`string_view` 零拷贝切分、数字互转与 UTF-8 字节数 | `include/texttool.hpp` + `src/texttool.cpp` 骨架（4 个阶段）、`src/main_cli.cpp`；按 TODO 实现 |
| [`06-standard-library/04-cpp-smart-pointers`](06-standard-library/04-cpp-smart-pointers/) | CMake 多文件（核心 + 命令行） | `06-标准库` · 《06-标准库/B-03-智能指针的用法.md》第 1、2、3、4、5 节、《06-标准库/B-04-可调用物的包装.md》第 1、2、3、4 节 · 进阶 | `unique_ptr` 与自定义删除器、`shared_ptr` 的引用计数、循环引用与 `weak_ptr` 断环、`std::function` 回调注册表与 `reference_wrapper` | `include/resource.hpp` + `src/resource.cpp` 骨架（4 个阶段）、`src/main_cli.cpp`；按 TODO 实现 |
| [`06-standard-library/05-cpp-numeric-random`](06-standard-library/05-cpp-numeric-random/) | CMake 多文件（核心 + 命令行 + Win32 界面） | `06-标准库` · 《06-标准库/B-05-数值.md》第 1、2、3、4、5 节、《06-标准库/A-03-数值、数学与随机.md》第 3、4 节 · 进阶 | `<random>` 的引擎与分布分开、固定种子复现、`rand() % n` 的偏差对照、`<numeric>` 的统计量、文本直方图 | `include/stats.hpp` + `src/stats.cpp` 骨架（3 个阶段）、`src/main_cli.cpp`、`src/main_gui_win32.cpp`；按 TODO 实现核心逻辑与界面连接 |
| [`06-standard-library/06-cpp-chrono-benchmark`](06-standard-library/06-cpp-chrono-benchmark/) | CMake 多文件（核心 + 命令行） | `06-标准库` · 《06-标准库/B-06-时间：chrono.md》第 1、2、3、4、5 节、《06-标准库/B-10-内存与并发的基础设施.md》第 5 节、《06-标准库/A-04-时间与日期：time.h.md》第 6 节 · 进阶 | `duration` 的单位换算、`steady_clock` 多次测量取分位数、`system_clock` 与本地时间、`atomic` 计数与 `sleep_for` 实测 | `include/bench.hpp` + `src/bench.cpp` 骨架（4 个阶段）、`src/main_cli.cpp`；按 TODO 实现 |
| [`06-standard-library/07-cpp-filesystem`](06-standard-library/07-cpp-filesystem/) | CMake 多文件（核心 + 命令行 + Win32 界面 + Qt 界面，`WITH_QT` 默认关） | `06-标准库` · 《06-标准库/B-07-文件系统：filesystem.md》第 1、2、3、4、5、6 节、《06-标准库/A-01-输入输出：stdio.md》第 4 节 · 进阶 | `path` 的拆分与拼装、目录遍历与属性统计、`error_code` 与异常两条错误路径、创建与复制改名删除 | `include/dirscan.hpp` + `src/dirscan.cpp` 骨架（4 个阶段）、`src/main_cli.cpp`、两份界面、`data/tree/` 样本树；按 TODO 实现 |
| [`06-standard-library/08-cpp-config-parser`](06-standard-library/08-cpp-config-parser/) | CMake 多文件（核心 + 命令行） | `06-标准库` · 《06-标准库/B-08-工具类（上）：pair、tuple、optional、variant、any.md》第 1、2、3、4 节、《06-标准库/B-09-工具类（下）：type_traits 与 concepts.md》第 1、3、4 节、《04-语法/12-编译期能力.md》第 5 节 · 进阶 | `optional` 表达解析失败、`variant` 与 `visit` 表示多选一、`tuple` 与结构化绑定、`<type_traits>` 做约束 | `include/config.hpp` + `src/config.cpp` 骨架（4 个阶段）、`src/main_cli.cpp`、`data/app.ini`；按 TODO 实现 |
| [`06-standard-library/09-stdlib-capstone`](06-standard-library/09-stdlib-capstone/) | CMake 多文件（核心 + 命令行） | `06-标准库` · 《06-标准库/B-01-输入输出：iostream.md》第 3、7 节、《06-标准库/B-02-std-string 与 string_view.md》第 6 节、《06-标准库/B-06-时间：chrono.md》第 3 节、《06-标准库/B-07-文件系统：filesystem.md》第 5 节、《06-标准库/B-11-收尾：把标准库用对.md》第 5 节 · 综合 | 毕业练习：读日志到解析到统计与固定种子抽样、计时与原子计数、报表，外加 31 项内置自测 | `include/pipeline.hpp` + `src/pipeline.cpp` 骨架（5 个阶段）、`src/main_cli.cpp`、`data/app.log`；按 TODO 实现并让 `--selftest` 全过 |
| [`07-lower-level/01-layout-probe`](07-lower-level/01-layout-probe/) | CMake 多文件（C，命令行） | `07-更底层` · 《07-更底层/01-对象在哪里：栈、堆与静态区.md》第 1、3、5 节、《07-更底层/02-对齐、填充与缓存.md》第 2 节 · 进阶 | 六类对象落在哪一段、段间距与两次运行的差别、`sizeof`/`_Alignof`/`offsetof` 三份布局、对齐自检 | `include/layout_probe.h` + `include/shapes.h` + `src/layout_probe.c`、`src/shapes.c` 骨架（4 个阶段）、`src/main_cli.c`、`src/probe_data.c`；按 TODO 实现 |
| [`07-lower-level/02-mmio-lab`](07-lower-level/02-mmio-lab/) | CMake 多文件（C，命令行 + QEMU 交叉实验） | `07-更底层` · 《07-更底层/03-寄存器、位与 volatile.md》第 1、2、3 节 · 进阶 | 内存映射 I/O 的位操作、`BSRR`/`BRR` 一次写完成、`volatile` 有与无的汇编和运行对照、字段读改写 | `include/regs.h` + `include/gpio_lab.h` + `src/gpio_lab.c` 骨架（4 个阶段，阶段 3 在 `arm/` 下跑 QEMU）、`src/regs.c`、`src/main_cli.c`；按 TODO 实现 |
| [`07-lower-level/03-linker-script-lab`](07-lower-level/03-linker-script-lab/) | 交叉编译（arm-none-eabi-gcc + QEMU，无 CMake） | `07-更底层` · 《07-更底层/07-链接脚本与启动代码.md》第 2、3、5 节 · 综合 | 从一个「能链接但跑不起来」的链接脚本里找出并修好两处缺陷：向量表被 `--gc-sections` 回收、`.data` 少了 `AT> FLASH` | `STM32F103C8_FLASH.ld`（有缺陷）+ `startup_stm32f103xe.s` + `app/main.c` + `build.sh`；自行定位缺陷并改脚本 |
| [`07-lower-level/04-static-lib`](07-lower-level/04-static-lib/) | CMake 多文件（C，静态库 + 两个驱动） | `07-更底层` · 《07-更底层/06-符号与链接属性.md》第 2、3、4、6 节 · 进阶 | 内部链接与重复定义、静态库的成员粒度与链接顺序、弱的默认实现与强定义覆盖（含覆盖失败的静默后果） | `include/sensor.h` + `core/sensor.c`、`core/sensor_extra.c`、`core/sensor_default.c`、`drivers/driver_fast.c` 骨架（4 个阶段）、`src/main_cli.c`；按 TODO 实现 |
| [`07-lower-level/05-critical-section`](07-lower-level/05-critical-section/) | CMake 多文件（C++，命令行） | `07-更底层` · 《07-更底层/10-中断、并发与内存序.md》第 2、3、4 节 · 综合 | 丢更新复现、三种修法（关中断 / 可嵌套临界区 / 原子读-改-写）与四种写法的代价对照 | `include/sim_irq.hpp` + `include/counter.hpp` + `src/counter.cpp` 骨架（4 个阶段）、`src/sim_irq.cpp`、`src/main_cli.cpp`；按 TODO 实现 |

模板路径分两级：**第一级是板块目录**（`01-compiler`、`03-build-toolchain`、
`04-syntax`、`05-oop`、`06-standard-library`），第二级是模板目录。
**板块目录一律用 ASCII 名**，因为模板里的 CMake 工程会被构建工具读取，
而 CMake 的 `file(STRINGS)` 与 Qt 的 `syncqt` 在非 ASCII 路径下会失败。



> [!NOTE]
> `05-oop/01-cpp-class`、`05-oop/02-cpp-inheritance`、`05-oop/04-cpp-raii-exceptions`
> 与 `06-standard-library/07-cpp-filesystem` 各带**两份界面**：Win32 版（`app_gui_win32.exe`，用系统自带的
> `windows.h` 与 GDI，**不需要安装任何第三方界面库**）与 Qt 版（`app_gui_qt.exe`，Qt Widgets）。
> Qt 版**默认不构建**，需要时按各模板《配置步骤.md》的「两份界面」一节打开
> `-DWITH_QT=ON` 并指向自己的 Qt 套件目录。构建完成后会自动把 Qt 运行时
> （含 `platforms\qwindows.dll`）与 MinGW 运行时拷到 exe 旁边，双击即可运行；
> 判据是「连同这些 DLL 拷到没有 Qt、`PATH` 里也没有 Qt 与 MinGW 的目录，窗口能起来」。
> 部署产物全部落在 `build\` 里，已被 `.gitignore` 覆盖，**不要提交 DLL**；
> 目标机器仍需要对应的编译器运行库。
> `04-syntax/01-cpp-const-init` 与 `05-oop/03-cpp-templates` 只有命令行版。
> `06-standard-library/01-c-stdlib-toolbox` 与 `06-standard-library/05-cpp-numeric-random`
> 只有一份 Win32 界面（`app_gui_win32.exe`），`06-standard-library/` 下其余六个模板只有命令行版。
>
> 两份界面里的 TODO 是同一批、编号也一致，做一份即可，另一份留作对照。
> 骨架里的窗口类注册、消息循环、`WM_CREATE` / `WM_COMMAND` / `WM_PAINT` / `WM_DESTROY`
> 都已写好（Qt 版对应构造函数、信号槽与 `paintEvent`），留给你的是「界面与核心逻辑的连接处」。

---

## 关于 GDB 路线的重要说明

> [!WARNING]
> 工作区路径中含中文时，GDB（`cppdbg`）无法打开可执行文件，调试无法启动。
> 原因是 GDB 在 Windows 上经 MI 协议接收文件名时期望 ANSI 代码页字节，
> 而 VS Code 发送的是 UTF-8 字节。
> **路径全为英文时不受影响。**

在练习编写 `launch.json` 时请注意：

| 你编写的配置 | 在含中文的路径下，能否实际验证通过 |
|---|---|
| `cppdbg`（GDB 路线） | **不能**。按 F5 会提示 `Program path ... is missing or invalid`，或弹出空的调试会话列表后无反应 |
| `cppvsdbg`（MSVC 路线） | 可以 |

**这不代表你的配置写错了**，而是 GDB 在中文路径下的固有限制。
完整的原理与实测数据见《01-编译器/02-环境配置.md》第 1 章第 2 节。

**若**路径含中文，在本目录练习时建议：

1. 仍按 `配置步骤.md` 完整写出两条路线（这是学习目的）；
2. 用 `cppvsdbg` 路线实际验证 F5 是否可用；
3. `cppdbg` 路线可用命令行的 `gdb --batch` 方式验证工具链本身是否正常。

---

## 问题查阅指引

| 问题 | 查看位置 |
|---|---|
| 不清楚某个字段的含义 | 《01-编译器/02-环境配置.md》第 5 章（字段详解） |
| 需要现成的配置模板 | 同上，第 7 章（模板库，共 13 套） |
| 报错后不知如何修复 | 同上，第 9 章「故障排查」 |
| 不清楚需要编写哪些字段 | 每个示例的 `launch.json` 均带有中文注释 |
| 需要验证工具链本身是否可用 | 《01-编译器/02-环境配置.md》第 3 章步骤 10 的命令行验证 |
| CMake 报 `Compatibility with CMake < 3.5 has been removed` | 同上，第 9.2 小节（本目录的工程均不低于 3.16） |

---

## 预备知识

如果此前完全没有配置经验，建议先阅读《01-编译器/02-环境配置.md》
第 1 章（基本概念）与第 3 章（从零搭建），然后再进行本目录的练习。

若概念不清，配置结果往往表现为「能够运行但不知其原因」，更换场景后即无法复用。

`04-syntax/` 与 `05-oop/` 下的模板属于语言练习，与调试配置无关，可以单独使用；
但其中的 CMake 工程仍然需要 `cmake` 与 `ninja` 可用。

---

## 附录：历史序号对照

重排前模板按 `01` 至 `09` 的扁平编号排列，重排后该编号已不存在。
**源文件头的注释与程序的真实输出（窗口标题）中仍写着旧编号**，
因为这些属于模板自身的文字，本次整理没有改动。下表供对照旧笔记与旧提交信息。

| 旧编号 | 现路径 |
|---|---|
| `01` | `01-compiler/01-c-single-file` |
| `02` | `01-compiler/02-cpp-single-file` |
| `03` | `03-build-toolchain/01-cmake-c` |
| `04` | `03-build-toolchain/02-cmake-cpp` |
| `05` | `04-syntax/01-cpp-const-init` |
| `06` | `05-oop/01-cpp-class` |
| `07` | `05-oop/02-cpp-inheritance` |
| `08` | `05-oop/03-cpp-templates` |
| `09` | `05-oop/04-cpp-raii-exceptions` |
