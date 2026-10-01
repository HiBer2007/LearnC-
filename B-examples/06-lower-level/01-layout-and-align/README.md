# 示例 `06-lower-level/01-layout-and-align` · 布局与对齐

一份把「对象在哪里」和「结构体怎么摆」打成表的命令行工具。它做三件事：
打印六类对象各自的地址，打印三种写法下同一个结构体的 `sizeof` / `alignof` / `offsetof`，
再让两个线程各撞一个计数器、把「同一缓存行」与「各占一条缓存行」的耗时摆在一起。

项目的重点不是「查一次 `sizeof`」，而是把**地址、大小、偏移、时间**四种证据凑齐：
一条讲内存布局的结论，只有配上真实地址与真实数字才算落到实处。

`Text`

```text
01-layout-and-align/
  include/layout_lab.hpp   接口：地址表、布局表、偏移表、伪共享测量、自测
  src/layout_lab.cpp       实现，核心逻辑全在这里，命令行只调它
  src/main_cli.cpp         命令行版：认参数、调核心、按顺序打印
  CMakeLists.txt           目标：core（静态库）、app_cli
  CMakePresets.json        mingw-gdb、mingw-release（-O2）与 msvc 三套预设
  .vscode/                 三个调试配置与六个构建任务
```

`core` 是纯逻辑的静态库，删掉 `main_cli.cpp` 它照样能编译、能自测通过。

## 先读教材

| 章节 | 讲的是什么 | 本示例用在哪里 |
|---|---|---|
| 《06-更底层/01-对象在哪里：栈、堆与静态区.md》第 1.1 小节 | 六类对象，六个地址 | `take_address_map` 取的正是这六类 |
| 《06-更底层/01-对象在哪里：栈、堆与静态区.md》第 1.2 小节 | 用 `size` 与 `nm` 看各段 | 打印出来的地址与 `nm` 的输出互相对照 |
| 《06-更底层/01-对象在哪里：栈、堆与静态区.md》第 1.3 小节 | `.data` 的初值从哪来，`.bss` 为什么不占文件 | 有初值 / 无初值两个全局量为什么落在不同段 |
| 《06-更底层/01-对象在哪里：栈、堆与静态区.md》第 3 节 | 栈：一个栈帧由什么组成 | 「局部量」那一行的地址为什么每次运行都不同 |
| 《06-更底层/01-对象在哪里：栈、堆与静态区.md》第 5 节 | 堆：从 `_end` 往上 | 「new 出来的」那一行为什么离映像区很远 |
| 《06-更底层/02-对齐、填充与缓存.md》第 2.1 小节 | 一张 `sizeof`／`offsetof` 表 | 布局表与偏移表打印出来的就是这一张 |
| 《06-更底层/02-对齐、填充与缓存.md》第 2.3 小节 | `packed` 的访问代价 | 「打包之后 value 变成非对齐访问」那句结论 |
| 《06-更底层/02-对齐、填充与缓存.md》第 2.6 小节 | `alignas`：把对齐提高一档 | `stat_aligned` 与 `counters_padded` |
| 《06-更底层/02-对齐、填充与缓存.md》第 3.1 小节 | 缓存行有多大 | `cache_line_size` 为什么写 64 |
| 《06-更底层/02-对齐、填充与缓存.md》第 3.2 小节 | 伪共享：两个线程各改各的，却互相拖慢 | 本示例的伪共享计时 |

## 这个项目要解决什么问题

**地址不等于段。** 打印一个地址很容易，说清它属于哪一段不容易。
同样的「取地址」，函数体在 `.text`、字符串字面量在 `.rdata`、
有初值的全局量在 `.data`、没初值的在 `.bss`、`new` 出来的在堆、
函数里的局部量在栈。本示例把这六个地址并排打印，再补一句
「谁高谁低由操作系统与链接器决定」——这一句是本机实测得到的，不是抄来的：
在本机这一次运行里，栈的地址反而**最小**。

**填充不是浪费。** `struct { uint8_t tag; uint32_t value; uint8_t flags; }` 三个字段一共 6 字节，
`sizeof` 却是 12。多出来的 6 字节里，前 3 字节让 `value` 落在 4 的倍数上，
后 3 字节让整个结构体在数组里也能保持对齐。示例把默认布局与打包布局的
`offsetof` 并排打印，差值一眼可见：`value` 从偏移 4 变成偏移 1。

**缓存行会被「顺便」共享。** 两个线程各撞一个计数器，互不相干。
可只要这两个计数器落在同一条缓存行里，两个核就会轮流把整行标脏，
缓存行在两核之间来回搬。示例让两个版本各跑 2000 万次，把纳秒数摆在一起：

`实测数据`
`Text`

```text
  同一缓存行       : 19.324 ns/次
  各占一条缓存行   : 4.861 ns/次
```

**数据没有共享，缓存行被共享了**——这就是伪共享。修法只有一句：`alignas(64)`。

## 做完能掌握什么

- 会把一个地址归到具体的段，而不是只知道「它在内存里」
- 会用 `sizeof` / `alignof` / `offsetof` 三个工具把结构体的布局画出来
- 会说清 `__attribute__((packed))` 与 `#pragma pack` 省下了哪几个字节、代价是什么
- 会用 `alignas` 把两个热点变量分到不同缓存行，并用计时证明它有效
- 会写「只看结果正确、不断言时间」的自测：计时是不稳定的，断言不能压在它上面

## 构建与运行

`PowerShell`

```powershell
# 在 06-lower-level/01-layout-and-align 目录下：配置 + 编译
cmake --preset mingw-gdb
cmake --build --preset mingw-gdb

# 跑：打印地址表、布局表、偏移表与伪共享对照，最后跑一遍自测
build\mingw\bin\app_cli.exe

# 只跑自测
build\mingw\bin\app_cli.exe --selftest

# 计时想跑快一点就减迭代次数；想只看布局就跳过计时
build\mingw\bin\app_cli.exe --iterations 2000000
build\mingw\bin\app_cli.exe --no-timing
```

MSVC 那一条：

`PowerShell`

```powershell
# 配置（Visual Studio 17 2022，x64）+ 编译 Debug
cmake --preset msvc
cmake --build --preset msvc-debug

# 跑（多配置生成器会多一层 Debug 目录）
build\msvc\bin\Debug\app_cli.exe --selftest
```

不想用预设时，等价的手写命令是：

`PowerShell`

```powershell
cmake -S . -B build/mingw -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_COMPILER=g++
cmake --build build/mingw

# 或者完全不经过 CMake，直接编这一份源码
g++ -std=c++17 -O2 -Wall -Wextra -Iinclude src/layout_lab.cpp src/main_cli.cpp -o app_cli.exe
```

## 运行后应当看到什么

`build\mingw\bin\app_cli.exe --iterations 20000000`：

`实测数据`
`Text`

```text
示例 06-lower-level/01-layout-and-align · 布局与对齐

== 六类对象各在哪一段 ==
  函数体        = 0x7ff780d6187b   .text
  字符串字面量  = 0x7ff780d671a0   .rdata / .rodata
  有初值全局量  = 0x7ff780d66000   .data
  无初值全局量  = 0x7ff780d6b040   .bss
  new 出来的    = 0x1eb448a2700   堆
  局部量        = 0x2e50fff94c   栈
  说明：映像那四段由链接器摆在同一个基址上，栈与堆由系统另外分配。
        谁高谁低完全由操作系统与链接器决定，标准不做任何保证。
        本机这一次是「栈 < 堆 < 映像」，换个平台就可能换一个次序。

== 结构体的大小与对齐 ==
  类型                sizeof  alignof
  stat_default          12        4
  stat_packed            6        1
  stat_aligned          16       16
  counters_shared       16        8
  counters_padded      128       64
  stat_default  1+4+1，为了对齐 value 补 3 字节、尾部再补 3 字节 → 12
  stat_packed   打包之后没有填充 → 6，代价是 value 变成非对齐访问
  stat_aligned  alignas(16) 把整个结构体钉到 16 字节边界 → 16

== 字段偏移（offsetof）==
  字段    默认布局  打包之后
  tag            0         0
  value          4         1
  flags          8         5
  默认布局里 value 前面有 3 字节填充，打包之后紧挨着 tag。
  填充不是浪费：它让每个字段落在自己对齐要求的位置上，
  处理器取一次就能拿到；拿掉填充，取值要多走几条指令。

== 伪共享：两个线程各撞一个计数器 ==
  迭代次数         : 20000000 × 2 个计数器
  同一缓存行       : 19.324 ns/次
  各占一条缓存行   : 4.861 ns/次
  校验值 a+b       : 80000000（应当是 80000000）
  两个计数器地址相距 64 字节（同一行版本只相距 8 字节）。
  同一行时两个核轮流把整行标脏，缓存行在两核之间来回搬，
  这就是伪共享：数据本身没有共享，缓存行被共享了。
```

地址那几个十六进制数每次运行都不同（ASLR），其余数字稳定。
`--selftest` 的真实输出：

`实测数据`
`Text`

```text
== 自测 ==
  [通过] 1. 六类对象的地址都取到了
  [通过] 2. 六类对象的地址互不相同
  [通过] 3. 函数体与字符串字面量在同一个映像里（地址差 < 16 MiB）
  [通过] 4. 有初值与无初值的全局量在同一个静态区（地址差 < 64 KiB）
  [通过] 5. stat_default 有 6 字节填充，sizeof 是 12
  [通过] 6. stat_packed 无填充，sizeof 是 6（1 + 4 + 1）
  [通过] 7. stat_aligned 由 alignas(16) 撑到 16 字节
  [通过] 8. stat_default 的对齐是 4（由 uint32_t 决定）
  [通过] 9. stat_packed 的对齐降到 1
  [通过] 10. stat_aligned 的对齐是 16
  [通过] 11. 默认布局里 value 在偏移 4
  [通过] 12. 默认布局里 flags 在偏移 8
  [通过] 13. 打包之后 value 在偏移 1
  [通过] 14. 打包之后 flags 在偏移 5
  [通过] 15. 不填充时两个计数器相距 8 字节，落在同一条缓存行
  [通过] 16. alignas(64) 之后两个计数器相距 64 字节，分属两条缓存行
  [通过] 17. counters_padded 的大小是两条缓存行
  [通过] 18. 两种布局下两个计数器的自增一次都没丢（结果与线程数无关）
  [通过] 19. 两次计时都拿到了非零的纳秒数

  自测结果：19 项中 19 项通过，全部通过
```

MSVC 那一份跑 `--selftest` 也是 19 项全通过。

用 VS Code 打开本文件夹后按 `F5`，有三个配置可选：
`GDB · app_cli`、`GDB · 自测`、`MSVC · app_cli`。
调试路线的选择与产物位置见 [`../../README.md`](../../README.md)。

## 拿 `nm` 与这条输出去对

`nm` 能直接看出哪个符号落在哪一段，这是对上面那张地址表最直接的旁证。

`实测数据`
`PowerShell`

```powershell
# 在示例目录下，用 MinGW 自带的 nm
nm build\mingw\bin\app_cli.exe | Select-String "with_init|without_init|g_literal"
```

`实测数据`
`Text`

```text
0000000140006000 d _ZN2ll12_GLOBAL__N_1L11g_with_initE
000000014000b040 b _ZN2ll12_GLOBAL__N_1L14g_without_initE
00000001400071a0 r _ZN2ll12_GLOBAL__N_1L9g_literalE
```

小写的 `d` / `b` / `r` 表示这三个都是**局部符号**——
它们写在匿名命名空间里，属于内部链接，别的翻译单元看不见。
`d` 是 `.data`、`b` 是 `.bss`、`r` 是只读数据。

把上面程序打印的三个地址各减去映像基址，与本表中的偏移完全一致：

`实测数据`
`Text`

```text
程序打印的 .data  0x7ff780d66000  − 基址 0x7ff780d00000 = 0x6000   ← nm 给的 0x140006000
程序打印的 .bss   0x7ff780d6b040  − 基址 0x7ff780d00000 = 0xb040   ← nm 给的 0x14000b040
程序打印的 .rdata 0x7ff780d671a0  − 基址 0x7ff780d00000 = 0x71a0   ← nm 给的 0x1400071a0
```

读 `nm` 输出里的名字要先还原修饰名，加 `-C` 即可：

`PowerShell`

```powershell
nm -C build\mingw\bin\app_cli.exe | Select-String "with_init|without_init|g_literal"
```

`实测数据`
`Text`

```text
0000000140006000 d ll::(anonymous namespace)::g_with_init
000000014000b040 b ll::(anonymous namespace)::g_without_init
00000001400071a0 r ll::(anonymous namespace)::g_literal
```

## 代码里哪几处是要点

| 位置 | 要点 |
|---|---|
| `include/layout_lab.hpp` 的 `address_map` | 六个地址存成 `uintptr_t` 而不是指针：栈上的局部量在函数返回后就没了，把它的地址当指针带出去会被判为悬垂指针 |
| `src/layout_lab.cpp` 的 `stack_address_value` | 局部量的地址当场转成整数；MSVC 仍会报 C4172，因此就地压掉这一条并写明理由 |
| `src/layout_lab.cpp` 的 `LL_PACK_PUSH` / `LL_PACKED` 宏 | `#pragma pack` 与 `__attribute__((packed))` 分两边写，同一个结构体在两条工具链上得到同样的布局 |
| `src/layout_lab.cpp` 的 `bump_pair` | 函数模板把「同一行」与「各占一行」两种计数器套进同一段代码，两边只差一个类型；`static Pair` 放在静态区，保证 `alignas(64)` 一定生效 |
| `src/layout_lab.cpp` 的 `measure_false_sharing` | 计时前先把两个计数器清零，计时后校验 `a + b`，把「结果对不对」与「快不快」分开 |
| `src/layout_lab.cpp` 的 `run_self_test` | 19 项里没有一项断言时间；计时不稳定，自测只断言结构上的事实 |
| `src/main_cli.cpp` 的 `main` | 命令行只认四个参数，逻辑全在 `core` 里；`--iterations` 为 0 时兜到 1，避免除零 |

## 已知问题

- **伪共享的倍数依机器而定。** 本机实测「同一行」是「各占一行」的约 4 倍。
  核数少、频率低或系统繁忙时差距会变小，个别情况下甚至看不出差别；
  自测因此只校验计数结果，不校验时间。
- **缓存行按 64 字节写死。** x86-64 的一级数据缓存行是 64 字节，本示例直接用它。
  换成别的架构要改成那个架构的真实值；C++17 没有可移植的查询接口。
- **地址的次序不是保证。** 「栈 < 堆 < 映像」只是本机这一次的结果。
  标准对进程地址空间的布局没有任何要求，换平台、换链接选项都会变，
  因此自测里比较的是「同一个映像内部的地址差」，不是六者的绝对次序。
- **`mingw-release` 预设只是备用。** 本示例的结论与优化等级无关，
  该预设用于对照 `-O2` 下 `sizeof` 与地址是否变化。
