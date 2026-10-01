# 示例 `06-lower-level/03-bare-metal-boot` · 自己的链接脚本与向量表

一个不用 CubeMX、不用 HAL 的 Cortex-M 最小工程：**自己的链接脚本**加**自己的启动文件**，
加一个把「`Reset_Handler` 到底做了什么」一条条验出来的程序。
它可以跑在 QEMU 里，也可以烧进 STM32F103C8 最小系统板，两条路线都能跑完整套自测。

工程的重点不是「点亮一个灯」，而是**把上电到 `main` 之间的每一步都变成可验证的事实**：
向量表第 0 个字是不是 `_estack`、第 1 个字的低位是不是 1、
`.data` 的初值是不是真的从 flash 搬到了 RAM、`.bss` 是不是真的被清零。

`Text`

```text
03-bare-metal-boot/
  linker/STM32F103C8_FLASH.ld  链接脚本：64 KiB flash / 20 KiB RAM，.isr_vector 钉在最前
  linker/startup_stm32f103xe.s 真实启动文件：向量表、Reset_Handler、弱符号兜底处理函数
  src/boot_demo.c              核心程序：22 项自测，结论写进固定的 RAM 地址
  src/semihosting.h            semihosting 打印：最小板上唯一的输出通道
  scripts/build.sh             在 WSL 里交叉编译、看段表与 Reset_Handler、在 QEMU 里跑
  scripts/flash.ps1            烧真板并读回结论（一次 openocd，跑完停在 halt）
```

`boot_demo.c` 是单文件核心，没有命令行版：这个工程的「命令行」就是
`arm-none-eabi-gcc` 加 `qemu-system-arm`（或 `openocd`），
因此不经过 CMake，用脚本与手写命令两种方式都能构建。

## 先读教材

| 章节 | 讲的是什么 | 本示例用在哪里 |
|---|---|---|
| 《06-更底层/07-链接脚本与启动代码.md》第 1.2 小节 | 两套地址：VMA 与 LMA | 自测第 9 项：`_sidata` 与 `_sdata` 不相等 |
| 《06-更底层/07-链接脚本与启动代码.md》第 1.4 小节 | 段表：先看清结果 | `scripts/build.sh` 第 2 段打印出的那张表 |
| 《06-更底层/07-链接脚本与启动代码.md》第 1.5 小节 | 真板上的同一件事 | `scripts/flash.ps1` 读回的复位向量与结果区 |
| 《06-更底层/07-链接脚本与启动代码.md》第 2.1 小节 | `ENTRY` 与 `MEMORY` | `linker/STM32F103C8_FLASH.ld` 的头两段 |
| 《06-更底层/07-链接脚本与启动代码.md》第 2.4 小节 | `.isr_vector` 为什么必须放在最前面 | 自测第 1 至 3 项：直接读 `0x08000000` |
| 《06-更底层/07-链接脚本与启动代码.md》第 2.6 小节 | `.data` 与那句 `AT> FLASH` | 自测第 8、9、12 项 |
| 《06-更底层/07-链接脚本与启动代码.md》第 2.7 小节 | `.bss` 与 `NOLOAD` | 自测第 10、11、14 项 |
| 《06-更底层/07-链接脚本与启动代码.md》第 3.1 小节 | 在第一条指令之前：硬件做了什么 | 向量表第 0、1 个字就是硬件取的两样东西 |
| 《06-更底层/07-链接脚本与启动代码.md》第 3.3 小节 | 把 `.data` 从 flash 搬到 RAM | `scripts/build.sh` 第 4 段的反汇编 |
| 《06-更底层/07-链接脚本与启动代码.md》第 3.4 小节 | 把 `.bss` 清零 | 自测第 10 项 |
| 《06-更底层/07-链接脚本与启动代码.md》第 4 节 | 从复位到 `main`：与通用 CRT 的分工 | 本工程没有 `__libc_init_array`，因此没有全局构造函数 |
| 《06-更底层/07-链接脚本与启动代码.md》第 5.5 小节 | 真板：`LENGTH` 照抄错，复位后立刻 HardFault | 为什么这份脚本写的是 64K / 20K |

## 这个项目要解决什么问题

**上电那一刻，硬件只认两个字。** 复位后，Cortex-M 从 flash 的第一个字节起
读两个 32 位字：第 0 个字装进 SP，第 1 个字装进 PC。
其余什么都不做。所以向量表必须钉在 `0x08000000`，
而 `.isr_vector` 也必须是链接脚本里第一个输出段。自测前 3 项验的就是这两件事：

`实测数据`
`Text`

```text
  [0] 0x20005000      ← 栈顶，等于 RAM 的最高地址
  [1] 0x0800032d      ← Reset_Handler，低位为 1 表示 Thumb 态
  [2] 0x08000375      ← NMI
  [3] 0x08000375      ← HardFault
```

**`.data` 与 `.bss` 是两件不同的事，靠两个不同的机制解决。**
`.data` 有初值，初值必须待在 flash 里（掉电不丢），运行时又必须在 RAM 里（可写），
于是链接脚本给它两套地址：VMA 在 RAM、LMA 在 flash，`AT> FLASH` 就是这件事。
`.bss` 没有初值，干脆不占 flash，启动时清零即可。

`实测数据`
`Text`

```text
    20000010 B _ebss
    20000004 D _edata
    20005000 R _estack
    20000004 B _sbss
    20000000 D _sdata
    08000ca0 A _sidata      ← 同一个 .data，LMA 在 flash 的 0x08000ca0
    2000000c B g_from_bss
    20000000 D g_from_data
```

**内存大小抄错不是「结果不对」，是连 `main` 都进不去。**
STM32F103C8 是 64 KiB flash / 20 KiB RAM；素材里那份链接脚本是给
F103xC（256 KiB / 48 KiB）的，照抄之后 `_estack` 会落到 `0x2000C000`，
**超出 20 KiB RAM**，复位后第一次压栈就越界，直接进 HardFault。
本示例的脚本因此写的是 64K / 20K。

**最小板上没有串口。** 输出走 semihosting：程序执行 `bkpt 0xAB`，
调试器截住它并按功能号办事。QEMU 加 `-semihosting-config`，
OpenOCD 加 `arm semihosting enable`，同一份程序两边都能打印。

**打印不出来的时候，结论还能留在 RAM 里。** 程序把 22 项的结果写进固定的
`0x20004000`，调试器一条 `mdw` 就能读：

`实测数据`
`Text`

```text
0x20004000: 600d0703 00000016 00000016 20005000 20000000 08000ca0 20000004 20000010
             magic    pass=22   total=22  _estack   _sdata    _sidata   _sbss     _ebss
```

## 做完能掌握什么

- 会读一份链接脚本，说清 `MEMORY` / `SECTIONS` / `AT>` / `KEEP` 各管什么
- 会读 `objdump -h` 的段表，分清「占文件」与「占内存」
- 会说清 `Reset_Handler` 每一步在干什么，以及哪些事是硬件替它做的
- 会把「启动代码做对了没有」写成可执行的自测，而不是靠肉眼看内存
- 会在 QEMU 与真板上跑同一份镜像，并知道两者的差别在哪
- 会用 semihosting 打印，也会用固定 RAM 地址留结论给调试器读

## 构建与运行

这个工程不经过 CMake。交叉编译与 QEMU 在 WSL 的 Ubuntu 里跑：

`Bash`

```bash
# 在示例目录下：编译 + 看段表 + 看 Reset_Handler + 在 QEMU 里跑
wsl -d Ubuntu -e bash "/mnt/k/C相关课程/B-examples/06-lower-level/03-bare-metal-boot/scripts/build.sh"
```

等价的单条命令：

`Bash`

```bash
# 交叉编译（Cortex-M3 / thumb / 自己的链接脚本与启动文件）
arm-none-eabi-gcc -mcpu=cortex-m3 -mthumb -O2 -g -Wall -Wextra \
  -nostartfiles --specs=nosys.specs \
  -T linker/STM32F103C8_FLASH.ld linker/startup_stm32f103xe.s src/boot_demo.c \
  -o build/boot_demo.elf

# 段表：.isr_vector 在 0x08000000，.data 有两套地址
arm-none-eabi-objdump -h build/boot_demo.elf

# Reset_Handler 逐条
arm-none-eabi-objdump -d --disassemble=Reset_Handler build/boot_demo.elf

# 在 QEMU 里真跑
qemu-system-arm -M netduinoplus2 -kernel build/boot_demo.elf -nographic \
  -semihosting-config enable=on,target=native
```

真板那一条（在 Windows 侧，板子与 CMSIS-DAP 已插好）：

`PowerShell`

```powershell
# 在示例目录下
pwsh -File scripts\flash.ps1
```

它做的事与下面这条命令等价，只是把四步合成一次 `openocd`：

`PowerShell`

```powershell
# OpenOCD 的 -c 按 Tcl 规则解析，路径一律用正斜杠
& "H:\OpenOCD\bin\openocd.exe" -s "H:\OpenOCD\share\openocd\scripts" `
  -f interface/cmsis-dap.cfg -f target/stm32f1x.cfg `
  -c "init" -c "reset halt" -c "arm semihosting enable" `
  -c "flash write_image erase K:/.../build/boot_demo.elf" `
  -c "reset run" -c "sleep 12000" -c "halt" `
  -c "mdw 0x20004000 8" -c "mdw 0x08000000 2" -c "shutdown"
```

改成 MSVC 那一套没有意义：这份程序编给 Cortex-M3，主机侧的编译器不参与。

## 运行后应当看到什么

`scripts/build.sh` 在 QEMU 里的输出（节选，完整 22 项）：

`实测数据`
`Text`

```text
=== 2. 段表：.isr_vector 钉在 flash 起始，.data 有两套地址
    Sections:
    Idx Name          Size      VMA       LMA       File off  Algn
      0 .isr_vector   000001e4  08000000  08000000  00001000  2**0
                      CONTENTS, ALLOC, LOAD, READONLY, DATA
      1 .text         000005a4  080001e4  080001e4  000011e4  2**2
                      CONTENTS, ALLOC, LOAD, READONLY, CODE
      2 .rodata       00000518  08000788  08000788  00001788  2**2
                      CONTENTS, ALLOC, LOAD, READONLY, DATA

=== 5. 在 QEMU 里真跑（semihosting 输出接到 stdout）
    boot_demo on Cortex-M3 (STM32F103C8: 64 KiB flash / 20 KiB RAM)
    --- 向量表的前 8 个字 ---
      [0] 0x20005000
      [1] 0x0800032d
      [2] 0x08000375
      [3] 0x08000375
      [4] 0x08000375
      [5] 0x08000375
      [6] 0x08000375
      [7] 0x00000000
    --- 自测 ---
    [PASS] 1. vector[0] is _estack (the linker script put it there)
    [PASS] 2. vector[1] has bit 0 set (Thumb entry)
    [PASS] 3. vector[1] is Reset_Handler | 1
    [PASS] 4. _estack equals the top of RAM (0x20005000)
    [PASS] 5. _estack is 8-byte aligned (AAPCS requires it)
    [PASS] 6. g_from_data holds its initial value
    [PASS] 7. g_from_data lives in RAM (that is what .data means)
    [PASS] 8. the same value is still readable in flash at _sidata
    [PASS] 9. _sidata and _sdata differ: that is what AT> FLASH buys
    [PASS] 10. g_from_bss is zero (cleared by Reset_Handler)
    [PASS] 11. g_from_bss lives in RAM
    [PASS] 12. _sidata is inside flash (LMA of .data)
    [PASS] 13. .data VMA range is inside RAM
    [PASS] 14. .bss range is inside RAM
    [PASS] 15. .bss starts at or after the end of .data (no overlap)
    [PASS] 16. .bss ends below the stack top
    [PASS] 17. Reset_Handler runs from flash
    [PASS] 18. main runs from flash
    [PASS] 19. the string literal lives in flash, not in RAM
    [PASS] 20. the string literal is readable
    [PASS] 21. the current stack pointer is inside RAM
    [PASS] 22. the current stack pointer is at or below _estack
    --- 结果 ---
    result block at 0x20004000: pass=22 total=22
    _estack=0x20005000  _sdata=0x20000000  _sidata=0x08000ca0
    _sbss=0x20000004  _ebss=0x20000010
    22 项中 22 项通过，全部通过
    ALL DONE
```

### 真板上的同一份镜像

`实测数据`
`PowerShell`

```powershell
pwsh -File scripts\flash.ps1
```

`实测数据`
`Text`

```text
Info : device id = 0x20036410
Info : flash size = 64 KiB
Warn : Adding extra erase range, 0x08000ca4 .. 0x08000fff
auto erase enabled
wrote 3236 bytes from file K:/.../build/boot_demo.elf in 0.327962s (9.636 KiB/s)
boot_demo on Cortex-M3 (STM32F103C8: 64 KiB flash / 20 KiB RAM)

--- 向量表的前 8 个字 ---

  [0] 0x20005000
  [1] 0x0800032d
  ...
--- 自测 ---
[PASS] 1. vector[0] is _estack (the linker script put it there)
...
[PASS] 22. the current stack pointer is at or below _estack

--- 结果 ---

result block at 0x20004000: pass=22 total=22

_estack=0x20005000  _sdata=0x20000000  _sidata=0x08000ca0

_sbss=0x20000004  _ebss=0x20000010

22 项中 22 项通过，全部通过

ALL DONE

== result block in RAM (0x20004000) ==
0x20004000: 600d0703 00000016 00000016 20005000 20000000 08000ca0 20000004 20000010
== reset vector (first two words of flash) ==
0x08000000: 20005000 0800032d
== .data initial value still in flash at its LMA ==
0x08000ca0: 1234abcd ffffffff
pc (/32): 0x08000694
msp (/32): 0x20004fc8
shutdown command invoked
```

三组数字互相对得上：

| 位置 | QEMU（`netduinoplus2`） | 真板（STM32F103C8） |
|---|---|---|
| 复位向量第 0 个字 | `0x20005000` | `0x20005000` |
| 复位向量第 1 个字 | `0x0800032d` | `0x0800032d` |
| `_sidata`（.data 的 LMA） | `0x08000ca0` | `0x08000ca0` |
| flash 里 `_sidata` 处的值 | `0x1234abcd` | `0x1234abcd` |
| 自测 | 22 项全通过 | 22 项全通过 |

两边一致的原因是**地址与布局由链接脚本决定，与机型无关**；
QEMU 的 `netduinoplus2` 与 STM32F103 的 flash 都在 `0x08000000`、
RAM 都在 `0x20000000`，因此同一份镜像两边都跑得起来。
**涉及具体外设的结论不能这样推广**——本示例一个外设也没碰。

## 代码里哪几处是要点

| 位置 | 要点 |
|---|---|
| `linker/STM32F103C8_FLASH.ld` 的 `MEMORY` | 64K flash / 20K RAM，照这块板写；照抄大容量型号会让栈顶落到 RAM 之外 |
| `linker/STM32F103C8_FLASH.ld` 的 `.isr_vector` 段 | 用 `KEEP` 钉住，否则链接器可能把没人引用的向量表当垃圾回收 |
| `linker/STM32F103C8_FLASH.ld` 的 `>RAM AT> FLASH` | `.data` 的 VMA 在 RAM、LMA 在 flash，全靠这一句 |
| `linker/startup_stm32f103xe.s` 的向量表 | 真实启动文件，表项带 `weak` 别名；本示例只用到复位与兜底两个 |
| `linker/startup_stm32f103xe.s` 的 `Reset_Handler` | 设栈是硬件做的，它只负责搬 `.data`、清 `.bss`、调 `__libc_init_array`、进 `main` |
| `src/boot_demo.c` 的 `extern uint32_t _estack;` | 这些不是变量，是链接器算出来的地址；取地址就能验段的位置 |
| `src/boot_demo.c` 的 `RESULT_BASE` | 结果区放固定地址 `0x20004000`，卡死时调试器也能读回结论 |
| `src/boot_demo.c` 的 `check()` | 每一项都打印 `[PASS]`/`[FAIL]` 加序号，QEMU 与真板上格式一致 |
| `src/semihosting.h` 的 `sh_puts` | `bkpt 0xAB` 加 `SYS_WRITE0`；写成 `static inline` 是为了让这个示例只有两个源文件 |
| `scripts/build.sh` 的 `--specs=nosys.specs` | 提供 `_exit` 等桩函数，省掉半主机之外的系统调用 |
| `scripts/flash.ps1` 的 `sleep 12000` | semihosting 每次 `bkpt` 都要跟调试器来回一趟，比 QEMU 慢得多 |

## 已知问题

- **链接脚本文件头的注释是过时的。** 它仍写着「256Kbytes FLASH and 48Kbytes RAM」，
  那是 ST 生成时的原文；实际的两行 `LENGTH` 已按这块板改成 `64K` 与 `20K`。
  这份脚本是第三方文件，文件头的版权与免责声明原样保留，未作改动。
- **QEMU 的 `netduinoplus2` 是 Cortex-M4 近似板。** flash 与 RAM 的地址与
  STM32F103 一致，因此同一份镜像两边都能跑；但它没有 STM32 的外设，
  凡涉及 USART、GPIO、定时器具体行为的结论都不能拿它当依据。本示例一个外设也没用。
- **结果区用的是固定地址 `0x20004000`。** 本示例的 `.bss` 只有 12 字节、
  栈也只有几百字节，撞不上；工程里更稳妥的做法是让链接脚本专门划一段出来，
  或者先查符号表再读。
- **真板上 semihosting 的输出要等。** 每次 `bkpt` 都要跟调试器来回一趟，
  22 项自测要十几秒才打印完。QEMU 里几乎瞬间跑完。
- **启动文件的 `__libc_init_array` 会被调用，但本程序没有全局构造函数。**
  它是 C 程序，因此这一步不产生可观察的效果；要观察它得看
  《06-更底层/08-CRT 与程序启动.md》。
- **`0x08000ca0` 这个 LMA 会随源码变化。** 它是 `.rodata` 之后的位置，
  改一行字符串就可能挪动；真板上要读它之前先在 `nm` 里确认一次。
