# STM32 与 VS Code

> **授权**：本章节属教材文档部分，采用 [CC BY-NC-ND 4.0](../LICENSE)
> 加[附加条款](../许可附加条款.md)授权：署名、非商业、禁止演绎、不允许二次分发。
> 文中引用的代码不受此限，各自保留原有许可证，详见[版权与许可说明.md](../版权与许可说明.md)。

《01-编译器/03-嵌入式与交叉编译.md》第 9 节把一块真实的 STM32F103C8 接到本机，
用 OpenOCD 完成了识别、烧写与 semihosting 输出。那一步走通之后，
剩下的事情是把它变成一个可以长期使用的工程：源码、启动文件、链接脚本、
构建规则、烧写命令、调试配置各自放在哪里，换一块芯片或换一个人接手时要改哪几行。

本章节按动手顺序把它补齐。命令行部分在本机与这块板子上从头跑过一遍，
命令、输出与数字都是执行结果；图形界面的部分涉及所装扩展及其版本、
向导页面的选项、以及生成的文件名，这些换一台机器就是另一套结果，
因此那一部分不给统一答案，只说明每一步在做什么以及以什么为准。

工程要被反复构建，构建规则就必须写进文件而不是记在命令行里；
程序要被反复调试，调试配置就必须能被另一个人直接复用。
本章节的落点是这两件事的可复制版本：一份能编、能烧、能下断点的最小工程，
以及它在 VS Code 里的对应配置。

> **约定**：标注以行内代码单独成行——`实测数据` 表示实际执行验证过，
> 环境与复现命令见附录 A；`文档` 表示引自标准或官方资料，出处写在引用下方；
> `待确认` 表示尚未验证，正文会写明卡在哪一步。
> **存疑**表示这一步的结论随环境而变、无法给出统一答案，正文会写明为什么。
> 路径占位符的含义见《README.md》。

---

# 本章节定位

| 项目 | 内容 |
|---|---|
| **前置知识** | 编译与链接的四个阶段，见《01-编译器/01-编译与链接.md》第 2 节；交叉编译、链接脚本与启动文件的实物，见《01-编译器/03-嵌入式与交叉编译.md》第 1 至 3 节；真板烧写与 semihosting，见同章节第 9 节；CMake 工程的组织方式与 `CMakePresets.json`，见《03-构建工具链/01-构建工具链.md》第 4 节 |
| **相邻章节** | 上承《01-编译器/03-嵌入式与交叉编译.md》，那一章讲机制，本章节讲工具怎么用；`tasks.json` 与 `launch.json` 的通用字段见《01-编译器/02-环境配置.md》第 5.2、5.3、5.7 小节；调试器的操作方式见《02-调试器/01-原理与使用.md》第 8 节 |
| **不重复讲的部分** | 链接脚本逐段、`Reset_Handler` 逐条、CRT 与 `__libc_init_array` 的机制在 `06-更底层` 板块，见《06-更底层/07-链接脚本与启动代码.md》第 2、3 节与《06-更底层/08-CRT 与程序启动.md》第 1.4 小节 |
| **三件东西的分工** | 第 1 节 |
| **一个工程需要哪些文件** | 第 2 节 |
| **只用手敲命令：构建** | 第 3 节 |
| **只用手敲命令：烧写与观察** | 第 4 节 |
| **只用手敲命令：下断点调试** | 第 5 节 |
| **让 CubeMX 在无界面模式下生成工程** | 第 6 节 |
| **搬进 VS Code** | 第 7 节 |
| **报错原文与处置** | 第 8 节 |
| **换一块板要改什么（G431 实测）** | 第 8.10 小节 |
| **与机制章节的对应关系** | 第 9 节 |

---

# 第 1 节 三件东西各管什么

## 1.1 图形配置：STM32CubeMX

STM32CubeMX 负责回答「这块芯片上电之后要变成什么样」：选型号、开时钟源、
配引脚与外设、设定中断优先级，然后把答案写成两类产物——一份配置文件
（`.ioc`）和一套初始化代码。`.ioc` 是配置的唯一真相，
代码是它的投影；改配置要改 `.ioc`，改业务逻辑才改代码。

配置与代码的这种关系决定了工作方式：**能重新生成的文件不要手改**。
CubeMX 重新生成时会覆盖它自己管理的部分，只保留 `USER CODE` 区块内的内容，
规则见《01-编译器/03-嵌入式与交叉编译.md》第 7.2 小节。
判断一个文件属于哪一类，看文件头的 `@author` 标注，见
《01-编译器/03-嵌入式与交叉编译.md》第 7.4 小节。

CubeMX 不是必须的。第 3 至 5 节的最小工程里没有一行代码来自 CubeMX，
启动文件与链接脚本是从 ST 的固件包里取来的两个文件。
CubeMX 解决的是「外设很多、配置项很杂」时的效率问题，
不用它并不会缺少什么，用它的代价是要接受一套重新生成的规则。

`文档`

> "To facilitate its integration with other tools, STM32CubeMX provides
> command-line modes."
>
> —— UM1718 Rev 48 §3.3.2（STM32CubeMX 用户手册，
> 随 CubeMX 安装在 `help` 目录下）

这条命令行的细节见第 6 节，那里用的是 `-q` 模式：不给界面，只吃一份脚本。

## 1.2 命令行工具集：CubeCLT 与新版扩展里的替代品

CubeCLT 是 ST 打包的一套命令行工具，把构建与调试要用的程序放进同一个安装目录
（完整清单以 ST 下载页面的说明为准，见附录 B）。它解决的是
「Windows 上没有一套现成的 ARM 工具链」这个问题：
第 3 节的构建命令默认在 WSL 里执行，正是因为本机的 Windows 侧没有这些程序。
《01-编译器/03-嵌入式与交叉编译.md》第 7.5.1 小节登记过它作为扩展运行前提时的版本要求。

`文档`

> "**CLI tools**: A new **bundle manager** is enabling VS Code to download
> required CLI tools. This replaces the all-in-one *STM32CubeCLT* package."
>
> —— STM32CubeIDE for Visual Studio Code 扩展页面（版本 3.11.0）

`文档`

> "A new ST provided Debug Adapter Protocol (DAP) implementation supporting
> ST-LINK and SEGGER J-Link probes"
>
> —— 同上

两条放在一起看，得到的是与《01-编译器/03-嵌入式与交叉编译.md》第 7.5 节
略有出入的一幅图景：**扩展在 3.x 版本上换了架构**，CLT 从「必须先自己装好的
工具包」变成「由 bundle manager 下载的组件」，调试后端也换成了 ST 自己实现的
DAP。这类信息随版本变化的速度快于教材的修订速度，因此这里只登记结论，
具体到某一台机器上以扩展自己的说明为准（原因见第 7.3 小节）。

对本机这块板子还有一条实际影响：ST 扩展自带的 DAP 面向 ST-LINK 与 J-Link，
而这块板子用的是 CMSIS-DAP 调试器。要用它，走的是 OpenOCD 那条路，
也就是本章节第 3 至 5 节实测的同一套命令。

## 1.3 编辑器：VS Code 负责什么

VS Code 本身不编译、不烧写，它把前面两件事的命令装进任务与调试配置里，
再把调试器（这里是 GDB）的输出画成界面。它提供的三样东西是：

| 文件 | 作用 | 相当于命令行的哪一步 |
|---|---|---|
| `tasks.json` | 把构建、烧写、探测写成可重复执行的任务 | 第 3 节的 `make` / `cmake --build`，第 4 节的 OpenOCD 命令 |
| `launch.json` | 描述一次调试会话：程序在哪、调试器在哪、连到哪个 GDB 服务器 | 第 5 节的 `gdb-multiarch -x gdb.cmd` |
| `CMakePresets.json` | 配置阶段的参数集合，VS Code 的 CMake Tools 与命令行共用同一份 | 第 3 节的 `cmake --preset debug` |

第三行是这套组合里最值得留意的一点：预设文件是 CMake 的格式，
不是 VS Code 的格式，因此它在命令行与图形界面下是同一份内容。
界面里点出来的配置，命令行下能用同样的方式重现，这一点在第 3.2 小节会看到实物。

## 1.4 本机缺什么，装完长什么样

教材正文不列举某一台机器上装了什么，但这条路线对工具的依赖是结论的一部分：
少了交叉编译器就没有产物，少了 GDB 就连不上目标。下面这张表列的是
**要跑通本章节需要哪些程序、各自出现在哪一步**。

| 程序 | 出现在哪一步 | 本机的形态 |
|---|---|---|
| `arm-none-eabi-gcc` | 第 3 节构建、第 6 节构建 CubeMX 工程 | 在 WSL 的 Ubuntu 里，13.2.1 |
| `cmake` 与 `ninja` | 第 3.2 节、第 6 节 | 在 WSL 的 Ubuntu 里，3.28.3 与 1.11.1 |
| `openocd` | 第 4 节烧写、第 5 节调试、第 7 节任务 | 在 Windows 侧，0.12.0 |
| `arm-none-eabi-gdb` 或 `gdb-multiarch` | 第 5 节调试 | `gdb-multiarch` 在 WSL 里，15.1；**Windows 侧没有 ARM 版 GDB**，原因见第 8.8 小节 |
| CubeMX | 第 6 节生成工程 | 已装，6.15.0 |
| VS Code 及其扩展 | 第 7 节 | 已装 VS Code 与 C/C++ 扩展；**本章节用到的嵌入式调试扩展未装**，因此第 7 节只给配置与依据，不给操作截图 |

**安装步骤属于图形界面流程**：安装程序里的选项、扩展市场里的搜索结果、
向导页面的字段都随版本与语言变化，本教材不给统一答案。
读者以本机安装完成后 `--version` 的输出、以及扩展自己生成的配置文件为准；
装完之后能跑通第 3 至 5 节的命令，就说明工具链这一层是完整的。
第 7.3 小节会把这条路线里「随环境而变」的部分逐条列出来。

> [!IMPORTANT]
> **本章节可以只用命令行走完。** 第 3 至 6 节的每一条命令都在本机执行过，
> 输出抄自真实运行结果；VS Code 只是把这些命令装进两个 JSON 文件，
> 它不引入新的构建或调试机制。

## 1.5 三条路线共用地基

命令行、命令行加脚本、VS Code，这三条路线共用同一份工程文件与同一套 OpenOCD 配置，
区别只在「谁把命令打出来」。因此第 3 至 5 节的结果是后两条路线的地基：
命令行跑不通时，换到图形界面里也不会通。

---

# 第 2 节 一个最小工程需要哪些文件

## 2.1 文件清单

一个能被编译、烧写、调试的最小 STM32 工程需要六类文件。
少于这个数量就跑不起来，多于这个数量通常是为了组织更大的代码。

| 文件 | 必需 | 作用 | 从哪来 |
|---|---|---|---|
| `main.c` 等源码 | 是 | 业务逻辑与 `main` | 自己写 |
| `startup_stm32f103xe.s` | 是 | 向量表、`Reset_Handler`、默认中断处理函数 | ST 固件包，或 CubeMX 生成 |
| `STM32F103C8_FLASH.ld` | 是 | `MEMORY` 与 `SECTIONS`：flash 与 RAM 的大小、段怎么摆 | 自己改自 CubeMX 生成的模板 |
| `Makefile` 或 `CMakeLists.txt` | 是 | 把编译与链接的参数固定下来 | 自己写 |
| `cmake/gcc-arm-none-eabi.cmake` | 用 CMake 时必需 | 告诉 CMake 用哪套工具、目标是什么 | 自己写，或 CubeMX 生成 |
| `CMakePresets.json` | 否 | 配置参数的集合，命令行与 VS Code 共用 | 自己写，或 CubeMX 生成 |
| `tasks.json` / `launch.json` | 用 VS Code 时 | 把上面的命令装进界面 | 见第 7 节 |

`startup_stm32f103xe.s` 与 `.ld` 这两个文件的内容在
《06-更底层/07-链接脚本与启动代码.md》第 2、3 节已经逐段讲过，
本章节只用不改，因此不重复它的内容。**唯一必须核对的数字是内存大小**：
`MEMORY` 块里 flash 与 RAM 的 `LENGTH` 必须与芯片一致。
写大了链接器不报错，但复位后的第一次压栈就会越界，
现场与成因见《01-编译器/03-嵌入式与交叉编译.md》第 9.6 小节。

## 2.2 目录结构

下面这套结构是第 3 至 6 节实际使用的结构，命令行与 VS Code 都按它组织。

`Text`

```text
工程根目录/
├── main.c                       业务代码
├── startup_stm32f103xe.s        启动文件（向量表与 Reset_Handler）
├── STM32F103C8_FLASH.ld         链接脚本（20 KiB RAM / 64 KiB flash）
├── Makefile                     路线一：make
├── CMakeLists.txt               路线二：CMake
├── CMakePresets.json            路线二的参数集合
├── cmake/
│   └── gcc-arm-none-eabi.cmake  交叉编译工具链描述
├── gdb.cmd                      GDB 命令脚本（第 5 节）
└── build/                       产物目录，可整个删掉
    ├── main.elf                 make 的产物
    └── debug/
        └── stm32f103c8.elf      CMake 的产物
```

`build/` 是可再生的：删掉它，两条构建路线都能重新产出同样的文件。
把可再生的东西与手写的文件放在同一层，是这类小工程里最容易出问题的地方，
因为它让人分不清哪个文件改坏了要重写、哪个文件删了没关系。

## 2.3 三条获取启动文件与链接脚本的途径

| 途径 | 做法 | 特点 |
|---|---|---|
| ST 固件包 | 从 `STM32Cube_FW_F1_V*` 里取 `startup_stm32f103xe.s`，链接脚本按芯片改 `LENGTH` | 文件固定不变，适合长期维护；本章节用这一条 |
| CubeMX 生成 | 让 CubeMX 按型号生成，见第 6 节 | 型号对应的内存大小自动写对，改配置后重新生成 |
| 教材素材副本 | `A-教学素材/01-编译器/嵌入式/链接脚本与启动/` | 与《01-编译器/03-嵌入式与交叉编译.md》第 8.1 小节引用的同一份，保留原始版权头 |

三条途径得到的启动文件差别不大，选哪一条不影响后面的命令。
**决定成败的是 `LENGTH` 与芯片是否一致**，而这一点只有把程序烧进去才能确认，
判断方法在第 4.2 小节。

> [!NOTE]
> 到这里，工程需要哪些文件、每个文件从哪来已经确定。后面三节按
> 构建、烧写、调试的顺序把命令走一遍，每一条都在本机执行过。

---

# 第 3 节 命令行路线（一）：构建

## 3.1 工具链放在哪一侧

这块板子的编译在 WSL 里做，烧写与调试在 Windows 侧做。这个分工不是设计出来的，
而是本机工具分布的结果：`arm-none-eabi-gcc` 装在 WSL 的发行版里，
`openocd.exe` 是 Windows 程序。两边通过文件系统互通——WSL 里访问同一份源码，
路径是 `/mnt/k/<工作区>/...`，Windows 侧看到的是 `K:\<工作区>\...`。

**工作路径含中文不影响编译**：第 3 至 6 节的工程都建在中文目录下，
`arm-none-eabi-gcc`、CMake、Ninja 与 GDB 都能正常处理。
受影响的是 OpenOCD 打印出来的路径，见第 8.7 小节。

`Bash`

```bash
# 确认工具链在哪个环境里。默认发行版可能没有它，因此显式指定发行版名
wsl -d <发行版名> -e bash -lc "arm-none-eabi-gcc --version | head -1"
```

`实测数据`
`Text`

```text
arm-none-eabi-gcc (15:13.2.rel1-2) 13.2.1 20231009
```

## 3.2 路线一：Makefile

先看源码。它是本章节的调试对象：用 semihosting 打印，留一个循环供单步，
再留一个 RAM 里的全局量供调试器直接读。

`C`

```c
/* main.c    STM32F103C8 最小工程：semihosting 打印 + 一个可下断点的函数

   编译（WSL Ubuntu，arm-none-eabi-gcc 13.2.1）：
     arm-none-eabi-gcc -mcpu=cortex-m3 -mthumb -O0 -g3 -nostartfiles \
       --specs=nosys.specs -T STM32F103C8_FLASH.ld startup_stm32f103xe.s \
       main.c -o build/main.elf
*/
#include <stdint.h>

/* 启动文件的 Reset_Handler 会调用这两个函数，而 -nostartfiles 之后没有别人提供它们：
     SystemInit()  CubeMX 工程里由 Core/Src/system_stm32f1xx.c 提供，负责时钟与向量表
     _init()       平时由 C 运行库的 crti.o 提供，-nostartfiles 把它一起去掉了
   缺了它们，链接会停在 undefined reference */
void SystemInit(void) { }
void _init(void) { }

/* 放在 RAM 里的全局量：调试器可以直接按地址读它，不必下断点 */
volatile uint32_t g_total;

/* semihosting 的 SYS_WRITE0：r0 = 功能号 4，r1 = 以 0 结尾的字符串 */
static void sh_puts(const char *s)
{
    register uint32_t r0 __asm__("r0") = 4;
    register uint32_t r1 __asm__("r1") = (uint32_t)s;
    __asm__ volatile ("bkpt 0xAB" : "+r"(r0) : "r"(r1) : "memory");
}

/* 求 1..n 的和：留一个循环，便于单步并观察 i 与 acc */
static uint32_t sum_to(uint32_t n)
{
    uint32_t acc = 0;
    for (uint32_t i = 1; i <= n; ++i) {
        acc += i;
    }
    return acc;
}

int main(void)
{
    sh_puts("boot ok\n");
    for (uint32_t round = 0; round < 3; ++round) {
        g_total = sum_to(100);      /* 断点在这三圈里各命中一次 */
        sh_puts("tick\n");
    }
    sh_puts("done\n");
    for (;;) { }                    /* 裸机上 main 不返回 */
}
```

**这份源码同时是第 4 节与第 5 节的被测对象**：第 4 节看它的 semihosting 输出，
第 5 节在 `sum_to` 与 `main` 上下断点。它没有用到 HAL，
因此不需要 CubeMX 参与；用的是 CubeMX 也没有坏处，第 6 节会给出对照。

接下来把编译与链接的参数写进 `Makefile`。规则只有四条：
源码与启动文件各编一个目标文件，链接成 `.elf`，另外给两个方便目标
（看体积、生成 hex）。

`Makefile`

```makefile
# Makefile    STM32F103C8 最小工程的构建规则
#
#   make            编译出 build/main.elf
#   make size       显示 flash 与 RAM 占用
#   make hex        额外生成 build/main.hex
#   make clean      删除 build 目录

TARGET   := main
BUILD    := build
LDSCRIPT := STM32F103C8_FLASH.ld
STARTUP  := startup_stm32f103xe.s
OBJS     := $(BUILD)/main.o $(BUILD)/startup_stm32f103xe.o

CC       := arm-none-eabi-gcc
OBJCOPY  := arm-none-eabi-objcopy
SIZE     := arm-none-eabi-size

CPU      := -mcpu=cortex-m3 -mthumb
CFLAGS   := $(CPU) -O0 -g3 -Wall -ffunction-sections -fdata-sections
LDFLAGS  := $(CPU) -nostartfiles --specs=nosys.specs -T $(LDSCRIPT) \
            -Wl,--gc-sections -Wl,-Map=$(BUILD)/$(TARGET).map

.PHONY: all size hex clean

all: $(BUILD)/$(TARGET).elf

$(BUILD)/$(TARGET).elf: $(OBJS) $(LDSCRIPT)
	$(CC) $(OBJS) $(LDFLAGS) -o $@

$(BUILD)/main.o: main.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/startup_stm32f103xe.o: $(STARTUP) | $(BUILD)
	$(CC) $(CPU) -c $< -o $@

$(BUILD):
	mkdir -p $(BUILD)

size: all
	$(SIZE) $(BUILD)/$(TARGET).elf

hex: all
	$(OBJCOPY) -O ihex $(BUILD)/$(TARGET).elf $(BUILD)/$(TARGET).hex

clean:
	rm -rf $(BUILD)
```

几个参数决定了这个工程能不能跑起来：

| 参数 | 作用 | 省掉会怎样 |
|---|---|---|
| `-mcpu=cortex-m3 -mthumb` | 选内核与指令集 | 编出来的指令内核不认，或落在 ARM 态 |
| `-nostartfiles` | 不用工具链自带的启动代码 | 与 `startup_*.s` 里的 `Reset_Handler` 冲突，或入口点不对 |
| `--specs=nosys.specs` | 提供空的系统调用桩 | 链接停在 `undefined reference to '_sbrk'`，见第 8.4 小节 |
| `-T STM32F103C8_FLASH.ld` | 指定内存地图 | 用工具链默认布局，地址全错 |
| `-ffunction-sections -fdata-sections` 与 `-Wl,--gc-sections` | 每个函数单独成段，链接时丢掉没用的 | 镜像变大，功能不受影响 |
| `-g3` | 生成调试信息 | GDB 找不到源码与行号 |

`实测数据`
`Bash`

```bash
# 在 WSL 里执行；工作目录是工程根目录
make
```

`实测数据`
`Text`

```text
mkdir -p build
arm-none-eabi-gcc -mcpu=cortex-m3 -mthumb -O0 -g3 -Wall -ffunction-sections -fdata-sections -c main.c -o build/main.o
arm-none-eabi-gcc -mcpu=cortex-m3 -mthumb -c startup_stm32f103xe.s -o build/startup_stm32f103xe.o
arm-none-eabi-gcc build/main.o build/startup_stm32f103xe.o -mcpu=cortex-m3 -mthumb -nostartfiles --specs=nosys.specs -T STM32F103C8_FLASH.ld -Wl,--gc-sections -Wl,-Map=build/main.map -o build/main.elf
make exit = 0
-rwxrwxrwx 1 root root 30760 Oct  1  2026 build/main.elf
   text	   data	    bss	    dec	    hex	filename
    848	      0	   1544	   2392	    958	build/main.elf
```

**`text` 是烧进 flash 的部分，`bss` 是上电后清零的 RAM 部分。**
`data` 为 0 是因为这份源码里没有「有初值的全局量」——
`g_total` 没有初值，落在 `.bss`；`.data` 的搬运代码仍然在，
它在第 3.4 小节的反汇编里。

## 3.3 路线二：CMake 与 Ninja

同一个工程换成 CMake，需要三个文件：目标描述、工具链描述、参数集合。
分成三个文件的理由是各自回答不同的问题：
`CMakeLists.txt` 回答「编什么」，工具链文件回答「用什么编」，
预设文件回答「这次配置用哪些参数」。

`CMake`

```cmake
# CMakeLists.txt    STM32F103C8 最小工程的 CMake 描述
#
# 配置（在 WSL Ubuntu 里，cmake 3.28.3 + ninja 1.11.1）：
#   cmake --preset debug
# 构建：
#   cmake --build --preset debug

cmake_minimum_required(VERSION 3.22)

# 工具链文件必须在 project() 之前生效：由 CMakePresets.json 的
# toolchainFile 字段指定，或在命令行加 -DCMAKE_TOOLCHAIN_FILE=...
project(stm32f103c8 LANGUAGES C ASM)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(${PROJECT_NAME}
  main.c
  startup_stm32f103xe.s
)

target_compile_definitions(${PROJECT_NAME} PRIVATE STM32F103xB)

# 预处理与编译选项：与 Makefile 版保持一致，改一处要改两处
target_compile_options(${PROJECT_NAME} PRIVATE
  -mcpu=cortex-m3
  -mthumb
  -O0
  -g3
  -Wall
  -ffunction-sections
  -fdata-sections
  --specs=nosys.specs
)

# 链接选项：-nostartfiles 去掉工具链自带的启动代码，入口由 startup_*.s 提供
target_link_options(${PROJECT_NAME} PRIVATE
  -mcpu=cortex-m3
  -mthumb
  -nostartfiles
  --specs=nosys.specs
  -T${CMAKE_CURRENT_SOURCE_DIR}/STM32F103C8_FLASH.ld
  -Wl,--gc-sections
  -Wl,-Map=${PROJECT_NAME}.map
)

# 产物统一带 .elf 后缀：默认的可执行文件后缀在 Windows 上是 .exe，
# 交叉编译时不希望它跟着平台变
set_target_properties(${PROJECT_NAME} PROPERTIES SUFFIX ".elf")

# 构建完成后自动生成 hex 与体积报告，省掉两条手工命令
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
  COMMAND ${CMAKE_OBJCOPY} -O ihex $<TARGET_FILE:${PROJECT_NAME}> ${PROJECT_NAME}.hex
  COMMAND ${CMAKE_SIZE} $<TARGET_FILE:${PROJECT_NAME}>
  COMMENT "生成 hex 并报告体积"
)
```

`CMake`

```cmake
# cmake/gcc-arm-none-eabi.cmake    交叉编译工具链描述
#
# 这个文件只回答一个问题：用哪一套工具、目标是什么。
# 它由 CMakePresets.json 的 toolchainFile 字段引入，必须早于 project() 生效。

# 目标系统：Generic 表示「没有操作系统的裸机」，CMake 因此不会去找主机上的 libc
set(CMAKE_SYSTEM_NAME      Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)

# 工具名统一加前缀。CubeCLT、xpack、发行版包管理器装出来的都叫这个名字，
# 装在别处时把目录加进 PATH，或在这里写绝对路径
set(TOOLCHAIN_PREFIX arm-none-eabi-)

set(CMAKE_C_COMPILER   ${TOOLCHAIN_PREFIX}gcc)
set(CMAKE_ASM_COMPILER ${TOOLCHAIN_PREFIX}gcc)
set(CMAKE_CXX_COMPILER ${TOOLCHAIN_PREFIX}g++)
set(CMAKE_AR           ${TOOLCHAIN_PREFIX}ar)
set(CMAKE_OBJCOPY      ${TOOLCHAIN_PREFIX}objcopy)
set(CMAKE_SIZE         ${TOOLCHAIN_PREFIX}size)
set(CMAKE_GDB          ${TOOLCHAIN_PREFIX}gdb)

# 交叉编译出的可执行文件在主机上跑不起来，CMake 的编译器自检会因此误判失败；
# 改成只编译成静态库，自检就不会走到链接与运行那一步
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

# 主机上找程序（如 ninja），目标侧找头文件与库，两边不混
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
```

`JSONC`

```jsonc
{
  // CMakePresets.json    一次 configure 的全部参数
  // version 3 需要 CMake 3.21 及以上（本机 3.28.3）
  "version": 3,
  "cmakeMinimumRequired": { "major": 3, "minor": 22, "patch": 0 },
  "configurePresets": [
    {
      "name": "debug",
      "displayName": "Debug · Ninja · arm-none-eabi-gcc",
      // binaryDir 用 ${sourceDir} 展开，写死绝对路径会让工程无法移植
      "binaryDir": "${sourceDir}/build/debug",
      "generator": "Ninja",
      // 交叉编译的全部信息都在这一个文件里，命令行因此不必再写 -D 参数
      "toolchainFile": "${sourceDir}/cmake/gcc-arm-none-eabi.cmake",
      "cacheVariables": {
        "CMAKE_BUILD_TYPE": "Debug"
      }
    },
    {
      "name": "release",
      "inherits": "debug",
      "displayName": "Release · Ninja · arm-none-eabi-gcc",
      "binaryDir": "${sourceDir}/build/release",
      // 发布版按体积优先：嵌入式上 flash 比速度更紧张
      "cacheVariables": {
        "CMAKE_BUILD_TYPE": "Release",
        "CMAKE_C_FLAGS_RELEASE": "-Os -g0"
      }
    }
  ],
  "buildPresets": [
    { "name": "debug", "configurePreset": "debug" },
    { "name": "release", "configurePreset": "release" }
  ]
}
```

预设文件里每个字段的作用：

| 字段 | 作用 | 备注 |
|---|---|---|
| `version` | 预设文件格式版本 | 3 对应 CMake 3.21 及以上 |
| `configurePresets[].name` | 预设名，供 `--preset` 使用 | 命令行与 VS Code 的 CMake Tools 用同一个名字 |
| `binaryDir` | 配置产物目录 | 用 `${sourceDir}` 展开，避免绝对路径 |
| `generator` | 构建执行器 | `Ninja` 走《03-构建工具链/02-构建执行器.md》第 4 节讲的执行器 |
| `toolchainFile` | 工具链描述文件 | **它必须在这里给出**，否则 CMake 用主机的编译器 |
| `cacheVariables` | 传给 CMake 的缓存变量 | `CMAKE_BUILD_TYPE` 决定优化与调试信息 |
| `inherits` | 继承另一个预设 | 减少重复，`release` 只覆盖差异部分 |
| `buildPresets` | 把构建命令也固定下来 | 于是 `cmake --build --preset debug` 不必再写目录 |

`实测数据`
`Bash`

```bash
# 配置与构建，两条命令都只引用预设名
cmake --preset debug
cmake --build --preset debug
```

`实测数据`
`Text`

```text
-- The C compiler identification is GNU 13.2.1
-- The ASM compiler identification is GNU
-- Found assembler: /usr/bin/arm-none-eabi-gcc
-- Configuring done (5.1s)
-- Generating done (0.0s)
-- Build files have been written to: /mnt/k/<工作区>/build/debug
[1/3] Building ASM object CMakeFiles/stm32f103c8.dir/startup_stm32f103xe.s.obj
[2/3] Building C object CMakeFiles/stm32f103c8.dir/main.c.obj
[3/3] Linking C executable stm32f103c8.elf
   text	   data	    bss	    dec	    hex	filename
    848	      0	   1544	   2392	    958	/mnt/k/<工作区>/build/debug/stm32f103c8.elf
cmake exit = 0
```

`Found assembler: /usr/bin/arm-none-eabi-gcc` 这一行值得看一眼：
启动文件是汇编，CMake 需要知道用哪个汇编器，工具链文件里的
`CMAKE_ASM_COMPILER` 把答案给了它。

## 3.4 两条路线的产物对照

`实测数据`

| 路线 | 产物 | text | data | bss | 合计 |
|---|---|---|---|---|---|
| Makefile | `build/main.elf` | 848 | 0 | 1544 | 2392 |
| CMake + Ninja | `build/debug/stm32f103c8.elf` | 848 | 0 | 1544 | 2392 |

两条路线的数字完全相同，因为它们传给编译器的参数相同。
这类数字只在自己的工程里可比：换优化档、换工具链版本都会变，
`-O0` 到 `-Os` 的体积对照见《03-构建工具链/05-优化等级.md》第 3 节。

接下来确认三处与硬件直接相关的值：栈顶、全局量的地址、向量表。

`实测数据`
`Bash`

```bash
arm-none-eabi-nm build/main.elf | grep -E " (_estack|g_total|main|sum_to)$"
arm-none-eabi-objdump -s -j .isr_vector build/main.elf | head -5
```

`实测数据`
`Text`

```text
20005000 R _estack
20000000 B g_total
0800029c T main
08000266 t sum_to

build/main.elf:     file format elf32-littlearm

Contents of section .isr_vector:
 8000000 00500020 e9020008 31030008 31030008  .P. ....1...1...
```

三处读数：

| 值 | 含义 | 核对方式 |
|---|---|---|
| `_estack = 0x20005000` | 栈顶 | 应正好等于 RAM 起始地址加 RAM 大小，即 `0x20000000 + 20 KiB` |
| `g_total` 在 `0x20000000` | 第一个 RAM 变量 | 调试器可以直接按这个地址读它的值 |
| 向量表前两个字 `20005000` 与 `080002e9` | 栈顶初值与复位入口 | 第二个字的低位是 1，表示 Thumb 态 |

复位入口是 `0x080002e9`，反汇编里 `Reset_Handler` 的地址是 `0x080002e8`，
差的正是那一位。这两个数字的来历在
《06-更底层/07-链接脚本与启动代码.md》第 2.4 小节。

`Assembly`

```asm
; 下面是节选：Reset_Handler 开头的十几条指令（arm-none-eabi-objdump -d）
080002e8 <Reset_Handler>:
 80002e8:	f7ff ffa4 	bl	8000234 <SystemInit>
 80002ec:	480b      	ldr	r0, [pc, #44]	@ (800031c <LoopFillZerobss+0xe>)
 80002ee:	490c      	ldr	r1, [pc, #48]	@ (8000320 <LoopFillZerobss+0x12>)
 80002f0:	4a0c      	ldr	r2, [pc, #48]	@ (8000324 <LoopFillZerobss+0x16>)
 80002f2:	2300      	movs	r3, #0
 80002f4:	e002      	b.n	80002fc <LoopCopyDataInit>

080002f6 <CopyDataInit>:
 80002f6:	58d4      	ldr	r4, [r2, r3]
 80002f8:	50c4      	str	r4, [r0, r3]
 80002fa:	3304      	adds	r3, #4
```

**第一条就是 `bl SystemInit`**，而 `SystemInit` 在那个源文件里是空函数。
CubeMX 工程里这个函数由 `system_stm32f1xx.c` 提供，负责配置时钟，
第 6 节生成出来的工程就是那样；本章节的最小工程把它留空，
因此芯片跑在复位默认的 HSI 8 MHz 上，这一点在第 8.5 小节会被拿来做对照。

> [!NOTE]
> 构建这一层已经完成：产物是 `build/main.elf`，内存占用 2392 字节，
> 栈顶与向量表都对着芯片。后面两节把同一个文件烧进 flash 并调试它。

---

# 第 4 节 命令行路线（二）：烧写与观察

第 3 节产出的 `build/main.elf` 还在硬盘上，接下来把它写进芯片并看见它的输出。
这一步只用两个程序：OpenOCD 与那个 `.elf` 文件。

## 4.1 先让调试器把芯片认出来

OpenOCD 需要两份配置：一份描述调试器，一份描述目标芯片。
`-f` 按顺序加载它们，`-s` 指向 OpenOCD 自带的脚本目录，
`-c` 里的命令按出现顺序执行。

`PowerShell`

```powershell
# 识别：init 初始化调试器与目标，reset halt 复位后立刻停住，flash probe 0 读回 flash 描述
& "<OpenOCD>\bin\openocd.exe" -s "<OpenOCD>/share/openocd/scripts" `
  -f interface/cmsis-dap.cfg -f target/stm32f1x.cfg `
  -c "init" -c "reset halt" -c "flash probe 0"
```

`实测数据`
`Text`

```text
（下面是节选：适配器初始化的其余行与第 4.1 小节相同）
Info : CMSIS-DAP: FW Version = 1.2.0
Info : CMSIS-DAP: Serial# = 6D656D6F7279
Info : clock speed 1000 kHz
Info : SWD DPIDR 0x1ba01477
Info : [stm32f1x.cpu] Cortex-M3 r1p1 processor detected
Info : [stm32f1x.cpu] target has 6 breakpoints, 4 watchpoints
[stm32f1x.cpu] halted due to debug-request, current mode: Thread
xPSR: 0x01000000 pc: 0x080002e8 msp: 0x20005000
Info : device id = 0x20036410
Info : flash size = 64 KiB
shutdown command invoked
```

`-s` 里的路径用正斜杠，这条纪律的理由见第 8.6 小节。

三行读数各回答一个问题：

| 读数 | 回答什么 | 与工程的对应 |
|---|---|---|
| `DPIDR = 0x1ba01477` | 内核是谁 | Cortex-M3 r1p1，6 个硬件断点、4 个观察点 |
| `device id = 0x20036410` | 芯片是谁 | 低 12 位 `0x410`，中容量 F103，与 `target/stm32f1x.cfg` 对得上 |
| `flash size = 64 KiB` | flash 有多大 | 链接脚本 `MEMORY` 里 `FLASH` 的 `LENGTH` 必须是这个数 |

三条里任何一条对不上，先解决它再往下走。板子上的功率不够、SWD 接错线、
或者调试器被别的进程占着，症状都出现在这一段。

## 4.2 烧写、校验与读回

烧写、运行、读回可以用一条 OpenOCD 命令做完。命令里的路径交给 OpenOCD 之后，
它会自己打开文件，因此要写 Windows 侧看得见的那个路径。

`PowerShell`

```powershell
# 烧写 + 复位运行 + 读回（路径一律用正斜杠，理由见第 8.6 小节）
& "<OpenOCD>\bin\openocd.exe" -s "<OpenOCD>/share/openocd/scripts" `
  -f interface/cmsis-dap.cfg -f target/stm32f1x.cfg `
  -c "init" -c "reset halt" -c "arm semihosting enable" `
  -c "flash write_image erase <工作区>/build/main.elf" `
  -c "verify_image <工作区>/build/main.elf" `
  -c "reset run" -c "sleep 1500" -c "halt" `
  -c "mdw 0x08000000 4" -c "mdw 0x20000000 4" -c "reg pc" -c "reg msp" `
  -c "shutdown"
```

`实测数据`
`Text`

```text
（下面是节选：适配器的初始化行与第 4.1 小节相同，从 `reset halt` 之后开始）
[stm32f1x.cpu] halted due to debug-request, current mode: Thread
xPSR: 0x01000000 pc: 0x080002e8 msp: 0x20005000
semihosting is enabled
Info : device id = 0x20036410
Info : flash size = 64 KiB
Warn : Adding extra erase range, 0x08000350 .. 0x080003ff
wrote 848 bytes from file K:/<工作区>/build/main.elf in 0.135810s (6.098 KiB/s)
verified 848 bytes in 0.050067s (16.540 KiB/s)
boot ok
tick
tick
tick
done
[stm32f1x.cpu] halted due to debug-request, current mode: Thread
xPSR: 0x01000000 pc: 0x080002d4 msp: 0x20004ff0, semihosting
0x08000000: 20005000 080002e9 08000331 08000331
0x20000000: 000013ba 4e0a43c0 2400460b 5d11e00d
pc (/32): 0x080002d4
msp (/32): 0x20004ff0
shutdown command invoked
```

几处读数与它们的含义：

| 读数 | 含义 |
|---|---|
| `wrote 848 bytes ... in 0.136s` | flash 写入的字节数与耗时（约 6 KiB/s） |
| `verified 848 bytes in 0.050s` | 写完之后的读回校验（约 16.5 KiB/s） |
| `Warn : Adding extra erase range` | 擦除按扇区进行，最后一个扇区多擦了一点，正常现象 |
| `boot ok` 到 `done` 五行 | 程序真的跑起来了，输出的来源见第 4.3 小节 |
| `0x08000000: 20005000 080002e9 ...` | flash 最前面就是向量表：栈顶、复位入口、NMI、HardFault |
| `0x20000000: 000013ba ...` | RAM 第一个字是 `g_total`，`0x13ba` 即 5050，等于三次 `sum_to(100)` 的结果 |
| `pc (/32): 0x080002d4`，末尾带 `, semihosting` | 程序被停住时正在做 semihosting 调用；`msp` 比初值低，差值是栈上留下的现场 |

**复位入口的读数在第 4.1 小节**：那里是 `reset halt` 之后立刻读的，
因此 `pc` 正好是 `0x080002e8`；这里先让程序跑了一秒半，
`pc` 落在它当时所在的位置，也就是那五次打印里的最后一次。

**最后一个字是程序真的跑过的证据。** `g_total` 在源码里没有初值，
它从 0 变成 5050，说明 `main` 里的三圈循环执行完了；
在它后面读到的几个字是栈上残留的数据，不必解释。

> [!CAUTION]
> **只写 flash 主区，不碰选项字节与读保护位。**
> 烧写落在 `0x08000000` 起的主区；选项字节区在 `0x1FFFF800`，
> 读保护一旦置上，调试访问会被拒绝，恢复要整片擦除并重新解锁。
> 本章节的全部命令都不涉及那个区域。

`flash write_image erase` 之前目标必须是停住的。少了 `reset halt` 或 `halt`，
得到的是第 8.3 小节那两条报错。

## 4.3 程序的输出：semihosting

这块板子上没有串口，程序的输出只有一条通道：ARM 为调试留的 semihosting。
约定是 `r0` 放功能号、`r1` 放参数，`bkpt 0xAB` 陷入调试器，
由 OpenOCD 把内容转发到自己的控制台。功能号 `4` 是 `SYS_WRITE0`：
把 `r1` 指向的、以 0 结尾的字符串写出去。这段代码在第 3.2 小节的
`main.c` 里就是 `sh_puts()`。

**三处必须同时成立，缺一处就看不到输出**：

| 位置 | 要做的事 |
|---|---|
| 程序 | `bkpt 0xAB`，`r0` 放功能号、`r1` 放参数 |
| OpenOCD 会话 | 加 `-c "arm semihosting enable"` |
| 看输出的地方 | **OpenOCD 自己的控制台**，不是别的窗口 |

第 4.2 小节那次会话的输出里，`boot ok` 到 `done` 这五行就是这条通道的内容：
它们与源码里的五次 `sh_puts()` 一一对应，打印完之后程序进入空循环。
停住目标时看到的那一行末尾多出的 `semihosting` 表示它当时正停在 semihosting 调用里。
**把 `-c "arm semihosting enable"` 去掉，这五行就会消失**，
程序照跑不误，只是 `bkpt 0xAB` 不再被翻译成输出。

**裸机上没有 `printf`**：newlib 的 `printf` 需要 `_write` 之类的系统调用桩，
桩没接上时它会返回而不输出。在这块板子上有三种写法，结果都不相同，
报错原文与对照见第 8.4 小节。要用 `printf`，就得为它指定输出通道，
这条路线在 CubeMX 生成的工程里由 `syscalls.c` 承担，
也可以在 `_write` 里调用上面的 `sh_puts()`。

## 4.4 读寄存器与内存：`reg` 与 `mdw`

`reg` 读寄存器，`mdw` 按字读内存。`mdw` 的参数是「起始地址」与「字数」，
输出每行四个字，地址在行首。

`实测数据`
`PowerShell`

```powershell
# 读一个寄存器、读一段内存；每条命令的输出见下
& "<OpenOCD>\bin\openocd.exe" -s "<OpenOCD>/share/openocd/scripts" `
  -f interface/cmsis-dap.cfg -f target/stm32f1x.cfg `
  -c "init" -c "reset halt" -c "reg pc" -c "reg msp" `
  -c "mdw 0x08000000 4" -c "shutdown"
```

`实测数据`
`Text`

```text
pc (/32): 0x080002e8
msp (/32): 0x20005000
0x08000000: 20005000 080002e9 08000331 08000331
```

**一条 `reg` 里写多个寄存器名会被拒绝。** 写成 `reg pc msp xpsr` 时，
OpenOCD 不读寄存器，而是把 `reg` 的用法与相关命令列出来：

`实测数据`
`Text`

```text
reg [(register_number|register_name) [(value|'force')]]
...
  stm32f1x.cpu get_reg [-force] list
  stm32f1x.cpu set_reg dict
```

要读多个寄存器，就写多条 `-c`：`-c "reg pc" -c "reg msp" -c "reg xpsr"`。
这条输出在排查「命令打错了还是目标没连上」时有用——
它的出现说明调试器已经连上，只是命令语法不对。

> [!NOTE]
> 到这里，只用手敲命令已经能完成构建、烧写、校验、运行与观察这一整圈。
> 这一圈里没有图形界面，也没有 VS Code，因此它也是判断问题出在
> 工具链还是出在编辑器配置上的基准。

---

# 第 5 节 命令行路线（三）：下断点调试

GDB 需要两样东西：一个提供 GDB 远程协议的服务器，以及一份告诉它做什么的命令脚本。
服务器由 OpenOCD 提供，脚本由自己写。这一节把两者接起来，
并把一次完整的会话连同真实输出记录下来，作为 VS Code 那条路线的对照物。

## 5.1 两种接法：管道与 TCP

OpenOCD 起 GDB 服务器有两种方式。默认方式监听本机 TCP 端口（`3333`），
另一种把 GDB 的协议通道接到标准输入输出上，称为管道方式。

两种方式在本机的可用性并不相同，原因是两边的程序被操作系统分开了：
OpenOCD 是 Windows 程序，带 ARM 支持的 GDB 在 WSL 里。

`实测数据`
`Bash`

```bash
# 从 WSL 侧测试能否连上 Windows 上 OpenOCD 的 3333 端口
# 默认网关就是 WSL 看到的 Windows 主机地址
GW=$(ip route show default | awk '{print $3}')
python3 - "$GW" <<'EOF'
import socket, sys
for h in ('127.0.0.1', sys.argv[1]):
    s = socket.socket(); s.settimeout(4)
    try:
        s.connect((h, 3333)); print(h, 'connect OK')
    except Exception as e:
        print(h, 'FAIL', type(e).__name__, e)
    finally:
        s.close()
EOF
```

`实测数据`
`Text`

```text
WSL 看到的 Windows 主机地址（默认网关）= 192.168.224.1
127.0.0.1 connect FAIL ConnectionRefusedError [Errno 111] Connection refused
192.168.224.1 connect FAIL TimeoutError timed out
```

**两条失败各有一个原因**：WSL 里的 `127.0.0.1` 是 WSL 自己的回环地址，
那里没有 OpenOCD；而网关地址上的 3333 端口连不通，
说明这条入站通路被挡在外面。**结果是 WSL 里的 GDB 无法用 TCP 方式连过来**，
这也是本章节推荐管道方式的原因。

`实测数据`
`Text`

```text
# 在 Windows 侧，同一个 3333 端口是通的
# 用本机唯一的一个 x86-64 版 GDB 连上去，得到的是目标描述与随后的寄存器解析失败
warning: while parsing target description (at line 4): Target description specified unknown architecture "arm"
warning: Could not load XML target description; ignoring
Error in sourced command file:
Truncated register 16 in remote 'g' packet
```

这一组输出有两层意思：端口在 Windows 侧是通的（否则不会有目标描述），
而这个 GDB 不认识 ARM 目标，因此它读不懂寄存器包。
**在 Windows 上做这套调试，需要一个 ARM 版的 GDB**，
原因与安装途径见第 8.8 小节。

管道方式把两边接在一起，不经过网络：

| 方式 | 命令形态 | 本机可用 | 说明 |
|---|---|---|---|
| 管道 | `target extended-remote \| <openocd.exe> ... -c "gdb_port pipe"` | **可用** | GDB 自己拉起 OpenOCD，会话结束即退出，不受防火墙影响 |
| TCP | OpenOCD 起服务，GDB 连 `127.0.0.1:3333` | **仅 Windows 侧可用** | 需要 ARM 版 GDB；服务器就绪需要几秒 |

两行的可用性都在本机验证过：管道方式跑完了第 5.2 小节的整个会话；
TCP 方式在 WSL 侧两次都连不上，在 Windows 侧能连上（随后因 GDB 不认识 ARM 目标而停在寄存器解析上）。

> [!TIP]
> **管道方式的两个额外好处**：OpenOCD 的生命周期跟着 GDB 走，
> 因此不会留下占着调试器的进程；会话结束时会话自然收尾，
> 不会出现「上一次没退干净」的情况。

`待确认`

OpenOCD 起 TCP 服务需要多久才可以接受连接，本机测得「第 8 秒已经监听」，
但这与 USB 适配器的枚举速度有关。写到配置里时把等待时间放宽一些即可，
OpenOCD 侧没有需要改的参数。

## 5.2 一次完整的会话

会话由两个文件组成：命令脚本与启动脚本。命令脚本是纯文本，
每行一条 GDB 命令；`-x` 让 GDB 执行它，`-batch` 让它在脚本结束后退出。

`Bash`

```bash
# gdb.cmd    GDB 命令脚本（不是源码）
# 用法：gdb-multiarch -q -batch -x gdb.cmd build/main.elf
# 说明：这一份是 WSL + Windows 混合环境下的形态——GDB 在 WSL 里，
#       OpenOCD 是 Windows 程序，用管道把两者接起来，不经过 TCP。

set confirm off
set pagination off

echo \n=== 1. 连接：GDB 拉起 OpenOCD，两者走管道 ===\n
target extended-remote | /mnt/h/OpenOCD/bin/openocd.exe -s H:/OpenOCD/share/openocd/scripts -f interface/cmsis-dap.cfg -f target/stm32f1x.cfg -c "gdb_port pipe" -c "init"

echo \n=== 2. 让 OpenOCD 转发 semihosting 输出 ===\n
monitor arm semihosting enable

echo \n=== 3. 复位并停在复位向量之后 ===\n
monitor reset halt
info registers pc msp

echo \n=== 4. 把程序下载进 flash 并逐段核对 ===\n
load
compare-sections

echo \n=== 5. 在 main 上下断点并运行 ===\n
break main
continue

echo \n=== 6. 单步两行，观察 pc ===\n
next
info registers pc
next
info registers pc

echo \n=== 7. 走进 sum_to，看形参与局部变量 ===\n
break sum_to
continue
info args
info locals
next
next
info locals

echo \n=== 8. 去掉循环内的断点，跑到收尾那一行 ===\n
delete 2
break main.c:45
continue
print g_total
print &g_total

echo \n=== 9. 收工：先停住目标，再断开 ===\n
monitor halt
detach
quit
```

`Bash`

```bash
# gdb.sh    启动脚本
set -u
cd "$(dirname "$0")" || exit 1
gdb-multiarch -q -batch -x gdb.cmd build/main.elf 2>&1
echo "gdb exit = $?"
```

下面是一次真实运行的输出。OpenOCD 的启动横幅与第 4.1 小节相同，此处从略；
`===` 之间的分隔行来自脚本里的 `echo`。

`实测数据`
`Text`

```text
=== 1. 连接：GDB 拉起 OpenOCD，两者走管道 ===
DEPRECATED! use 'gdb port', not 'gdb_port'
Info : [stm32f1x.cpu] starting gdb server on pipe
Info : Listening on port 6666 for tcl connections
Info : Listening on port 4444 for telnet connections
Info : accepting 'gdb' connection from pipe
Warn : [stm32f1x.cpu] target was in unknown state when halt was requested
Info : device id = 0x20036410
Info : flash size = 64 KiB
0x2000002e in ?? ()

=== 2. 让 OpenOCD 转发 semihosting 输出 ===
semihosting is enabled

=== 3. 复位并停在复位向量之后 ===
[stm32f1x.cpu] halted due to debug-request, current mode: Thread
xPSR: 0x01000000 pc: 0x080002e8 msp: 0x20005000, semihosting
pc             0x2000002e          0x2000002e
msp            0x20005000          0x20005000

=== 4. 把程序下载进 flash 并逐段核对 ===
Loading section .isr_vector, size 0x1e4 lma 0x8000000
Loading section .text, size 0x150 lma 0x80001e4
Loading section .rodata, size 0x1c lma 0x8000334
Start address 0x080002e8, load size 848
Transfer rate: 1 KB/sec, 282 bytes/write.
Section .isr_vector, range 0x8000000 -- 0x80001e4: matched.
Section .text, range 0x80001e4 -- 0x8000334: matched.
Section .rodata, range 0x8000334 -- 0x8000350: matched.

=== 5. 在 main 上下断点并运行 ===
Breakpoint 1 at 0x80002a2: file main.c, line 40.
Note: automatically using hardware breakpoints for read-only addresses.

Breakpoint 1, main () at main.c:40
40	    sh_puts("boot ok\n");

=== 6. 单步两行，观察 pc ===
41	    for (uint32_t round = 0; round < 3; ++round) {
pc             0x80002a8           0x80002a8 <main+12>
42	        g_total = sum_to(100);      /* 断点在这三圈里各命中一次 */
pc             0x80002ae           0x80002ae <main+18>

=== 7. 走进 sum_to，看形参与局部变量 ===
Breakpoint 2 at 0x800026e: file main.c, line 31.

Breakpoint 2, sum_to (n=100) at main.c:31
31	    uint32_t acc = 0;
n = 100
acc = 134218548
32	    for (uint32_t i = 1; i <= n; ++i) {
33	        acc += i;
i = 1
acc = 0

=== 8. 去掉循环内的断点，跑到收尾那一行 ===
Breakpoint 3 at 0x80002cc: file main.c, line 45.

Breakpoint 3, main () at main.c:45
45	    sh_puts("done\n");
$1 = 5050
$2 = (volatile uint32_t *) 0x20000000 <g_total>

=== 9. 收工：先停住目标，再断开 ===
[Inferior 1 (Remote target) detached]
gdb exit = 0
```

几处关键输出与它们的含义：

| 输出 | 含义 |
|---|---|
| `starting gdb server on pipe` | OpenOCD 的 GDB 通道接在标准输入输出上 |
| `compare-sections` 的三行 `matched` | 芯片里三个段的字节与本地 `.elf` 逐段一致，这是比 `verify_image` 更细的核对 |
| `Breakpoint 1 at 0x80002a2: file main.c, line 40` | 断点落在源码行上，说明 `-g3` 的调试信息可用 |
| `Note: automatically using hardware breakpoints for read-only addresses` | flash 里的代码不能改写为断点指令，因此用的是硬件断点；这块芯片有 6 个 |
| `sum_to (n=100)` 与 `info locals` | 形参与局部变量都能看到，第 5.3 小节解释其中一处异常 |
| `$1 = 5050` | 三圈循环跑完之后 `g_total` 的值 |
| `$2 = (volatile uint32_t *) 0x20000000 <g_total>` | 它在 RAM 里的地址，与第 3.4 小节的 `nm` 输出一致 |

**semihosting 的输出没有出现在这段记录里。** 程序确实执行了那五次打印
（`g_total` 变成 5050 就是证据），但走管道时 OpenOCD 的标准输出被用作调试协议通道，
打印的内容不落在 GDB 的控制台上。要看见 semihosting 的文字，
用第 4.3 小节那种「OpenOCD 单独运行」的方式；
VS Code 路线下这类输出由调试扩展提供专门的终端，
`文档` 的说法是 Cortex-Debug 在 `TERMINAL` 里开一个名为 `gdb-server` 的子窗口专供 semihosting。

## 5.3 三处细节

**一、`monitor` 命令不会刷新 GDB 里的寄存器缓存。**
`monitor reset halt` 让 OpenOCD 复位并停住目标，
OpenOCD 自己的输出显示 `pc: 0x080002e8`，
而紧接着的 `info registers pc` 给出的却是 `0x2000002e`——
那是复位之前目标所在的位置。执行一次 `load`、`next` 或 `continue` 之后，
GDB 重新读寄存器，值就对上了：

`实测数据`
`Text`

```text
pc             0x2000002e          0x2000002e          ← monitor reset halt 之后的缓存值
pc             0x80002a8           0x80002a8 <main+12> ← 执行 next 之后重新读取的值
```

**二、断点停在声明行时，变量显示的是尚未赋值的值。**
`sum_to` 的第一行是 `uint32_t acc = 0;`，断点停在这一行时
`acc` 读出来是 `134218548`，执行一次 `next` 之后才变成 0。
这不是调试器读错了地址，而是这一行还没执行。
同样的现象与解释见《02-调试器/00-本机环境与路线.md》第 7.1 小节。

**三、`continue` 只在有断点可以命中时才返回。**
这个程序在 `sum_to` 之后进入 `for (;;) { }`，如果此时没有别的断点，
`continue` 会让会话停在运行状态，直到人为中断。
脚本因此保留了第 8 步那个收尾断点 `break main.c:45`——
它的作用不是观察某一行，而是让会话能够结束。

> [!NOTE]
> 命令行这条线到这里是完整的：构建、烧写、校验、运行、semihosting、
> 断点、单步、看变量、读内存。第 6、7 节把 CubeMX 与 VS Code 接到这条线上，
> 它们各自替换其中一段，不改变其余部分。

---

# 第 6 节 让 CubeMX 在无界面模式下生成工程

前几节的工程是手写的：启动文件与链接脚本取自固件包，构建规则自己写。
另一条路是让 CubeMX 按型号生成整套文件，包括它自己的 CMake 工程。
这条路可以不走图形界面：CubeMX 提供命令行模式，吃一份脚本文件。

`文档`

> "To run STM32CubeMX in command-line mode getting commands from a script and
> without UI, use the following command lines:
> – On Windows: `cd <STM32CubeMX installation path>` /
> `jre\bin\java -jar STM32CubeMX.exe –q <script filename>`"
>
> —— UM1718 Rev 48 §3.3.2

同一小节还给出另外两种模式：`-i` 是交互式命令行（出现 `MX>` 提示符），
`-s` 是带界面的脚本模式。手册的 Table 1「Command line summary」列出脚本里可用的命令，
其中与本章节有关的是这几条：

`文档`

| 命令 | 作用 | 手册里的示例 |
|---|---|---|
| `load <mcu>` | 按型号新建配置 | `load STM32F101RCTx` |
| `loadboard <板名> <allmodes\|nomode>` | 按官方板新建配置 | `loadboard NUCLEO-F030R8 allmodes` |
| `config load <文件名>` | 载入已有的 `.ioc` | `config load "C:\Cube\ccmram\ccmram.ioc"` |
| `project name <名字>` | 设定工程名 | `project name ccmram` |
| `project path <路径>` | 设定生成目录 | `project path C:\Cube\ccmram` |
| `project toolchain <工具链>` | 选生成哪一套工程文件 | 取值含 `EWARM`、`MDK-Arm`、`STM32CubeIDE`、`Makefile`、**`CMake`** |
| `project compiler <编译器>` | 选编译器 | `GCC` 或 `Starm-Clang` |
| `project generate` | 生成完整工程 | `project generate` |
| `exit` | 退出 | `exit` |

`login` 那一行在手册里有脚注：涉及下载软件包的命令要先登录。
本机的 F1 固件包已经在 `STM32Cube\Repository` 下，因此生成过程没有触发下载。

## 6.1 脚本与执行

脚本是一行一条命令的纯文本。下面这份在中文路径下执行过，
生成的工程与第 2.2 小节的目录结构不同：它把源码、驱动、CMake 文件都放进同一个目录树。

`Text`

```text
load STM32F103C8Tx
project name MxDemo
project toolchain CMake
project compiler GCC
project path <工作区>/mxcube
project generate
exit
```

`实测数据`
`PowerShell`

```powershell
# -q 表示不给界面，只执行脚本；脚本路径里的中文要写成 UTF-8
& "<CubeMX>\STM32CubeMX.exe" -q "<工作区>\mx_script.txt"
```

`实测数据`
`Text`

```text
OK
project generate
OK
exit
Bye bye
CubeMX 会话墙钟 = 19.2 s, exit = 0
```

脚本逐行执行，每行回一个 `OK`。**19.2 秒**里包含 CubeMX 自身的启动时间，
真正的生成只占其中一部分，日志里的分项是 `Copy HAL[0] : 390mS`、
`Generating toolchain IDE Files: 561mS`、`Copy CMSIS : 2551mS`。

生成出来的目录树（只列到第二层）：

`Text`

```text
mxcube/MxDemo/
├── MxDemo.ioc                    配置的唯一真相
├── CMakeLists.txt                顶层构建描述
├── CMakePresets.json             预设（Debug / Release）
├── cmake/
│   ├── gcc-arm-none-eabi.cmake   工具链描述
│   ├── starm-clang.cmake         另一套工具链描述
│   └── stm32cubemx/CMakeLists.txt  驱动与 CMSIS 的构建描述
├── Src/                          用户的 .c（main.c、stm32f1xx_it.c、syscalls.c 等）
├── Inc/                          对应的头文件
├── startup_stm32f103xb.s         启动文件
├── STM32F103XX_FLASH.ld          链接脚本
└── Drivers/                      HAL 驱动与 CMSIS
```

`实测数据`
`Text`

```text
# 链接脚本 MEMORY 块的两行（CubeMX 按型号写入，与这块板子一致）
RAM (xrw)      : ORIGIN = 0x20000000, LENGTH = 20K
FLASH (rx)      : ORIGIN = 0x8000000, LENGTH = 64K
_estack = ORIGIN(RAM) + LENGTH(RAM);    /* end of RAM */
```

**这两行就是第 2.1 小节要求核对的数字**，CubeMX 生成的版本是对的：
型号来自 `.ioc` 里的 `ProjectManager.DeviceId=STM32F103C8Tx`。

## 6.2 生成物直接构建

CubeMX 生成的 `CMakePresets.json` 与第 3.3 小节手写的那份结构相同，
只是预设名换成了 `Debug` 与 `Release`：

`JSONC`

```jsonc
{
  "version": 3,
  "configurePresets": [
    {
      "name": "default",
      "hidden": true,
      "generator": "Ninja",
      "binaryDir": "${sourceDir}/build/${presetName}",
      "toolchainFile": "${sourceDir}/cmake/gcc-arm-none-eabi.cmake",
      "cacheVariables": {}
    },
    { "name": "Debug",   "inherits": "default", "cacheVariables": { "CMAKE_BUILD_TYPE": "Debug" } },
    { "name": "Release", "inherits": "default", "cacheVariables": { "CMAKE_BUILD_TYPE": "Release" } }
  ],
  "buildPresets": [
    { "name": "Debug",   "configurePreset": "Debug" },
    { "name": "Release", "configurePreset": "Release" }
  ]
}
```

构建命令与第 3.3 小节完全一样，只是预设名不同：

`实测数据`
`Bash`

```bash
cd <工作区>/mxcube/MxDemo
cmake --preset Debug
cmake --build --preset Debug
```

`实测数据`
`Text`

```text
-- The ASM compiler identification is GNU
-- Found assembler: /usr/bin/arm-none-eabi-gcc
-- Configuring done (9.6s)
-- Generating done (0.2s)
[19/19] Linking C executable MxDemo.elf
Memory region         Used Size  Region Size  %age Used
             RAM:        1584 B        20 KB      7.73%
           FLASH:        3776 B        64 KB      5.76%
构建墙钟 = 15.5 s
```

体积报告是 CubeMX 在 `CMakeLists.txt` 里挂的一个构建后步骤，
输出格式与 `arm-none-eabi-size` 不同：它按内存区域给占用百分比。
烧写它得到的读数如下：

`实测数据`
`Text`

```text
wrote 3776 bytes from file K:/<工作区>/mxcube/MxDemo/build/Debug/MxDemo.elf in 0.352996s (10.446 KiB/s)
```

这份工程里 `main()` 的结构是 `HAL_Init()`、`SystemClock_Config()`、
然后是空的 `while (1)`；`SystemClock_Config()` 用 HSI 8 MHz、不启用 PLL，
因此它在时钟上与第 3 节那个最小工程是同一档，这一点在第 8.5 小节会用到。

## 6.3 一件必须提前处理的事

用 `load` 新建的配置里，SYS 的调试口设置默认是 `No Debug`，
生成的代码会**主动关掉 SWJ**（也就是 SWD 与 JTAG 引脚）。相关行如下：

`C`

```c
/* 节选：MxDemo/Src/stm32f1xx_hal_msp.c 第 70 至 77 行，CubeMX 6.15.0 生成 */
void HAL_MspInit(void)
{
  __HAL_RCC_AFIO_CLK_ENABLE();
  __HAL_RCC_PWR_CLK_ENABLE();

  /* System interrupt init*/

  /** DISABLE: JTAG-DP Disabled and SW-DP Disabled
  */
  __HAL_AFIO_REMAP_SWJ_DISABLE();
}
```

对应的 `.ioc` 设置是：

`INI`

```ini
Mcu.Pin0=VP_SYS_VS_ND
VP_SYS_VS_ND.Mode=No_Debug
VP_SYS_VS_ND.Signal=SYS_VS_ND
```

把这份工程烧进去之后，程序一开始运行，调试器就再也连不上，
报错与恢复办法见第 8.2 小节。**处理办法是在生成之前把 SYS 的 Debug
改成 Serial Wire**，或者生成之后确认 `stm32f1xx_hal_msp.c` 里没有那一行。
图形界面里这个下拉框的位置随 CubeMX 版本变化，属于存疑范围；
`.ioc` 里对应的写法由界面写入，手改有风险，本教材建议在界面里改完再生成。

> [!WARNING]
> **这一节的工程与第 3 节的工程不是同一份，不要混用产物目录。**
> 前者的主程序由 CubeMX 生成，后者由自己写；两者的启动文件、
> 链接脚本与构建规则都不同，混在一起会出现「明明改了源码却没变化」这类现象。

---

# 第 7 节 搬进 VS Code

VS Code 在这条路线上的作用是包装命令：把第 3 至 5 节的命令写成任务与调试配置，
之后按界面上的按钮就等于把那些命令打一遍。因此两个 JSON 文件的内容
可以直接对着前面几节的命令行理解。

## 7.1 `tasks.json`

三个任务：构建、烧写、探测。字段的通用含义见
《01-编译器/02-环境配置.md》第 5.7 小节与第 6 节的任务示例。

`JSONC`

```jsonc
{
  "version": "2.0.0",
  "tasks": [
    // 构建：命令行是 cmake --build --preset Debug，工作目录是工程根
    {
      "label": "STM32: 构建 (Debug)",
      "type": "process",
      "command": "cmake",
      "args": ["--build", "--preset", "Debug"],
      "options": { "cwd": "${workspaceFolder}" },
      "problemMatcher": ["$gcc"],
      "presentation": { "reveal": "always", "panel": "shared", "clear": true }
    },

    // 烧写：把 -c 之后的每条命令各写成一个数组元素，不要合并
    // 路径一律用正斜杠，理由见第 8.6 小节
    {
      "label": "STM32: 烧写",
      "type": "process",
      "command": "<OpenOCD>/bin/openocd.exe",
      "args": [
        "-s", "<OpenOCD>/share/openocd/scripts",
        "-f", "interface/cmsis-dap.cfg",
        "-f", "target/stm32f1x.cfg",
        "-c", "init",
        "-c", "reset halt",
        "-c", "arm semihosting enable",
        "-c", "flash write_image erase ${workspaceFolder}/build/debug/stm32f103c8.elf",
        "-c", "reset run",
        "-c", "shutdown"
      ],
      "options": { "cwd": "${workspaceFolder}" },
      "dependsOn": ["STM32: 构建 (Debug)"],
      "problemMatcher": [],
      "presentation": { "reveal": "always", "panel": "shared" }
    },

    // 探测：只认芯片，不写 flash；连不上时先跑这一条
    {
      "label": "STM32: 探测芯片",
      "type": "process",
      "command": "<OpenOCD>/bin/openocd.exe",
      "args": [
        "-s", "<OpenOCD>/share/openocd/scripts",
        "-f", "interface/cmsis-dap.cfg",
        "-f", "target/stm32f1x.cfg",
        "-c", "init",
        "-c", "reset halt",
        "-c", "flash probe 0",
        "-c", "shutdown"
      ],
      "options": { "cwd": "${workspaceFolder}" },
      "problemMatcher": [],
      "presentation": { "reveal": "always", "panel": "shared" }
    }
  ]
}
```

| 字段 | 作用 | 本文件里的取值理由 |
|---|---|---|
| `label` | 任务名，供 `dependsOn` 与 `preLaunchTask` 引用 | 三处引用必须逐字一致 |
| `type` | 任务的执行方式 | `process` 不经过 shell，参数按数组逐项传递，空格不会被拆开 |
| `command` | 要执行的程序 | 构建用 PATH 中的 `cmake`；烧写用 OpenOCD 的绝对路径 |
| `args` | 参数数组 | `-c` 与它的内容是**两个**元素，理由见第 8.9 小节 |
| `options.cwd` | 工作目录 | 工程根目录，`${workspaceFolder}` 即打开的那个文件夹 |
| `dependsOn` | 先跑哪个任务 | 烧写前必须先构建 |
| `problemMatcher` | 把输出里的报错变成可点击的问题列表 | 构建用 `$gcc`；OpenOCD 的输出没有匹配器 |
| `presentation` | 面板行为 | 构建与烧写都要看输出，因此 `reveal: always` |

## 7.2 `launch.json`

调试配置的方案是：让 VS Code 自己拉起 OpenOCD 的 GDB 服务器，
再让 GDB 连上去。用到的字段都在
《01-编译器/02-环境配置.md》第 5.7 小节里出现过：
`debugServerPath`、`debugServerArgs`、`serverStarted`、`miDebuggerPath`。

`JSONC`

```jsonc
{
  "version": "0.2.0",
  "configurations": [
    {
      "name": "STM32: OpenOCD 调试 (CMSIS-DAP)",
      "type": "cppdbg",
      "request": "launch",

      // 符号来自本地 .elf；目标机上的代码与它是同一份
      "program": "${workspaceFolder}/build/debug/stm32f103c8.elf",
      "cwd": "${workspaceFolder}",

      // MIMode 必填；miDebuggerPath 指向 ARM 版 GDB，不写则在 PATH 里找
      // 注意：x86-64 版 GDB 连得上但读不懂寄存器包，见第 8.8 小节
      "MIMode": "gdb",
      "miDebuggerPath": "<ARM GDB>/bin/arm-none-eabi-gdb.exe",

      // 由 VS Code 启动 GDB 服务器：程序、参数、以及「服务器已就绪」的判据
      "debugServerPath": "<OpenOCD>/bin/openocd.exe",
      "debugServerArgs": "-s <OpenOCD>/share/openocd/scripts -f interface/cmsis-dap.cfg -f target/stm32f1x.cfg -c \"gdb port 3333\" -c \"init\" -c \"arm semihosting enable\"",
      "serverStarted": "Listening on port 3333 for gdb connections",
      "serverLaunchTimeout": 20000,
      "filterStderr": true,

      // 先构建，再进调试
      "preLaunchTask": "STM32: 构建 (Debug)",

      // 目标机上没有控制台：程序的实际输出走 semihosting
      "externalConsole": false,
      "stopAtEntry": true,
      "setupCommands": [
        { "text": "-enable-pretty-printing", "ignoreFailures": true },
        { "text": "set pagination off", "ignoreFailures": true }
      ]
    }
  ]
}
```

| 字段 | 作用 | 本文件里的取值理由 |
|---|---|---|
| `type` / `request` | 适配器与启动方式 | `cppdbg` + `launch`，与《01-编译器/02-环境配置.md》第 5.2 小节一致 |
| `program` | 符号与源码的来源 | 指向本地 `.elf`；目标机上并没有文件系统 |
| `MIMode` | 后端调试器类型 | `gdb` |
| `miDebuggerPath` | GDB 的位置 | **必须是 ARM 版**，这一条本机用报错验证过 |
| `debugServerPath` | 由 VS Code 启动的服务器程序 | OpenOCD |
| `debugServerArgs` | 服务器的参数 | 与第 4.1、5.1 小节的命令行逐字对应 |
| `serverStarted` | 判定服务器就绪的输出模式 | 取自 OpenOCD 的真实输出，见第 4.1 小节 |
| `serverLaunchTimeout` | 等待服务器就绪的超时 | 本机 OpenOCD 就绪需要数秒，给 20 秒余量 |
| `filterStderr` | 是否收集服务器的标准错误 | OpenOCD 的信息行走标准错误，收集后能在调试控制台看到 |
| `preLaunchTask` | 调试前执行的任务 | 名字必须与 `tasks.json` 的 `label` 一致 |
| `stopAtEntry` | 是否在入口停住 | 与命令行下的 `load` 后下断点等价 |
| `setupCommands` | 连接后的命令序列 | 关闭分页、允许整齐打印 |
| `externalConsole` | 是否用独立控制台 | 目标机上没有控制台，保持 `false` |

另一个方案是手工先起 OpenOCD（第 4.1 小节的命令），
再让 VS Code 用 `miDebuggerServerAddress` 连 `localhost:3333`。
这个字段的作用见《01-编译器/02-环境配置.md》第 5.7 小节。
它的前提是 GDB 与 OpenOCD 在同一台机器上：本机实测过，
WSL 里的 GDB 连不上 Windows 侧的 3333 端口（第 5.1 小节）。

## 7.3 这一节里哪些结论随环境而变

**图形界面与扩展相关的一切都属于存疑范围，不给统一答案。** 变的东西有三类：

| 变的是什么 | 为什么给不出统一答案 | 读者以什么为准 |
|---|---|---|
| 所装扩展及其版本 | ST 的扩展在 3.x 版本上换了架构：调试后端从开源的 Cortex-Debug 换成 ST 自己的 DAP，CLI 工具从 CubeCLT 换成 bundle manager；不同版本生成的配置文件、支持的调试器都不同 | 扩展自己生成的 `launch.json`；第 7.2 小节给出的配置是通用字段的组合，不是某个版本的生成结果 |
| Windows 侧的工具链 | 本章节的命令行路线把 GDB 放在 WSL 里，而 VS Code 里的 GDB 必须与 OpenOCD 同机；Windows 上有没有 ARM 版 GDB 取决于装了什么 | 本机 `arm-none-eabi-gdb --version` 的输出；没有就装一套，见第 8.8 小节 |
| 服务器就绪的判据与等待时间 | `Listening on port 3333 for gdb connections` 这句来自本机 OpenOCD 0.12.0 的输出，别的版本措辞可能不同；就绪耗时与适配器有关 | 本机 OpenOCD 的实际输出，把它整句抄进 `serverStarted` |

因此这一节**没有**「按某个键就可以」这类说法：界面上的按钮名称、
向导页面的字段、扩展自动生成的配置，都随上面三类因素变化。
可以用来自查的判据是命令行的等价物——同一个工程在命令行下能构建、能烧写、
能用 GDB 下断点，那么界面里剩下的问题就只可能在配置文件的字段上，
排查方向也因此窄了很多。

`待确认`

第 7.2 小节的 `launch.json` **没有在 VS Code 里实际跑过**：
本机没有安装 ARM 版 GDB，也没有安装这一节用到的调试扩展。
其中的字段名与语义有前面的章节与官方页面为依据，
`debugServerArgs` 与 `serverStarted` 的取值来自本机 OpenOCD 的真实输出，
但「VS Code 能否按这份配置启动服务器并停到断点」这一步没有验证。

> [!IMPORTANT]
> **命令行能跑通、界面里跑不通，问题通常在三个地方**：
> GDB 不是 ARM 版、`debugServerPath` 指错了程序、
> 或者 `serverStarted` 的匹配串与本机 OpenOCD 的输出不一致。
> 这三处都能用第 4 节与第 5 节的命令单独验证。

---

# 第 8 节 报错原文与处置

这一节的每一条都来自本机运行时的真实输出。报错原文照抄，
因为同一条错误在不同工具链版本里的措辞会变，而搜索与提问要用原文。

## 8.1 调试器被别的进程占着

板子只有一块，SWD 链路只有一条。上一个 OpenOCD 没有退出时再起一个，
第二个会话得到的不是「设备忙」这类清楚的提示，而是链路层的错误：

`实测数据`
`Text`

```text
Error: CMSIS-DAP transfer count mismatch: expected 3, got 2
Error: CMSIS-DAP command mismatch. Expected 0x5 received 0x12
Error: Failed to read memory and, additionally, failed to find out where
Error: [stm32f1x.cpu] Execution of event examine-end failed:
H:/OpenOCD/share/openocd/scripts/mem_helper.tcl:7: Error: failed to read memory
```

**这组错误看起来像硬件坏了，实际是两条会话在同一根线上互相打断。**
排查顺序是：先确认没有别的 OpenOCD 或 IDE 调试会话在跑，
再重试一次；重试之前不要动接线，因为接线是好的。

`PowerShell`

```powershell
# 看还有没有 OpenOCD 在跑；有就先结束它，再重试
Get-Process openocd -ErrorAction SilentlyContinue | Select-Object Id, StartTime
```

另外有一行在**每次**会话里都会出现，而会话本身完全正常：

`实测数据`
`Text`

```text
Error: could not get configuration descriptor 0 for device 0x0416:0x5021: Input/Output Error
```

它是调试器 USB 描述符读取的一个问题，出现在 `init` 之前。
本机的每一次成功会话都带着这一行，因此它不构成失败判据；
判断连上没有，看的是 `SWD DPIDR` 与 `Cortex-M3 r1p1 processor detected` 两行。

## 8.2 连不上调试器：程序把 SWJ 关掉了

这一条是本机在真板上撞出来的，处置过程完整记录如下。

**现象**：CubeMX 生成的工程（第 6 节）烧写成功、程序开始运行之后，
任何一次新的连接都失败：

`实测数据`
`Text`

```text
Error: Error connecting DP: cannot read IDR
Error: [stm32f1x.cpu] DP initialisation failed
Error: [stm32f1x.cpu] Polling failed, trying to reexamine
Error: [stm32f1x.cpu] Examination failed
Error: [stm32f1x.cpu] Examination failed, GDB will be halted. Polling again in 100ms
```

**原因**：生成出来的 `stm32f1xx_hal_msp.c` 里有
`__HAL_AFIO_REMAP_SWJ_DISABLE();`（第 6.3 小节），
它把 SWD 与 JTAG 引脚从调试口上摘下来当作普通引脚。
在这之后，调试口在芯片内部就不存在了，直到下一次复位才恢复。

**恢复办法**：连接时拉住复位，让调试器在程序运行之前接管。
OpenOCD 提供了这个选项：

`实测数据`
`PowerShell`

```powershell
# connect_assert_srst：连接期间保持复位被拉低，程序还没机会关掉调试口
& "<OpenOCD>\bin\openocd.exe" -s "<OpenOCD>/share/openocd/scripts" `
  -f interface/cmsis-dap.cfg -f target/stm32f1x.cfg `
  -c "reset_config srst_only srst_nogate connect_assert_srst" `
  -c "init" -c "reset halt" -c "shutdown"
```

`实测数据`
`Text`

```text
Info : Connecting under reset
Info : SWD DPIDR 0x1ba01477
Info : [stm32f1x.cpu] Cortex-M3 r1p1 processor detected
[stm32f1x.cpu] halted due to debug-request, current mode: Thread
pc (/32): 0x08000e24
```

**这条命令在本机有效**：`Info : Connecting under reset` 之后立刻读到 `DPIDR`，
说明调试器在程序运行之前接管了目标。**它依赖复位线接到适配器**，
因此读者的板子是否同样有效要以本机为准。

`待确认`

复位线不可用时的两种替代办法（按住板子上的复位键并在松开的瞬间开始连接、
或把 BOOT0 拉高让芯片进系统引导程序再擦除）在本机没有验证过，
列在这里只作为方向。

**根治办法**：在 CubeMX 里把 SYS 的 Debug 由 `No Debug` 改成 `Serial Wire`
（第 6.3 小节），重新生成之后 `HAL_MspInit()` 里不再有那一行。
本机按这个方向重新生成并烧写，程序运行期间用普通方式连接成功：

`实测数据`
`Text`

```text
Info : SWD DPIDR 0x1ba01477
Info : [stm32f1x.cpu] Cortex-M3 r1p1 processor detected
pc (/32): 0x08000ce4
```

> [!CAUTION]
> **生成工程之前先看 SYS 的 Debug 设置。**
> 这一条与看门狗、低功耗模式属于同一类问题：程序一运行，
> 调试通路就断了，而现场又在芯片里，形成闭环。
> 恢复手段要提前想好，不能等它发生之后再找。

## 8.3 没先停住就烧写

`flash write_image erase` 要求目标处于停住状态。
少了 `reset halt` 或 `halt`，得到的是：

`实测数据`
`Text`

```text
Error: Target not halted
Error: failed erasing sectors 0 to 0
auto erase enabled
```

命令的退出码是 1，而 `init` 与 flash 探测都已经成功，
因此这一条与「连不上」是两回事：链路是通的，只是目标在运行。
在 OpenOCD 的命令序列里把 `reset halt` 放在 `flash write_image erase` 之前即可。

## 8.4 `printf` 没有输出

newlib 的 `printf` 最终会调用一组系统调用桩（`_write`、`_sbrk`、`_fstat` 等）。
桩的来源由 `--specs` 决定，三种写法的结果完全不同。

**一、完全不给 `--specs`**：链接阶段就失败，错误里点名了缺哪些符号。

`实测数据`
`Text`

```text
undefined reference to `_sbrk'
undefined reference to `_write'
undefined reference to `_fstat'
undefined reference to `_isatty'
undefined reference to `_close'
undefined reference to `_read'
undefined reference to `_lseek'
collect2: error: ld returned 1 exit status
```

**二、`--specs=nosys.specs`**：链接器给出警告，明确写着这些桩不会被实现。

`实测数据`
`Text`

```text
warning: _close is not implemented and will always fail
warning: _fstat is not implemented and will always fail
warning: _isatty is not implemented and will always fail
warning: _lseek is not implemented and will always fail
warning: _read is not implemented and will always fail
warning: _write is not implemented and will always fail
```

链接通过，程序也能跑，**但控制台上一个字都没有**，因为 `_write` 是空桩。
本机在真板上验证过：程序停在 `for (;;)` 里正常运行，OpenOCD 侧没有任何输出。

**三、`--specs=rdimon.specs`**：把标准输入输出接到 semihosting。
它需要程序自己先调用 `initialise_monitor_handles()` 建立句柄；
不调用时同样没有输出，调用了才有：

`实测数据`

| 编译方式 | 程序里的调用 | 真板上的输出 | text 大小 |
|---|---|---|---|
| `--specs=nosys.specs` | 无 | **无** | 7836 |
| `--specs=rdimon.specs` | 无 | **无** | 10220 |
| `--specs=rdimon.specs -DCALL_INIT` | `initialise_monitor_handles()` | **`printf via rdimon`** | 10224 |

`实测数据`
`Text`

```text
wrote 11584 bytes from file .../printf_rdimon_init.elf in 0.852958s (13.263 KiB/s)
printf via rdimon
```

**三行输出对应三种不同的原因**：缺桩导致链接失败、
桩是空的导致静默丢弃、句柄没建立导致写不出去。
诊断办法一致：看链接器的警告与错误，它们会说出是哪一种。
CubeMX 生成的工程里这一层由 `Src/syscalls.c` 承担：它定义了一个 `_write`
（第 80 行），函数体里调用 `__io_putchar`（第 87 行），
而 `__io_putchar` 在第 35 行被声明为弱符号。
因此「CubeMX 工程里 `printf` 没有输出」的常见原因是没有实现那个弱符号。

## 8.5 时钟不对：8 MHz 与 72 MHz 的九倍差

这块板子在复位之后跑 HSI 8 MHz，第 3 节的 `SystemInit()` 是空函数，
因此程序全程 8 MHz；CubeMX 生成的默认工程同样用 HSI 8 MHz、
不启用 PLL（第 6.2 小节）。如果软件按 72 MHz 来算时间，
所有延时都会变成九倍。

本机用一个探针程序测了这件事：同一个程序编两个版本，
一个保持复位默认，另一个启用 HSE 8 MHz 并让 PLL 乘 9 切到 72 MHz；
两个版本都用 SysTick 等 1000 次中断，而 SysTick 的重载值按 72 MHz 写（72000）。

`实测数据`

| 项 | 复位默认（HSI） | 启用 HSE 与 PLL |
|---|---|---|
| `RCC_CFGR` 读数 | `0x00000000`（SWS = 00 → HSI） | `0x001d000a`（SWS = 10 → PLL，PLLMUL = ×9，PLLSRC = HSE） |
| HSE 是否起振 | 未启用 | **是**（`RCC_CR` 的 HSERDY 置位） |
| 等待期间的内核周期数 | `0x044aa222`（72,000,034） | `0x044aa22c`（72,000,044） |
| 只给 3 秒观察 | **没有任何输出**，还在等 | 输出 1000 次中断的全部读数 |
| 给 11 秒观察 | 输出全部读数，会话墙钟 11.69 s | 会话墙钟 3.69 s（其中程序只等 1 秒） |

**两次的周期数几乎一样，墙钟时间差九倍**，因为一个周期在 8 MHz 下是 125 ns、
在 72 MHz 下是 13.9 ns。这就是「时钟配错」的后果：
周期数算对了，时间全错。

处置办法有两条，选哪一条取决于工程怎么建：

| 工程形态 | 做法 |
|---|---|
| 手写的最小工程 | 自己实现 `SystemInit()`，或在 `main` 开头配 RCC；配完把 `SystemCoreClock` 更新成实际值，所有按频率算出来的常量都以它为准 |
| CubeMX 生成的工程 | 在 Clock Configuration 页配好时钟树，重新生成；`SystemClock_Config()` 与 `SystemCoreClock` 会跟着更新 |

`待确认`

外设时序与时钟的关系（例如把 USART 波特率算错会怎样）不在本章节的验证范围内：
这块板子上没有可以观测的外设接口。

## 8.6 `-c` 里的反斜杠会被 Tcl 吃掉

OpenOCD 的 `-c` 参数按 Tcl 规则解析，反斜杠是转义符。
把路径写成 Windows 形式时，`\C`、`\临`、`\b`、`\t` 都会被当成转义序列：

`实测数据`
`PowerShell`

```powershell
# 错法：路径用了反斜杠
& "<OpenOCD>\bin\openocd.exe" -s "<OpenOCD>/share/openocd/scripts" `
  -f interface/cmsis-dap.cfg -f target/stm32f1x.cfg `
  -c "init" -c "reset halt" `
  -c "flash write_image erase K:\C相关课程\临时\ops_lab\stm32vsc\build\main.elf" `
  -c "shutdown"
```

`实测数据`
`Text`

```text
Error: couldn't open K:C��ؿγ���ʱops_labstm32vscuildmain.elf
exit = 1
```

报错里的路径已经看不出原样：`\C`、`\临` 被吃掉，`\build` 里的 `\b`
变成了退格符，因此只剩 `uild`。**这类报错会被误认为「文件不存在」**，
排查方向也会跟着错。写成正斜杠就没有这个问题：

`实测数据`
`Text`

```text
wrote 848 bytes from file K:/C相关课程/临时/ops_lab/stm32vsc/build/main.elf in 0.136890s (6.050 KiB/s)
```

编译在 WSL 里做、烧写在 Windows 侧做时，有两件事要一起检查：
交给 OpenOCD 的必须是 Windows 侧看得见的路径，而且其中的反斜杠要换成正斜杠。

## 8.7 中文路径：能用，但打印出来是乱码

编译、烧写、调试都能在中文目录下正常工作，本机第 3 至 6 节的工程都在中文目录里。
受影响的是两个程序打印路径的方式：

`实测数据`
`Text`

```text
# OpenOCD 的输出
wrote 848 bytes from file K:/C��ؿγ�/��ʱ/ops_lab/stm32vsc/build/main.elf in 0.136890s (6.050 KiB/s)

# CubeMX 的输出
pathGccArm : K:\C��ؿγ�\��ʱ\ops_lab\stm32vsc\mxcube\MxDemo\cmake\gcc-arm-none-eabi.cmake
```

两个程序都按本机的 ANSI 代码页写出路径，而控制台按 UTF-8 解释，
于是中文部分变成乱码。**文件本身没有问题**：同一行的字节数、
`wrote` 与 `verified` 的结果、以及程序在板子上的行为都正常。
处置办法是不要照抄输出里的路径，需要确认路径时用文件管理器或
`Get-Item` 看真实的名字。

WSL 侧访问同一份文件时要换一种写法：`K:\<工作区>` 对应 `/mnt/<盘符小写>/<工作区>`，
盘符转小写、反斜杠转正斜杠。**GDB 能不能吃中文路径取决于它是哪一版**：
第 5.2 小节那次会话里，WSL 里的 `gdb-multiarch` 正常读到了源码与行号
（输出里有 `file main.c, line 40`）；而 Windows 侧的 MinGW GDB 在中文路径下会报
`No such file or directory`，对照记录见《01-编译器/02-环境配置.md》第 9.1 小节
与《02-调试器/00-本机环境与路线.md》第 7.2 小节。

## 8.8 Windows 侧的 GDB 不是 ARM 版

Windows 上能装到的 GDB 通常是 x86-64 目标版，它连得上 OpenOCD 的 GDB 服务器，
但读不懂 ARM 的寄存器包：

`实测数据`
`Text`

```text
warning: while parsing target description (at line 4): Target description specified unknown architecture "arm"
warning: Could not load XML target description; ignoring
warning: No executable has been specified and target does not support
determining executable automatically.  Try using the "file" command.
Error in sourced command file:
Truncated register 16 in remote 'g' packet
```

`Truncated register 16` 的意思是「寄存器包里的第 16 号寄存器比预期短」，
根因是这个 GDB 按 x86-64 的寄存器表去解析 ARM 的数据。
**处置办法是换一个带 ARM 支持的 GDB**：ARM 官方的 GNU 工具链
（提供 `arm-none-eabi-gdb`）或 ST 的命令行工具集里都有。
`文档` 的说法是 Cortex-Debug 把「提供 `arm-none-eabi-gdb` 及相关工具的 ARM GCC 工具链」
列为它的运行前提之一（见 Cortex-Debug 扩展页面）。

本机的 WSL 里装的是 `gdb-multiarch`，它同时支持多种目标，
因此第 5 节的会话能跑通；换成 Windows 上的 VS Code 时，
这个 GDB 帮不上忙，因为管道方式要求 GDB 能启动那个 Windows 程序，
而跨系统的路径与进程启动是另一套问题。

## 8.9 参数里的空格把一条命令拆成了三条

在 PowerShell 里用 `Start-Process` 启动 OpenOCD 并传入 `-c` 参数时，
如果参数没有正确引用，`-c "gdb port 3333"` 会被拆成三个独立参数：

`实测数据`
`Text`

```text
Unexpected command line argument: port
```

OpenOCD 收到了 `-c gdb`，然后看到孤立的 `port`，于是拒绝启动。
处置办法有两种：把整条参数串写成一个带引号的字符串
（`'-c "gdb port 3333"'`），或者改用 `&` 直接调用并把每个参数分开写。
**`tasks.json` 里没有这个问题**：`args` 数组的每个元素天然是一个参数，
因此第 7.1 小节的写法是 `"-c", "gdb port 3333"` 两个元素。

## 8.10 换一块板要改什么（G431 实测）

第 2 节到第 8 节的板子是 STM32F103C8。把同一套流程换到一块 STM32G431 上，
要改的东西比预想的少：启动文件与链接脚本从本机已有的那份 G431 工程里取，
编译选项改四处，其余步骤照旧。这一小节记下那次换板的过程，以及两处「配置不等于实际」。

**先看这块板是什么**：

`实测数据`
`Text`

```text
Info : CMSIS-DAP: FW Version = 1.2.0
Info : CMSIS-DAP: Serial# = 6D656D6F7279
Info : clock speed 2000 kHz
Info : SWD DPIDR 0x2ba01477
Info : [stm32g4x.cpu] Cortex-M4 r0p1 processor detected
Info : [stm32g4x.cpu] target has 6 breakpoints, 4 watchpoints
Info : device idcode = 0x20036468 (STM32G43/G44xx - Rev 'unknown' : 0x2003)
Info : RDP level 0 (0xAA)
Info : flash size = 128 KiB
Info : flash mode : single-bank
pc (/32): 0x080078ac
sp (/32): 0x20008000
```

| 项目 | 值 | 依据 |
|---|---|---|
| 内核 | Cortex-M4 r0p1（有 FPU） | OpenOCD 识别 |
| 器件 | STM32G43/G44xx，idcode `0x20036468` | DP IDCODE |
| flash | 128 KiB，single-bank | `flash probe 0` |
| RAM | 32 KiB（SP = `0x20008000`） | `reg sp` |
| 读保护 | RDP level 0（`0xAA`），可自由烧写 | DP 读出 |
| 调试器 | CMSIS-DAP，SWD 2 MHz | OpenOCD 输出 |

flash 驱动识别成 `stm32l4x` 不是配错了：G4 与 L4 共用 flash 控制器，
OpenOCD 里就是这个驱动。

**要改的四处**：

| 项目 | F103 上的写法 | G431 上的写法 |
|---|---|---|
| 启动文件与链接脚本 | 素材里的 F103 版本（第 2.3 小节） | 从本机已有的 G431 工程里原样复制 `startup_stm32g431xx.s` 与 `STM32G431XX_FLASH.ld`（案例来源：`K:\Hardware\信息显示驱动-G431\`） |
| 内核与浮点 | `-mcpu=cortex-m3 -mthumb` | `-mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard` |
| 运行库 | `--specs=nosys.specs` 的空桩 | `--specs=rdimon.specs`，semihosting 由它提供（第 8.4 小节） |
| 目标配置 | `target/stm32f1x.cfg` | `target/stm32g4x.cfg` |

链接脚本里写的是 **128K flash 与 32K RAM**，与上面读出来的实测值一致，
因此这两个文件可以原样使用。

`Makefile`

```make
# Makefile   G431 侧的关键选项（节选）
CC      := arm-none-eabi-gcc
CPU     := -mcpu=cortex-m4 -mthumb
CFLAGS  := $(CPU) -O2 -Wall -Wextra -ffunction-sections -fdata-sections
LDFLAGS := $(CPU) -T STM32G431XX_FLASH.ld -Wl,--gc-sections --specs=rdimon.specs

g431_hard.elf: main.c startup_stm32g431xx.s
	$(CC) $(CFLAGS) -mfpu=fpv4-sp-d16 -mfloat-abi=hard $(LDFLAGS) $^ -o $@
```

烧写与第 4 节是同一套命令，只换目标配置：

`Bash`

```bash
openocd -f interface/cmsis-dap.cfg -f target/stm32g4x.cfg \
  -c "init" -c "reset halt" \
  -c "flash write_image erase <纯 ASCII 路径>/g431_hard.elf" \
  -c "verify_image <纯 ASCII 路径>/g431_hard.elf" \
  -c "arm semihosting enable" \
  -c "reset run" -c "sleep 3000" -c "halt" -c "shutdown"
```

`实测数据`
`Text`

```text
wrote 32736 bytes from file .../g431_hard.elf in 1.246965s (25.637 KiB/s)
verified 32732 bytes in 0.321009s (99.576 KiB/s)
semihosting is enabled
g431 bringup ok
```

产物先复制到纯 ASCII 路径再烧，理由见第 8.7 小节。

**第一处「配置不等于实际」**：工程里的 `.ioc` 勾的是 HSE 8 MHz
（`RCC.HSE_VALUE=8000000`，PF0 与 PF1 配成 `HSE-External-Oscillator`），
但它生成的 `SystemClock_Config()` 用的是 HSI：

`C`

```c
/* main.c（节选，案例来源：K:\Hardware\信息显示驱动-G431\Core\Src\main.c 第 142 至 166 行）
   .ioc 里勾的是 HSE，生成出来的却是 HSI 加 PLL。 */
RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
RCC_OscInitStruct.HSIState = RCC_HSI_ON;
RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
RCC_OscInitStruct.PLL.PLLM = RCC_PLLM_DIV1;
RCC_OscInitStruct.PLL.PLLN = 18;
RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;
if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK) { Error_Handler(); }
```

16 MHz ÷ 1 × 18 ÷ 2 = **144 MHz**，与下面实测的落地值一致。
**`.ioc` 是配置，生成的代码才是实际**；两者不一致时，以代码与寄存器读数为准。

**第二处「配置不等于实际」**：`.ioc` 里写 HSE 是 8 MHz，而这颗晶振实测是 **24.01 MHz**。
`HSERDY` 只能证明「有晶振」，证明不了「多大」——频率要靠墙钟测，见下面那张表。
**以实测为准**，并把 `.ioc` 的时钟页改过来。

**时钟树的落地值**：

`实测数据`
`Text`

```text
PLLCFGR     = 0x01001202   （PLLSRC = HSI、PLLM 字段 0 = ÷1、PLLN = 18、PLLR 字段 0 = ÷2、PLLREN 置位）
SWS         = 0xc          （PLL 已接管 SYSCLK）
FLASH->ACR  = 0x00040704   （LATENCY = 4WS、PRFTEN 与 ICEN/DCEN 都开）
PWR->CR1    = 0x00000200   （VOS = 01 = range 1）
```

`PWR_CR1_VOS_0` 不是 0 号位，它是 VOS 字段的 01 取值（`0x200`），
在 HAL 里对应 `PWR_REGULATOR_VOLTAGE_SCALE1`，即 range 1。
144 MHz 用 range 1 就够，超过 150 MHz 才需要 boost。
flash 等待周期取 `FLASH_LATENCY_4`，预取与两个 cache 都打开。

**周期数不随频率变**。同一段循环分别在 16 MHz 与 144 MHz 下各跑一遍
（20,000 次迭代，`DWT->CYCCNT` 计周期，`-O2`）：

`实测数据`

| 每次迭代周期数 | HSI 16 MHz | PLL 144 MHz |
|---|---:|---:|
| `int` 加法 | 9.00 | 9.00 |
| `int` 除常量 3（编译期换成魔数乘法） | 14.92 | 14.92 |
| `float` 乘加 | 11.00 | 11.00 |
| `double` 乘加 | 166.00 | 166.01 |

前三行逐位相同，这直接说明 `DWT->CYCCNT` 数的是周期、不是时间。
最后一行差 0.01，落在 `double` 那一行本身的抖动范围内
（同一段代码重编一次，它会在 165 与 166 之间跳 1 个周期），不是频率带来的差。

**`int` 除常量 3 这一行名不副实**：除数是编译期常量，gcc 把它换成了魔数乘法加符号修正。

`实测数据`
`Text`

```text
0x080096de:  smull  r6, r3, r0, r1
0x080096e2:  sub.w  r3, r3, r1, asr #31
```

G431 上生成的是这两条指令；除数要来自运行期，这一行才是除法的代价，G431 侧的运行期除数**待补**。

**墙钟怎么测**。用固定 `-c "sleep <毫秒>"` 收尾是测不出时间的：
墙钟会被那个固定的睡眠盖住，程序跑多久测出来都一样。
改成让程序在结尾写一个哨兵变量，OpenOCD 一边让它跑、一边用 `mdw` 轮询：

`C`

```c
/* delay.c（节选）    程序最后一行写哨兵；地址由 arm-none-eabi-nm 读出（本例为 0x20000a90） */
volatile uint32_t delay_done = 0;
/* …工作循环… */
delay_done = 0xDEADBEEFu;
```

`Text`

```tcl
# waitdone.cfg    等哨兵出现再收尾
proc wait_done {} {
    for {set n 0} {$n < 60000} {incr n} {
        set v [lindex [mdw 0x20000a90] 1]
        if {$v eq "deadbeef"} {
            echo "SENTINEL-OK polls=$n"
            return 0
        }
        sleep 5
    }
    echo "SENTINEL-TIMEOUT"
    return 1
}
```

`PowerShell`

```powershell
# 主机侧计时：加载 waitdone.cfg，由 wait_done 轮询哨兵
Measure-Command {
  & openocd -f interface/cmsis-dap.cfg -f target/stm32g4x.cfg `
    -c "init" -c "reset halt" `
    -c "flash write_image erase C:/g431lab/delay-hsi.elf" `
    -c "verify_image C:/g431lab/delay-hsi.elf" `
    -c "arm semihosting enable" `
    -c "reset run" -f C:/g431lab/waitdone.cfg -c "wait_done" -c "halt" -c "shutdown"
}
```

轮询本身有代价：目标在跑的时候一次 `mdw` 大约 10 ms，一次 `sleep 5` 加一次读约 15 ms，
这就是墙钟分辨率的上限。因此**工作循环要跑得够长**，短了会被这个粒度吞掉。

`实测数据`

| 产物（`WORK` = 96,000,000，`cycles` ≈ 8.64 亿） | 时钟 | 墙钟（主机） | 减 baseline | 实测频率 |
|---|---|---:|---:|---:|
| `delay-base.elf` | HSI 16 MHz | 2.682 s | — | — |
| `delay-base.elf`（第二次） | HSI 16 MHz | 2.618 s | — | — |
| `delay-hsi-long.elf` | HSI 16 MHz | 56.586 s | 53.936 s | **16.02 MHz** |
| `delay-hse-long.elf` | HSE | 38.643 s | 35.993 s | **24.01 MHz** |
| `delay-pll144-long.elf` | PLL 144 MHz | 8.632 s | 5.982 s | **144.43 MHz** |

baseline 取两次的平均 2.650 s，实测频率 = 程序打印的 `cycles` ÷（墙钟 − baseline）。

**短版的数据为什么不可用**：把工作量缩到四分之一时，减掉 baseline 之后 PLL 那一行只剩 1.53 s，
而 baseline 自己在两次运行之间就从 2.665 s 晃到 2.596 s（差 0.069 s），
摊到 1.53 s 上就是 ±4.5%，测出来是 141.0 MHz。
**141 不是芯片真的跑 141 MHz，是误差被放大四倍的结果**；
长版测出的 144.43 MHz 与配置值 144 MHz 相符。

**换一块板要检查的事**：

| 检查项 | 为什么查它 |
|---|---|
| 晶振有没有、多少 MHz | 时钟树按它算，而 `.ioc` 里的值未必是板上那颗；本次实测 24.01 MHz，与 `.ioc` 写的 8 MHz 不符 |
| 复位线是否引出 | 程序把 SWJ 关掉时，`connect_assert_srst` 要靠复位引脚在连接期间拉住复位（第 8.2 小节） |
| 有没有读保护 | RDP 不为 0 时烧写会被拒；本次读到 RDP level 0（`0xAA`），可自由烧写 |
| 只有一块调试器时 | 绝不加载另一块板的目标配置，理由见下 |

最后一条的后果值得单独写：`target/stm32f1x.cfg` 配到 G431 上时，
SWD 侧确实认得出 DPIDR 不同（`0x2ba01477` 对 `0x1ba01477`），
但 flash 驱动会照着配置里写的地址与容量往下写——**镜像会被写进手里这块芯片**。
这台调试器读不出 USB 序列号
（`could not read serial number for device 0x0416:0x5021: Entity not found`），
`adapter serial` 那条路走不通，两块板又不可能同时接上，
因此**一次只操作一块板、换板靠人工换插**是这里的操作规程。

> [!NOTE]
> 换板要改的是四件事：启动文件与链接脚本、`-mcpu` 与浮点选项、运行库的 `--specs`、目标配置。
> 这一次真正花时间的不在这四件里，而在两处「配置不等于实际」：
> `.ioc` 写的时钟源与生成的代码不一致，`.ioc` 写的晶振频率与板上的实物不一致。
> 两者都只能靠读寄存器与测频率来定，拿到新板先做这两步。

---

# 第 9 节 与机制章节的对应关系

本章节只讲工具怎么用，机制在别的章节。这张表用来回答
「这个现象该去哪一章看」。

| 本章节的现象 | 机制所在 | 章节 |
|---|---|---|
| `_estack` 等于 RAM 顶端，向量表前两个字是栈顶与复位入口 | 链接脚本的 `MEMORY`、`.isr_vector` 的位置 | 《06-更底层/07-链接脚本与启动代码.md》第 2.1、2.4 小节 |
| `Reset_Handler` 里先搬 `.data`、再清 `.bss` | 启动代码逐条 | 《06-更底层/07-链接脚本与启动代码.md》第 3.2 至 3.4 小节 |
| `SystemInit()` 与 `_init()` 是空函数也能链接过 | 裸机侧只有两层入口，`__libc_init_array` 的作用 | 《06-更底层/08-CRT 与程序启动.md》第 1.4、2.4 小节 |
| `printf` 要靠 `_write` 之类的桩 | 系统调用是程序与运行库的边界 | 《06-更底层/08-CRT 与程序启动.md》第 1.4 小节 |
| semihosting 用 `bkpt 0xAB` 与寄存器传参 | `volatile` 与寄存器操作的现场 | 《06-更底层/03-寄存器、位与 volatile.md》第 2.4 小节 |
| `LENGTH` 写大了不报错，复位后进 HardFault | 链接脚本写错的三类后果 | 《06-更底层/07-链接脚本与启动代码.md》第 5.3、5.5 小节 |
| 烧写、校验、读回的命令 | 真板那一节 | 《01-编译器/03-嵌入式与交叉编译.md》第 9 节 |
| `tasks.json` 与 `launch.json` 的通用字段 | 环境配置章节 | 《01-编译器/02-环境配置.md》第 5.2、5.3、5.7 小节 |
| 断点、单步、看变量的操作方式 | 调试器的使用 | 《02-调试器/01-原理与使用.md》第 8 节 |
| CMake 预设与工具链文件 | CMake 工程的组织 | 《03-构建工具链/01-构建工具链.md》第 4.8 小节 |
| Ninja 为什么快、别名目标 | 构建执行器 | 《03-构建工具链/02-构建执行器.md》第 4 节 |

> [!NOTE]
> 这一章的五篇到此形成一个闭环：《01-编译器/00-语言的实现.md》讲标准与实现的分工，
> 《01-编译器/01-编译与链接.md》讲编译与链接做了什么，
> 《01-编译器/02-环境配置.md》讲本机环境怎么搭，
> 《01-编译器/03-嵌入式与交叉编译.md》讲交叉编译与真板，
> 本章节讲把这些放进一个可以重复执行的工程里。
> 再往下是机制层，在 `06-更底层` 板块。

---

# 附录 A 实测环境与复现

## A.1 环境

`实测数据`

| 项 | 值 |
|---|---|
| 芯片 | STM32F103C8（中容量 F103，Cortex-M3 r1p1），flash 64 KiB，RAM 20 KiB |
| 调试器 | CMSIS-DAP，FW 1.2.0，Serial `6D656D6F7279`，SWD 时钟 1000 kHz |
| 交叉编译器 | `arm-none-eabi-gcc` 13.2.1（`15:13.2.rel1-2`），在 WSL 的 Ubuntu 里 |
| 构建工具 | CMake 3.28.3、Ninja 1.11.1、GNU Make 4.3，同一环境 |
| GDB | `gdb-multiarch` 15.1，同一环境 |
| 烧写与调试服务器 | OpenOCD 0.12.0（2025-12-11），Windows 侧 |
| 生成工程 | STM32CubeMX 6.15.0，Windows 侧 |
| 工程所在路径 | 含中文的目录，例如 `<盘符>:\C相关课程\...`；WSL 侧为 `/mnt/<盘符小写>/C相关课程/...` |

## A.2 复现顺序

下面这条顺序与第 3 至 6 节一致，每一步都能单独验证。

`Bash`

```bash
# 1. 构建（WSL）
make                                    # 或 cmake --preset debug && cmake --build --preset debug
arm-none-eabi-size build/main.elf
arm-none-eabi-nm build/main.elf | grep -E " (_estack|g_total|main|sum_to)$"
```

`PowerShell`

```powershell
# 2. 识别芯片
& "<OpenOCD>\bin\openocd.exe" -s "<OpenOCD>/share/openocd/scripts" `
  -f interface/cmsis-dap.cfg -f target/stm32f1x.cfg `
  -c "init" -c "reset halt" -c "flash probe 0" -c "shutdown"

# 3. 烧写、校验、运行、读回
& "<OpenOCD>\bin\openocd.exe" -s "<OpenOCD>/share/openocd/scripts" `
  -f interface/cmsis-dap.cfg -f target/stm32f1x.cfg `
  -c "init" -c "reset halt" -c "arm semihosting enable" `
  -c "flash write_image erase <工作区>/build/main.elf" `
  -c "verify_image <工作区>/build/main.elf" `
  -c "reset run" -c "sleep 1500" -c "halt" `
  -c "mdw 0x08000000 4" -c "mdw 0x20000000 4" -c "reg pc" -c "reg msp" `
  -c "shutdown"
```

`Bash`

```bash
# 4. GDB 会话（WSL 里执行；管道方式，OpenOCD 由 GDB 自己拉起）
gdb-multiarch -q -batch -x gdb.cmd build/main.elf
```

`PowerShell`

```powershell
# 5. CubeMX 无界面生成（脚本内容见第 6.1 小节）
& "<CubeMX>\STM32CubeMX.exe" -q "<工作区>\mx_script.txt"
```

## A.3 主要的实测数字一览

`实测数据`

| 项 | 数字 |
|---|---|
| 最小工程体积 | text 848、data 0、bss 1544，合计 2392 字节 |
| `_estack` | `0x20005000` |
| 向量表前两个字 | `20005000`、`080002e9` |
| 写 848 字节 | 0.136 s（约 6.1 KiB/s） |
| 校验 848 字节 | 0.050 s（约 16.5 KiB/s） |
| 程序跑完三圈后的 `g_total` | `0x000013ba`，即 5050 |
| CubeMX 生成工程耗时 | 19.2 s（含启动） |
| CubeMX 工程构建耗时 | 15.5 s |
| CubeMX 工程体积 | RAM 1584 B（7.73%）、FLASH 3776 B（5.76%） |
| 写 3776 字节 | 0.353 s（约 10.4 KiB/s） |
| 同一段等待循环的周期数 | 8 MHz 下 72,000,034；72 MHz 下 72,000,044 |
| 同一段等待循环的墙钟 | 8 MHz 下约 9 s；72 MHz 下约 1 s |

## A.4 本章节用到的工程文件

| 文件 | 在第几节出现 | 说明 |
|---|---|---|
| `main.c` | 第 3.2 小节 | 被测程序，含 semihosting 打印与一个可下断点的函数 |
| `Makefile` | 第 3.2 小节 | 构建规则，四个目标 |
| `CMakeLists.txt` | 第 3.3 小节 | 同一件事的 CMake 写法 |
| `cmake/gcc-arm-none-eabi.cmake` | 第 3.3 小节 | 工具链描述 |
| `CMakePresets.json` | 第 3.3 小节 | 预设，命令行与 VS Code 共用 |
| `gdb.cmd` 与 `gdb.sh` | 第 5.2 小节 | GDB 命令脚本与启动脚本 |
| `tasks.json` 与 `launch.json` | 第 7 节 | VS Code 的两个配置 |

这两个配置文件与 `gdb.cmd` 放在同一层目录下，
`launch.json` 里的相对路径因此不必再加前缀。

---

# 附录 B 官方资料

| 资料 | 内容 | 地址或位置 |
|---|---|---|
| UM1718 Rev 48 | STM32CubeMX 用户手册，§3.3.2 与 Table 1 是命令行模式的全部依据 | CubeMX 安装目录下的 `help\UM1718.pdf` |
| STM32 VS Code 扩展页面 | 扩展的功能、架构变化、调试后端与支持的调试器 | <https://marketplace.visualstudio.com/items?itemName=stmicroelectronics.stm32-vscode-extension> |
| Cortex-Debug 扩展页面 | 支持的 GDB 服务器、运行前提、semihosting 输出的位置 | <https://marketplace.visualstudio.com/items?itemName=marus25.cortex-debug> |
| OpenOCD 用户手册 | 命令与配置的完整说明 | <https://openocd.org/doc/html/index.html> |
| Arm GNU 工具链 | Windows 与 Linux 上的 `arm-none-eabi-gcc`、`arm-none-eabi-gdb` | <https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads> |
| STM32CubeMX 下载 | 版本与安装说明 | <https://www.st.com/zh/development-tools/stm32cubemx.html> |
| STM32CubeCLT 下载 | 命令行工具集 | <https://www.st.com/zh/development-tools/stm32cubeclt.html> |
| STM32F10xxx 参考手册 | 寄存器地址与位定义（第 8.5 小节的 RCC 与 FLASH 寄存器由此而来） | ST 官网，文档编号 RM0008 |

`文档`

> "NOTE: If a chip vendor ships it's own OpenOCD version, for sure use NOTHING but that"
>
> —— Cortex-Debug 扩展页面（安装要求一节）

这条提示对本机的情况不适用：这里的板子是通用最小系统板，
ST 并没有为它提供 OpenOCD，用的是社区构建的 0.12.0 版本。
选用与芯片厂商配套的 OpenOCD 是另一类场合下的建议，
因为配置脚本与目标的对应关系由厂商维护。
