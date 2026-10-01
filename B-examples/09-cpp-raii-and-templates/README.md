# 示例 09 · 小库与使用者：RAII + 模板（CMake 多文件）

一个静态库 `mini` 加一个使用者程序 `app`：

```
build\mingw\bin\app.exe                写数据文件 → 读回 → 统计 → 自测 → 删掉文件
```

小库提供两件东西：

| 头文件 | 内容 |
|---|---|
| `include/file_handle.hpp` | `FileHandle`（持有 `FILE*`，析构即关闭）、`ScopedPath`（析构即删文件） |
| `include/fixed_vector.hpp` | 类模板 `FixedVector<T, N>`、函数模板 `sum_of` / `max_of` / `to_text` 与两个**全特化** |

使用者只包含头文件、链接 `mini`，不碰任何实现细节。

## 先读教材

| 章节 | 讲的是什么 | 本示例用在哪里 |
|---|---|---|
| 《05-类与面向对象/06-RAII 与资源管理.md》第 2、3、4 节 | RAII 把释放写进析构、异常安全、自己写包装 | `FileHandle` 与 `ScopedPath`；自测第 9 项故意抛异常 |
| 《05-类与面向对象/04-构造与析构.md》第 3、7 节 | 析构函数做什么、RAII 的雏形 | `~FileHandle` 里一句 `close()` |
| 《05-类与面向对象/05-拷贝与移动.md》第 4、5 节 | 移动语义、什么时候该删掉拷贝 | 两个包装类都 `= delete` 拷贝，只留移动 |
| 《05-类与面向对象/11-模板.md》第 2、4、5、6 节 | 函数模板、类模板、实例化、特化 | `FixedVector<T, N>`、`sum_of`、`to_text` 的两个全特化 |

## 这个项目要解决什么问题

两件事凑在一个小项目里，因为它们经常一起出现：

1. **资源必须有人负责释放。** 程序要写一个数据文件、读回来、再删掉它。
   中途任何一步提前返回或抛异常，文件都不能留在磁盘上，句柄也不能漏关。
   靠「记得在每个出口写 `fclose`」是做不到的，靠析构函数才做得到。
2. **同一段逻辑要能用在不同的类型与容器上。** 求和、求最大值、转成文本，
   这三件事对 `FixedVector<double, 32>` 与 `std::vector<double>` 都该成立，
   对 `int`、`double`、`bool` 的格式化又各有各的写法。
   前者用函数模板，后者用全特化。

自测把这两件事都变成可观察的：句柄计数、打开计数、文件是否存在、
容器满了会不会越界、特化到底有没有生效。

## 做完能掌握什么

- 会把「成对的资源操作」写成一个类，并解释为什么它不能拷贝、只能移动
- 会说明「对象声明顺序」与「析构顺序」的关系，以及它为什么会影响到删文件
- 会在析构函数里释放资源，从而让提前返回与抛异常都不必单独处理
- 会写类模板与函数模板，并知道模板的实现为什么必须放在头文件里
- 会写全特化，并知道全特化已经是普通函数（定义要放进 `.cpp`）

## 文件

```
include/file_handle.hpp    两个 RAII 包装的声明
include/fixed_vector.hpp   容器模板、函数模板、两个全特化的声明
src/file_handle.cpp        RAII 包装的实现
src/fixed_vector.cpp       两个全特化的定义
src/main.cpp               使用者：写、读、统计、自测
CMakeLists.txt             两个目标：mini（静态库）、app（可执行）
CMakePresets.json          mingw-gdb 与 msvc 两套预设
.vscode/                   调试与任务配置
```

库与使用者分开，是为了让「接口」与「实现」的界限看得见：
`app` 只包含两个头文件，改 `file_handle.cpp` 里的实现不必重新编译 `main.cpp` 的逻辑。

## 阶段推进

| 阶段 | 目标 | 做什么 | 怎么验收 |
|---|---|---|---|
| **阶段 1** | 跑通 | 配置、编译，运行 `app.exe` | 看到统计结果与「19 项中 19 项通过，全部通过」；运行目录里**不留**数据文件 |
| **阶段 2** | 看住资源 | 把 `ScopedPath guard(path);` 那一行注释掉再跑一次 | 数据文件留在了目录里 —— 这一行就是「谁负责删」的全部答案 |
| **阶段 3** | 加一个模板函数 | 加 `min_of`（仿照 `max_of`），并用它输出最小值，再补一条自测 | 对 `FixedVector` 与 `std::vector` 都成立，不必写两份 |
| **阶段 4** | 加一个特化 | 给 `to_text<const char *>` 写一个全特化，把空指针显示成 `(空)` | 自测里加一项：`to_text(static_cast<const char *>(nullptr))` 得到 `(空)` |

阶段 2 是重点：把那一行注释掉，程序照样跑完，但文件留在磁盘上 ——
RAII 的价值只有在「忘了写清理」的对比下才看得出来。

## 构建与运行

`PowerShell`

```powershell
# 在示例 09 目录下：配置 + 编译
cmake --preset mingw-gdb
cmake --build --preset mingw-gdb

# 运行（默认在当前目录写 raii_sample.txt，结束时删掉）
build\mingw\bin\app.exe

# 换一个文件名
build\mingw\bin\app.exe 我的数据.txt
```

不想用预设时的等价写法：

`PowerShell`

```powershell
cmake -S . -B build/mingw -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_COMPILER=g++
cmake --build build/mingw
```

用 VS Code 打开本文件夹后按 `F5`，两个配置分别是
`GDB · CMake 工程 (MinGW g++)` 与 `MSVC · CMake 工程 (MSVC cl)`。
调试路线的选择与 `.vscode` 的用法见 [`../README.md`](../README.md)。

## 运行后应当看到什么

`实测数据`
`Text`

```text
示例 09 · 小库与使用者（RAII + 模板）
数据文件：raii_sample.txt

== 1. 写出数据文件 ==
  写入 7 行（含注释与空行）

== 2. 读回并解析 ==
  跳过 2 行（注释与空行），留下 5 个有效数据
    3.500
    1.250
    8.000
    -2.500
    4.750

== 3. 装进 FixedVector<double, 32> 并统计 ==
  个数 5，总和 15.000，最大 8.000，平均 3.000

== 4. 模板与全特化 ==
  to_text<int>(42)           = 42
  to_text<double>(3.14159265) = 3.142  ← 全特化，固定三位小数
  to_text<bool>(true)        = 是  ← 全特化，不是 1

== 5. 退出前的状态 ==
  活着的句柄对象 0 个，打开着的文件 0 个

== 自测 ==
  [通过] 1. 打开文件后，对象计数与打开计数都加一
  [通过] 2. 离开作用域后文件已关闭，不必手写 fclose
  [通过] 3. 读回三行，内容与写入一致
  ……（自测共 19 项，此处省略中间几行）
  [通过] 17. to_text<double> 走全特化，固定三位小数
  [通过] 18. to_text<bool> 走全特化，输出是或否，不是 1 和 0
  [通过] 19. FixedVector 能用范围 for 遍历

  自测结果：19 项中 19 项通过，全部通过

== 6. 收尾 ==
  main 即将返回，ScopedPath 会删掉 raii_sample.txt
```

运行结束后当前目录里除了可执行文件本身，不会留下数据文件：
自测用的临时文件也被一并删除。程序在第 5 段打印的
「活着的句柄对象 0 个，打开着的文件 0 个」就是这一点的直接证据。

## 代码里哪几处是要点

| 位置 | 要点 |
|---|---|
| `file_handle.hpp` 的 `FileHandle` | 拷贝构造与拷贝赋值 `= delete`：一个文件不能有两个主人 |
| `file_handle.cpp` 的构造函数 | 打开失败就抛异常，因此拿到的对象一定是可用的，不必再判空 |
| `file_handle.cpp` 的移动构造 | 交出去之后把源对象的指针置空，源对象变成合法的空句柄 |
| `file_handle.cpp` 的 `read_line` | 手写逐字符读取：Windows 的回车一并吃掉，最后一行没有换行也算读到 |
| `file_handle.cpp` 的 `~ScopedPath` | 析构里 `std::remove`：删文件的责任挂在一个对象上 |
| `file_handle.hpp` 的 `file_exists` | 查文件要用它。写成 `ScopedPath("x").exists()` 会临时造一个对象，析构时把文件删掉 |
| `main.cpp` 的自测第 8、9 项 | `guard` 声明在 `creator` 之前，于是 `creator` 先关文件、`guard` 后删文件；反过来写在 Windows 上会删不掉 |
| `fixed_vector.hpp` 的 `FixedVector<T, N>` | `T data_[N]{}` 整块在对象里，不碰堆；满了 `push_back` 返回 `false`，不越界 |
| `fixed_vector.hpp` 的 `sum_of` | 用 `Container::value_type`，因此 `FixedVector` 与 `std::vector` 共用一份代码 |
| `fixed_vector.cpp` | 两个全特化的定义：全特化已经不是模板，而是普通函数 |

## 已知问题

- `FileHandle` 只支持 C 的 `FILE*`；要管别的资源（套接字、句柄），
  照着同样的五件事（构造获取、析构释放、禁拷贝、可移动、可提前释放）再写一个类即可
- `FixedVector` 的容量是编译期常量，装不下时由调用方决定怎么办
- 程序默认在当前目录写数据文件；命令行参数可以换路径，但目录必须已存在
