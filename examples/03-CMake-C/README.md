# 示例 03 · CMake 工程（C，多文件）

## 使用方法

1. **用 VS Code 打开本文件夹**（`文件 → 打开文件夹`，选中 `03-CMake-C`）。
2. 打开 `src/main.c`，在行号左侧单击以设置断点。
3. 按 **`F5`**，选择：

   | 选项 | 编译器 | 调试器 | 产物 | 本工作区是否可用 |
   |---|---|---|---|---|
   | `GDB · CMake 工程 (MinGW gcc)` | gcc | GDB | `build/mingw/bin/app.exe` | **不可用**，见下 |
   | `VS2022 · CMake 工程 (MSVC cl)` | cl.exe | VS2022 调试器 | `build/msvc/bin/Debug/app.exe` | 可用 |

**CMake 的“配置”与“编译”两个步骤均自动完成**，无需手动输入命令。

> **注意**：工程所在路径含中文时（本工作区即是如此），
> 因此 **`GDB ·` 开头的配置无法启动调试**——GDB 打不开中文路径下的可执行文件。
> 请选择 `VS2022 ·` 开头的配置。
>
> 原因与解决方案见《编译器/02-环境配置.md》第 1 章第 2 节。
> 把本文件夹移到全英文路径后，GDB 路线即可恢复可用。

## 演示内容

这是一个**跨文件**的工程，与单文件调试的最大差异即在于此：

```
src/main.c    ← 主程序，调用 calc.h 里声明的函数
src/calc.c    ← 函数实现（在另一个文件里）
include/calc.h ← 两者共用的声明
```

| 行号（main.c） | 观察内容 |
|---|---|
| 34 | `add(a, b)` —— 按 **F11** 单步进入，会**跳转到 `src/calc.c`** |
| 39 | `factorial(n)` —— 反复按 F11，观察「调用堆栈」逐层增高 |
| 44 | `sum_array(...)` —— 进入后观察 `i` 与 `total` 的变化 |

**跨文件单步是多文件工程调试的核心内容**，建议完整执行一次。

## 目录结构

```
03-CMake-C\
├── CMakeLists.txt        工程定义（源文件必须显式列出）
├── CMakePresets.json     两套预设：mingw-gdb / msvc-vs2022
├── include\calc.h
├── src\main.c, calc.c
├── .vscode\              调试与构建配置
└── build\                产物（可随时删，会自动重建）
```

## 关键配置点

### 1. 产物路径不同（“找不到 exe”的常见原因）

```
Ninja（单配置生成器）        → build/mingw/bin/app.exe
Visual Studio（多配置生成器） → build/msvc/bin/Debug/app.exe
                                                    ↑ 多出来的这层是生成器加的
```

编写 `launch.json` 的 `program` 时不可遗漏 `Debug/`。

### 2. CMakePresets.json 不能写注释

该文件是**严格 JSON**。CMake 底层的 jsoncpp 恰好容忍注释，
但 VS Code 的 CMake Tools 扩展可能使用严格解析器。该文件会被多个工具读取，不应依赖这一容错行为。

### 3. 使用 Visual Studio 生成器时不需要 vcvars

CMake 会自行找到 Visual Studio。这也是 MSVC 路线使用
`"generator": "Visual Studio 17 2022"` 而非 `Ninja` 的原因：可省去整套环境变量配置。

## 命令行方式（不使用 VS Code）

```powershell
cmake --preset mingw-gdb    ; cmake --build --preset mingw-gdb
cmake --preset msvc-vs2022  ; cmake --build --preset msvc-debug
```

## 使用 CMake Tools 扩展的图形界面（可选）

1. `Ctrl+Shift+P` → `CMake: Select Configure Preset` → 选择一个预设
2. `Ctrl+Shift+P` → `CMake: Configure`
3. 此后状态栏中会出现「构建」按钮

> **说明**：另有一条更简便的路径：在状态栏点击 **Set Launch/Debug Target** 选择目标，
> 再点击 **Debug**，CMake Tools 会自动生成调试配置，**无需编写 launch.json**。
