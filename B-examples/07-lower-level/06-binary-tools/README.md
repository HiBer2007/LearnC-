# 示例 `07-lower-level/06-binary-tools` · 把工具的输出读成结论

一份把 `nm` / `objdump` / `size` / `readelf` 的输出读成结论的命令行工具。
它默认分析**它自己**：跑一遍四个工具，把原始输出解析成结构，
再打印一段「这段话是什么意思」。也可以指向别的文件——
交叉编译出来的 `.elf` 是最合适的第二个对象，因为那里能同时看到
`readelf` 与「两套地址」。

`Text`

```text
06-binary-tools/
  include/bin_tools.h    接口：跑工具、四个解析器、四段结论、自测
  src/bin_tools.c        实现，核心逻辑全在这里
  src/tool_fixtures.c    内嵌的样例输出（真实工具的真实输出），供自测使用
  src/main_cli.c         命令行版：认参数、调核心、按子命令打印
  CMakeLists.txt         目标：core（静态库）、app_cli
  CMakePresets.json      mingw-gdb、mingw-release（-O2）与 msvc 三套预设
  .vscode/               三个调试配置与七个构建任务
```

与另外五个示例不同，这个示例的重点不是某个硬件现象，而是**读工具输出的方法**：
解析与「跑工具」分开写，这样自测可以不依赖本机装没装 binutils。

## 先读教材

| 章节 | 讲的是什么 | 本示例用在哪里 |
|---|---|---|
| 《07-更底层/13-收尾：什么时候需要下到这一层.md》第 1.4 小节 | 体积超：先分清看的是哪个数字 | `size` 那一段的两句结论 |
| 《07-更底层/13-收尾：什么时候需要下到这一层.md》第 1.7 小节 | 一张总表 | 四个子命令合起来就是那张表的前四行 |
| 《07-更底层/13-收尾：什么时候需要下到这一层.md》第 1.1 小节 | 链接期报错：三种错，三条路 | `nm` 里那些 `U` 就是链接期的驱动力 |
| 《07-更底层/09-C++ 对象布局与它的硬件代价.md》第 2.1 小节 | 把编译器的账本翻出来 | 用 `nm` 看 `_ZTV` / `_ZTI` 开头的符号 |
| 《07-更底层/09-C++ 对象布局与它的硬件代价.md》第 2.4 小节 | 表放在哪个段 | 虚表落在 `.rdata`，`nm` 的类型字母是 `R` |
| 《07-更底层/09-C++ 对象布局与它的硬件代价.md》第 6 节 | 在 MCU 上这些代价意味着什么 | 用同一个工具分析交叉编译出来的 `.elf` |
| 《07-更底层/01-对象在哪里：栈、堆与静态区.md》第 1.2 小节 | 用 `size` 与 `nm` 看各段 | 本示例把这一步做成了程序 |

## 这个项目要解决什么问题

**四个工具回答四个不同的问题，别混着看。**

| 工具 | 回答什么 | 关键列 |
|---|---|---|
| `size` | 产物分三段各占多少字节 | `text` / `data` / `bss` |
| `objdump -h` | 每一段放在哪、占多少 | `Size` / `VMA` / `LMA` / `File off` |
| `nm` | 每个名字落在哪一段 | 类型字母：`T` `D` `B` `R` `U` `W` |
| `readelf -S` | ELF 的节表，含节的类型与标志 | `Type`（`PROGBITS` / `NOBITS`）、`Flg` |

**同一份产物，两个数字体系。** `size` 给的是三段之和；
`objdump -h` 给的是每一段各自的大小与地址。两者口径不同：
`size` 的 `text` 把 `.text`、`.rodata`、`.rdata` 都算进去了，
而 `objdump -h` 会把它们分开列。**「体积超了」这句话先要问清看的是哪个数字。**

**「两套地址」是链接脚本写在段表里的。** 拿本示例去分析
`03-bare-metal-boot` 编出来的 `.elf`，`.data` 那一行会露出两个地址：

`实测数据`
`Text`

```text
    .isr_vector      大小      484  地址 08000000
    .text            大小     1444  地址 080001e4
    .rodata          大小     1304  地址 08000788
    .data            大小        4  运行地址 20000000  装载地址 08000ca0  ← 两套地址
    .bss             大小       12  运行地址 20000004  装载地址 08000ca4  ← 两套地址
    ._user_heap_stack 大小     1536  运行地址 20000010  装载地址 08000ca4  ← 两套地址
    .ARM.attributes  大小       47  不占内存（只有文件里的字节）
  合计：带 CONTENTS（要写进文件）的段共 3236 字节；只有 ALLOC（只占内存）的段共 1548 字节。
```

**PE 上没有 `readelf`。** `readelf` 只认 ELF；用它看 MinGW 编出来的 PE 会直接报错。
PE 那边的对应物是 `objdump -p` 或 MSVC 的 `dumpbin /headers`。
示例遇到这种情况会打印一句指路，而不是把这当成失败。

**解析器要认得出两种排版。** 同一个 `size`，制表符对齐与空格对齐都出现过；
把 `hex` 那一列按十进制读会被截断（`12b0` 读成 `12`，剩下的 `b0` 被当成文件名）。
自测里专门有一条盯这件事：

`实测数据`
`Text`

```text
  [通过] 15. size：一次给两个文件时，两行都解析出来了（空格对齐）
  [通过] 16. size：hex 那一列按十六进制读，不会被当成十进制截断
```

**跑外部工具有一处 cmd 的坑。** `_popen` 把整串交给 `cmd.exe /c`，
而 cmd 有一条规矩：如果 `/c` 后面第一个字符是引号、且整串的引号多于两个，
它会把最外层的一对引号剥掉。剥掉之后 `"prog" "arg"` 变成 `prog" "arg`，
cmd 直接报「文件名、目录名或卷标语法不正确」。加一个 `call` 前缀即可绕开。

## 做完能掌握什么

- 会用一句话说清四个工具各回答什么问题，不再混着看
- 会从 `objdump -h` 的 `VMA` 与 `LMA` 两列认出「这段上电时要被搬一次」
- 会从 `nm` 的类型字母判断一个名字落在代码段、数据段还是只读段
- 会从 `readelf -S` 的 `NOBITS` 认出「这段不占文件、只占内存」
- 会写一个「跑工具 + 解析 + 给结论」的小工具，并把解析与执行分开测试
- 会绕开 `_popen` 与 `cmd.exe` 的引号规矩

## 构建与运行

`PowerShell`

```powershell
# 在 07-lower-level/06-binary-tools 目录下
cmake --preset mingw-gdb
cmake --build --preset mingw-gdb

# 分析它自己：四个工具各跑一遍
build\mingw\bin\app_cli.exe

# 只跑其中一段
build\mingw\bin\app_cli.exe size
build\mingw\bin\app_cli.exe sections
build\mingw\bin\app_cli.exe symbols
build\mingw\bin\app_cli.exe headers

# 只跑自测
build\mingw\bin\app_cli.exe selftest

# 分析交叉编译出来的 ELF（先把 03 编出来）
build\mingw\bin\app_cli.exe --file ..\03-bare-metal-boot\build\boot_demo.elf
```

MSVC 那一条：

`PowerShell`

```powershell
cmake --preset msvc
cmake --build --preset msvc-debug
build\msvc\bin\Debug\app_cli.exe selftest
```

配置阶段的记录（工具路径是在这一步定下来的）：

`实测数据`
`Text`

```text
-- BT_OBJDUMP_EXE = H:/mingw64/bin/objdump.exe
-- BT_NM_EXE = H:/mingw64/bin/nm.exe
-- BT_SIZE_EXE = H:/mingw64/bin/size.exe
-- BT_READELF_EXE = H:/mingw64/bin/readelf.exe
```

不想用预设、也不经过 CMake：

`PowerShell`

```powershell
gcc -std=c17 -O2 -Wall -Wextra -Iinclude src\bin_tools.c src\tool_fixtures.c `
    src\main_cli.c -o bin_tools.exe
```

这时四个工具的路径都是空串，程序会打印「没找到」并跳过对应的那一段；
自测里那两条也会记作「跳过」。

## 运行后应当看到什么

`build\mingw\bin\app_cli.exe`（节选）：

`实测数据`
`Text`

```text
== size K:\...\build\mingw\bin\app_cli.exe ==
  size 说的是「产物分三段各占多少字节」：
    K:\...\build\mingw\bin\app_cli.exe text    91804  data    752  bss   4032  合计    96588
      写进文件的是 text + data（92556），上电后占内存的是 data + bss（4784）。
  结论：text 与 data 要占 flash，data 与 bss 要占 RAM。

== objdump -h K:\...\build\mingw\bin\app_cli.exe ==
  段表说的是「每一段放在哪、占多少」：
    .text            大小    76208  地址 140001000
    .data            大小      736  地址 140014000
    .rdata           大小     9352  地址 140015000
    .bss             大小     4032  地址 14001a000
    .debug_aranges   大小      208  不占内存（只有文件里的字节）
    .debug_info      大小    13877  不占内存（只有文件里的字节）
    ...
  合计：带 CONTENTS（要写进文件）的段共 92556 字节；只有 ALLOC（只占内存）的段共 4032 字节。

== nm K:\...\build\mingw\bin\app_cli.exe ==
  符号表说的是「这个名字最后落在哪一段」：
    合计 1397 个：代码段 T 365、已初始化数据 D 174、未初始化数据 B 179、
              只读数据 R 205、未定义 U 1、弱符号 W 0、其它 473
    其中局部符号（小写字母）971 个，外部符号 426 个。
  结论：U 是链接期的驱动力：每有一个 U，就一定要有一处定义来配它。

== readelf -S K:\...\build\mingw\bin\app_cli.exe ==
  readelf 只认 ELF。用它看 PE 会直接报错，这是正常的：
  PE 那边的对应工具是 objdump -p 或 MSVC 的 dumpbin /headers。
```

自测：

`实测数据`
`Text`

```text
== 自测 ==
  [通过] 1. objdump -h：从样例里解析出 5 个段
  [通过] 2. objdump -h：.isr_vector 的运行地址是 0x08000000
  [通过] 3. objdump -h：.data 的运行地址与装载地址不同（两套地址）
  [通过] 4. objdump -h：.data 的装载地址在 flash 里
  [通过] 5. objdump -h：.bss 占内存但没有 CONTENTS（不占文件）
  [通过] 6. objdump -h：.text 是代码段且占文件
  [通过] 7. nm：从样例里解析出 12 个符号
  [通过] 8. nm：代码段符号 T 有 4 个（含一个局部 t 之外的外部符号）
  [通过] 9. nm：未定义符号 U 有 2 个
  [通过] 10. nm：弱符号 W 有 1 个
  [通过] 11. nm：数据段符号按字母分到 D / B / R 三类
  [通过] 12. nm：局部符号（小写字母）有 1 个
  [通过] 13. size：GNU 格式的三列数字都解析对了
  [通过] 14. size：GNU 格式的 dec 列也读到了
  [通过] 15. size：一次给两个文件时，两行都解析出来了（空格对齐）
  [通过] 16. size：hex 那一列按十六进制读，不会被当成十进制截断
  [通过] 17. readelf -S：从样例里解析出 5 个段（下标 0 那一行没有名字，跳过）
  [通过] 18. readelf -S：.bss 的类型是 NOBITS，因此不占文件
  [通过] 19. readelf -S：.isr_vector 的地址是 0x08000000
  [通过] 20. 构建时找到了 objdump / nm / size 三个工具
  [通过] 21. 真的跑一次 objdump -h，并拿到了段表

  自测结果：21 项中 21 项通过，全部通过
```

## 代码里哪几处是要点

| 位置 | 要点 |
|---|---|
| `src/bin_tools.c` 的 `build_cmdline` | `call` 前缀不能省：`cmd.exe /c` 会剥掉最外层的一对引号，剥完命令就废了 |
| `src/bin_tools.c` 的 `bt_run` | 末尾补 `2>&1`，工具的错误信息也进同一个缓冲区；解析失败时才看得到原因 |
| `src/bin_tools.c` 的 `bt_self_path` | 默认分析对象是自己：`GetModuleFileNameA` 取路径，符号表里就能看到本程序的段 |
| `src/bin_tools.c` 的 `bt_parse_objdump_h` | 段头行与属性行要分两步读：属性行里有没有 `CONTENTS` 决定了这段占不占文件 |
| `src/bin_tools.c` 的 `bt_parse_readelf_sections` | 用 `%n` 记住标志列的位置；`NOBITS` 就是「不占文件」 |
| `src/bin_tools.c` 的 `bt_parse_size` | `hex` 那一列必须按十六进制读，否则 `12b0` 会被截成 `12`，剩下的 `b0` 被当成文件名 |
| `src/bin_tools.c` 的 `bt_parse_nm` | 两种行形态都要认：有地址列的与没有地址列的（未定义符号那一种是空白） |
| `src/bin_tools.c` 的地址用 `unsigned long long` | Windows 上 `unsigned long` 是 32 位，PE 的 VMA 是 64 位，装不下会读成 `ffffffff` |
| `src/tool_fixtures.c` | 样例用的是真实工具的真实输出，自测因此不依赖本机装没装 binutils |
| `CMakeLists.txt` 的 `bt_slash` | 把路径里的反斜杠换成正斜杠再传进 C 字符串：反斜杠在 C 里是转义字符 |

## 已知问题

- **文件名那一列按 63 字节截断。** 路径很长时会被切掉，而且可能切在多字节字符
  中间，显示成乱码。示例打印时用了固定的列宽，没有做按字符边界的截断。
- **`readelf` 只认 ELF。** 分析 PE 时它必定报错，示例把这当成正常情况并给出一句指路；
  想看 PE 的头该用 `objdump -p` 或 `dumpbin /headers`。
- **MSVC 那套工具路径找不全。** `find_program` 找不到 `size` 与 `readelf` 时
  把它们留成空串，程序打印「没找到」并跳过对应段落；自测里那两条记作「跳过」。
- **`nm` 的输出格式随平台变。** 本示例只认「地址 + 类型字母 + 名字」与
  「类型字母 + 名字」两种形态；带 `-C` 还原过的名字含空格时，
  `%s` 会在第一个空格处停下，此时只解析到名字的前半截。
- **解析器不做越界保护之外的校验。** 段数超过 64、符号数超过 4096 时多出来的部分
  会被丢掉，不报错。示例的规模远小于上限，但改成通用工具时要留意。
- **`objdump -h` 在 PE 上不区分 `.text` 与 `.rdata` 的归属。** 那两段在 PE 里
  各占一个节，与 ELF 的划分方式不同，因此 `size` 与 `objdump -h` 对不上的时候
  先要确认看的是哪种格式。
