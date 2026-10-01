# 示例 `03-build-toolchain/03-qt-gui` · Qt 界面最小示例

本示例演示**把 Qt 接进 CMake 工程**这件事本身。
界面只有两个控件：一个标签、一个按钮，点按钮刷新计数。

> [!NOTE]
> 界面路线的取舍、装哪个套件、许可义务，都写在
> 《工具/获取依赖/README.md》里，此处不重复。
> 本文件只讲怎么把这个示例跑起来。

## 一、先确认 Qt 在哪

本示例不安装 Qt。先按《工具/获取依赖/README.md》第三节装好 Qt，
再用校验脚本确认路径与工具链：

`PowerShell`

```powershell
pwsh -File <工作区>\工具\获取依赖\检查Qt.ps1 -QtRoot '<Qt>/6.11.1/mingw_64'
```

校验脚本会告诉你这套 Qt 是哪个编译器编的。**范例必须用同一个编译器构建。**

## 二、构建

`PowerShell`

```powershell
# 1. Qt 的套件目录
$env:QT_ROOT = '<Qt>/6.11.1/mingw_64'

# 2. 配置。生成器必须与那套 Qt 匹配：MinGW 套件用 Ninja，
#    msvc2022_64 套件用 Visual Studio 生成器。
cmake -S . -B build-qt -G Ninja -DWITH_QT=ON

# 3. 构建
cmake --build build-qt
```

产物在 `build-qt\bin\app_gui_qt.exe`。

## 三、不打开 `WITH_QT` 会怎样

`WITH_QT` 默认为 `OFF`。不打开时这个工程什么都不构建，
**也不会因为找不到 Qt 而报错**：

`实测数据`
`Text`

```text
-- WITH_QT=OFF，Qt 版界面不构建。
-- 要构建它：-DWITH_QT=ON 并设好 QT_ROOT，见同目录 README.md。
```

## 四、让 exe 自己带着 Qt 跑

这是动态版 Qt 与静态版最大的区别：**可执行文件本身很小，Qt 的代码在 DLL 里。**
程序启动时找不到 DLL 就直接起不来——进程还在，但界面不出现。

`实测数据`

| 情形 | 结果 |
|---|---|
| PATH 里只有 MinGW，没有 Qt | **起不来**：未加载任何 `Qt6*.dll`，无窗口 |
| PATH 里有 `<Qt>\bin` | 正常，加载 3 个 Qt 模块，窗口标题正确 |

本示例的 `CMakeLists.txt` 已经调用了 `qt_enable_deploy()`，
**构建完就会自动把 Qt 的运行时与编译器运行时拷到 exe 旁边**，不需要额外操作：

`CMake`

```cmake
add_executable(app_gui_qt WIN32 src/main.cpp)
target_link_libraries(app_gui_qt PRIVATE Qt6::Widgets)
qt_enable_deploy(app_gui_qt)
```

构建输出里会出现「部署 Qt 运行时到 ……」与三行「拷贝编译器运行时……」。
部署之后 `build-qt\bin\` 里是：

`实测数据`
`Text`

```text
bin\
├── app_gui_qt.exe                     92,662
├── Qt6Core.dll                    11,309,920
├── Qt6Gui.dll                     11,544,920
├── Qt6Widgets.dll                  7,199,064
├── libgcc_s_seh-1.dll                142,336     ← MinGW 运行库
├── libstdc++-6.dll                 2,354,176     ← MinGW 运行库
├── libwinpthread-1.dll                56,320     ← MinGW 运行库
├── platforms\qwindows.dll          1,272,168     ← 平台插件，最容易漏
└── opengl32sw.dll、D3Dcompiler_47.dll、styles\ 与几个插件目录
```

**为什么连 MinGW 的三个运行库也要拷**

`libgcc_s_seh-1.dll`（异常处理）、`libstdc++-6.dll`（C++ 标准库）、
`libwinpthread-1.dll`（线程）是**编译器**的东西，不是 Qt 的东西。
但**Qt 自己的 DLL 也依赖它们**：

`实测数据`
`Text`

```text
Qt6Core.dll 的直接依赖里包含：libgcc_s_seh-1.dll、libstdc++-6.dll、libwinpthread-1.dll
platforms\qwindows.dll 的直接依赖里包含：libgcc_s_seh-1.dll、libstdc++-6.dll
```

所以少了它们，哪怕 exe 编得好好的，程序照样起不来。
部署函数从**编这个工程的编译器**（而不是 Qt 自带的那个 MinGW）的 `bin` 目录拷，
保证运行时与编译器同源。

**三个开关**

| 开关 | 默认 | 作用 |
|---|---|---|
| `-DWITH_QT_DEPLOY=OFF` | ON | 关掉自动部署，改为自己把 `<Qt>\bin` 加进 PATH |
| `-DQT_DEPLOY_COMPILER_RUNTIME=OFF` | ON | 不拷编译器运行时 |
| `-DQT_MINGW_STATIC_GCC_RUNTIME=ON` | OFF | 把 GCC 运行时静态链进 exe |

**另一条路：把 GCC 运行时静态链进 exe**

打开 `-DQT_MINGW_STATIC_GCC_RUNTIME=ON`（或在 `CMakeLists.txt` 里调用
`qt_link_static_gcc_runtime(app_gui_qt)`）之后：

`实测数据`

| | 不打开（默认） | 打开 |
|---|---|---|
| exe 的直接依赖 | 含 `libgcc_s_seh-1.dll`、`libstdc++-6.dll` | 只剩 `Qt6Core.dll`、`Qt6Widgets.dll` 与系统 DLL |
| exe 大小 | 92,662 字节 | 273,440 字节 |
| 部署目录 | 31 个文件，84.5 MiB | 31 个文件，84.5 MiB |

**打开它也免不掉那三个 DLL**：Qt 自己的 DLL 依赖它们，目录里照样要放。
它只让 exe 自己的依赖表变短，代价是同一进程里出现两份 `libstdc++`
（exe 内一份静态的，Qt 的 DLL 用一份动态的）。因此默认不打开。

**验收判据：拷到干净目录能跑**

部署完只在本机双击还不够，要验证的是**目录整体搬走还能跑**：

`PowerShell`

```powershell
$env:Path = 'C:\Windows\system32'      # 只剩系统目录，没有 Qt，也没有 MinGW
& '<干净目录>\app_gui_qt.exe'
```

`实测数据`
`Text`

```text
存活=True  标题='示例 10 · Qt 界面最小示例'  已加载 Qt 模块=3
```

**不想自动部署时**，手工跑一次也行：

`PowerShell`

```powershell
& '<Qt>\6.11.1\mingw_64\bin\windeployqt.exe' --release <exe 的完整路径>
```

手工方式**不会**拷 MinGW 的三个运行库，需要自己从 `<MinGW>\bin` 补上。
完整的说明见《工具/获取依赖/README.md》第 3.5 小节。

## 五、动态版与静态版的产物对比

`实测数据`

| | 动态版 | 静态版 |
|---|---|---|
| `app_gui_qt.exe` | **92,662 字节** | 20,952,064 字节 |
| 还要带什么 | 29 个文件、69.2 MiB 的 DLL 与插件 | 无 |
| 拷单个 exe 到别的机器 | **不能运行** | 能运行 |

动态版的 exe 小得多，代价是发布时要连 DLL 一起带。
静态版的取舍与它的许可义务见《工具/获取依赖/README.md》第六节。

## 六、这个示例为什么只有 Qt 一份界面

主线 GUI 示例（`05-oop/01-cpp-class-basics`、`05-oop/02-cpp-inheritance-polymorphism`）
与练习模板一律给两份界面：`main_gui_win32.cpp` 与 `main_gui_qt.cpp`，
共用同一个 `core` 静态库。理由见《工具/获取依赖/README.md》第 2.1 小节。

本示例的用途不同：它演示的是 **Qt 接入本身**，
写成两份反而模糊了重点，因此只有 Qt 一份。

## 七、代码要点

`src/main.cpp` 里有两处值得注意：

| 位置 | 内容 |
|---|---|
| 信号与槽 | 用 `QObject::connect(button, &QPushButton::clicked, lambda)`。这是 Qt 5 之后推荐的写法，连接在**编译期**检查，签名写错编译就过不去 |
| 没有 `Q_OBJECT` | 全靠 lambda 连接，因此不需要 moc，`CMakeLists.txt` 里也就不必开 `AUTOMOC`。读者若加自定义信号，再打开它 |

> [!WARNING]
> **不要给链接了 Qt 的目标传字符集选项。**
> MSVC 下 Qt 6 会自动加 `/utf-8`，再补 `/source-charset:utf-8` 或
> `/execution-charset:gbk` 会让编译期直接报
> `D8016: “/source-charset:utf-8”和“/utf-8”命令行选项不兼容`；
> MinGW 下同理不要加 `-fexec-charset=GBK`。
> Qt 的字符串走 `QString`，本来也不依赖窄字符串的执行字符集。
> 本示例的 `CMakeLists.txt` 里刻意没有这些选项。

## 八、把它抄到自己的工程里

最小可用的接线只有三段：

`CMake`

```cmake
option(WITH_QT "构建 Qt 版界面" OFF)

if(WITH_QT)
    include(<仓库>/工具/获取依赖/qt-dynamic.cmake)
    find_package(Qt6 COMPONENTS Widgets REQUIRED)
    add_executable(app_gui_qt WIN32 src/main.cpp)
    target_link_libraries(app_gui_qt PRIVATE core Qt6::Widgets)
endif()
```

`qt-dynamic.cmake` 从 `-DQT_ROOT` 或环境变量 `QT_ROOT` 取路径，
并检查编译器与当前工程是否匹配。工程复制到仓库之外后，
把这段 `include` 换成自己的一份路径说明，或直接把 `QT_ROOT` 传进来即可。
