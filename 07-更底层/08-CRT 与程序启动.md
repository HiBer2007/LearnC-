# CRT 与程序启动

> **授权**：本章节属教材文档部分，采用 [CC BY-NC-ND 4.0](../LICENSE)
> 加[附加条款](../许可附加条款.md)授权：署名、非商业、禁止演绎、不允许二次分发。
> 文中引用的代码不受此限，各自保留原有许可证，详见[版权与许可说明.md](../版权与许可说明.md)。

上一章讲的是裸机上的 `main` 之前：谁把栈指针放好、谁把 `.data` 从 flash 搬进 RAM。
桌面程序里没有 `Reset_Handler`，也没有链接脚本里那几个边界符号，但 `main` 之前
同样有一段代码在跑：它取参数、建环境、调用全局对象的构造函数、注册退出处理。
这段代码不随程序发布，它来自工具链自带的一个库——**C 运行时库，CRT**。

两个世界在这里是同一个问题的两种解法。裸机侧把「准备数据」写进启动文件，
由 `Reset_Handler` 自己做；通用系统侧把「准备进程」写进 CRT，由操作系统的加载器
与运行库分工完成。**唯一完全相同的部分是构造表**：这一章会看到，
`__libc_init_array`、`__main` 与 `_initterm` 读的是同一种东西，
只是名字与摆法各不相同。

读者读完这一章应当能回答三个问题：程序的第一条指令在哪、全局对象什么时候被构造、
把异常关掉到底省下多少。这些都是可测量的，本章给出的每一组数字都来自真跑。

---

> **约定**：标注以行内代码单独成行——`实测数据` 表示实际执行验证过，紧跟它所说明的表格或代码块；
> `文档` 表示引自标准或官方资料，末尾给出草案章节号；`待确认` 表示尚未验证。
> 路径占位符（如 `<MinGW>`、`<工作区>`）的含义见《README.md》。

# 本章节定位

| 需要先知道 | 在哪 |
|---|---|
| 编译的四个阶段与目标文件 | 《01-编译器/01-编译与链接.md》第 2.5 小节 |
| 名字修饰与 `extern "C"` | 《07-更底层/05-ABI 与调用约定.md》第 5.2 小节 |
| 符号的强弱与构造表 | 《07-更底层/06-符号与链接属性.md》第 4 节、第 5 节 |
| 裸机侧的 `main` 之前 | 《07-更底层/07-链接脚本与启动代码.md》第 3 节 |
| 析构函数与 RAII | 《05-类与面向对象/06-RAII 与资源管理.md》第 2 节 |

**相邻的章节**：`07-链接脚本与启动代码` 讲裸机侧「上电到 `main`」的每一步，
本章讲通用系统上的同一段路；两者在 `__libc_init_array` 这一处接上。
`09-C++ 对象布局与它的硬件代价` 讲对象在内存里的形状，
本章只讲这些对象**什么时候被构造**。

| 节 | 讲什么 |
|---|---|
| **第 1 节** | 入口点：PE 头的入口地址、`mainCRTStartup`、`__tmainCRTStartup` 逐段、MSVC 的同一层 |
| **第 2 节** | 构造表：`__CTOR_LIST__` 与 `__do_global_ctors`、`__main`、`.CRT$XCU`、`__libc_init_array` |
| **第 3 节** | 跨翻译单元的构造顺序：三份证据、标准原文、初始化顺序问题、四种让顺序确定的办法 |
| **第 4 节** | `atexit` 的顺序、全局析构、`main` 返回之后、`abort` 与正常退出 |
| **第 5 节** | 异常的运行时支持：展开表、零开销思路、三个平台上的体积数字、关掉异常会怎样 |
| **第 6 节** | 速查表 |

---

# 第 1 节 入口点：谁在调用 `main`

**链接器写进可执行文件头里的那个入口地址，不是 `main`。**
它指向 CRT 里的一个函数，那个函数做完准备工作才调用 `main`。
这一节把这条链在 MinGW 与 MSVC 上各读一遍。

## 1.1 三个名字，三层调用

先看名字。一份最小的 C 程序编出来，符号表里与启动有关的至少有三个：

| 名字 | 谁提供 | 干什么 |
|---|---|---|
| `main` | 你的代码 | 程序的主体 |
| `mainCRTStartup` / `WinMainCRTStartup` | CRT | PE 头里写的入口点，只做极少的准备 |
| `__tmainCRTStartup` | CRT | 真正干活的那个：初始化、取参数、调 `__main`、调 `main`、调 `exit` |

**三层的分工是「越靠外层越薄」。** 最外层由链接器按程序里有没有 `main`、
是控制台还是窗口程序自动挑一个；中间层是所有程序共用的初始化流程；
里层才是应用代码。

`C`

```c
/* crt_entry.c   入口点与 CRT 符号：只看链接结果，不关心输出
   编译：gcc -std=c23 -O0 crt_entry.c -o crt_entry.exe */
#include <stdio.h>

int main(void) { printf("main reached\n"); return 0; }
```

## 1.2 MinGW：从 PE 头的入口点一路读下来

先问可执行文件：入口在哪。

`Bash`

```bash
objdump -p crt_entry.exe | grep -E "AddressOfEntryPoint|ImageBase"
objdump -f crt_entry.exe
```

`实测数据`
`Text`

```text
AddressOfEntryPoint	00000000000013f0
ImageBase		0000000140000000

crt_entry.exe:     file format pei-x86-64
architecture: i386:x86-64, flags 0x0000013b:
HAS_RELOC, EXEC_P, HAS_DEBUG, HAS_SYMS, HAS_LOCALS, D_PAGED
start address 0x00000001400013f0
```

**入口地址是 `0x1400013f0`**（`ImageBase` 加 `AddressOfEntryPoint`）。
把它交给反汇编，看那里是谁：

`实测数据`
`Bash`

```bash
objdump -d crt_entry.exe | grep -A2 "1400013f0>:"
```

`实测数据`
`Text`

```text
00000001400013f0 <mainCRTStartup>:
   1400013f0:	48 83 ec 28          	sub    $0x28,%rsp
   1400013f4:	48 8b 05 25 30 00 00 	mov    0x3025(%rip),%rax        # 140004420 <.refptr.__mingw_app_type>
```

**`mainCRTStartup` 只做两件事**：把 `__mingw_app_type` 置 0（表示控制台程序），
然后跳到 `__tmainCRTStartup`。符号表里这一整套都在：

`实测数据`
`Bash`

```bash
nm crt_entry.exe | grep -E " (T|t) (mainCRTStartup|__tmainCRTStartup|__main|__do_global_ctors|_initterm|_initterm_e|atexit|exit|_cexit)$"
```

`实测数据`
`Text`

```text
00000001400014b0 T __do_global_ctors
0000000140001530 T __main
0000000140001010 t __tmainCRTStartup
00000001400026a0 T _cexit
00000001400026a8 T _initterm
00000001400025a0 T _initterm_e
0000000140001410 T atexit
0000000140002140 T exit
00000001400013f0 T mainCRTStartup
```

`__tmainCRTStartup` 有一百多条指令，但按调用顺序读下来只有八步（下面是节选）：

`实测数据`
`Assembly`

```asm
; ……省略了 TLS 与启动锁的部分……
   14000110e:	call   1400018e0 <_pei386_runtime_relocator>      ; 修正伪重定位
   14000111a:	call   *0x7070(%rip)                             ; SetUnhandledExceptionFilter
   140001136:	call   140002160 <_fpreset>                       ; 浮点环境复位
   1400011ce:	call   140002688 <__set_app_type>
   1400011e3:	call   1400025f0 <__p__commode>                    ; 标准流的默认值
   1400011f3:	call   140001550 <_setargv>
   14000122e:	call   1400025a0 <_initterm_e>                    ; 跑 __xi_a..__xi_z
   140001271:	call   140002678 <__getmainargs>                  ; 取 argc/argv/envp
   140001358:	call   1400026a8 <_initterm>                      ; 跑 __xc_a..__xc_z
   14000135d:	call   140001530 <__main>                         ; 全局对象的构造函数
   1400010c2:	call   140001440 <main>                           ; 终于到 main
   1400013c4:	call   140002140 <exit>                           ; main 返回之后
```

八步可以归成四类：**修地址**（伪重定位）、**建环境**（异常过滤器、浮点、
标准流、argv）、**跑构造表**（`_initterm_e` 与 `_initterm`）、**进 `main` 并负责收尾**（`exit`）。

> [!IMPORTANT]
> **`main` 不是被操作系统调用的，是被 CRT 调用的。**
> `main` 的返回值也不是进程的退出码本身，而是被 `exit` 收下之后
> 才变成退出码——这也解释了为什么在 `main` 里写 `return 0` 与写 `exit(0)`
> 在可观察行为上几乎一样，而 `abort()` 完全是另一回事（第 4 节）。

## 1.3 MSVC：同一件事的另一种排法

MSVC 的入口点在 PE 头里同样是 CRT 的函数，只是名字与内部结构不同。

`C++`

```cpp
/* msvc_hello.cpp   MSVC 侧的入口点：一个最小的控制台程序
   编译：cl /nologo /std:c++17 /utf-8 /EHsc /O2 msvc_hello.cpp /Fe:msvc_hello.exe */
#include <cstdio>

int main()
{
    std::puts("main reached");
    return 0;
}
```

`Batch`

```bat
cl /nologo /std:c++17 /utf-8 /EHsc /O2 msvc_hello.cpp /Fe:msvc_hello.exe
dumpbin /headers msvc_hello.exe | findstr /C:"entry point" /C:"image base"
```

`实测数据`
`Text`

```text
            1278 entry point (0000000140001278)
       140000000 image base (0000000140000000 to 0000014001FFFF)
```

把入口地址反汇编出来，第一条同样是给栈帧留位置，接下来调进 CRT 的公共例程：

`实测数据`
`Bash`

```bash
dumpbin /disasm:nobytes msvc_hello.exe | grep -E "140001278|140001290"
```

`实测数据`
`Text`

```text
  0000000140001278: sub         rsp,28h
  0000000140001290: call        0000000140001B28
```

**同一件事，换了个函数名。** 按 MSVC 的文档，`0x140001B28` 那个例程是
`__scrt_common_main`，它内部依次调用 `_initterm_e`、`_initterm`、`main`、`exit`
（`文档`：Visual C++ 运行库的启动流程），与 MinGW 的排法一一对应：

| 步骤 | MinGW | MSVC |
|---|---|---|
| 入口点函数 | `mainCRTStartup` | `mainCRTStartup` |
| 真正干活 | `__tmainCRTStartup` | `__scrt_common_main` |
| 第一遍构造表 | `_initterm_e(__xi_a, __xi_z)` | `_initterm_e(__xi_a, __xi_z)` |
| 第二遍构造表 | `_initterm(__xc_a, __xc_z)` | `_initterm(__xc_a, __xc_z)` |
| 全局对象 | `__main` → `__do_global_ctors` | `_initterm` 扫 `.CRT$XCU` |
| 收尾 | `exit` | `exit` |

两家的第一步与最后一步用的是同一套名字，因为它们都来自同一份
Visual C++ 运行库的历史约定。真正不同的是**构造表放在哪个段**，见第 2 节。

## 1.4 裸机侧只有两层

回到单片机：那里没有 PE 头，也没有 CRT 的入口函数。
内核从 `0x00000004` 取复位向量，直接进 `Reset_Handler`，
中间不存在「链接器挑一个入口函数」这一步。

| | 通用系统（本章） | 裸机（第 7 章） |
|---|---|---|
| 第一条指令从哪来 | PE/ELF 头的入口地址，由链接器按 `main` 形态挑 | 向量表第 2 个字，硬件硬连线 |
| 参数从哪来 | CRT 调 `__getmainargs` 从系统取 | 没有参数 |
| 构造表谁读 | `__main` / `_initterm` | `__libc_init_array` |
| `main` 返回之后 | `exit` 收尾 | 无处可去，通常死循环 |
| 谁提供这些代码 | 工具链自带的 CRT 目标文件 | 同一个 CRT，静态链进镜像 |

**两边的 CRT 就是同一份 newlib 或同一份 msvcrt**，只是被裁剪与调用方式不同。
裸机上 `--specs=nosys.specs` 让 CRT 把系统调用换成空实现，
`-nostartfiles` 则干脆不要它提供的启动文件，改用芯片厂商的 `startup_*.s`。

---

# 第 2 节 构造表：全局对象在哪里被构造

**编译器没法把构造函数的调用直接写进 `main` 的第一行**，因为它不知道一共有多少个
翻译单元、每个里面有几个全局对象。它采取的办法是：把每个构造函数包装成一个
「初始化函数」，把它们的地址塞进一个数组，再由 CRT 遍历这个数组逐个调用。

## 2.1 MinGW 的 `__CTOR_LIST__` 与 `__do_global_ctors`

数组的边界由 CRT 定义，元素由编译器生成。用 1.2 小节那份 `crt_entry.c` 编译出的
可执行文件，可以直接把两样东西都读出来。

`实测数据`
`Bash`

```bash
nm crt_entry.exe | grep -E "CTOR|xc_a|xc_z"
objdump -d --disassemble=__do_global_ctors crt_entry.exe
```

`实测数据`
`Text`

```text
0000000140004580 R ___CTOR_LIST__
0000000140004580 R __CTOR_LIST__
00000001400014b0 T __do_global_ctors
00000001400045a8 R __xc_a
00000001400045b0 R __xc_z
```

`__do_global_ctors` 的全部工作就是「从表的末尾往前走，逐个调用」：

`实测数据`
`Assembly`

```asm
00000001400014b0 <__do_global_ctors>:
   1400014b0:	56                   	push   %rsi
   1400014b1:	53                   	push   %rbx
   1400014b2:	48 83 ec 28          	sub    $0x28,%rsp
   1400014b6:	48 8b 15 b3 2e 00 00 	mov    0x2eb3(%rip),%rdx      # 140004370 <.refptr.__CTOR_LIST__>
   1400014bd:	48 8b 02             	mov    (%rdx),%rax            ; 表里第一个字是元素个数
   1400014c0:	83 f8 ff             	cmp    $0xffffffff,%eax
   1400014c3:	89 c1                	mov    %eax,%ecx
   1400014c5:	74 39                	je     140001500             ; -1 表示「数不清」，去数一遍
   1400014c7:	85 c9                	test   %ecx,%ecx
   1400014c9:	74 20                	je     1400014eb             ; 空表：什么都不做
   1400014cb:	89 c8                	mov    %ecx,%eax
   1400014cd:	83 e9 01             	sub    $0x1,%ecx
   1400014d0:	48 8d 1c c2          	lea    (%rdx,%rax,8),%rbx     ; rbx = 最后一个元素
   1400014dc:	0f 1f 40 00          	nopl   0x0(%rax)
   1400014e0:	ff 13                	call   *(%rbx)                ; 调用它
   1400014e2:	48 83 eb 08          	sub    $0x8,%rbx             ; 指针往回退一格
   1400014e6:	48 39 f3             	cmp    %rsi,%rbx
   1400014e9:	75 f5                	jne    1400014e0
   1400014eb:	48 8d 0d 7e ff ff ff 	lea    -0x82(%rip),%rcx       # 140001470 <__do_global_dtors>
   1400014f8:	e9 13 ff ff ff       	jmp    140001410 <atexit>      ; 把析构登记给 atexit
```

**三件事在这一段里同时成立**：构造函数的地址存在一个数组里；
遍历是**从后往前**的；遍历结束之后，`__do_global_dtors` 被登记给 `atexit`。
第二条解释了第 3 节要讲的顺序问题，第三条解释了第 4 节的析构时机。

## 2.2 `__main` 只做一件事

`main` 的代码里被编译器插进了一次对 `__main` 的调用（第 1.2 小节的符号表里那条
`U __main` 就是它）。那个函数短得出奇：

`实测数据`
`Assembly`

```asm
0000000140001530 <__main>:
   140001530:	8b 05 fa 5a 00 00    	mov    0x5afa(%rip),%eax      # 140007030 <initialized>
   140001536:	85 c0                	test   %eax,%eax
   140001538:	74 06                	je     140001540 <__main+0x10>
   14000153a:	c3                   	ret                           ; 已经构造过：直接返回
   140001540:	c7 05 e6 5a 00 00 01 	movl   $0x1,0x5ae6(%rip)      # 140007030 <initialized>
   14000154a:	e9 61 ff ff ff       	jmp    1400014b0 <__do_global_ctors>
```

**一个静态标志加一次跳转。** 标志的作用是防止重复构造：如果某个构造函数又调用了
一个会触发 `__main` 的路径，第二次进来会直接返回。

## 2.3 MSVC 的 `.CRT$XCU` 与 `_initterm`

MSVC 不用一个大数组，而是用**按名字排序的段**。每个需要动态初始化的翻译单元，
编译器都往 `.CRT$XCU` 里放一个函数指针；链接器把所有 `.CRT$XCU` 段按名字拼起来，
`_initterm` 拿到首尾两个符号，逐个调用。

`C++`

```cpp
/* msvc_ctor_a.cpp   MSVC 侧的构造表：A 翻译单元里的全局对象
   编译：cl /nologo /std:c++17 /utf-8 /EHsc /O0 msvc_ctor_a.cpp msvc_ctor_b.cpp msvc_main.cpp /Fe:msvc_ab.exe */
#include <cstdio>

struct Init { const char *who; Init(const char *w) : who(w) { std::printf("ctor %s\n", who); } };
Init g_a("A");
```

`C++`

```cpp
/* msvc_ctor_b.cpp   MSVC 侧的构造表：B 翻译单元里的全局对象
   编译：cl /nologo /std:c++17 /utf-8 /EHsc /O0 msvc_ctor_a.cpp msvc_ctor_b.cpp msvc_main.cpp /Fe:msvc_ab.exe */
#include <cstdio>

struct Init { const char *who; Init(const char *w) : who(w) { std::printf("ctor %s\n", who); } };
Init g_b("B");
```

`C++`

```cpp
/* msvc_main.cpp   MSVC 侧的主程序
   编译：cl /nologo /std:c++17 /utf-8 /EHsc /O0 msvc_ctor_a.cpp msvc_ctor_b.cpp msvc_main.cpp /Fe:msvc_ab.exe */
#include <cstdio>

int main() { std::puts("main runs"); return 0; }
```

把目标文件交给 `dumpbin`，能看到放进去的到底是什么：

`实测数据`
`Batch`

```bat
cl /nologo /std:c++17 /utf-8 /EHsc /O0 /c msvc_ctor_a.cpp /Fo:msvc_ctor_a.obj
dumpbin /symbols msvc_ctor_a.obj | findstr /C:"XCU" /C:"dynamic initializer"
```

`实测数据`
`Text`

```text
01A 00000000 SECT5  notype ()    Static       | ??__Eg_a@@YAXXZ (void __cdecl `dynamic initializer for 'g_a''(void))
03F 00000000 SECT14 notype       Static       | .CRT$XCU
```

**符号名就是自解释的**：`dynamic initializer for 'g_a'`。
函数名被修饰成 `??__Eg_a@@YAXXZ`，`_initterm` 调用的是它的地址，
而不是名字——所以修饰与否不影响能不能跑，只影响你在符号表里认不认得出来。

MSVC 侧的构造顺序是**按命令行顺序**，与 MinGW 的反序不同：

`实测数据`
`Text`

```text
--- msvc_ab.exe，连跑两次
ctor A
ctor B
main runs
ctor A
ctor B
main runs
```

两个编译器对同一份源码给出两种顺序，这一点本身就说明「跨翻译单元的构造顺序不可依赖」。

## 2.4 裸机侧的 `__libc_init_array`

newlib 的 `__libc_init_array` 就是第 7 章 `Reset_Handler` 里 `bl` 的那个函数。
它比 MinGW 的 `__do_global_ctors` 更规整：**两个数组、两遍循环、中间夹一次 `_init`**。

`C++`

```cpp
/* arm_plain.cpp    Cortex-M 上的构造表：一个全局对象都没有的 C++ 程序
   交叉编译：arm-none-eabi-g++ -mcpu=cortex-m3 -mthumb -nostartfiles --specs=nosys.specs \
             -T STM32F103XX_FLASH.ld startup_stm32f103xe.s arm_plain.cpp -o arm_plain.elf
   （链接脚本与启动文件来自 A-教学素材/01-编译器/嵌入式/链接脚本与启动/） */
#include <cstdio>

struct Item {
    int qty;
    ~Item() { }
};

extern "C" void SystemInit(void) { }
extern "C" void _init(void) { }

int main(void)
{
    Item a{12};
    std::printf("qty = %d\n", a.qty);
    for (;;) { }
}
```

`实测数据`
`Bash`

```bash
arm-none-eabi-objdump -d --disassemble=__libc_init_array arm_plain.elf
arm-none-eabi-nm arm_plain.elf | grep -E "preinit_array|init_array"
```

`实测数据`
`Assembly`

```asm
08001830 <__libc_init_array>:
 8001830:	b570      	push	{r4, r5, r6, lr}
 8001832:	4b0f      	ldr	r3, [pc, #60]	@ (8001870)     ; __preinit_array_end
 8001834:	4d0f      	ldr	r5, [pc, #60]	@ (8001874)     ; __preinit_array_start
 8001836:	42ab      	cmp	r3, r5
 8001838:	eba3 0605 	sub.w	r6, r3, r5
 800183c:	d007      	beq.n	800184e <__libc_init_array+0x1e>
 800183e:	2400      	movs	r4, #0
 8001840:	10b6      	asrs	r6, r6, #2                              ; 元素个数 = 字节数 / 4
 8001842:	f855 3b04 	ldr.w	r3, [r5], #4                    ; 取一个函数指针
 8001846:	3401      	adds	r4, #1
 8001848:	4798      	blx	r3                                      ; 调用它
 800184a:	42a6      	cmp	r6, r4
 800184c:	d8f9      	bhi.n	8001842
 800184e:	f7fe fcdb 	bl	8000208 <_init>                        ; 第一遍跑完，调 _init
 8001852:	4d09      	ldr	r5, [pc, #36]	@ (8001878)     ; __init_array_start
 8001854:	4b09      	ldr	r3, [pc, #36]	@ (800187c)     ; __init_array_end
 8001856:	1b5e      	subs	r6, r3, r5
 8001858:	42ab      	cmp	r3, r5
 800185a:	ea4f 06a6 	mov.w	r6, r6, asr #2
 800185e:	d006      	beq.n	800186e <__libc_init_array+0x3e>
 8001860:	2400      	movs	r4, #0
 8001862:	f855 3b04 	ldr.w	r3, [r5], #4
 8001866:	3401      	adds	r4, #1
 8001868:	4798      	blx	r3                                      ; 第二遍：构造函数
 800186a:	42a6      	cmp	r6, r4
 800186c:	d8f9      	bhi.n	8001862
 800186e:	bd70      	pop	{r4, r5, r6, pc}
```

两遍循环的写法是**从前往后**，与 MinGW 的从后往前正好相反。
顺序上的差别在 3.2 小节会看到后果。

> [!IMPORTANT]
> **构造表是本板块唯一在三种环境下同名同形的东西。**
> 裸机上是 `.preinit_array` 与 `.init_array`，由链接脚本划边界
> （《07-更底层/07-链接脚本与启动代码.md》第 2.9 小节）；
> MinGW 上是 `__CTOR_LIST__` 与 `__DTOR_LIST__`；
> MSVC 上是 `.CRT$XIA`/`.CRT$XIZ` 与 `.CRT$XCU` 这些段。
> 名字不同，都是「一张函数指针表加一个遍历它的函数」。

---

# 第 3 节 跨翻译单元的构造顺序为什么不定

**同一个翻译单元内部，全局对象按定义顺序构造，这条是标准保证的。**
跨翻译单元就没有保证了：编译器各编各的，链接器把它们的构造表拼在一起，
谁在前谁在后取决于链接命令行的顺序，而标准没有规定链接器该怎么排。

## 3.1 三个翻译单元，三次运行

三个文件各放一个全局对象，第四个文件是 `main`。

`C++`

```cpp
/* order_a.cpp   构造顺序实验：A 翻译单元里的全局对象
   编译：g++ -std=c++17 -O0 order_main.cpp order_a.cpp order_b.cpp order_c.cpp -o order_abc.exe */
#include <cstdio>

struct Init { const char *who; Init(const char *w) : who(w) { std::printf("ctor %s\n", who); } };
Init g_a("A");
```

`C++`

```cpp
/* order_b.cpp   构造顺序实验：B 翻译单元里的全局对象
   编译：g++ -std=c++17 -O0 order_main.cpp order_a.cpp order_b.cpp order_c.cpp -o order_abc.exe */
#include <cstdio>

struct Init { const char *who; Init(const char *w) : who(w) { std::printf("ctor %s\n", who); } };
Init g_b("B");
```

`C++`

```cpp
/* order_c.cpp   构造顺序实验：C 翻译单元里的全局对象
   编译：g++ -std=c++17 -O0 order_main.cpp order_a.cpp order_b.cpp order_c.cpp -o order_abc.exe */
#include <cstdio>

struct Init { const char *who; Init(const char *w) : who(w) { std::printf("ctor %s\n", who); } };
Init g_c("C");
```

`C++`

```cpp
/* order_main.cpp   构造顺序实验的主程序
   编译：g++ -std=c++17 -O0 order_main.cpp order_a.cpp order_b.cpp order_c.cpp -o order_abc.exe */
#include <cstdio>

int main(void) { std::puts("main runs"); return 0; }
```

按 `A B C` 的顺序写进命令行，连跑三次：

`实测数据`
`Bash`

```bash
g++ -std=c++17 -O0 order_main.cpp order_a.cpp order_b.cpp order_c.cpp -o order_abc.exe
./order_abc.exe && ./order_abc.exe && ./order_abc.exe
```

`实测数据`
`Text`

```text
ctor C
ctor B
ctor A
main runs
ctor C
ctor B
ctor A
main runs
ctor C
ctor B
ctor A
main runs
```

**三次完全一样，但顺序是反的**：命令行里 `A` 在最前面，构造却从 `C` 开始。
原因就在 2.1 小节那段汇编里——`__do_global_ctors` 是**从表尾往表头**走的。

## 3.2 换个链接顺序，顺序就翻过来

把命令行里的顺序倒过来，构造顺序跟着倒过来：

`实测数据`
`Bash`

```bash
g++ -std=c++17 -O0 order_main.cpp order_c.cpp order_b.cpp order_a.cpp -o order_cba.exe
./order_cba.exe && ./order_cba.exe && ./order_cba.exe
```

`实测数据`
`Text`

```text
ctor A
ctor B
ctor C
main runs
ctor A
ctor B
ctor C
main runs
ctor A
ctor B
ctor C
main runs
```

符号表也能看出这件事：两个镜像里三个初始化函数的**地址高低正好相反**。

`实测数据`
`Text`

```text
--- order_abc.exe（命令行 A B C）
0000000140001495 t _GLOBAL__sub_I_g_a
00000001400014d5 t _GLOBAL__sub_I_g_b
0000000140001515 t _GLOBAL__sub_I_g_c

--- order_cba.exe（命令行 C B A）
0000000140001515 t _GLOBAL__sub_I_g_a
00000001400014d5 t _GLOBAL__sub_I_g_b
0000000140001495 t _GLOBAL__sub_I_g_c
```

同一份源码，只改命令行里文件名出现的次序，构造顺序就变了。
**换一个构建系统、加一个源文件、改一次 `CMakeLists.txt` 里的顺序，
都可能把它变掉**——而程序照样能编能跑。

## 3.3 标准怎么说

标准对这一件事的态度是「分开管」：单元内保证，单元间不保证。

`文档`

> "If V and W have ordered initialization and V is defined before W within a
> single translation unit, the initialization of V is sequenced before the
> initialization of W."
>
> —— N4659 §6.6.3/2.1

`文档`

> "Otherwise, the initializations of V and W are indeterminately sequenced."
>
> —— N4659 §6.6.3/2.4

同一节还有一条容易被忽略的规定：**动态初始化是否被推迟到 `main` 的第一条语句之后，
由实现决定**。

`文档`

> "It is implementation-defined whether the dynamic initialization of a
> non-local non-inline variable with static storage duration is sequenced
> before the first statement of main or is deferred."
>
> —— N4659 §6.6.3/4

**实测中看到的是「不推迟」**：`ctor C/B/A` 三行都出现在 `main runs` 之前。
这是本机这套 MinGW 的行为，换成别的实现（尤其是动态库里的全局对象）
可能是推迟的，那属于 `待确认` 的范围。

## 3.4 写错了会怎样：读到没构造完的对象

构造顺序不定这件事本身不会出错，**出错的是「假定它一定按自己想的顺序」**。
下面这份程序里，`Client` 的构造函数要读另一个翻译单元里的 `g_config`。

`C++`

```cpp
/* use_before_init.cpp   静态初始化顺序问题：这个对象在构造时读另一个翻译单元的全局对象
   编译：g++ -std=c++17 -O0 use_before_init.cpp provider.cpp -o ub1.exe
         g++ -std=c++17 -O0 provider.cpp use_before_init.cpp -o ub2.exe */
#include <cstdio>

struct Config { int value; Config() : value(1234) { std::printf("Config ctor\n"); } };
extern Config g_config;              /* 定义在 provider.cpp 里 */

struct Client {
    int seen;
    Client() : seen(g_config.value) { std::printf("Client ctor, g_config.value = %d\n", seen); }
};
Client g_client;

int main(void)
{
    std::printf("main: g_client.seen = %d\n", g_client.seen);
    return 0;
}
```

`C++`

```cpp
/* provider.cpp   被 use_before_init.cpp 依赖的那个全局对象
   编译（只编这一份）：g++ -std=c++17 -O0 -c provider.cpp -o provider.o */
#include <cstdio>

struct Config { int value; Config() : value(1234) { std::printf("Config ctor\n"); } };
Config g_config;
```

两个链接顺序，两次运行：

`实测数据`
`Bash`

```bash
g++ -std=c++17 -O0 use_before_init.cpp provider.cpp -o ub1.exe && ./ub1.exe
g++ -std=c++17 -O0 provider.cpp use_before_init.cpp -o ub2.exe && ./ub2.exe
```

`实测数据`
`Text`

```text
--- ub1.exe（Client 所在的文件写在前面）
Config ctor
Client ctor, g_config.value = 1234
main: g_client.seen = 1234

--- ub2.exe（Config 所在的文件写在前面）
Client ctor, g_config.value = 0
Config ctor
main: g_client.seen = 0
```

**编译通过、链接通过、运行不崩，只是数字错了。** 第二个版本里 `g_config` 的存储
已经被零初始化（这是静态初始化，一定在动态初始化之前完成），所以读出来是 `0`。
这类错误的形状是「某个值偶尔不对」，而排查方向很容易跑偏到别处去。

> [!CAUTION]
> **一个全局对象的构造函数里，不要使用另一个翻译单元的全局对象。**
> 顺序不定意味着这段代码可能对、可能错，而且换一次链接顺序就换一个结果。
> 上面这份程序两个版本只差命令行里两个文件名的先后。

## 3.5 让顺序确定的四种办法

| 办法 | 怎么做 | 代价 |
|---|---|---|
| **放在同一个翻译单元** | 保证按定义顺序构造 | 只解决一个文件内部的问题 |
| **构造时初始化（懒汉式）** | 用函数内 `static`，第一次调用时才构造 | C++11 起线程安全；多一次判断 |
| **显式初始化函数** | 不用全局对象，写 `void init_all()` 在 `main` 开头调 | 顺序由你写死；忘了调就没有初始化 |
| **`init_priority` 属性** | `__attribute__((init_priority(101)))` | 只有 GCC 系支持，跨平台会失效 |

函数内 `static` 之所以可靠，是因为它的初始化时机由标准规定：
**第一次执行到那条声明时才初始化**（N4659 §9.7），与链接顺序无关。
它的代价与线程安全细节见《05-类与面向对象/04-构造与析构.md》第 3 节。

---

# 第 4 节 `atexit`、析构与 `main` 返回之后

**`main` 返回不是进程结束，只是进程结束流程的开始。**
这一节按发生顺序把那几步排开。

## 4.1 `atexit` 的注册与调用顺序

`atexit` 登记的函数在 `exit` 里被调用，**顺序与登记顺序相反**。

`C++`

```cpp
/* atexit_lab.cpp   atexit 与全局析构的时机
   编译：g++ -std=c++17 -O0 atexit_lab.cpp -o atexit_lab.exe */
#include <cstdio>
#include <cstdlib>

struct Tracker {
    const char *name;
    Tracker(const char *n) : name(n) { std::printf("ctor  %s\n", name); }
    ~Tracker() { std::printf("dtor  %s\n", name); }
};
Tracker g_global("global object");

void h1(void) { std::puts("atexit handler 1"); }
void h2(void) { std::puts("atexit handler 2"); }
void h3(void) { std::puts("atexit handler 3"); }

int main(void)
{
    std::atexit(h1);
    std::atexit(h2);
    std::atexit(h3);
    std::puts("main returns now");
    return 0;
}
```

`实测数据`
`Bash`

```bash
g++ -std=c++17 -O0 atexit_lab.cpp -o atexit_lab.exe && ./atexit_lab.exe
```

`实测数据`
`Text`

```text
ctor  global object
main returns now
atexit handler 3
atexit handler 2
atexit handler 1
dtor  global object
```

**登记顺序 1、2、3，执行顺序 3、2、1。** 这条规则的理由很实际：
后登记的处理往往依赖先登记的那些资源，反过来执行才安全。

## 4.2 全局对象的析构也是 `atexit` 注册的

注意最后一行：全局对象的析构在三个处理函数**之后**才发生。
这不是巧合，而是 2.1 小节那段汇编的直接后果——`__do_global_ctors` 在遍历完构造表之后
才把 `__do_global_dtors` 交给 `atexit`。而三个处理函数是在 `main` 里登记的，
比它晚，按「后进先出」自然排在它前面。

构造函数那边也一样，编译器给每个全局对象生成的东西不止一个函数：

`实测数据`
`Assembly`

```asm
0000000140001540 <_GLOBAL__sub_I_g_global>:
   140001540:	55                   	push   %rbp
   140001541:	48 89 e5             	mov    %rsp,%rbp
   140001544:	48 83 ec 20          	sub    $0x20,%rsp
   140001548:	e8 bf ff ff ff       	call   14000150c <_Z41__static_initialization_and_destruction_0v>
   14000154d:	90                   	nop
   14000154e:	48 83 c4 20          	add    $0x20,%rsp
   140001552:	5d                   	pop    %rbp
   140001553:	c3                   	ret
```

名字里的 `static_initialization_and_destruction` 说明了它的职责：
**构造与登记析构是一件事**，都在这一个函数里完成。

## 4.3 `main` 返回之后到进程结束

`main` 里写 `return 0`，等价于调用 `exit(0)`。两门语言的标准都这么规定：

`文档`

> "A return statement in main has the effect of leaving the main function
> (destroying any objects with automatic storage duration) and calling
> std::exit with the return value as the argument."
>
> —— N4659 §6.6.1/5

`exit` 按顺序做三件事：调用 `atexit` 登记的函数（后进先出）、
冲刷并关闭标准流、把控制权交回运行库。
而**异常终止**走的是另一条路：

`C++`

```cpp
/* exit_lab.cpp   main 返回与 abort：谁跑谁不跑
   编译：g++ -std=c++17 -O0 exit_lab.cpp -o exit_lab.exe
         g++ -std=c++17 -O0 -DUSE_ABORT exit_lab.cpp -o exit_abort.exe */
#include <cstdio>
#include <cstdlib>

struct Tracker {
    ~Tracker() { std::puts("global object destroyed"); }
};
Tracker g_tracker;

void bye(void) { std::puts("atexit handler ran"); }

int main(void)
{
    std::atexit(bye);
#if defined(USE_ABORT)
    std::puts("calling abort()");
    std::abort();                 /* 异常终止：atexit 与全局析构都不跑 */
#else
    std::puts("returning from main");
    return 0;                     /* 等价于 exit(0)：两者都会跑 */
#endif
}
```

`实测数据`
`Bash`

```bash
g++ -std=c++17 -O0 exit_lab.cpp -o exit_lab.exe && ./exit_lab.exe
g++ -std=c++17 -O0 -DUSE_ABORT exit_lab.cpp -o exit_abort.exe && ./exit_abort.exe
```

`实测数据`
`Text`

```text
--- 正常返回（退出码 0）
returning from main
atexit handler ran
global object destroyed

--- abort（退出码 3）
calling abort()
```

**`abort` 一个都不跑。** 缓冲区里的数据不冲刷、临时文件不删、
析构函数不执行。写嵌入式或服务端程序时，把「正常退出」与「异常终止」
当成同一件事处理，丢的往往就是这一步的内容。

## 4.4 静态库里的构造函数

构造函数能不能跑，前提是**它在不在最终的可执行文件里**。
静态库按成员加载，没人引用的成员整个都不会进镜像，它里面的全局对象自然也不会被构造。

`C++`

```cpp
/* libctor.cpp   静态库里的全局对象：被引用才构造
   编译（只编这一份）：g++ -std=c++17 -O0 -c libctor.cpp -o libctor.o */
#include <cstdio>

struct Reg { Reg() { std::puts("libctor: global object constructed"); } };
Reg g_reg;

int lib_answer(void) { return 42; }
```

`C++`

```cpp
/* libmain.cpp   引用库里的一个函数，看构造函数会不会跟着进来
   编译：ar rcs libctor.a libctor.o
         g++ -std=c++17 -O0 libmain.cpp libctor.a -o lib_demo.exe */
#include <cstdio>

int lib_answer(void);

int main(void)
{
    std::printf("main: lib_answer() = %d\n", lib_answer());
    return 0;
}
```

`实测数据`
`Bash`

```bash
ar rcs libctor.a libctor.o
g++ -std=c++17 -O0 libmain.cpp libctor.a -o lib_demo.exe && ./lib_demo.exe
```

`实测数据`
`Text`

```text
libctor: global object constructed
main: lib_answer() = 42
```

这里 `lib_answer` 被引用了，所以整个成员进了镜像，构造函数跟着进来。
**反过来，如果那个成员里没有任何被引用的符号，构造函数也不会执行**——
机制与报错原文见《07-更底层/06-符号与链接属性.md》第 6.1 小节，
这里只记结论：**「注册用的全局对象」必须保证它所在的成员被引用到**。

---

# 第 5 节 异常的运行时支持与体积代价

**异常是 C++ 里唯一「不用也要付钱」的机制吗。** 这句话只对一半：
它的设计目标恰恰是「不抛出就不花运行时间」，但静态空间确实要花。
这一节把花在哪里、花多少，用三个平台的数字说清楚。

## 5.1 零开销异常的思路

传统做法（很多早期编译器用过）是给每个函数在入口登记一个「我在栈上的位置」的记录，
返回时注销，抛出时沿着这条链往回找处理者。**这个登记动作在正常路径上也要执行**，
所以不用异常也要付时间。

现在通行的做法叫**表驱动**，也叫零开销异常：

| 做法 | 正常执行时 | 抛出时 |
|---|---|---|
| 运行时登记链 | 每次进出函数都要改链表 | 顺着链表找处理者 |
| **表驱动（现代做法）** | **什么都不做** | 查表：由返回地址查出当前函数的展开信息 |

「零开销」指的是**正常路径零开销**，代价挪到了两处：
编译器要在每个可能抛出的函数旁边生成一条展开记录，
链接器要把一套展开器（读表、恢复寄存器、找 `catch`）链进镜像。

## 5.2 展开表在二进制里长什么样

展开记录放在专门的段里，各平台的段名不同，内容都是「哪个地址范围、
按什么规则恢复现场、有没有 `catch`」。

| 平台 | 段名 | 查表用的边界符号 | 谁读它 |
|---|---|---|---|
| Linux / ELF（x86-64） | `.eh_frame`、`.gcc_except_table` | `__eh_frame_start` 等 | `libgcc_s` 的展开器 |
| 裸机 Cortex-M（ELF） | `.ARM.exidx`、`.ARM.extab` | `__exidx_start` / `__exidx_end` | newlib 的 `_Unwind_*` |
| Windows（PE） | `.pdata`、`.xdata` | PE 异常目录 | 系统的 SEH 与 `__gxx_personality_seh0` |

**裸机上的边界符号是链接脚本给的**，第 7 章抄过那一段
（《07-更底层/07-链接脚本与启动代码.md》第 2.10 小节）：

`Linker Script`

```ld
  .ARM (READONLY) :
  {
    . = ALIGN(4);
    __exidx_start = .;
    *(.ARM.exidx*)
    __exidx_end = .;
    . = ALIGN(4);
  } >FLASH
```

（上面是 `STM32F103XX_FLASH.ld` 第 112 至 119 行的节选。）
`__exidx_start` 与 `__exidx_end` 就是展开器遍历这张表的起点与终点。

**表是编译器为每个函数生成的，与写不写 `try` 无关。** MSVC 的目标文件里能看到
三样东西成对出现：函数本身、展开信息、以及指向展开信息的 `.pdata` 条目。

`实测数据`
`Batch`

```bat
cl /nologo /std:c++17 /utf-8 /EHsc /O2 /c msvc_hello.cpp /Fo:msvc_hello.obj
dumpbin /symbols msvc_hello.obj | findstr /C:"main" /C:"unwind" /C:"pdata"
```

`实测数据`
`Text`

```text
00A 00000000 SECT3  notype ()    External     | main
00E 00000000 SECT4  notype       Static       | $unwind$main
011 00000000 SECT5  notype       Static       | $pdata$main
014 00000000 SECT6  notype       External     | ??_C@_0N@PHDLGBKO@main?5reached@ (`string')
```

`main` 里一个 `try` 都没有，`$unwind$main` 与 `$pdata$main` 照样生成了。
它们的作用不是处理异常，而是**让任何一次栈展开都能正确地恢复现场**——
包括 `catch`，也包括调试器与 `backtrace`。

## 5.3 一个数字：展开器本身有多大

要量出「异常花了多少」，最干净的做法是同一份源码编两次，只差一个开关。
源码里**不写 `throw`、不写 `catch`**，这样两次都能编过——
用的就是 2.4 小节那份 `arm_plain.cpp`（完整源码在上），两条编译命令都写在它的首行注释里。

`实测数据`
`Bash`

```bash
arm-none-eabi-size arm_plain.elf arm_plain_noexc.elf
arm-none-eabi-objdump -h arm_plain.elf | grep -E "ARM.extab| \.ARM "
arm-none-eabi-objdump -h arm_plain_noexc.elf | grep -E "ARM.extab| \.ARM "
```

`实测数据`
`Text`

```text
--- 带异常（默认）
   text	   data	    bss	    dec	    hex	filename
  39016	   1724	   1952	  42692	   a6c4	arm_plain.elf
  3 .ARM.extab    000000c4  0800961c  0800961c  0000a61c  2**2
  4 .ARM          00000188  080096e0  080096e0  0000a6e0  2**2

--- 带 -fno-exceptions -fno-rtti
   text	   data	    bss	    dec	    hex	filename
  28364	   1712	   1920	  31996	   7cfc	arm_plain_noexc.elf
  3 .ARM.extab    00000000  08006ec8  08006ec8  00007ec8  2**0
  4 .ARM          00000008  08006ec8  08006ec8  00007ec8  2**2
```

**flash 侧少了 10,652 字节（`text` 从 39,016 降到 28,364，约 27%），
`.ARM.extab` 直接变成 0。** 省下来的不是 `try` 块，而是**整套展开器**：
带异常时 `_Unwind_RaiseException`、`_Unwind_Resume`、`_Unwind_VRS_Pop`
这些函数都被链了进来；关掉之后符号表里只剩下 `__exidx_start/__exidx_end` 两个边界。

`实测数据`
`Text`

```text
--- 带异常：展开器进来了（节选）
08000e2c T _Unwind_RaiseException
08000e50 T _Unwind_Resume
08000a48 T _Unwind_VRS_Pop
08009028 T _Unwind_GetDataRelBase

--- 不带异常：只剩两个边界符号
08006ed0 R __exidx_end
08006ec8 R __exidx_start
```

ELF 文件的体积差更大：**515,500 字节降到 301,948 字节**（含调试信息，
所以绝对数大，比例约 41%）。

## 5.4 写错了会怎样：`-fno-exceptions` 下编译带 `throw` 的代码

开关一旦打开，**源码里任何一处 `throw` 都会变成编译错误**，
连它所在的 `try` 也会跟着报第二个错：

`C++`

```cpp
/* arm_exc.cpp   带 try/catch 的 C++ 程序：看展开表被拉进来多少，以及关掉异常之后的报错
   交叉编译（带异常）：arm-none-eabi-g++ -mcpu=cortex-m3 -mthumb -nostartfiles --specs=nosys.specs \
             -T STM32F103XX_FLASH.ld startup_stm32f103xe.s arm_exc.cpp -o arm_exc.elf
   交叉编译（不带，期望失败）：同上，加 -fno-exceptions */
#include <cstdio>

struct Box { int v; Box(int x) : v(x) { } };

extern "C" void SystemInit(void) { }
extern "C" void _init(void) { }

int parse(const char *s)
{
    if (s == nullptr) { throw Box(-1); }
    int r = 0;
    for (const char *p = s; *p; ++p) { r = r * 10 + (*p - '0'); }
    return r;
}

int main(void)
{
    try { std::printf("parse = %d\n", parse("2026")); }
    catch (const Box &b) { std::printf("caught %d\n", b.v); }
    for (;;) { }
}
```

`实测数据`
`Bash`

```bash
arm-none-eabi-g++ -mcpu=cortex-m3 -mthumb -nostartfiles --specs=nosys.specs \
  -fno-exceptions -T STM32F103XX_FLASH.ld startup_stm32f103xe.s arm_exc.cpp -o arm_exc_off.elf
```

`实测数据`
`Text`

```text
arm_exc.cpp: In function 'int parse(const char*)':
arm_exc.cpp:13:37: error: exception handling disabled, use '-fexceptions' to enable
   13 |     if (s == nullptr) { throw Box(-1); }
      |                                     ^
arm_exc.cpp: In function 'int main()':
arm_exc.cpp:22:55: error: 'b' was not declared in this scope
   22 |     catch (const Box &b) { std::printf("caught %d\n", b.v); }
      |                                                       ^
```

MSVC 侧没有 `-fno-exceptions` 这个开关，对应的是 `/EHs-c-` 加
`_HAS_EXCEPTIONS=0`，效果类似：**用了 `throw` 的代码同样编不过，
而标准库会换掉一批内部实现**。

> [!WARNING]
> **真正麻烦的不是自己写的 `throw`，而是别人头文件里的。**
> `-fno-exceptions` 是全有全无的开关：只要某个第三方库里有一处 `throw`，
> 整条链路就不能用它编译。
> 用之前先确认依赖闭包，或者在工程里只对不抛异常的模块开这个开关。

## 5.5 三个平台上的三组数字

三组数据来自同一份逻辑的三个平台版本，源码都不含 `throw`（除了标出的那一组），
差别只在编译开关。

`实测数据`

| 平台 | 带异常 | 关掉异常 | 差值 |
|---|---|---|---|
| Cortex-M3（`arm-none-eabi-g++` 13.2.1，`-nostartfiles`） | `text` 39,016 / `.ARM.extab` 196 B | `text` 28,364 / `.ARM.extab` 0 B | **-10,652 字节** |
| Windows + MinGW（`g++` 15.2.0，`-O2`） | `text` 38,484 / `.pdata` 1,200 B | `text` 37,876 / `.pdata` 1,188 B | -608 字节（文件 -1,334 字节） |
| Windows + MSVC（cl 19.44，`/O2`） | `size of code` 98,304 | `size of code` 88,064 | **-10,240 字节** |

**桌面平台上差值小得多**，因为 CRT 与运行库是动态链接的，展开器不在你的镜像里；
而裸机与 MSVC 静态链接的场景下，那套代码必须整个放进镜像。

单独写 `try/catch` 的代价反而不大。下面这份程序两种写法各量了一次，
另一个版本只是把 `try` 块去掉、让 `parse(nullptr)` 直接调用（其余不动）：

`C++`

```cpp
/* exc_size.cpp   同一份源码，两种异常写法：用于 try/catch 本身的体积对照
   编译：g++ -std=c++17 -O2 exc_size.cpp -o exc_on.exe
   对照：把 main 里的 try 块去掉、只留 throw，存成 throw_only.cpp 再编一次 */
#include <cstdio>

struct Box { int v; Box(int x) : v(x) { } };

int parse(const char *s)
{
    if (s == nullptr) { throw Box(-1); }
    int r = 0;
    for (const char *p = s; *p; ++p) { r = r * 10 + (*p - '0'); }
    return r;
}

int main(void)
{
    try {
        std::printf("parse = %d\n", parse("2026"));
        std::printf("parse = %d\n", parse(nullptr));
    } catch (const Box &b) {
        std::printf("caught Box(%d)\n", b.v);
    }
    return 0;
}
```

`实测数据`
`Text`

```text
--- 运行
parse = 2026
caught Box(-1)

--- size（g++ 15.2.0，-O2）
   text	   data	    bss	    dec	    hex	filename
  36984	    288	   2944	  40216	   9d18	exc_on.exe        有 throw、有 try/catch
  36560	    272	   2944	  39776	   9b60	throw_only.exe    有 throw、没有 try/catch

--- 关掉异常
exc_size.cpp:10:37: error: exception handling disabled, use '-fexceptions' to enable
```

两者的差只有 424 字节，因为大头是**链接进来的展开器**，不是 `try` 块本身。
这一组也解释了为什么「把异常关掉能瘦身」在裸机与静态链接的工程里是常见做法，
而在用动态运行库的桌面程序里收益有限。

> [!NOTE]
> **小结**：异常的代价分两块——**静态的是展开表与展开器**（关掉开关才省得掉），
> **动态的是抛出时的查表与栈展开**（不抛就不花）。`try` 块本身几乎不花钱，
> 真正贵的是「让整个程序具备抛出能力」。
> 展开器读表的过程与 RAII 在展开时的作用，见《05-类与面向对象/06-RAII 与资源管理.md》第 2.3 小节。

---

# 第 6 节 速查表

| 常用件 | 一句话用途 | 典型坑 |
|---|---|---|
| `objdump -p` / `dumpbin /headers` 的入口点 | 看程序第一条指令在哪 | 它指向 CRT，不是 `main` |
| `mainCRTStartup` | PE 的入口点函数，只做极少准备 | 它调用的是 `__tmainCRTStartup`，别在它上面找逻辑 |
| `__tmainCRTStartup` | 初始化环境、跑构造表、调 `main`、调 `exit` | 调试时跳过它会让全局对象没构造 |
| `__main` | MinGW 上遍历 `__CTOR_LIST__` 的入口 | 一个静态标志加一次跳转，不是 `main` 的别名 |
| `_initterm` / `_initterm_e` | MSVC 上遍历 `.CRT$XI*` 与 `.CRT$XCU` | 名字里的 `_e` 版本会检查返回值 |
| `__do_global_ctors` | 从表尾往表头调用构造函数 | **反序**是构造顺序看起来「倒过来」的原因 |
| `__libc_init_array` | newlib 上遍历 `.preinit_array` 与 `.init_array` | 从前往后，与 MinGW 相反 |
| `.CRT$XCU` | MSVC 放动态初始化函数指针的段 | 段名排序决定顺序，改链接顺序会影响它 |
| `_GLOBAL__sub_I_xxx` | 编译器为某个全局对象生成的初始化函数 | 名字里的 `xxx` 是被初始化的对象 |
| 构造顺序 | 同一翻译单元内按定义顺序 | **跨翻译单元不定**，取决于链接顺序 |
| 函数内 `static` | 把初始化推迟到第一次执行 | 现在是线程安全的，但多一次判断 |
| `atexit` | 登记退出处理 | **后进先出**；`abort` 下不执行 |
| `exit` / `main` 返回 | 跑 `atexit`、析构全局对象、冲刷流 | `abort` 与 `_Exit` 都不跑这些 |
| `.pdata` / `.xdata` | PE 的展开信息 | 关掉异常后它们仍然存在，只是变小 |
| `.ARM.exidx` / `.ARM.extab` | 裸机 ELF 的展开表与展开指令 | 由链接脚本划边界，`__exidx_start/end` |
| `-fno-exceptions` | 关掉异常的生成 | 源码里任何 `throw` 都会变成编译错误 |
| `-fno-rtti` | 关掉 RTTI | 与异常独立，可分别开关 |
| `__gxx_personality_seh0` | PE 上的 personality 函数 | 只在实际用到异常时才被链进来 |

**配套件**：本章的实验工程随 `07-更底层` 板块一起整理，
两个配套目录（`B-examples/07-lower-level/` 与 `C-templates/07-lower-level/`）尚在建设中，
建成后在本节补上相对链接。
裸机侧「上电到 `main`」的每一步在《07-更底层/07-链接脚本与启动代码.md》，
对象在内存里的形状在《07-更底层/09-C++ 对象布局与它的硬件代价.md》。
