# 示例 `06-lower-level/02-volatile-and-registers` · 寄存器与 `volatile`

一份把「少了 `volatile` 会怎样」演三遍的命令行工具。
第一遍在主机上：同一段等待循环写成两份，只差一个关键字，
一份每次都真读，另一份在 `-O2` 下把读提到了循环之外。
第二遍看汇编：`-O0` 与 `-O2` 各反汇编一次，两条路径摆在一起。
第三遍上真板：把同一份源码编成镜像烧进 STM32F103C8，
用 `mww` 改内存、用 SysTick 触发，看主循环读不读得到。

三遍的证据指向同一句话：**`volatile` 不是性能开关，是语义要求**。

`Text`

```text
02-volatile-and-registers/
  include/volatile_regs.h   接口：模拟寄存器表、两种等待写法、带看门狗的探针、自测
  src/volatile_regs.c       实现，核心逻辑全在这里，命令行只调它
  src/main_cli.c            命令行版：认参数、调核心、按顺序打印
  board/volatile_flag.c     Cortex-M 版：真板与 QEMU 上跑的那份
  board/startup_min.s       精简启动文件：向量表、搬 .data、清 .bss
  board/stm32f103c8_min.ld  精简链接脚本：C8 的 64 KiB flash / 20 KiB RAM
  board/build.sh            在 WSL 里交叉编译四份镜像并在 QEMU 里跑
  tools/asm-compare.ps1     主机侧的 -O0 / -O2 汇编对照
  tools/board-run.ps1       烧写真板并读回结果（一次 openocd，跑完停在 halt）
  CMakeLists.txt            目标：core（静态库）、app_cli
  CMakePresets.json        mingw-gdb、mingw-release（-O2）与 msvc 三套预设
  .vscode/                 三个调试配置与九个构建任务
```

## 先读教材

| 章节 | 讲的是什么 | 本示例用在哪里 |
|---|---|---|
| 《06-更底层/03-寄存器、位与 volatile.md》第 1.1 小节 | 从 `gpio.c` 看真实写法 | `vr_reg_table` 里那四个模拟寄存器就是这套写法的抽象 |
| 《06-更底层/03-寄存器、位与 volatile.md》第 1.4 小节 | 一个必须用 `volatile` 的完整程序 | `vr_wait_volatile` 与 `vr_wait_plain` 这一对 |
| 《06-更底层/03-寄存器、位与 volatile.md》第 2.1 小节 | 两份 x86-64 汇编 | `tools/asm-compare.ps1` 打出来的就是这两份 |
| 《06-更底层/03-寄存器、位与 volatile.md》第 2.2 小节 | 同一份源码在 Cortex-M 上 | `board/volatile_flag.c` 编出来的两份镜像 |
| 《06-更底层/03-寄存器、位与 volatile.md》第 2.3 小节 | 真机上的同一段代码：内存改了，程序不知道 | `tools/board-run.ps1` 里 A 与 B 两组结果 |
| 《06-更底层/03-寄存器、位与 volatile.md》第 2.4 小节 | 让 Cortex-M 打印出来：`semihosting` | `board/volatile_flag.c` 里的 `sh_puts` |
| 《06-更底层/03-寄存器、位与 volatile.md》第 2.7 小节 | 哪些地方必须写 `volatile` | 示例结尾的两条自测说明 |
| 《06-更底层/03-寄存器、位与 volatile.md》第 2.8 小节 | `volatile` 做不到的四件事 | 它不保证原子、不保证次序——那是《06-更底层/10-中断、并发与内存序.md》的事 |

## 这个项目要解决什么问题

**「结果对」不等于「读对了」。** `vr_sum_reads` 读同一个字 100 次再相加。
带 `volatile` 的那一份真读 100 次，不带的那一份在 `-O2` 下只读一次再乘 100。
两个和都是 100——**结果一模一样**。这就是这类错最难查的地方：
换成外设寄存器，丢掉的是 99 次状态变化，而程序只会给出一个看起来正常的数值。

**编译器假定没有别人在动这块内存。** 不带 `volatile` 时，编译器要证明
「循环体里没人写它」才敢把读提出来。空循环里当然没人写，于是它提了。
主机侧 `-O2` 编译出来的 `vr_wait_plain` 只有三条指令：

`Assembly`

```asm
00000000000000e0 <vr_wait_plain>:
  e0:	8b 05 10 00 00 00    	mov    0x10(%rip),%eax
  e6:	83 e0 01             	and    $0x1,%eax
  e9:	c3                   	ret
```

循环整个不见了。同一份源码把 `volatile` 加回去，`-O2` 下循环里每次都有
一条 `mov` 去读内存：

`Assembly`

```asm
00000000000000b0 <vr_wait_volatile>:
  b0:	31 c0                	xor    %eax,%eax
  b2:	eb 10                	jmp    c4 <vr_wait_volatile+0x14>
  c0:	39 c8                	cmp    %ecx,%eax
  c2:	73 14                	jae    d8 <vr_wait_volatile+0x28>
  c4:	8b 15 14 00 00 00    	mov    0x14(%rip),%edx        ← 每次都真读
  ca:	83 c0 01             	add    $0x1,%eax
  cd:	83 e2 01             	and    $0x1,%edx
  d0:	74 ee                	je     c0 <vr_wait_volatile+0x10>
  d2:	c3                   	ret
```

**真板上是同一回事。** Cortex-M 那一份的 `-O2` 反汇编里，
去掉 `volatile` 之后等待循环只剩一条 `b.n` 跳到自身：

`Assembly`

```asm
 800010e:	6879      	ldr	r1, [r7, #4]      ← 只读这一次
 8000110:	685c      	ldr	r4, [r3, #4]
 8000112:	b901      	cbnz	r1, 8000116 <main+0x7a>
 8000114:	e7fe      	b.n	8000114 <main+0x78>   ← 跳到自身，永远不再读
```

**「设备」可以是中断，也可以是调试器。** 本示例让 Cortex-M 那一份有两套触发器：
SysTick 中断在 8000 个周期后置位，或者由调试器 `mww <地址> 1` 直接改内存。
两套都能跑出同一个结论。

## 做完能掌握什么

- 会说清 `volatile` 到底阻止了哪一类优化，而不是只会背「防止编译器优化」
- 会用 `objdump -d --disassemble=<函数>` 把两个版本的汇编摆在一起对照
- 会用「另一个线程改内存」在主机上复现外设寄存器的语义，并给探针加看门狗
- 会读 Cortex-M 的向量表与自己写一个最小启动文件
- 会用 semihosting 在最小板上打印，用 `mww` 从调试器改目标内存
- 会区分「结果对不对」与「读了几次」——前者查不出这类错

## 构建与运行

`PowerShell`

```powershell
# 在 06-lower-level/02-volatile-and-registers 目录下
cmake --preset mingw-gdb
cmake --build --preset mingw-gdb

# 不带优化：两种写法的结果相同，看不出差别
build\mingw\bin\app_cli.exe

# 开优化：差别就出来了，本示例的结论在这一档
cmake --preset mingw-release
cmake --build --preset mingw-release
build\mingw-release\bin\app_cli.exe

# 只跑自测
build\mingw-release\bin\app_cli.exe --selftest

# 汇编对照（需要 MinGW 的 gcc 与 objdump 在 PATH 上）
pwsh -File tools\asm-compare.ps1
```

MSVC 那一条：

`PowerShell`

```powershell
cmake --preset msvc
cmake --build --preset msvc-debug
build\msvc\bin\Debug\app_cli.exe --selftest
```

不想用预设、也不经过 CMake，直接编：

`PowerShell`

```powershell
gcc -std=c17 -O2 -Wall -Wextra -Iinclude src\volatile_regs.c src\main_cli.c -o app_cli.exe
```

板级那一份（需要 WSL 的 Ubuntu 里装了 `arm-none-eabi-gcc` 与 `qemu-system-arm`）：

`Bash`

```bash
# 在示例目录下
wsl -d Ubuntu -e bash "/mnt/k/C相关课程/B-examples/06-lower-level/02-volatile-and-registers/board/build.sh"
```

等价的单条命令（四个变体各一条）：

`Bash`

```bash
# SysTick 触发 + 带 volatile
arm-none-eabi-gcc -mcpu=cortex-m3 -mthumb -O2 -g -nostartfiles --specs=nosys.specs \
  -DTRIGGER_SYSTICK -T stm32f103c8_min.ld startup_min.s volatile_flag.c -o flag_systick_vol.elf
# 同一个文件去掉 volatile
arm-none-eabi-gcc -mcpu=cortex-m3 -mthumb -O2 -g -nostartfiles --specs=nosys.specs \
  -DTRIGGER_SYSTICK -DNO_VOLATILE -T stm32f103c8_min.ld startup_min.s volatile_flag.c -o flag_systick_nv.elf

# 在 QEMU 里跑（semihosting 把输出接到 stdout）
qemu-system-arm -M netduinoplus2 -kernel flag_systick_vol.elf -nographic \
  -semihosting-config enable=on,target=native
```

## 运行后应当看到什么

`-O0` 那一档（`mingw-gdb` 预设）：

`实测数据`
`Text`

```text
== 同一个等待循环，两种写法 ==
  三行源码只差一个关键字，起一个新线程去等，本线程 50 ms 后把状态位置 1。
  带 volatile：                结果=读到了，跳出来  自旋=212840425 圈  用时=54 ms
  不带 volatile：              结果=读到了，跳出来  自旋=243389339 圈  用时=62 ms
  结论：本档没有开优化，两份都读到了。
        要看出差别得开优化：用 mingw-release 预设再跑一次。
```

`-O2` 那一档（`mingw-release` 预设），同一台机器、同一份源码：

`实测数据`
`Text`

```text
== 同一个等待循环，两种写法 ==
  三行源码只差一个关键字，起一个新线程去等，本线程 50 ms 后把状态位置 1。
  带 volatile：                结果=读到了，跳出来  自旋=335662952 圈  用时=65 ms
  不带 volatile：              结果=没读到，一直等  自旋=0 圈  用时=62 ms
  结论：带 volatile 的读到了；不带的那一份把读提到了循环之外，
        内存已经改了，循环里读到的仍是旧值。
```

`自旋=0 圈` 说的是「循环一圈都没转就返回了」——
编译器把整个循环删掉，直接返回「状态位还是 0」。自测的真实输出：

`实测数据`
`Text`

```text
== 自测 ==
  [通过] 1. 寄存器表有 4 个寄存器
  [通过] 2. VR_SR 的地址与 VR_CR 不同
  [通过] 3. VR_CR 是 4 字节对齐的
  [通过] 4. VR_DR 的复位值在表里写着
  [通过] 5. 带 volatile：等待循环没有卡死
  [通过] 6. 带 volatile：另一个线程改了状态位，循环读到了并跳出来
  [通过] 7. 不带 volatile：等待循环也在自旋上限内结束
  [通过] 8. 不带 volatile（本档开了优化）：状态位改了，循环却一直读的是旧值
  [通过] 9. 两种写法的结果不同，差别只在那一个关键字
  [通过] 10. 带 volatile 读 100 次，得到 100 次真实读的和
  [通过] 11. 不带 volatile 读 100 次，结果相同——正因为结果相同，这类错才不容易被发现
  [说明] 结果的差别在这里看不出来，差别在「读了几次」：
         不带 volatile 的版本在 -O2 下只读一次再乘 100。
         看汇编用 tools/asm-compare.ps1，或 objdump -d --disassemble=vr_sum_reads。

  自测结果：11 项中 11 项通过，全部通过
```

`-O0` 那一档是 10 项，第 8 项换成「本档没有开优化，两种写法看不出差别」，
自测同样全通过。

### 交叉编译与 QEMU

`board/build.sh` 编出四份镜像，段布局与前两份的反汇编：

`实测数据`
`Text`

```text
=== 段布局（.isr_vector 在 flash 起始，.data 两套地址）
Sections:
Idx Name          Size      VMA       LMA       File off  Algn
  0 .isr_vector   00000040  08000000  08000000  00001000  2**0
                  CONTENTS, ALLOC, LOAD, READONLY, DATA
  1 .text         000001d9  08000040  08000040  00001040  2**2
                  CONTENTS, ALLOC, LOAD, READONLY, CODE
  2 .bss          00000028  20000000  08000219  00002000  2**2
                  ALLOC

=== g_flag 的地址（真板上调试器 mww 要用）
  flag_mww_vol     20000004 B g_flag
  flag_mww_nv      20000004 B g_flag
```

QEMU 里跑 SysTick 那一对：

`实测数据`
`Text`

```text
--- flag_systick_vol.elf
    trigger: SysTick
    waiting for g_flag at 0x20000004
    escaped, cycles = 0
    ALL DONE
    qemu-system-arm: terminating on signal 15 from pid 420 (timeout)
--- flag_systick_nv.elf
    trigger: SysTick
    waiting for g_flag at 0x20000004
    qemu-system-arm: terminating on signal 15 from pid 456 (timeout)
```

带 `volatile` 的打印了 `escaped` 与 `ALL DONE`；去掉的那一份只打印到
`waiting for g_flag`，之后再无输出——它卡在 `b.n` 上了。
`cycles = 0` 是因为 QEMU 的 `netduinoplus2` 没有实现 `DWT_CYCCNT`；
真板上有数，见下一节。

### 真板上的四组结果

`tools/board-run.ps1` 一次启动 `openocd`，依次烧写四份镜像并读回 RAM。
四组结果都来自 **STM32F103C8 最小系统板 + CMSIS-DAP**：

`实测数据`
`PowerShell`

```powershell
& "H:\OpenOCD\bin\openocd.exe" -s "H:\OpenOCD\share\openocd\scripts" `
  -f interface/cmsis-dap.cfg -f target/stm32f1x.cfg `
  -c "init" -c "reset halt" -c "arm semihosting enable" `
  -c "flash write_image erase K:/.../flag_mww_vol.elf" `
  -c "reset run" -c "sleep 300" -c "mww 0x20000004 1" -c "sleep 500" -c "halt" `
  -c "mdw 0x20000008 5" -c "reg pc" -c "shutdown"
```

路径一律用正斜杠：OpenOCD 的 `-c` 按 Tcl 规则解析，反斜杠会被当转义吃掉。

`实测数据`
`Text`

```text
== A: mww + volatile ==
0x20000008: 00000001 0015d6ab 20000004 600d0702 00000000
pc (/32): 0x0800016e
（semihosting 上先打印了：escaped, cycles = 1431211 / ALL DONE）

== B: mww + NO_VOLATILE ==
0x20000008: 00000000 00000000 20000004 600d0702 00000000
pc (/32): 0x08000114

== C: SysTick + volatile ==
0x20000000: 00000108 00000001 00000001 00001f6b 20000004 600d0702
pc (/32): 0x0800017e
（semihosting 上先打印了：escaped, cycles = 8043 / ALL DONE）

== D: SysTick + NO_VOLATILE ==
0x20000000: 00000563 00000001 00000000 00000000 20000004 600d0702
pc (/32): 0x08000124
```

`0x20000000` 起是 `g_ticks`（SysTick 中断次数）、`g_flag`、然后是
`g_results[0..3]`：是否跳出来、等待周期数、`g_flag` 的地址、标识字 `0x600D0702`。

| 组 | 触发器 | `volatile` | 中断次数 | `g_flag` | 跳出来了吗 | 结束时 PC |
|---|---|---|---|---|---|---|
| A | `mww` | 有 | 0 | 1 | **是** | `0x0800016e`（循环之后） |
| B | `mww` | 无 | 0 | 1 | **否** | `0x08000114`（`b.n` 自身） |
| C | SysTick | 有 | 264 | 1 | **是**，等了 8043 周期 | `0x0800017e`（循环之后） |
| D | SysTick | 无 | **1379** | **1** | **否** | `0x08000124`（`b.n` 自身） |

D 组是最有说服力的一组：**中断进来过 1379 次，`g_flag` 明明白白是 1，
主循环却一次都没读到**。PC 停在 `0x08000124`，那正是上面那条 `b.n`：

`Assembly`

```asm
 8000124:	e7fe      	b.n	8000124 <main+0x88>   ← 跳到自身
```

`cycles = 1431211` 是 A 组等待的周期数：调试器从 `reset run` 到 `mww` 之间
隔了 300 ms，板子跑在 HSI 8 MHz 上，1 周期 = 125 ns。

## 代码里哪几处是要点

| 位置 | 要点 |
|---|---|
| `src/volatile_regs.c` 的 `vr_wait_volatile` / `vr_wait_plain` | 两份源码逐字相同，只差 `volatile`；对照才有意义 |
| `src/volatile_regs.c` 的 `vr_probe_flag` | 起一个线程去等、本线程睡够再置位；`WaitForSingleObject` 是看门狗，超时也算一种结果 |
| `src/volatile_regs.c` 的 `vr_sum_reads` | 两份的和都是 `n × 值`，说明这类错查不出来；差别在反汇编里 |
| `src/volatile_regs.c` 的 `vr_now_ms` | Windows 上用 `QueryPerformanceCounter`；标准库在 Windows 上没有可用的单调钟 |
| `include/volatile_regs.h` 的 `vr_reg_info` | 寄存器表只是「名字 + 地址 + 复位值 + 访问属性」，模拟件与真外设共用这一张表 |
| `board/volatile_flag.c` 的 `#ifdef NO_VOLATILE` | 整个演示就出在这一处；加 `-DNO_VOLATILE` 编出的镜像与另一份同源 |
| `board/volatile_flag.c` 的 SysTick 使能位置 | 必须放在所有 semihosting 打印**之后**：打印要花上万周期，提前打开会让中断先置位，循环一圈都不转 |
| `board/startup_min.s` 的向量表 | 第 0 个字是栈顶、第 1 个字是复位向量；SysTick 是第 15 项，本示例的「设备」挂在那里 |
| `board/stm32f103c8_min.ld` 的 `>RAM AT> FLASH` | `.data` 的两套地址；照抄大容量型号的脚本会让栈顶落到 RAM 之外 |
| `tools/asm-compare.ps1` | 只用 `gcc -c` 加 `objdump --disassemble=`，不链接、不运行，看的就是编译结果 |
| `tools/board-run.ps1` | 一次 `openocd` 跑完四组；只写 flash 主区，跑完 `halt` 再 `shutdown` |

## 已知问题

- **主机上的「设备」是一次数据竞争。** C11 内存模型下，一个线程读、
  另一个线程写同一个非原子对象属于未定义行为。真外设不是这样：
  它的值由硬件改，程序侧只有读。示例用普通全局量加线程来近似，
  正是要演示「不带 `volatile` 时编译器假定它不会变」这一条。
- **判据依赖优化等级。** 自测里第 8 项用 `__OPTIMIZE__` 分了两支：
  `-O0` 下两种写法结果相同，`-O1` 及以上才分得出来。这是编译器行为，不是标准规定。
- **`board/` 的线程与睡眠只有 Windows 分支实测过。** `volatile_regs.c`
  里的 `#else` 分支走 `pthread` / `clock_gettime`，本机没有在这条分支上跑过。
- **`linux` 侧的 `vr_thread_join` 忽略超时。** 那条分支只保证语义等价，
  看门狗功能只在 Windows 上有效。
- **QEMU 里 `DWT_CYCCNT` 读出来是 0。** `netduinoplus2` 没有实现这个计数器，
  周期数只能在真板上量。
- **`cycles` 在两次运行之间会变。** 它取决于调试器从 `reset run` 到 `mww`
  之间隔了多久，不是一个可复现的常数。
