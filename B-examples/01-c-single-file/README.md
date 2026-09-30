# 示例 01 · C 单文件调试

## 使用方法

1. **用 VS Code 打开本文件夹**（`文件 → 打开文件夹`，选中 `01-c-single-file`）。
2. 打开 `main.c`，在行号左侧单击以设置断点（也可将光标停在目标行后按 `F9`）。
3. 按 **`F5`**，在弹出的列表中选择：

   | 选项 | 编译器 | 调试器 | 可用性 |
   |---|---|---|---|
   | `GDB · 调试当前 .c 文件` | MinGW **gcc** | **GDB** | 路径全为英文时可用，见下 |
   | `MSVC · 调试当前 .c 文件` | MSVC **cl.exe** | **Visual Studio 调试器** | 可用 |

编译与启动调试均自动完成。

> [!WARNING]
> 工作区路径中含中文时，GDB 无法打开可执行文件，调试无法启动。
> 原因是 GDB 在 Windows 上经 MI 协议接收文件名时期望 ANSI 代码页字节，
> 而 VS Code 发送的是 UTF-8 字节。
> **路径全为英文时不受影响。**
>
> **若**路径含中文，请选择 `MSVC ·` 开头的配置。
>
> 原因与解决方案见《01-编译器/02-环境配置.md》第 1 章第 2 节。
> 把本文件夹移到全英文路径后，GDB 路线即可恢复可用。

## 注意事项：必须打开本文件夹

VS Code 只识别**工作区根目录**的 `.vscode`。

- 打开 `01-c-single-file` 这个文件夹：使用的是本目录的 `.vscode`，配置正常生效。
- 打开上层的 `C相关课程`：使用的是**上层**的 `.vscode`，本目录的配置不生效。

## 演示内容

`main.c` 围绕**指针、数组、初始化**展开（对应课程《指针、数组、初始化》）：

| 行号 | 内容 |
|---|---|
| 34 | `int arr[N] = {10, 20, 30};` —— 部分初始化，其余自动补 0 |
| 40 | `int *p = arr;` —— 指针与数组的关系 |
| 54 | `int *q = arr + 2;` —— 指针算术 |

**建议在该断点处观察**：程序停在第 34 行后，在「变量」面板展开 `arr`，
可见 `[3]` 与 `[4]` 的值为 `0`，即“部分初始化补 0”。

## 配置要点

| 文件 | 作用 |
|---|---|
| `.vscode/launch.json` | 两个调试配置；`miDebuggerPath` 指向 `<MinGW>\bin\gdb.exe` |
| `.vscode/tasks.json` | 两个构建任务；**`-g` 不可省略**，缺少它就没有调试信息 |
| `.vscode/settings.json` | 智能感知指向 gcc |
| `.vscode/tools/build-msvc.ps1` | MSVC 路线的编译脚本（需加载 vcvars 环境） |

产物位置：
- gcc 路线 → `build/gcc/main.exe`
- MSVC 路线 → `build/msvc/main.exe`（另有 `.pdb`）

## 换用其他源文件

`launch.json` 中使用的是 `${fileBasenameNoExtension}`，因此**对本目录下任何 `.c` 文件均成立**。
新建 `test.c` 后设置断点并按 F5 即可调试，无需修改配置。

## 常见问题

**按 F5 时提示找不到任务**
`launch.json` 的 `preLaunchTask` 与 `tasks.json` 的 `label` 必须完全一致。建议直接复制粘贴。

**MSVC 路线提示找不到 cl.exe**
检查 `.vscode/tools/build-msvc.ps1` 能否单独运行通过：

```powershell
pwsh -File .vscode\tools\build-msvc.ps1 -Source main.c -Output build\msvc\main.exe
```

该脚本会打印实际选中的 Visual Studio 路径。

**调试控制台出现 `No such file or directory` 警告**
参见《02-调试器/00-本机环境与路线.md》中的说明：这是中文路径下 GDB 的已知现象，**不影响断点与单步**。
