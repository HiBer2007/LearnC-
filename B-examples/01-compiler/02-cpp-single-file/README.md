# 示例 `01-compiler/02-cpp-single-file` · C++ 单文件调试

## 使用方法

1. **用 VS Code 打开本文件夹**（`文件 → 打开文件夹`，选中 `02-cpp-single-file`）。
2. 打开 `main.cpp`，在行号左侧单击以设置断点。
3. 按 **`F5`**，选择：

   | 选项 | 编译器 | 调试器 | 路径含中文时是否可用 |
   |---|---|---|---|
   | `GDB · 调试当前 .cpp 文件` | MinGW **g++** | **GDB** | **不可用**，见下 |
   | `MSVC · 调试当前 .cpp 文件` | MSVC **cl.exe** | **Visual Studio 调试器** | 可用 |

> [!WARNING]
> 工作区路径中含中文时，GDB 无法打开可执行文件，调试无法启动，
> **`GDB ·` 开头的配置不可用**。
> 原因是 GDB 在 Windows 上经 MI 协议接收文件名时期望 ANSI 代码页字节，
> 而 VS Code 发送的是 UTF-8 字节。
> **路径全为英文时不受影响。**
>
> 若路径含中文，请选择 `MSVC ·` 开头的配置。
> 原因与解决方案见《01-编译器/02-环境配置.md》第 1 章第 2 节。
> 把该文件夹移到全英文路径后，GDB 路线即可恢复可用。

## 与 `01-compiler/01-c-single-file` 唯一的实质区别：使用 g++ 而非 gcc

**`gcc` 编译 `.cpp` 文件时会按 C++ 语法编译，但不会链接 `libstdc++`。**
结果是 `std::cout`、`std::vector` 全部报 `undefined reference to ...`。

因此 C++ 必须使用 **`g++`**。可查看 `tasks.json` 中的 `command` 字段。

> **说明**：MSVC 的 `cl.exe` 不存在该问题，它按文件扩展名自动判断语言，两种语言的参数相同。

## 演示内容

<details open>
<summary><b>说明：STL 是什么</b>（C 语言中不存在这一概念）</summary>

**STL = Standard Template Library（标准模板库）**，
是 **C++ 标准库的一部分**，提供现成的数据结构与算法。

| 部件 | 作用 | 本示例中的例子 |
|---|---|---|
| 容器 | 现成的数据结构 | `std::vector<Student>`、`std::string` |
| 迭代器 | 遍历容器的通用指针 | `list.begin()` / `list.end()` |
| 算法 | 现成的算法 | `std::sort(...)` |

**C 语言中没有 STL。** C 需要自行 `malloc` / `free` 并另外记录长度。

> **完整讲解见《09-高阶数据结构/A-00-导读：数据结构是问题的形状.md》第 5 节、《08-一些散落的算法/05-排序（一）：比较排序的三种策略.md》第 5 节**。
> 容器、迭代器与算法的用法与实现原理在那里展开；
> 模板的用法与实现原理见《05-类与面向对象/11-模板.md》第 1 节。

</details>

**`launch.json` 中那几条 `setupCommands` 的作用**

调试器默认**无法识别 `vector` 的内部结构**，会将其拆解为一组内部指针
（显示的是它的“实现”，而非所要查看的“内容”）。

> **注意**：下述说法在网上流传较广，但实测表明其表述**并不完整**。
> 网上普遍写有“加一个 `-enable-pretty-printing` 即可”。
> 该开关仅表示“**允许使用** pretty-printer”，而 **MinGW 版 GDB 通常未将
> libstdc++ 的 printer 加入搜索路径**，因此仅添加该开关仍然无效。
>
> 实测对比（MI 协议输出，即 VS Code 收到的原始数据）：
>
> | 配置 | VS Code 拿到的值 |
> |---|---|
> | 只写 `-enable-pretty-printing` | `value="{...}"` ← 只能显示成不透明的可展开节点 |
> | 加上下面两条 `python` 命令 | `value="std::vector of length 5, capacity 5"` + `displayhint="string"` |
>
> 因此 `launch.json` 中增加了两条**真正使其生效**的命令：将脚本目录加入
> GDB 的 Python 搜索路径，再注册 printer。

</details>

`main.cpp` 围绕**类、STL 容器、引用、范围 for** 展开：

| 行号 | 内容 |
|---|---|
| 54 | `std::vector<Student> list = {...}` —— 构造对象列表 |
| 63 | `std::sort(...)` —— 排序后对比 |
| 77 | range-for 里 —— 看引用变量 `s` |

**建议进行以下两项观察**：

1. **查看 STL 容器**：停在第 63 行后展开 `list`。
   `launch.json` 中那几条 `setupCommands` 即用于使其显示成整齐的内容，
   而不是一组内部指针（原因见上文说明）。
2. **在调试器中修改变量值**：停在第 77 行，在「变量」面板中双击某个 `score_` 并修改，
   按 F5 继续运行，程序输出会随之改变。

## 配置要点

产物位置：
- g++ 路线 → `build/gcc/main.exe`
- MSVC 路线 → `build/msvc/main.exe`（另有 `.pdb`）

> **注意**：用 g++ 编译出的 exe 依赖 `libstdc++-6.dll` 与 `libgcc_s_seh-1.dll`。
> 由于 `<MinGW>\bin` 在 PATH 中，在已配置好 PATH 的计算机上可直接运行；
> 但**复制到其他计算机时会提示缺少 DLL**。如需分发给他人，请使用静态链接：
> `g++ -static-libgcc -static-libstdc++ ...`

## 换用其他源文件

`launch.json` 使用 `${fileBasenameNoExtension}`，对本目录下任何 `.cpp` 文件均成立。
