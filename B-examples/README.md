# 示例工程（`B-examples`）

本节提供五个可直接使用的调试示例。每个示例均自带完整的 `.vscode` 配置，单独打开该示例文件夹即可开始调试。

> **说明**：若不清楚编译与链接是怎么回事，建议先阅读《01-编译器/01-编译与链接.md》。
> 想看在真实项目里这些知识如何应用，见《01-编译器/04-语言与案例.md》与《01-编译器/05-嵌入式与交叉编译.md》。
>
> 若尚不清楚调试器的用途与操作方式，建议阅读
> 《02-调试器/01-原理与使用.md》。该文说明了调试器能解决哪些 `printf` 解决不好的问题，
> 以及断点、单步、调用堆栈、数据断点等具体操作。

| 示例 | 类型 | 演示重点 | 调试器 |
|---|---|---|---|
| [`01-c-single-file`](01-c-single-file/) | C 单文件 | 指针、数组、初始化 | GDB / VS2022 |
| [`02-cpp-single-file`](02-cpp-single-file/) | C++ 单文件 | 类、STL 容器、引用 | GDB / VS2022 |
| [`03-CMake-C`](03-CMake-C/) | CMake 多文件（C） | 跨文件单步、递归堆栈 | GDB / VS2022 |
| [`04-CMake-CPP`](04-CMake-CPP/) | CMake 多文件（C++） | 同上 + STL 整齐打印 | GDB / VS2022 |
| [`05-joint-debug`](05-joint-debug/) | 联合调试（Windows + Linux） | 一个窗口同时调试两个程序 | GDB（跨系统） |

> **说明**：STL 指 `std::vector` / `std::string` / `std::sort` 这类 **C++ 标准库中现成的**
> 数据结构和算法。**C 语言中没有这些内容**（C 需要自行调用 `malloc` / `free`）。
> 完整解释见 [`02-cpp-single-file/README.md`](02-cpp-single-file/README.md)。
>
> STL 与调试直接相关：GDB 默认无法识别 `vector` 的内部结构，
> 会将其显示为一组内部指针。
>
> **注意**：让 STL 整齐显示的关键，是 `setupCommands` 中的 `-enable-pretty-printing`
> （详见 [`02-cpp-single-file/README.md`](02-cpp-single-file/README.md) 的实测对比）。
> 部分 GDB 版本还需要额外的路径配置，本工作区的配置一并写入了一条基于
> `gdb.PYTHONDIR` 的 `python` 命令作为兜底；该命令在不需要时不会产生副作用。
>
> **关于示例 05**：它需要 WSL 环境。搭建方法、原理与远程调试见《02-调试器/02-跨系统调试.md》。
> 该示例演示 Windows 客户端与 Linux 服务端在同一窗口中的联合调试，
> 并已验证端到端可用。

每个示例内均提供 `README.md`，说明使用方法、断点位置与常见问题。

---

## 首要事项：必须单独打开示例文件夹

VS Code **只识别工作区根目录的 `.vscode`**。

```
正确：文件 → 打开文件夹 → 选中  B-examples\01-c-single-file
         → 使用的是 01 自带的 .vscode，F5 正常工作

错误：打开  examples  或  C相关课程  这一层文件夹
         → 使用的是上层的 .vscode，示例自带的配置不生效
```

若在**根工作区**中直接打开 `03-CMake-C/src/main.c` 并按 F5，
使用的将是根目录下的“单文件”配置：该配置会试图把 `main.c` 当作单文件单独编译，
而 `main.c` 调用了 `calc.c` 中的函数，因此会出现**链接失败**：

```
undefined reference to `add'
```

**该现象并非配置损坏**，而是打开方式不正确。需要调试 CMake 示例时，请单独打开对应的示例文件夹。

---

## 两种调试器的选择

每个示例均提供两条路线，按 F5 后在列表中选择：

| 路线 | 配置名里含 | 编译器 | 调试信息格式 | 本工作区是否可用 |
|---|---|---|---|---|
| **GDB** | `GDB ·` | MinGW `gcc` / `g++` | DWARF | **不可用**，见下 |
| **VS2022** | `VS2022 ·` | MSVC `cl.exe` | PDB | 可用 |

> **关于 GDB 路线不可用的原因（重要）**
>
> 工程所在路径中含中文时（本工作区即是如此）。GDB 在 Windows 上经 MI 协议
> 接收文件名时期望 ANSI 代码页（936）字节，而 VS Code 发送 UTF-8 字节，
> 导致 GDB **无法打开可执行文件**，调试完全无法启动。
> 该问题无法通过任何配置项解决。
>
> 表现：按 F5 后提示 `Program path ... is missing or invalid`（但文件确实存在），
> 或弹出空的"选择要终止的调试会话"列表后无反应。
>
> **在本工作区中请使用 VS2022 路线。** 若需要调试 gcc 编译的产物，
> 可安装 CodeLLDB 扩展（其 LLDB 不受该限制）。详见
> 《01-编译器/02-环境配置.md》第 1 章第 2 节。
>
> 把示例移到全英文路径后，GDB 路线即可恢复可用。

> **说明**：GDB 只识别 gcc 产出的 DWARF，VS2022 调试器只识别 cl 产出的 PDB，两者互不兼容。
> （唯一的例外是 LLDB，见《01-编译器/02-环境配置.md》第 1 章。）

---

## 产物位置

每个示例的产物均位于**各自的 `build/` 子目录**中，互不干扰：

```
01-c-single-file\build\gcc\main.exe      ← gcc 路线
01-c-single-file\build\msvc\main.exe     ← MSVC 路线（另有 .pdb）

03-CMake-C\build\mingw\bin\app.exe              ← Ninja 生成器
03-CMake-C\build\msvc\bin\Debug\app.exe         ← VS 生成器（多一层 Debug）
```

`build/` 目录可以随时删除，重新按 F5 时会自动重建。

---

## 配置写法参考

每个示例的 `.vscode/launch.json` 均带有**逐字段中文注释**，可直接作为模板使用。
如需更多变体（attach 到进程、带参数调试、CodeLLDB、跨平台），
见《01-编译器/02-环境配置.md》第 7 章的 13 套模板。

如需**自行练习配置过程**，可使用同级的 [`空模板`](../C-空模板/) 文件夹。

---

## 关于中文输出

各示例的编译参数中均包含字符集设置：

| 编译器 | 参数 |
|---|---|
| gcc / g++ | `-finput-charset=UTF-8 -fexec-charset=GBK` |
| MSVC cl.exe | `/source-charset:utf-8 /execution-charset:gbk` |

**这两项不可省略**。缺少时 gcc 与 MSVC 都会把字符串常量按 UTF-8 写入 exe，
而中文 Windows 的控制台按 CP936（GBK）解释，中文会显示为 `涓枃` 一类的乱码。
复制本目录的编译参数到自己的工程时，请注意一并复制这两项。

原因与字节级实测对照见《01-编译器/02-环境配置.md》第 9 章第 6 节。

> **注意**：MSVC 参数**不能写成 `/utf-8`**。该选项是
> `/source-charset:utf-8` 与 `/execution-charset:utf-8` 的合并写法，
> 会把执行字符集也设为 UTF-8，中文同样会乱码。
