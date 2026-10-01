# 示例 `07-lower-level/04-symbols-and-linking` · 符号与链接

一份把「链接器怎么挑定义」演出来的命令行工具。工程里有一个静态库、三个成员，
加上应用侧的强定义、一个 C++ 翻译单元，两套可执行目标。
它回答三个问题：**同名的两份定义最后用了哪一份**、**没人引用的库成员会不会进产物**、
**C 与 C++ 混编时符号名长什么样**。

`Text`

```text
04-symbols-and-linking/
  include/link_lab.h      接口：弱符号钩子、库成员、C++ 侧的两个函数、自测
  src/link_core.c         核心：弱默认定义（GCC 用 weak，MSVC 用 /alternatename）+ 自测
  src/link_static_a.c     静态库成员 A：1024 字节的只读表
  src/link_static_b.c     静态库成员 B：4096 字节的只读表
  src/link_override.c     应用侧的强定义，覆盖 core 里的弱定义
  src/link_cpp_side.cpp   C++ 翻译单元：一个 extern "C" 函数，一个 C++ 链接的函数
  src/main_cli.c          命令行版；对成员 B 的引用只在这里，且由 WITH_B 控制
  CMakeLists.txt          目标：core（静态库）、app_cli、app_cli_slim
  CMakePresets.json       mingw-gdb、mingw-release（-O2）与 msvc 三套预设
  .vscode/                三个调试配置与七个构建任务
```

`app_cli` 与 `app_cli_slim` 用的是**同一份源码**，只差一个 `-DWITH_B`。

## 先读教材

| 章节 | 讲的是什么 | 本示例用在哪里 |
|---|---|---|
| 《07-更底层/06-符号与链接属性.md》第 1.1 小节 | 一条符号记录由什么组成 | `nm` 输出里那几列：地址、类型字母、名字 |
| 《07-更底层/06-符号与链接属性.md》第 1.2 小节 | 未定义符号：链接的驱动力 | `libcore.a` 里的 `U ll_cpp_name` 正是把 C++ 目标文件拉进来的原因 |
| 《07-更底层/06-符号与链接属性.md》第 1.3 小节 | 符号表在可执行文件里也留着 | 对 exe 跑 `nm`，能看到解析之后的地址 |
| 《07-更底层/06-符号与链接属性.md》第 3.1 小节 | GCC 路线：`multiple definition` | README 里那段真实报错原文 |
| 《07-更底层/06-符号与链接属性.md》第 4.1 小节 | 真启动文件里的弱符号 | `link_core.c` 里那份默认实现是同一个机制 |
| 《07-更底层/06-符号与链接属性.md》第 4.2 小节 | 覆盖实验：同一个程序，两个版本 | `app_cli` 与去掉 `link_override.c` 之后的对照 |
| 《07-更底层/06-符号与链接属性.md》第 6.1 小节 | 静态库：成员粒度是目标文件 | `link_static_a.c` / `link_static_b.c` 两个成员 |
| 《07-更底层/06-符号与链接属性.md》第 6.2 小节 | 顺序敏感：库必须放在引用者之后 | CMake 把 `core` 放在目标列表最后 |

## 这个项目要解决什么问题

**弱定义是「留个位置」，强定义是「把它顶掉」。** 真启动文件里那一长串
`weak Default_Handler` 就是这件事：库里先给一份兜底实现，
应用侧写了同名函数，链接器就换强的。本示例把这件事缩小到一眼能看完：

`C`

```c
/* core 里：弱定义，别处没有强定义时用它 */
__attribute__((weak)) const char *ll_provider_name(void) {
    return "core-weak-default";
}

/* 应用侧：强定义，链接器选这一份 */
const char *ll_provider_name(void) { return "app-strong"; }
```

`nm` 上能直接看到这个替换：库里是小写的 `w`，exe 里变成大写的 `T`。

`实测数据`
`Text`

```text
$ nm build/mingw/libcore.a | Select-String ll_provider
0000000000000000 T .weak.ll_provider_name.ll_report
                 w ll_provider_name

$ nm build/mingw/bin/app_cli.exe | Select-String ll_provider
0000000140001790 T .weak.ll_provider_name.ll_report
0000000140001750 T ll_provider_name
```

**静态库按目标文件取舍，不按函数。** 这一点常被说成「静态库只链用到的函数」，
不准确：粒度是**成员**（一个 `.o`）。`link_static_b.c` 里有一张 4096 字节的表，
只要有一个符号被引用，整张表都会进产物；反过来，一个符号都不引用，
整个目标文件一个字节也不进。两个目标的 `size` 输出差得很清楚：

`实测数据`
`Text`

```text
   text	   data	    bss	    dec	    hex	filename
  46016	    272	   2976	  49264	   c070	build\mingw\bin\app_cli.exe
  41660	    272	   2976	  44908	   af6c	build\mingw\bin\app_cli_slim.exe
```

**引用写在哪个文件里，决定了这个演示成不成立。** 一开始把成员 B 的检查写在
`core` 的自测里，结果两个目标一样大——因为 `core` 自己也引用了它，
链接器照样把成员 B 拉进来。**只要 core 里还剩一句引用，粒度就无从谈起。**
现在的写法是：对成员 B 的引用只出现在 `main_cli.c` 里，且由 `#ifdef WITH_B` 圈住。

**`extern "C"` 只做一件事：不改名字。** C++ 编译出来的函数名带上参数类型，
C 侧找不到。加一层 `extern "C"` 之后符号名就是源码里的名字。
同一个目标文件里两个符号摆在一起，差别一目了然：

`实测数据`
`Text`

```text
$ objdump -t build/mingw/CMakeFiles/app_cli.dir/src/link_cpp_side.cpp.obj | Select-String "ll_cpp_name|_ZN"
[  2](sec  1)(fl 0x00)(ty   20)(scl   2) (nx 1) 0x0000000000000000 _ZN2ll9cpp_probeEv
[  4](sec  1)(fl 0x00)(ty   20)(scl   2) (nx 1) 0x000000000000000d ll_cpp_name
```

`_ZN2ll9cpp_probeEv` 读作：`ll` 命名空间里的 `cpp_probe()`，参数表为空。

**重复定义不是警告，是链接失败。** 两个翻译单元都定义同一个外部链接的变量，
链接器直接停下：

`实测数据`
`Text`

```text
ld.exe: b.c:(.data+0x0): multiple definition of `dup_symbol';
        a.c:(.data+0x0): first defined here
ld.exe: m.c:(.bss+0x0): multiple definition of `dup_symbol';
        a.c:(.data+0x0): first defined here
collect2.exe: error: ld returned 1 exit status
```

注意第二行：`m.c` 里写的是 `int dup_symbol;`（C 里这叫**暂定定义**，
C23 起也不再是定义），它同样算重复。要让它成为「只是声明」，
写 `extern int dup_symbol;`。

## 做完能掌握什么

- 会用 `nm` 的类型字母读符号：`T` 在代码段、`D` 在 `.data`、`B` 在 `.bss`、
  `R` 在只读段、`U` 是未定义、小写表示局部符号
- 会给一个弱定义再用强定义覆盖它，并能解释链接器凭什么选强的
- 会说清静态库的取舍粒度是目标文件，并用 `size` 证明它
- 会用 `extern "C"` 打通 C 与 C++，并能从修饰名反推函数签名
- 会读 `multiple definition` 报错，知道两种常见来源（重复定义、暂定定义）
- 会在 MSVC 上做同样的事：没有 `weak`，改用 `/alternatename`

## 构建与运行

`PowerShell`

```powershell
# 在 07-lower-level/04-symbols-and-linking 目录下
cmake --preset mingw-gdb
cmake --build --preset mingw-gdb

# 正常版本：引用了静态库的成员 B
build\mingw\bin\app_cli.exe

# 精简版本：同一份源码，没有引用成员 B
build\mingw\bin\app_cli_slim.exe

# 只看自测
build\mingw\bin\app_cli.exe --selftest

# 两个目标的体积差
size build\mingw\bin\app_cli.exe build\mingw\bin\app_cli_slim.exe

# 符号表：库里的弱定义与 exe 里的强定义
nm build\mingw\libcore.a | Select-String ll_provider
nm build\mingw\bin\app_cli.exe | Select-String ll_provider

# slim 版里找不到成员 B 的符号
nm build\mingw\bin\app_cli_slim.exe | Select-String ll_static_b
```

MSVC 那一条：

`PowerShell`

```powershell
cmake --preset msvc
cmake --build --preset msvc-debug
build\msvc\bin\Debug\app_cli.exe --selftest

# MSVC 的符号表工具是 dumpbin，不是 nm
dumpbin /symbols build\msvc\core.dir\Debug\link_core.obj | Select-String ll_provider
```

不想用预设、也不经过 CMake，手工编一遍：

`PowerShell`

```powershell
gcc -std=c17 -O2 -Wall -Wextra -Iinclude -c src\link_core.c     -o link_core.o
gcc -std=c17 -O2 -Wall -Wextra -Iinclude -c src\link_static_a.c -o link_static_a.o
gcc -std=c17 -O2 -Wall -Wextra -Iinclude -c src\link_static_b.c -o link_static_b.o
ar rcs libcore.a link_core.o link_static_a.o link_static_b.o

gcc -std=c17 -O2 -Wall -Wextra -Iinclude -DWITH_B=1 -c src\main_cli.c      -o main_cli.o
gcc -std=c17 -O2 -Wall -Wextra -Iinclude -c src\link_override.c            -o link_override.o
g++ -std=c++17 -O2 -Wall -Wextra -Iinclude -c src\link_cpp_side.cpp        -o link_cpp_side.o
g++ main_cli.o link_override.o link_cpp_side.o libcore.a -o app_cli.exe

# 精简版：不加 -DWITH_B，也不把 link_static_b.o 放进库里
ar rcs libcore_slim.a link_core.o link_static_a.o
gcc -std=c17 -O2 -Wall -Wextra -Iinclude -c src\main_cli.c -o main_slim.o
g++ main_slim.o link_override.o link_cpp_side.o libcore_slim.a -o app_cli_slim.exe
```

最后一行的库必须放在引用者**之后**：链接器从左到右扫一遍，
先看到 `libcore_slim.a` 时还没有任何未定义符号，它一个成员也不会取。

## 运行后应当看到什么

`build\mingw\bin\app_cli.exe`：

`实测数据`
`Text`

```text
示例 07-lower-level/04-symbols-and-linking · 符号与链接

== 同名符号最后落到了哪一份定义 ==
  ll_provider_name()   = app-strong
      core 里那份弱定义的名字是 core-weak-default；
      应用侧 link_override.c 给了强定义，链接器选了强的。

== 静态库的两个成员 ==
  ll_static_a_name()   = static-a   表 1024 字节
  ll_static_b_name()   = static-b   表 4096 字节
      本目标引用了成员 B，link_static_b.o 被链进来了。
      用 nm 看两个 exe 就能看出差别：slim 版里找不到 ll_static_b_name。

== C 与 C++ 混编 ==
  ll_cpp_name()        = cpp-side（C++ 翻译单元，用 extern "C" 导出）
  ll_cpp_tag()         = cpp-mangled（绕了一层 C++ 链接的函数）
      用 nm 看这两个符号：前者就是 ll_cpp_name，后者带着 _ZN 开头的修饰名。

== 自测 ==
  [通过] 1. 成员 B 的符号解析到了 link_static_b.o
  [通过] 2. 成员 B 里那张表是 4096 字节
  [通过] 3. ll_provider_name() 返回应用侧的强定义（弱符号被覆盖）
  [通过] 4. 返回值不是 core 里的那个默认名
  [通过] 5. 成员 A 的符号解析到了 link_static_a.o
  [通过] 6. 成员 A 里那张表是 1024 字节
  [通过] 7. ll_cpp_name() 来自 C++ 翻译单元（extern "C" 让 C 侧找得到它）
  [通过] 8. ll_cpp_tag() 绕了一层 C++ 链接的函数，名字会被修饰
  [通过] 9. 三个函数的地址互不相同
  [通过] 10. 三个函数落在同一个映像里（这是链接器把它们放进同一个 exe 的意思）
  [通过] 11. 同一件事问两次答案一样（自测本身是可重复的）

  自测结果：11 项中 11 项通过，全部通过
```

`build\mingw\bin\app_cli_slim.exe --selftest`：

`实测数据`
`Text`

```text
== 自测 ==
  [跳过] 1. 成员 B（本目标没有引用它，整个目标文件都不会被链进来）
  [跳过] 2. 成员 B 里那张表的大小
  [通过] 3. ll_provider_name() 返回应用侧的强定义（弱符号被覆盖）
  ...
  [通过] 11. 同一件事问两次答案一样（自测本身是可重复的）

  自测结果：11 项中 11 项通过，全部通过
```

两个目标都是 11 项，`slim` 版把成员 B 那两项记作「跳过」并计入通过。

MSVC 那一份（`build\msvc\bin\Debug\app_cli.exe --selftest`）同样是 11 项全通过，
说明 `/alternatename` 那条路线在这里与 `__attribute__((weak))` 等价。

## 代码里哪几处是要点

| 位置 | 要点 |
|---|---|
| `src/link_core.c` 的 `#if defined(_MSC_VER)` | GCC 用 `__attribute__((weak))`，MSVC 用 `#pragma comment(linker, "/alternatename:...")`；两条路线应用侧都是同一个调用 |
| `src/link_override.c` | 只被编进可执行文件，不进静态库；把它从目标里去掉，自测第 3 项立刻变失败 |
| `src/link_static_a.c` / `link_static_b.c` | 各带一张只读表，让「成员有没有进产物」在 `size` 上有数字 |
| `src/main_cli.c` 的 `#ifdef WITH_B` | **对成员 B 的唯一一处引用**；搬进 core 里这个演示就失效 |
| `src/link_cpp_side.cpp` 的 `extern "C"` | 只影响符号名，不影响调用约定；同一文件里的 `ll::cpp_probe` 保留修饰名做对照 |
| `include/link_lab.h` 的 `ll_report` / `ll_report_skip` | 计数器在 core 里，调用方能在自测中间插入自己的检查项 |
| `CMakeLists.txt` 的 `LINKER_LANGUAGE CXX` | 目标里既有 C 又有 C++ 源文件，必须用 g++ / cl 链接，C++ 运行时才会被带上 |
| `CMakeLists.txt` 的 `target_compile_definitions(app_cli PRIVATE WITH_B=1)` | 两个目标的唯一差别就在这一行 |

## 已知问题

- **`link_core.c` 里那份弱定义在 MinGW 上被放进了 COMDAT。**
  `nm` 会多打一行 `.weak.ll_provider_name.ll_report`，那是 GCC 把弱函数
  与同组的 `ll_report` 放进同一个 COMDAT 段留下的名字，不影响解析结果。
- **MSVC 的 `/alternatename` 与 `weak` 不完全等价。** `weak` 在链接期参与
  「强胜弱」的选择，`/alternatename` 只是「这个名字没定义时用那个」。
  两者的可观察结果在覆盖这一件事上一致，但 `weak` 还能表达
  「可以被覆盖，也可以被丢弃」，`/alternatename` 表达不了。
- **静态库的顺序敏感只在手工命令行下才会遇到。** CMake 会自己算依赖顺序，
  因此 `cmake --build` 无论怎么排都成功；上一条手工命令里的顺序要求
  是链接器的行为，不是 CMake 的。
- **`size` 的差值不等于那张表的大小。** 本机实测两个目标相差 4356 字节，
  表本身是 4096 字节，其余是成员 B 里两个函数与对齐带来的。
- **`dumpbin /symbols` 要指向中间目录。** MSVC 的中间文件在
  `build\msvc\core.dir\Debug\` 下，不是 `build\msvc\Debug\`。
