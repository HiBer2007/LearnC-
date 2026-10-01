# 依赖接入：Qt

本目录解决一件事：**让想用 Qt 写界面的读者，把自己那套 Qt 接进范例工程，
并让编出来的程序双击就能跑。**

> [!IMPORTANT]
> **本仓库不安装 Qt，也不随附 Qt。**
> Qt 由读者自行安装（官方在线安装器，或系统的包管理器），
> 套件目录通过环境变量或 CMake 变量传入。**仓库里不出现任何本机 Qt 路径。**
>
> 界面路线的默认选项仍然是 **Win32 API（零依赖）**。
> Qt 是可选路线，`WITH_QT` 默认为 `OFF`。

---

## 一、目录内容

| 文件 | 作用 |
|---|---|
| `检查Qt.ps1` | 给一个路径，报告版本、套件、动态还是静态、运行期依赖是否齐全，并给出构建命令 |
| `qt-dynamic.cmake` | 一行接入的接线，外加 `qt_enable_deploy()` 部署函数 |
| `qt-static.cmake` | **旧名字，仅作兼容**。转发到 `qt-dynamic.cmake`，见第 4.4 小节 |

没有安装脚本。Qt 用官方安装器装，见第三节。

---

## 二、两种界面

### 2.1 为什么同一个工程给两份界面

用到 GUI 的范例与练习模板里，界面代码都分成两份，共用同一个 `core` 静态库：

`Text`

```text
        ┌──────────────────────────────────────┐
        │  core（纯逻辑，不含任何界面代码）      │
        └──────────────────────────────────────┘
                 │                    │
    ┌────────────▼─────────┐  ┌───────▼──────────────┐
    │ main_gui_win32.cpp   │  │ main_gui_qt.cpp       │
    │ Win32 API，零依赖     │  │ Qt Widgets，跨平台    │
    │ 默认构建             │  │ WITH_QT=ON 时构建     │
    └──────────────────────┘  └───────────────────────┘
```

界面分成两份，是因为两条路线各有各的适用场合，而**核心逻辑不该写两遍**。

| | Win32 API | Qt Widgets |
|---|---|---|
| 依赖 | **零**。头文件与库随编译器提供，运行时由系统自带 | 需要装一套 Qt |
| 读者要做的准备 | 装好编译器与 CMake | 还要装 Qt，并保证编译器与 Qt 的套件匹配 |
| 首次可用时间 | 克隆完就能编 | 装完 Qt 即可 |
| 平台 | 仅 Windows | 跨平台 |
| 界面代码量 | 大 | 小 |
| 发布时要带的东西 | 一个 exe | exe 加一批 Qt 的 DLL 与插件，见第 3.5 小节 |
| 许可负担 | 无 | 有，见第六节 |

`core` 单独成一个静态库还有三条好处：

- 命令行版与两个 GUI 版共用同一份实现，不会出现两套逻辑
- 界面出问题时可以先跑命令行版，判断是逻辑错了还是界面错了
- `core` 可以被别的程序直接链接

### 2.2 `WITH_QT` 默认为 `OFF` 是什么意思

`CMake`

```cmake
option(WITH_QT "构建 Qt 版界面" OFF)

if(WITH_QT)
    include(${CMAKE_CURRENT_SOURCE_DIR}/../../工具/获取依赖/qt-dynamic.cmake)
    find_package(Qt6 COMPONENTS Widgets REQUIRED)
endif()
```

- **默认 `OFF`**：不传 `-DWITH_QT=ON` 时，`find_package(Qt6 ...)` 那一段根本不会执行。
  没装 Qt 的读者照样能构建命令行版与 Win32 版，配置阶段**不会因为找不到 Qt 而报错**。
- **显式打开 `ON`**：这时才去找 Qt，找不到就报错退出。
  这是有意的：既然明确要求了 Qt 版，就不该悄悄降级成别的。

因此「这套教材需要装 Qt 吗」的答案是：**不需要；只有想编 Qt 版界面时才需要。**

> [!NOTE]
> 本节说明「两种界面」这条安排。
> 装哪个套件、怎么接进工程、发布时带什么、许可义务，见第三、四、六节。
> 为何不用 WinUI 3 见第七节。

---

## 三、准备 Qt

### 3.1 用官方安装器装

从 Qt 官网下载 **Qt Online Installer**，安装时在组件树里勾选：

`Text`

```text
Qt
└── Qt 6.11.1
    ├── MinGW 11.2.0 64-bit          ← 选这一项（见 3.2）
    ├── MSVC 2022 64-bit             ← 只用 MSVC 时才选
    └── ...
```

装完后的目录结构是「**版本号一层、套件一层**」：

`Text`

```text
<Qt>\
├── 6.11.1\
│   ├── mingw_64\          ← 套件目录，QT_ROOT 要指到这一层
│   │   ├── bin\           Qt6Core.dll、windeployqt.exe、qmake.exe
│   │   ├── lib\
│   │   ├── include\
│   │   ├── mkspecs\       qconfig.pri 在这里
│   │   └── plugins\
│   ├── msvc2022_64\
│   └── llvm-mingw_64\
└── Tools\
```

**`QT_ROOT` 要指向套件目录，不是版本号那一层。** 指错时校验脚本与
`qt-dynamic.cmake` 都会报错，脚本还会列出生效的候选路径。

### 3.2 选哪个套件

套件（kit）就是「Qt 是用哪个编译器编的」。
一套 Qt 里通常有多个套件，各自对应一条工具链。

`实测数据`

| 套件 | 编译器 | 库文件 | 适合谁 |
|---|---|---|---|
| **`mingw_64`** | MinGW-w64（gcc） | `.a` / `.dll` | **本仓库主线就是 MinGW，选它** |
| `llvm-mingw_64` | LLVM-MinGW（clang） | `.a` / `.dll` | 用 clang 的读者 |
| `msvc2022_64` | MSVC | `.lib` / `.dll` | 用 MSVC 的读者 |
| `android_*`、`wasm_*` | —— | —— | 非 Windows 桌面，本仓库用不上 |

选 `mingw_64` 的理由很直接：本仓库的范例默认用 MinGW 的 `g++` 构建，
**Qt 的套件必须与编译范例的编译器一致**。C++ 没有跨编译器的稳定 ABI，
用 MinGW 去链 MSVC 编的库，或者在 MSVC 工程里链 MinGW 编的库，
都会在链接期报出一大片未解析符号。

`实测数据`

| 项 | 值 |
|---|---|
| Qt 套件 | `mingw_64`，Qt 6.11.1，`x86_64` |
| 该套件搭配的 MinGW | 13.1（`<Qt>\Tools\mingw1310_64`） |
| 构建范例用的 MinGW | **15.2.0**（本仓库主线） |
| 结果 | **配置、编译、链接全部通过，程序正常运行** |

两个 MinGW 版本不同没有造成问题：`libstdc++` 保持向后兼容，
高版本的编译器能够链接低版本编译器产出的库。

> [!WARNING]
> 上面的结论只在 **MinGW 对 MinGW** 之间成立。
> 换成 `msvc2022_64` 套件时，范例也必须改用 MSVC 生成器构建。
> `qt-dynamic.cmake` 会在编译器不匹配时直接报错，不会让你编到一半才发现。

### 3.3 先校验再用

`PowerShell`

```powershell
pwsh -File <工作区>\工具\获取依赖\检查Qt.ps1 -QtRoot '<Qt>\6.11.1\mingw_64'
```

`实测数据`
`Text`

```text
检查目录：<Qt>\6.11.1\mingw_64

---- 这套 Qt 的基本情况 ----
  版本：6.11.1
  架构：x86_64
  套件：mingw_64（MinGW-w64（gcc））
  QT_CONFIG：shared no-pkg-config separate_debug_info reduce_exports openssl release
  链接方式：动态版（shared）
  构建类型：Debug 与 Release 都有

---- 文件齐不齐 ----
  cmake/Qt6：在
  Qt6Widgets.lib/.a：在
  Qt6Core.dll：在
  Qt6Widgets.dll：在
  bin 下 Qt6*.dll 数量：153
  plugins/platforms/qwindows：在
  windeployqt.exe：在
  qmake.exe：在

结论：可以用来构建本仓库的 Qt 版范例。

---- 构建命令 ----
  $env:QT_ROOT = '<Qt>\6.11.1\mingw_64'
  cmake -S <工程目录> -B build-qt -G Ninja -DWITH_QT=ON
  cmake --build build-qt
```

脚本读的是 `<套件>\mkspecs\qconfig.pri`——Qt 安装自带的配置总结，
动态还是静态、版本、编译器都写在其中，不需要猜。

> [!TIP]
> **路径通过 `-QtRoot` 或环境变量 `QT_ROOT` 传入。**
> 本目录的脚本与两个 `.cmake` 里都**没有**任何具体机器的路径。
> 谁把 Qt 装在哪里，与别人无关。

### 3.4 运行前让程序找到 DLL

动态版的程序启动时要能找到 Qt 的 DLL。找不到就直接起不来——
进程还在，界面不出现。

`实测数据`

| 情形 | 结果 |
|---|---|
| PATH 里只有 MinGW，没有 Qt | **起不来**：未加载任何 `Qt6*.dll`，无窗口 |
| PATH 里有 `<Qt>\bin` | 正常，加载 3 个 Qt 模块，窗口标题正确 |

让程序找到 DLL 有两种做法：

**其一，开发时最方便：把 Qt 的 bin 加进 PATH。**

`PowerShell`

```powershell
$env:Path = '<Qt>\6.11.1\mingw_64\bin;' + $env:Path
```

**其二，让 exe 自己带着 Qt 跑。** 见下一小节。

### 3.5 让 exe 自己带着 Qt 跑

只把 Qt 的 bin 加进 PATH，等于程序**只能从这台机器、这个环境启动**。
把 exe 拷给别人、或者在资源管理器里双击，都会因为找不到 DLL 而起不来。

`qt-dynamic.cmake` 提供了一个函数，把「拷 DLL」这一步接到构建上：

`CMake`

```cmake
add_executable(app_gui_qt WIN32 src/main_gui_qt.cpp)
target_link_libraries(app_gui_qt PRIVATE core Qt6::Widgets)

# 构建完成后，自动把 Qt 的运行时与编译器运行时拷到可执行文件旁边
qt_enable_deploy(app_gui_qt)
```

在 `add_executable` 与 `target_link_libraries` 之后调用一次即可。
构建输出里会出现这几行：

`实测数据`
`Text`

```text
[2/2] Linking CXX executable bin\app_gui_qt.exe; 部署 Qt 运行时到 .../bin
; 拷贝编译器运行时 libgcc_s_seh-1.dll
; 拷贝编译器运行时 libstdc++-6.dll
; 拷贝编译器运行时 libwinpthread-1.dll
```

**MinGW 版的 exe 还需要三个 DLL**

| DLL | 是什么 | 从哪来 |
|---|---|---|
| `libgcc_s_seh-1.dll` | GCC 的异常处理运行时（64 位用 SEH；32 位 MinGW 上是 `libgcc_s_dw2-1.dll`） | 编译器，`<MinGW>\bin` |
| `libstdc++-6.dll` | C++ 标准库（动态版） | 编译器 |
| `libwinpthread-1.dll` | 线程与互斥支持 | 编译器 |

**这三个是编译器的东西，不是 Qt 的东西，因此不能指望 `windeployqt`。**
更要紧的是：**Qt 自己的 DLL 也依赖它们**，
所以少了它们，哪怕 exe 本身编得好好的，程序照样起不来。

`实测数据`
`Text`

```text
Qt6Core.dll 的直接依赖里包含：libgcc_s_seh-1.dll、libstdc++-6.dll、libwinpthread-1.dll
platforms\qwindows.dll 的直接依赖里包含：libgcc_s_seh-1.dll、libstdc++-6.dll
```

部署函数从**编这个工程的那个编译器**的 `bin` 目录拷这三个：

`CMake`

```cmake
get_filename_component(_cxx_bin "${CMAKE_CXX_COMPILER}" DIRECTORY)
```

**运行时必须与编译器同源。** 从 Qt 自带的 `<Qt>\Tools\mingw1310_64` 里
拷一份给 GCC 15.2 编出来的 exe 用，属于混用不同版本的 `libstdc++`。
拷贝用 `copy_if_different` 完成，内容一致时不写盘。

**三个开关**

| 开关 | 默认 | 作用 |
|---|---|---|
| `-DWITH_QT_DEPLOY=OFF` | ON | 关掉自动部署，改为自己把 `<Qt>\bin` 加进 PATH |
| `-DQT_DEPLOY_COMPILER_RUNTIME=OFF` | ON | 不拷编译器运行时 |
| `-DQT_MINGW_STATIC_GCC_RUNTIME=ON` | OFF | 把 GCC 运行时静态链进 exe，见下 |

**路线一与路线二**

| | 路线一（默认） | 路线二 |
|---|---|---|
| 做法 | 拷三个运行库 DLL | `-static-libgcc -static-libstdc++` |
| exe 的直接依赖 | 含 `libgcc_s_seh-1.dll`、`libstdc++-6.dll` | 只剩 `Qt6Core.dll`、`Qt6Widgets.dll` 与系统 DLL |
| exe 大小 | 92,662 字节 | 273,440 字节 |
| 部署目录 | 31 个文件，84.5 MiB | 31 个文件，84.5 MiB |
| 进程里有几份 `libstdc++` | **一份**（共享 DLL） | **两份**（exe 内一份，Qt 的 DLL 用一份） |

**路线二并不能免掉那三个 DLL。** Qt 自己的 DLL 就依赖它们（见上），
目录里照样要放。它只让 exe 自己的依赖表变短。

路线二还会让同一个进程里出现两份 `libstdc++`：exe 里静态链进去的一份，
Qt 的 DLL 用的动态一份。两份各有各的全局状态，
异常与本地化一类的行为容易出偏差。
因此本目录**默认走路线一**，路线二留给明确需要它的读者。

打开路线二的方式有两种，效果相同：

`CMake`

```cmake
qt_link_static_gcc_runtime(app_gui_qt)      # 只影响这一个目标
```

`PowerShell`

```powershell
cmake -S . -B build-qt -G Ninja -DWITH_QT=ON -DQT_MINGW_STATIC_GCC_RUNTIME=ON
```

**部署之后目录长什么样**

`实测数据`
`Text`

```text
bin\
├── app_gui_qt.exe                     92,662
├── Qt6Core.dll                    11,309,920
├── Qt6Gui.dll                     11,544,920
├── Qt6Widgets.dll                  7,199,064
├── Qt6Network.dll                  1,987,928
├── Qt6Pdf.dll                      8,384,344
├── Qt6Svg.dll                        635,224
├── libgcc_s_seh-1.dll                142,336     ← 编译器运行时
├── libstdc++-6.dll                 2,354,176     ← 编译器运行时
├── libwinpthread-1.dll                56,320     ← 编译器运行时
├── D3Dcompiler_47.dll              4,173,928
├── dxcompiler.dll                 14,317,864
├── dxil.dll                        1,510,696
├── opengl32sw.dll                 20,639,888
├── platforms\qwindows.dll          1,272,168     ← 最容易漏的一个
├── styles\qmodernwindowsstyle.dll    257,384
├── generic\ iconengines\ imageformats\ networkinformation\ tls\
```

**`platforms\qwindows.dll` 是最容易漏的一个。** 少了它会报

`文档`
`Text`

```text
This application failed to start because no Qt platform plugin could be initialized.
```

`windeployqt` 会自己把它带上，手工拷 DLL 时则常常忘掉整个 `platforms` 目录。

**体积代价**

`实测数据`

| 项 | 值 |
|---|---|
| exe 本身 | 92,662 字节 |
| 部署后目录 | **31 个文件，84.5 MiB** |

**发布一个 Qt 程序，带的是 80 MiB 量级的东西**，这是动态版 Qt 的固有代价。
`--no-translations` 已经默认加上；若要再小一些，可以手工删掉
`opengl32sw.dll`（20.6 MiB）与 `dxcompiler.dll`（14.3 MiB），
但那样在缺少 OpenGL 或 Direct3D 编译器的机器上显示会出问题。
本目录的做法是保留它们，换取到处都能跑。

**验收判据：拷到干净目录能跑**

只有一条算数：**把部署后的整个目录拷到一台没有 Qt、
`PATH` 里也没有 Qt 与编译器的机器上，程序能起来。**
在本机模拟的做法是把 `PATH` 缩到只剩系统目录：

`PowerShell`

```powershell
$env:Path = 'C:\Windows\system32'
& '<干净目录>\app_gui_qt.exe'
```

`实测数据`
`Text`

```text
存活=True  标题='示例 10 · Qt 界面最小示例'  已加载 Qt 模块=3
```

**自己查缺哪个 DLL**

`objdump` 是 MinGW 自带的工具。看 exe 的直接依赖：

`PowerShell`

```powershell
& objdump -p <exe 的完整路径> | Select-String 'DLL Name'
```

`实测数据`
`Text`

```text
DLL Name: libgcc_s_seh-1.dll
DLL Name: libstdc++-6.dll
DLL Name: Qt6Core.dll
DLL Name: Qt6Widgets.dll
DLL Name: KERNEL32.dll / msvcrt.dll / SHELL32.dll（系统自带）
```

`objdump -p` 只列**直接**依赖。`Qt6Gui.dll` 没有出现在上面，
它是 `Qt6Widgets.dll` 的依赖；`libwinpthread-1.dll` 也没有出现，
它是 `Qt6Core.dll` 的依赖。**排查时要把 Qt 的那几个 DLL 也过一遍**，
只看 exe 会漏。系统目录里本来就有的（`KERNEL32.dll`、`ntdll.dll`、
`DWrite.dll`、`AUTHZ.dll` 等）不必拷。


---

## 四、把 Qt 接进工程

### 4.1 三条命令

`PowerShell`

```powershell
# 第 1 步：告诉 CMake Qt 在哪里。写进环境变量，只设一次。
$env:QT_ROOT = '<Qt>\6.11.1\mingw_64'

# 第 2 步：配置。生成器必须与那套 Qt 匹配。
cmake -S <工程目录> -B build-qt -G Ninja -DWITH_QT=ON

# 第 3 步：构建（构建后会自动部署运行时，见 3.5 小节）
cmake --build build-qt
```

`mingw_64` 套件配 Ninja；`msvc2022_64` 套件改用
`-G "Visual Studio 17 2022" -A x64`，构建时加 `--config Release`。

`qt-dynamic.cmake` 会自动把 `QT_ROOT` 同时写进 `CMAKE_PREFIX_PATH` 与 `Qt6_DIR`，
因此工程里的 `find_package(Qt6 COMPONENTS Widgets REQUIRED)` 不需要额外参数。
也可以不用环境变量，直接传 CMake 变量：

`PowerShell`

```powershell
cmake -S <工程目录> -B build-qt -G Ninja -DWITH_QT=ON -DQT_ROOT='<Qt>\6.11.1\mingw_64'
```

### 4.2 `qt-dynamic.cmake` 做什么

按优先级找出 Qt：`-DQT_ROOT` → 环境变量 `QT_ROOT` → 都没有就什么都不做，
把选择权交回 `find_package`。找到之后它做四件事：

1. 确认目录里有 `lib/cmake/Qt6/Qt6Config.cmake`
2. 读 `qconfig.pri`，报告是动态版还是静态版
3. 从套件名判断工具链，与当前工程不符时**报错退出**
4. 设置 `CMAKE_PREFIX_PATH` 与 `Qt6_DIR`

它**不调用 `find_package`**：找不找 Qt、找哪些组件，由工程自己决定。

文件里还定义了 `qt_enable_deploy()`，见第 3.5 小节。

### 4.3 编译选项上的一条注意

Qt 在 MSVC 下会自动给目标加 `/utf-8`。
工程若再给同一个目标补 `/source-charset:utf-8` 或 `/execution-charset:gbk`，
编译期会直接报错：

`实测数据`
`Text`

```text
cl : 命令行 error D8016: “/source-charset:utf-8”和“/utf-8”命令行选项不兼容
cl : 命令行 error D8016: “/utf-8”和“/execution-charset:gbk”命令行选项不兼容
```

`/utf-8` 等价于同时指定源字符集与执行字符集，再单独指定其中任何一个都会冲突。

**结论：链接了 Qt 的目标不要再传任何字符集选项。**
Qt 的字符串走 `QString`，本来也不依赖窄字符串的执行字符集。
其它不链接 Qt 的目标（`core`、命令行版、Win32 版）不受影响，可以照旧。

用 MinGW 构建 Qt 界面时同理，不要再传 `-fexec-charset=GBK`。
MinGW 下这条不会报错，但界面上的中文会与 Qt 期望的编码不一致，问题更隐蔽。

### 4.4 关于 `qt-static.cmake` 这个旧名字

Qt 路线原先走「自建静态版」，文件名是 `qt-static.cmake`。
改成动态版之后文件名换成了 `qt-dynamic.cmake`，
但**旧文件保留了下来**，内容只是一行转发。

原因是有若干范例工程与练习模板里写着：

`CMake`

```cmake
include(${CMAKE_CURRENT_SOURCE_DIR}/../../工具/获取依赖/qt-static.cmake)
```

删掉旧文件会让它们立刻配置失败。旧文件同时把旧变量
`QT_STATIC_ROOT` 映射到新的 `QT_ROOT`，因此按老办法设的环境变量也还能用。

**新写的工程请直接包含 `qt-dynamic.cmake`。**

---

## 五、实测验证结果

对本仓库六个 GUI 工程的实际配置、构建与运行结果。

`实测数据`

| 项 | 值 |
|---|---|
| Qt | 6.11.1，套件 `mingw_64`，动态版 |
| 编译器 | MinGW-w64 GCC 15.2.0 |
| CMake | 4.4.3 |
| 生成器 | Ninja |

`PowerShell`

```powershell
$env:Path = '<MinGW>\bin;' + $env:Path
$env:QT_ROOT = '<Qt>\6.11.1\mingw_64'
cmake -S <工程目录> -B <构建目录> -G Ninja -DWITH_QT=ON
cmake --build <构建目录>
```

构建目录一律放在仓库之外，避免把中间产物写进工作区。

`实测数据`

| 工程 | 配置 | 构建 | `app_gui_qt.exe` |
|---|---|---|---|
| `B-examples/05-oop/01-cpp-class-basics` | 通过 | 通过 | 1,470,499 字节 |
| `B-examples/05-oop/02-cpp-inheritance-polymorphism` | 通过 | 通过 | 2,012,136 字节 |
| `B-examples/03-build-toolchain/03-qt-gui` | 通过 | 通过 | 92,662 字节 |
| `C-templates/05-oop/01-cpp-class` | 通过 | 通过 | 855,833 字节 |
| `C-templates/05-oop/02-cpp-inheritance` | 通过 | 通过 | 1,038,178 字节 |
| `C-templates/05-oop/04-cpp-raii-exceptions` | 通过 | 通过 | 842,646 字节 |

六个都真跑过，窗口全部正常出现：

`实测数据`
`Text`

```text
07 : 存活=True  标题='示例 07 · IntVector 值类型演示（Qt 版）'
08 : 存活=True  标题='示例 08 · 导出器：抽象基类、工厂与虚析构（Qt 版）'
10 : 存活=True  标题='示例 10 · Qt 界面最小示例'
C06: 存活=True  标题='空模板 06 · 自己写的字符串类（Qt 界面）'
C07: 存活=True  标题='空模板 07 · 继承与多态（Qt 界面）'
C09: 存活=True  标题='空模板 09 · RAII 与异常（Qt 界面）'
```

> [!NOTE]
> 这一轮验证走的是 MinGW 路线，运行方式是把 `<Qt>\bin` 加进 PATH。
> MSVC 路线的对应做法是把 `QT_ROOT` 换成 `msvc2022_64` 套件、
> 生成器换成 Visual Studio，其余不变。
> 自动部署只在 `B-examples/03-build-toolchain/03-qt-gui` 上验证过，见第 3.5 小节。

---

## 六、许可义务

### 6.1 Qt 开源版的许可

`文档`

开源版 Qt 采用 **LGPLv3**（部分模块为 GPLv3），另有商业许可可选。
具体到一套安装，许可文本在 Qt 安装目录的 `Licenses` 子目录下。

本路线只用到 `Qt6::Widgets` 及其依赖的 `Qt6::Gui`、`Qt6::Core`，
这些适用 **LGPL-3.0-only**。

### 6.2 动态链接：LGPLv3 的标准情形

**动态链接是最简单的一种用法。** LGPLv3 允许把库以动态链接方式与自己的程序结合，
只要满足下面几条：

| 义务 | 做法 |
|---|---|
| **附许可文本** | 随程序附上 LGPLv3 全文（从自己那套 Qt 的 `Licenses` 目录取） |
| **保留版权声明** | 不要删改 Qt 的版权信息 |
| **说明用了哪个库** | 在文档或「关于」里写明使用了 Qt 及其版本 |
| **允许替换** | 让用户能够替换 Qt 的 DLL——动态链接本身就满足这一条 |

**没有静态链接那套重链接义务。**
用户能直接替换 `Qt6Core.dll` 这类文件，就等价于能用上自己修改过的 Qt，
因此不需要提供应用侧的目标文件或额外机制。

> [!CAUTION]
> **若修改了 Qt 本身，仍须按 LGPLv3 提供修改后的 Qt 源代码。**
> 本路线不修改 Qt：读者用的是官方安装器装的现成 Qt，
> 仓库里的三个文件只负责查找路径、检查编译器与拷 DLL，不碰 Qt 的任何源码。

### 6.3 静态链接的额外义务（供对照）

第 3.2 小节的套件表里没有静态套件，但读者可能自己编过一套。
静态链接把 Qt 的代码并入了可执行文件，用户失去了替换 DLL 的能力，
LGPLv3 第 4 节因此要求提供应用侧的目标文件或完整源代码，以便重新链接。

`实测数据`

| | 动态版 | 静态版 |
|---|---|---|
| `app_gui_qt.exe`（示例 10） | **92,662 字节** | 20,952,064 字节 |
| 还要带什么 | 28 个文件、82.0 MiB 的 DLL 与插件 | 无 |
| 拷单个 exe 到别的机器 | 不能运行 | 能运行 |
| LGPLv3 义务 | 附许可文本、允许替换 DLL | 另有重链接义务 |

**本仓库的 Qt 路线采用动态版**，静态版只在读者自己的选择下出现。

### 6.4 与本仓库自身授权的关系

**这里有一处容易写错的地方，需要作者确认。**

本仓库的根目录 `LICENSE` 是 **CC BY-NC-ND 4.0**，
`许可附加条款.md` 第一节写明：

`文档`

> **文档**（全部 `.md` 文件）→ CC BY-NC-ND 4.0 + 本附加条款
> **代码**（`.c` `.h` `.cpp` …）→ **不适用**，各自保持原有许可证
> **本附加条款只约束文档，不约束任何代码。**

仓库里出现的 GPL-3.0，指的是**被引用的另一个项目**
（`版权与许可说明.md` 第 1 章里的 NeoServerUpdateModpack），不是本教材仓库。

因此：

| 关系 | 结论 |
|---|---|
| 文档与 Qt | 文档不参与链接，**不产生许可交互** |
| 示例代码与 Qt | 示例代码作者自有，可按 LGPLv3 的条件动态链接 Qt |
| 分发含 Qt DLL 的可执行文件 | 附上 LGPLv3 全文即可，见第 6.2 小节 |

> [!WARNING]
> **CC BY-NC-ND 4.0 是非商业且禁止演绎的，而 LGPLv3 允许商业使用与修改。**
> 两者约束的对象不同（前者约束文档，后者约束 Qt 库代码），并不直接冲突。
> 但**不能在文档中声称整个分发物都只受 CC BY-NC-ND 约束**，
> 也不能对其中 Qt 部分附加额外限制。
>
> `待确认`
> 上述是对条款的工程解读，不是法律意见。
> 是否要在仓库里补一份「再分发时随附哪些许可文本」的说明，需要作者拍板。

---

## 七、为什么不用 WinUI 3

WinUI 3 是微软当前的 Windows 原生界面框架，但它与本教材的工具链前提冲突。

| 障碍 | 说明 |
|---|---|
| **需要 MSVC** | Windows App SDK 只提供 MSVC 的 C++ 头文件与库，依赖 C++/WinRT 与 MSVC 的 ABI |
| **MinGW 用不了** | 本教材的默认工具链是 MinGW-w64，官方不支持它使用 WinUI 3，见 [WindowsAppSDK 讨论 #3532](https://github.com/microsoft/WindowsAppSDK/discussions/3532) |
| **不是零依赖** | 应用需要 Windows App SDK 运行时；要免安装就得做自包含打包，与「零依赖」相反 |
| **覆盖不了两条工具链** | Qt 官方的 Windows 构建文档同时列出 MSVC 2022 与 Mingw-w64 两种受支持编译器，见 [Qt 官方 Windows 构建文档](https://doc.qt.ac.cn/qt-6/windows-building.html)；WinUI 3 做不到这一点 |

> [!NOTE]
> 社区存在把 C++/WinRT 头文件移植到 MinGW 的非官方项目
> （例如 `alvinhochun/mingw-w64-cppwinrt`）。
> 那条路依赖非官方补丁，与「读者克隆仓库即可复现」的目标相悖，本教材不采用。

**结论**：界面路线的取舍是——

`Text`

```text
默认   Win32 API      零依赖，两条工具链都能用，读者不用装任何东西
可选   Qt             装一次官方安装器即可，界面代码短、跨平台，发布时带 80 MiB 量级的 DLL
不用   WinUI 3        只支持 MSVC，且需要 Windows App SDK 运行时
```

---

## 八、已知问题

### 8.1 第三方的大目录不要放进工作区

Qt 的安装目录是**上万个文件、数 GiB**。把这类目录放进工作区，编辑器会持续做三件事：

| 动作 | 后果 |
|---|---|
| 文件监视 | 大量文件变更事件持续占用 CPU |
| 语言服务索引 | 第三方头文件进入补全与跳转的候选集，搜索被淹没 |
| 系统搜索索引 | 后台建立索引，磁盘与 CPU 长期繁忙 |

**结论：第三方依赖一律放在工作区之外。**
本仓库的三个文件只负责「找到它、检查它、把运行时拷出来」，
Qt 装在哪里由读者的 `QT_ROOT` 决定。

本仓库的 `.vscode/settings.json` 已经把 `第三方/`、`build/` 一类目录加进
`files.watcherExclude` 与 `search.exclude`，`C_Cpp.files.exclude` 也做了排除。
排除规则只是兜底，**大目录仍然应当放在工作区之外**。

### 8.2 链接了 Qt 的目标不要传字符集选项

见第 4.3 小节。这条在 MSVC 下会直接编译失败，在 MinGW 下则是运行期中文乱码，
两种都值得先排除掉。

### 8.3 部署之后程序仍然起不来

按可能性从高到低排查：

| 现象 | 原因 |
|---|---|
| 报找不到平台插件 | `platforms\qwindows.dll` 没拷过去，见第 3.5 小节 |
| 报缺 `libstdc++-6.dll` 等 | 目标机器没有 MinGW 运行库，加 `-DQT_DEPLOY_COMPILER_RUNTIME=ON` |
| 报缺 `vcruntime140.dll` 等 | 目标机器没有 VC 运行库，同上，或让对方装 VC 运行库 |
| 完全没有反应，也不报错 | 检查是否真的拷到了 exe 所在目录，而不是它的上一层 |

### 8.4 自建静态 Qt 才会遇到的路径限制

Qt 官方要求**源码路径不含空格与 Windows 特殊字符，且要短**；
路径含非 ASCII 字符时，Qt 自带的 `syncqt` 会在生成头文件别名时失败。

`文档`

> The path to the source directory must not contain any spaces or
> Windows-specific file system characters. The path should also be kept short.

**这条与动态路线无关**：用官方安装器装好的 Qt 不编译，不会碰到。
只有自己从源码构建 Qt 时才需要把源码与构建目录放在纯 ASCII 路径下。

### 8.5 `-DWITH_QT=ON` 但没设 `QT_ROOT`

`qt-dynamic.cmake` 在 `QT_ROOT` 为空时什么都不做，
于是 `find_package(Qt6 ...)` 去系统里找。
找不到就报 `Could not find a package configuration file provided by "Qt6"`。

处理办法是设好 `QT_ROOT` 再配置。这条报错信息本身不指向根因，
因此在这里记一笔。

### 8.6 `QT_ROOT` 指到了版本号那一层

`<Qt>\6.11.1` 是版本目录，不是套件目录。指错时
`qt-dynamic.cmake` 与 `检查Qt.ps1` 都会报错，脚本还会列出生效的候选路径。

### 8.7 控制台上的中文路径可能显示为乱码

构建过程中，部分中文路径在控制台上显示成乱码。
这是捕获管道按 UTF-8 解码 GBK 输出造成的，**文件路径本身正确**，不影响构建。
