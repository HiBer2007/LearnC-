# 示例 04 · CMake 工程（C++，多文件）

## 使用方法

1. **用 VS Code 打开本文件夹**（`文件 → 打开文件夹`，选中 `04-CMake-CPP`）。
2. 打开 `src/main.cpp`，设置断点。
3. 按 **`F5`**，选择：

   | 选项 | 编译器 | 调试器 | 产物 | 本工作区是否可用 |
   |---|---|---|---|---|
   | `GDB · CMake 工程 (MinGW g++)` | **g++** | GDB | `build/mingw/bin/app.exe` | **不可用**，见下 |
   | `VS2022 · CMake 工程 (MSVC cl)` | cl.exe | VS2022 调试器 | `build/msvc/bin/Debug/app.exe` | 可用 |

> **注意**：工程所在路径含中文时（本工作区即是如此），
> 因此 **`GDB ·` 开头的配置无法启动调试**——GDB 打不开中文路径下的可执行文件。
> 请选择 `VS2022 ·` 开头的配置。
>
> 原因与解决方案见《01-编译器/02-环境配置.md》第 1 章第 2 节。
> 把本文件夹移到全英文路径后，GDB 路线即可恢复可用。

## 与示例 03（C 版）的区别

区别共四处，全部位于构建配置中：

| | 示例 03（C） | 示例 04（C++） |
|---|---|---|
| `CMakeLists.txt` | `LANGUAGES C` | `LANGUAGES CXX` |
| 标准变量 | `CMAKE_C_STANDARD` | `CMAKE_CXX_STANDARD` |
| `CMakePresets.json` | `CMAKE_C_COMPILER: gcc` | `CMAKE_CXX_COMPILER: g++` |
| `.vscode` 里 | gcc | g++ |

**调试配置（`launch.json`）基本相同**：调试器不区分所编写的是 C 还是 C++。

## 演示内容

> **说明**：如需了解 STL 的含义，可先阅读
> [`B-examples\02-cpp-single-file\README.md`](../02-cpp-single-file/README.md) 中的
> 「STL 是什么」一节。简要而言，`std::vector` / `std::string` / `std::sort`
> 这些**现成的数据结构和算法**即属于 STL，**C 语言中没有**。

```
src/main.cpp     ← 主程序
src/calc.cpp     ← 函数实现（另一个文件）
include/calc.hpp ← 共用声明
```

| 行号（main.cpp） | 观察内容 |
|---|---|
| 32 | `add(a, b)` —— **F11** 跳入 `src/calc.cpp` |
| 36 | `factorial(n)` —— 反复按 F11 观察堆栈逐层增高 |
| 45 | `join(words, " ")` —— 展开 `vector<string>` 查看内容 |

**C++ 特有的观察点**：`launch.json` 的 `setupCommands` 中有几条命令，
可使 `std::vector<std::string>` 在调试器中直接显示为 `{"Hello", "C++", "and", "VS Code"}`，
而不是一组内部指针。

> **注意**：不可只添加 `-enable-pretty-printing`。该开关仅表示“允许使用” pretty-printer，
> 而 MinGW 版 GDB 通常**未将 libstdc++ 的 printer 加入搜索路径**，添加后仍然无效。
> 真正生效还需要额外两条 `python` 命令（配置中已写好）：
> 将脚本目录加入 `sys.path`，再执行 `register_libstdcxx_printers(None)`。
> 详见 `B-examples\02-cpp-single-file\README.md` 中的实测对比。

## MinGW 编译 C++ 的两个注意事项

1. **必须使用 `g++`，不能使用 `gcc`**：`gcc` 编译 `.cpp` 不会链接 `libstdc++`，
   会报出大量 `undefined reference`
2. **产物依赖 DLL**：g++ 编译的 exe 依赖 `libstdc++-6.dll` 与 `libgcc_s_seh-1.dll`。
   本机因 `<MinGW>\bin` 在 PATH 中而可运行，**复制到其他计算机时会提示缺少 DLL**。
   如需分发，请添加 `-static-libgcc -static-libstdc++`

## 命令行方式

```powershell
cmake --preset mingw-gdb    ; cmake --build --preset mingw-gdb
cmake --preset msvc-vs2022  ; cmake --build --preset msvc-debug
```

## 新增源文件时需修改 CMakeLists.txt

```cmake
add_executable(app
    src/main.cpp
    src/calc.cpp
    src/你的新文件.cpp    # ← 加在这里
)
```

> **注意**：不要使用 `file(GLOB *.cpp)`：MSYS2 自 2024 年 11 月起默认禁用了通配符展开，
> 且 GLOB 不会在新增文件时自动重新配置。
