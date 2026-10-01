# 示例 `07-lower-level/05-interrupt-and-atomic` · 中断与原子

一份把「丢更新」演两遍的工具。主机上：主循环与一个「中断」线程同时给同一个计数器加一，
三种写法各跑一次，看最终值少了多少。真板上：主循环与 SysTick 中断同时加一，
三种写法各跑一次，再量一次中断延迟。

同一个实验在两处跑，是因为**它在 QEMU 里复现不出来**：QEMU 以基本块为单位执行，
三条指令的读-改-写通常落在同一个基本块里，中断插不进去。
真硅片上中断可以落在任意两条指令之间，丢更新立刻出现。

`Text`

```text
05-interrupt-and-atomic/
  include/race_lab.hpp   接口：三种写法、一次实验的结果、自测
  src/race_lab.cpp       实现：主循环与「中断」线程各做固定次数的自增
  src/main_cli.cpp       命令行版：认参数、跑三种写法、打印对照
  board/race_board.c     Cortex-M 版：SysTick 中断 + 主循环，外加中断延迟测量
  board/startup_min.s    精简启动文件：向量表补到第 28 号（TIM2）
  board/stm32f103c8_min.ld  精简链接脚本：C8 的 64 KiB flash / 20 KiB RAM
  board/build.sh         在 WSL 里交叉编译并在 QEMU 里跑
  tools/board-run.ps1    烧真板并读回结果（一次 openocd，跑完停在 halt）
  CMakeLists.txt         目标：core（静态库）、app_cli
  CMakePresets.json      mingw-gdb、mingw-release（-O2）与 msvc 三套预设
  .vscode/               三个调试配置与九个构建任务
```

## 先读教材

| 章节 | 讲的是什么 | 本示例用在哪里 |
|---|---|---|
| 《07-更底层/10-中断、并发与内存序.md》第 1.4 小节 | 中断里的活有多贵 | 真板上量到的中断延迟 21 个周期 |
| 《07-更底层/10-中断、并发与内存序.md》第 2.1 小节 | 一条源码，三条指令 | `bump()` 里那个 `g_plain = g_plain + 1u` |
| 《07-更底层/10-中断、并发与内存序.md》第 2.2 小节 | 主循环与中断共享数据的正确写法 | 三种修法的对照 |
| 《07-更底层/10-中断、并发与内存序.md》第 3.1 小节 | 关中断：`cpsid i` 与 `cpsie i` | 真板上「关中断」那一轮一次没丢 |
| 《07-更底层/10-中断、并发与内存序.md》第 3.3 小节 | 原子量：不关中断的做法 | 真板上 `__atomic_fetch_add` 那一轮 |
| 《07-更底层/10-中断、并发与内存序.md》第 3.4 小节 | 三种写法的实测对照 | 本示例就是把这一节做成了可运行的程序 |
| 《07-更底层/03-寄存器、位与 volatile.md》第 3.1 小节 | 一句源码，三条指令 | `bump()` 里那个 `g_plain = g_plain + 1u` |
| 《07-更底层/03-寄存器、位与 volatile.md》第 3.2 小节 | 真跑：一秒钟丢了一千次 | 真板上这一次丢了 963 次 |
| 《07-更底层/03-寄存器、位与 volatile.md》第 2.8 小节 | `volatile` 做不到的四件事 | 不加保护那一行：`volatile` 挡不住丢更新 |
| 《07-更底层/07-链接脚本与启动代码.md》第 3.3 小节 | 把 `.data` 从 flash 搬到 RAM | `board/startup_min.s` 里那两段循环 |
| 《07-更底层/07-链接脚本与启动代码.md》第 2.4 小节 | `.isr_vector` 为什么必须放在最前面 | 向量表不补齐就跳飞，README 下面有这一段 |

## 这个项目要解决什么问题

**一次自增不是一步。** `g_counter = g_counter + 1u;` 展开成三条指令：
读、加一、写回。中断可以落在任意两条之间。
主循环读到 100、加一得 101；中断插进来也读到 100、加一得 101、写回 101；
主循环再写回 101。**两次自增，结果只加了 1。**

**`volatile` 挡不住这件事。** 它只保证「每次都真的去内存读、真的写回内存」，
不保证「读-改-写」是一步。真板上的实测：

`实测数据`
`Text`

```text
--- 不加保护 ---
  expected = 201445
  actual   = 200482
  lost     = 963
```

主循环 200000 次、中断 1445 次，一共该有 201445 次自增；实际只加了 200482 次，
**丢了 963 次**。

**三种修法，两处演示。** 主机上有两种：

`实测数据`
`Text`

```text
  不加保护             期望    4000000  实测    2069170  丢了  1930830  7.3 ms
  临界区（自旋锁）     期望    4000000  实测    4000000  丢了        0  134.2 ms
  原子量 fetch_add     期望    4000000  实测    4000000  丢了        0  34.6 ms
```

第三种是**关中断**，主机上没有对应物：那是 `PRIMASK` 与 `cpsid i` / `cpsie i` 管的事，
只在 Cortex-M 那一份里演示。真板上三种齐了：

`实测数据`
`Text`

```text
--- 不加保护 ---
  expected = 201445   actual = 200482   lost = 963
--- 关中断（cpsid i / cpsie i）---
  expected = 202064   actual = 202064   lost = 0
--- 原子量（__atomic_fetch_add）---
  expected = 202739   actual = 202739   lost = 0
```

代价也摆在那里：临界区那一版在主机上慢了 18 倍（7.3 ms → 134.2 ms），
原子量慢了 4.7 倍。真板上原子量的指令序列多出两条 `dmb ish` 与一个 `ldrex`/`strex` 重试循环：

`Assembly`

```asm
 8000224:	e853 1f00 	ldrex	r1, [r3]        ← 独占读
 800022a:	e843 1200 	strex	r2, r1, [r3]    ← 独占写，失败则 r2 非零
 8000232:	f3bf 8f5b 	dmb	ish                 ← 两条屏障
```

**中断延迟可以量。** 写 `NVIC_ISPR` 把中断挂起，ISR 第一条指令读 `DWT_CYCCNT` 相减，
触发 64 次取最小与最大。真板上 64 次全是 21 个周期——**没有缓存、没有流水线干扰，
数字可复现**：

`实测数据`
`Text`

```text
--- 中断延迟（64 次，DWT 周期）---
  min = 21
  max = 21
```

## 做完能掌握什么

- 会复现丢更新，并说清它是「读-改-写不是一步」而不是「编译器优化掉了」
- 会用三种办法修它，并知道三种办法各自付出什么代价
- 会说清为什么自旋锁不能用在中断处理函数里（主循环拿着锁时中断会一直等）
- 会用 `cpsid i` / `cpsie i` 划临界区，并知道中断不会因此丢掉，只是延后
- 会用 `NVIC_ISPR` 加 `DWT_CYCCNT` 量中断延迟
- 会看出 QEMU 与真硅片的差别：基本块粒度让中断插不进三条指令中间

## 构建与运行

`PowerShell`

```powershell
# 在 07-lower-level/05-interrupt-and-atomic 目录下
cmake --preset mingw-gdb
cmake --build --preset mingw-gdb
build\mingw\bin\app_cli.exe

# 只跑自测
build\mingw\bin\app_cli.exe --selftest

# 换一个工作量
build\mingw\bin\app_cli.exe --ops 5000000

# 开优化那一档
cmake --preset mingw-release
cmake --build --preset mingw-release
build\mingw-release\bin\app_cli.exe
```

MSVC 那一条：

`PowerShell`

```powershell
cmake --preset msvc
cmake --build --preset msvc-debug
build\msvc\bin\Debug\app_cli.exe --selftest
```

不想用预设、也不经过 CMake：

`PowerShell`

```powershell
g++ -std=c++17 -O2 -Wall -Wextra -Iinclude src\race_lab.cpp src\main_cli.cpp -o app_cli.exe
```

板级那一份（需要 WSL 的 Ubuntu 里装了 `arm-none-eabi-gcc` 与 `qemu-system-arm`）：

`Bash`

```bash
# 在示例目录下
wsl -d Ubuntu -e bash "/mnt/k/C相关课程/B-examples/07-lower-level/05-interrupt-and-atomic/board/build.sh"
```

等价的单条命令：

`Bash`

```bash
arm-none-eabi-gcc -mcpu=cortex-m3 -mthumb -O2 -g -Wall -Wextra \
  -nostartfiles --specs=nosys.specs \
  -T board/stm32f103c8_min.ld board/startup_min.s board/race_board.c \
  -o board/build/race_board.elf

qemu-system-arm -M netduinoplus2 -kernel board/build/race_board.elf -nographic \
  -semihosting-config enable=on,target=native
```

真板那一条：

`PowerShell`

```powershell
# 在示例目录下，板子与 CMSIS-DAP 已插好
pwsh -File tools\board-run.ps1
```

它等价于下面这条命令（路径一律用正斜杠：OpenOCD 的 `-c` 按 Tcl 规则解析）：

`PowerShell`

```powershell
& "H:\OpenOCD\bin\openocd.exe" -s "H:\OpenOCD\share\openocd\scripts" `
  -f interface/cmsis-dap.cfg -f target/stm32f1x.cfg `
  -c "init" -c "reset halt" -c "arm semihosting enable" `
  -c "flash write_image erase K:/.../board/build/race_board.elf" `
  -c "reset run" -c "sleep 15000" -c "halt" `
  -c "mdw 0x20004000 8" -c "reg pc" -c "halt" -c "shutdown"
```

## 运行后应当看到什么

`build\mingw\bin\app_cli.exe`：

`实测数据`
`Text`

```text
示例 07-lower-level/05-interrupt-and-atomic · 中断与原子

== 主循环与「中断」同时给一个计数器加一 ==
  主循环与「中断」各做同样次数的自增，两边同时开工。
  不加保护时，一次自增要读内存、加一、写回三步；
  两边的三步交错在一起，后写回的那次把前一次的结果盖掉了。
  临界区把这三步圈起来，原子量把这三步合成一条指令，两边都不丢。
  主机上没有「关中断」这一说：那是 cpsid i / cpsie i 管的事，
  Cortex-M 那一份（board/race_board.c）里有。

  不加保护             期望    4000000  实测    2069170  丢了  1930830  7.3 ms
  临界区（自旋锁）     期望    4000000  实测    4000000  丢了        0  134.2 ms
  原子量 fetch_add     期望    4000000  实测    4000000  丢了        0  34.6 ms

== 自测 ==
  [通过] 1. 不加保护：结果不会超过期望值（只会少，不会多）
  [通过] 2. 临界区：两边各 200 万次，一次都没丢
  [通过] 3. 原子量：两边各 200 万次，一次都没丢
  [通过] 4. 三种写法的期望值相同（同样的工作量才谈得上对照）
  [通过] 5. 三次实验都真的跑起来了
  [通过] 6. 不加保护：这一次确实丢了更新（竞态复现出来了）
  [通过] 7. 两种修法的最终值一致（都是 400 万）

  自测结果：7 项中 7 项通过，全部通过
```

丢掉的数字每次运行都不同（本机这一次丢了 1930830，接近一半）；
「丢了 0」的那两行才是稳定的。

### QEMU 里

`实测数据`
`Text`

```text
race_board on Cortex-M3 (STM32F103C8)
main loop and SysTick both increment one counter
--- 不加保护 ---
  expected = 200038
  actual   = 200038
  lost     = 0
--- 关中断（cpsid i / cpsie i）---
  expected = 201092
  actual   = 201092
  lost     = 0
--- 原子量（__atomic_fetch_add）---
  expected = 200273
  actual   = 200273
  lost     = 0
--- 中断延迟（64 次，DWT 周期）---
  min = 0
  max = 0
6 项中 6 项通过，全部通过
ALL DONE
```

**QEMU 里三种写法都不丢**：它以基本块为单位执行，中断只在块边界被取用，
三条指令的读-改-写落在一个基本块里，插不进去。
`min = 0` 也是同一个原因：`netduinoplus2` 没有实现 `DWT_CYCCNT`。
这两条差别正好说明「QEMU 验逻辑、真板验时序」这个分工。

### 真板上

`实测数据`
`Text`

```text
--- 不加保护 ---
  expected = 201445
  actual   = 200482
  lost     = 963
--- 关中断（cpsid i / cpsie i）---
  expected = 202064
  actual   = 202064
  lost     = 0
--- 原子量（__atomic_fetch_add）---
  expected = 202739
  actual   = 202739
  lost     = 0
--- 中断延迟（64 次，DWT 周期）---
  min = 21
  max = 21
6 项中 6 项通过，全部通过
ALL DONE

[stm32f1x.cpu] halted due to debug-request, current mode: Thread
xPSR: 0x21000000 pc: 0x08000432 msp: 0x20004fd0, semihosting
== results at 0x20004000 ==
0x20004000: 600d0705 00000500 000312e5 00030f22 000003c3 000005a5 00000015 00000015
pc (/32): 0x08000432
shutdown command invoked
```

结果区那八个字的含义：

| 下标 | 值 | 含义 |
|---|---|---|
| `[0]` | `600d0705` | 标识字，确认读到的就是本程序 |
| `[1]` | `00000500` | 版本标记（05 示例） |
| `[2]` | `000312e5` = 201445 | 期望值 |
| `[3]` | `00030f22` = 200482 | 实测值 |
| `[4]` | `000003c3` = 963 | 丢掉的自增次数 |
| `[5]` | `000005a5` = 1445 | 这一轮的中断次数 |
| `[6]` `[7]` | `00000015` = 21 | 中断延迟的最小与最大值 |

三组数字互相对得上：`200482 + 963 = 201445`，`201445 − 200000 = 1445`。
中断延迟 21 个周期与 `07-真板实测数据.md` 里那一次完全相同，
说明这个数字在硅片上是稳定的。

## 代码里哪几处是要点

| 位置 | 要点 |
|---|---|
| `src/race_lab.cpp` 的 `bump()` | 三种写法各一行，差别只在有没有把读-改-写圈起来；对照才有意义 |
| `src/race_lab.cpp` 的 `isr_thread` | 「中断」是一个独立线程，两边由发令枪 `g_go` 同时开工，工作量才对等 |
| `src/race_lab.cpp` 的 `lock_acquire` | 自旋锁的注释里写明了为什么它不能用在中断处理函数里 |
| `src/race_lab.cpp` 的 `self_test` | 丢更新是竞态的结果、不是必然，因此那一条只报告不断言，复现不出来时记作「跳过」 |
| `board/race_board.c` 的 `cpsid i` / `cpsie i` | 临界区在这里；中断不会被丢掉，只是延后到 `cpsie i` 之后 |
| `board/race_board.c` 的 `g_counter` | 是 `volatile`：不加它，编译器会把读提到循环之外，实验本身就不成立了 |
| `board/race_board.c` 的 `measure_irq_latency` | 写 `NVIC_ISPR` 挂起、ISR 第一条指令读 `DWT_CYCCNT`，64 次取最小最大 |
| `board/startup_min.s` 的向量表 | 补到第 28 号：表不够长时 CPU 会从表的后面取到别的字，一跳就飞 |
| `board/race_board.c` 的 `RESULTS` | 结果写在固定的 `0x20004000`，卡死时调试器也读得到 |

## 已知问题

- **主机上的「中断」是一个线程，不是真正的中断。** 线程间没有 `PRIMASK`
  那样的硬件屏蔽，因此「关中断」这一种修法在主机上没有对应物，
  只在 Cortex-M 那一份里演示。
- **主机上的丢更新依赖调度。** 单核机器、或者系统把两个线程错开时，
  不加保护那一版也可能一次不丢。自测因此只断言「不会超过期望值」，
  把「确实丢了」记作报告项，复现不出来时计为「跳过」。
- **QEMU 里复现不出丢更新。** 它以基本块为单位执行，中断只在块边界被取用。
  想看到丢更新必须上真板，或者换一个能逐指令计时的模拟器。
- **QEMU 里 `DWT_CYCCNT` 读出来是 0。** `netduinoplus2` 没有实现它，
  中断延迟只能在真板上量。
- **真板上的期望值三轮各不相同。** 每轮跑多久、中断就进多少次，
  三者的期望值本来就不一样；能对照的是每一轮自己的「期望 − 实测」。
- **自旋锁在真板上没有演示。** 单核 MCU 上，主循环拿着锁时中断进来会一直等，
  而主循环又在等中断返回——死锁。这个坑写在 `lock_acquire` 的注释里，
  没有做成可运行的演示。
