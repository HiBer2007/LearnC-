# 寄存器、位与 `volatile`

> **授权**：本章节属教材文档部分，采用 [CC BY-NC-ND 4.0](../LICENSE)
> 加[附加条款](../许可附加条款.md)授权：署名、非商业、禁止演绎、不允许二次分发。
> 文中引用的代码不受此限，各自保留原有许可证，详见[版权与许可说明.md](../版权与许可说明.md)。

**单片机上没有操作系统替你挡住硬件。** 串口发一个字节、引脚输出一个电平，
都是往**一个固定地址**写一个数——那个地址上挂的是外设的寄存器，
不是内存里的普通变量。

**编译器不知道这件事。** 它按「内存里的变量」来推理：一段循环里没有人写这个变量，
读到的值就不会变。这条推理在普通变量上永远正确，在寄存器上会让程序**卡死在等待里**。
`volatile` 就是用来关掉这条推理的关键字，而它能管的事情比多数人以为的窄。

《01-编译器/03-嵌入式与交叉编译.md》第 4 节已经把「寄存器是什么、`volatile` 怎么用」
讲到了能上手写的程度。**本章往下再走一层**：把两份 `-O2` 的汇编摆在一起逐个字段对照，
在 QEMU 里数出「少了 `volatile` 到底差了多少个节拍」，再看清 `volatile` **不能**保证什么。

---

> **约定**：标注以行内代码单独成行——`实测数据` 表示实际执行验证过，
> 复现命令随程序给出；`文档` 表示引自标准或官方资料；
> `待确认` 表示尚未验证。路径占位符（如 `<MinGW>`、`<工作区>`）的含义见《README.md》。

# 本章节定位

| 需要先知道 | 在哪 |
|---|---|
| 寄存器就是固定地址上的一个 `uint32_t`，`GPIOA->ODR` 就是访问 `0x4001080C` | 《01-编译器/03-嵌入式与交叉编译.md》第 4 节 |
| `volatile` 的含义，以及一个「不加就死循环」的最小对照 | 《01-编译器/03-嵌入式与交叉编译.md》第 4.5 小节 |
| `volatile` 是类型限定符，与 `const` 同级 | 《04-语法/02-数据类型与类型系统.md》第 3.2 小节 |
| 位运算符 `&`、`\|`、`~`、`<<`、`>>` | 《04-语法/04-表达式与运算符.md》第 3.5 小节 |
| 位域的基本写法与「布局由实现决定」这一结论 | 《04-语法/09-结构体、联合体与 enum.md》第 4 节 |
| `-O2` 会做哪些优化 | 《03-构建工具链/05-优化等级.md》章节 |

**相邻的章节**：《07-更底层/02-对齐、填充与缓存.md》章节
讲同一个变量摆在哪个地址上；本章讲寄存器与位。
`07-更底层/04-字节序与数据表示`（《07-更底层/04-字节序与数据表示.md》章节）讲的是
同一串字节在不同机器上被解释成什么；本章只用到它的结论。
`07-更底层/10-中断、并发与内存序`（《07-更底层/10-中断、并发与内存序.md》第 3 节与第 4 节）
接着第 3 节往下讲原子与屏障。

| 节 | 讲什么 |
|---|---|
| **第 1 节** | 内存映射 I/O 的真实写法：`gpio.c` 逐行读、不用库直接写地址的三种包装 |
| **第 2 节** | `volatile` 到底阻止了哪些优化：两份 `-O2` 汇编对照、延时循环被整个删掉的实测、QEMU 里用 SysTick 数节拍 |
| **第 3 节** | 为什么一次读加一次写拼不成一个整体：丢更新的复现与硬件原子位操作 `BSRR` |
| **第 4 节** | 位带别名区：把「改一位」变成一次写 |
| **第 5 节** | 位域的布局、跨 ABI 与跨字节序的差异、四个陷阱 |
| **第 6 节** | `register` 关键字为什么没用了 |
| **第 7 节** | 速查表 |

---

# 第 1 节 内存映射 I/O：一个地址就是一个寄存器

## 1.1 从 `gpio.c` 看真实写法

`A-教学素材/01-编译器/嵌入式/寄存器与位运算/gpio.c` 是一份 CubeMX 生成的 GPIO 初始化代码，
共 66 行，**原文照抄如下**（版权头保留）：

`C`

```c
/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    gpio.c
  * @brief   This file provides code for the configuration
  *          of all used GPIO pins.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "gpio.h"

/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/*----------------------------------------------------------------------------*/
/* Configure GPIO                                                             */
/*----------------------------------------------------------------------------*/
/* USER CODE BEGIN 1 */

/* USER CODE END 1 */

/** Configure pins as
        * Analog
        * Input
        * Output
        * EVENT_OUT
        * EXTI
*/
void MX_GPIO_Init(void)
{

  GPIO_InitTypeDef GPIO_InitStruct = {0};

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_8, GPIO_PIN_RESET);

  /*Configure GPIO pin : PA8 */
  GPIO_InitStruct.Pin = GPIO_PIN_8;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

}

/* USER CODE BEGIN 2 */

/* USER CODE END 2 */
```

**这份代码里没有出现一个地址，但每一行最终都落到地址上。**
`USER CODE BEGIN`/`END` 是 CubeMX 的再生成保护标记，夹在中间的内容重新生成代码时不会被覆盖。

**`__HAL_RCC_GPIOA_CLK_ENABLE()`**：打开 GPIOA 的时钟。
F1 系列的外设时钟由 `RCC_APB2ENR` 控制，这个宏展开成一个「读—改—写」：
读出寄存器、把 `IOPAEN` 那一位置 1、写回。
**时钟没开就配置引脚，寄存器写不进去**，这是上电后第一个要过的关。

**`HAL_GPIO_WritePin(GPIOA, GPIO_PIN_8, GPIO_PIN_RESET)`**：把 PA8 先写成低电平。
F1 的 HAL 实现是往 `BSRR` 的高 16 位写 `1 << 8`（第 4.3 节讲过这个寄存器的分工）。

**`GPIO_InitStruct`** 是普通的自动变量，**它在栈上**：
`Pin`、`Mode`、`Pull`、`Speed` 四个字段先填好，
再由 `HAL_GPIO_Init` 依据它们去改 `GPIOA->CRL`（或 `CRH`）。
**一份在 RAM 里的结构体，最后变成对 `0x40010800` 的几次写入**——
这是「软件配置硬件」最常见的样子。

**整段代码真正的落点**可以写成一张表：

`实测数据`

| 源码里的一句 | 实际访问的地址 | 寄存器 |
|---|---|---|
| `__HAL_RCC_GPIOA_CLK_ENABLE()` | `0x40021018` | `RCC_APB2ENR` |
| `HAL_GPIO_WritePin(GPIOA, GPIO_PIN_8, GPIO_PIN_RESET)` | `0x40010810` | `GPIOA_BSRR` |
| `HAL_GPIO_Init(...)` 里的速度与模式位 | `0x40010800` | `GPIOA_CRL` |
| `HAL_GPIO_ReadPin` 会读 | `0x40010808` | `GPIOA_IDR` |

**这张表里的数字来自芯片参考手册与外设头文件**：GPIOA 的基址是 `0x40010800`，
`CRL`/`CRH`/`IDR`/`ODR`/`BSRR`/`BRR` 依次在 `+0x00`/`+0x04`/`+0x08`/`+0x0C`/`+0x10`/`+0x14`。
**换一款芯片偏移就会变**，用之前打开自己那颗芯片的头文件确认。

## 1.2 不用库，直接写地址

HAL 的好处是可移植，代价是「看不出最后访问了哪个地址」。
不依赖任何库的写法是把地址直接变成指针：

`C`

```c
/* mmio_raw.c    只编译不运行：gcc -std=c23 -O2 -c mmio_raw.c -o mmio_raw.o
                 （这些地址在主机上不存在，链接与运行都没有意义） */
#include <stdint.h>

/* 把整数地址变成指针，再解引用：这就是内存映射 I/O 的全部 */
#define GPIOA_ODR  (*(volatile uint32_t *)0x4001080Cu)   /* 输出数据寄存器 */
#define GPIOA_IDR  (*(volatile uint32_t *)0x40010808u)   /* 输入数据寄存器 */
#define GPIOA_BSRR (*(volatile uint32_t *)0x40010810u)   /* 置位/复位寄存器 */

void pa8_high(void)  { GPIOA_BSRR = (1u << 8); }             /* 置 1，只动 PA8 */
void pa8_low(void)   { GPIOA_BSRR = (1u << (8 + 16)); }      /* 清 0，只动 PA8 */
void pa8_toggle(void) { GPIOA_ODR ^= (1u << 8); }            /* 读—改—写，不是原子的 */
int  pa0_level(void) { return (int)(GPIOA_IDR & 1u); }       /* 读回引脚电平 */
```

**`(volatile uint32_t *)0x4001080C` 是一次强制类型转换**：
把一个整数变成「指向 `volatile uint32_t` 的指针」。
左边那个 `*` 是解引用，所以整句 `*(volatile uint32_t *)0x4001080C = 1u`
读作「往地址 `0x4001080C` 写一个 32 位数」。

**这个写法有两个独立的成分，缺一不可**：

| 成分 | 作用 | 少了会怎样 |
|---|---|---|
| `(uint32_t *)0x4001080C` | 让编译器把整数当地址用 | 编译不过：整数不能解引用 |
| `volatile` | 让每次读写都真的发生 | **编译通过、运行卡死**（第 2 节） |

**第二个成分才是危险的**：它带来的错误不是编译错误，
而是在打开优化之后才出现的死循环。

> [!IMPORTANT]
> **内存映射 I/O 的写法只有一句：把地址转成指向 `volatile` 的指针，然后解引用。**
> 库、`struct` 化的寄存器组、CMSIS 的 `__IO` 宏，都只是这句话的包装。

## 1.3 三种包装，同一件事

同一个寄存器可以有三种写法，编出来的指令完全一样：

`C`

```c
/* mmio_three.c   只编译不运行：gcc -std=c23 -O2 -c mmio_three.c -o mmio_three.o */
#include <stdint.h>

/* 写法一：宏 + 强制转换 */
#define GPIOA_ODR_A  (*(volatile uint32_t *)0x4001080Cu)

/* 写法二：先定义指针类型，再定宏 */
typedef volatile uint32_t *reg32_t;
#define GPIOA_ODR_B  (*(reg32_t)0x4001080Cu)

/* 写法三：CMSIS 风格的结构体，成员顺序与寄存器顺序一致 */
typedef struct {
    volatile uint32_t CRL;      /* +0x00 */
    volatile uint32_t CRH;      /* +0x04 */
    volatile uint32_t IDR;      /* +0x08 */
    volatile uint32_t ODR;      /* +0x0C */
    volatile uint32_t BSRR;     /* +0x10 */
    volatile uint32_t BRR;      /* +0x14 */
} GPIO_TypeDef;

#define GPIOA  ((GPIO_TypeDef *)0x40010800u)

void set_a(void) { GPIOA_ODR_A = 1u; }
void set_b(void) { GPIOA_ODR_B = 1u; }
void set_c(void) { GPIOA->ODR = 1u; }
```

**CMSIS 的 `core_cm3.h` 给这三种访问权限各留了一个宏**：

`文档`

> "#define     __I     volatile const       /*!< Defines 'read only' permissions */"
> "#define     __O     volatile             /*!< Defines 'write only' permissions */"
> "#define     __IO    volatile             /*!< Defines 'read / write' permissions */"
>
> —— CMSIS Core 头文件 `core_cm3.h`（《01-编译器/03-嵌入式与交叉编译.md》第 4.5 小节引过同一段）

**`__I` 是 `volatile const`**：只读的寄存器写它没有意义，`const` 让编译器替你把关；
**`__O` 与 `__IO` 都是 `volatile`**：写寄存器与读写寄存器在优化规则上没有区别。
**这份头文件的作者用 `const` 表达「权限」，用 `volatile` 表达「硬件会改」**，
两者分工清楚。

## 1.4 一个必须用 `volatile` 的完整程序

下面这个程序把同一个地址用两种方式声明，两个函数做同一件事——
等待 PA0 变高。**它是本章后面所有结论的源头**。

`实测数据`
`C`

```c
/* poll_o2.c    主机上只生成汇编：gcc -std=c23 -O2 -S poll_o2.c -o poll_o2.s
                交叉编译（Cortex-M）：arm-none-eabi-gcc -mcpu=cortex-m3 -mthumb -O2 \
                  -c poll_o2.c -o poll_o2.o && arm-none-eabi-objdump -d poll_o2.o */
#include <stdint.h>

/* STM32F103 的 GPIOA：外设区基址 0x40010800，IDR 在 +0x08 */
uint32_t          *plain_idr = (uint32_t *)0x40010808u;           /* 少了 volatile */
volatile uint32_t *vol_idr   = (volatile uint32_t *)0x40010808u;  /* 写法正确 */

/* 轮询等待 PA0 变高：没有 volatile，编译器认为循环里没有人改内存 */
void wait_plain(void)
{
    while ((*plain_idr & 1u) == 0u) {
    }
}

/* 同样的循环，加上 volatile 之后每次都真的去读 */
void wait_volatile(void)
{
    while ((*vol_idr & 1u) == 0u) {
    }
}
```

**指针本身是不是 `volatile` 也要分清**：
`volatile uint32_t *p` 是「`p` 指向的 `uint32_t` 是易变的」；
`uint32_t *volatile p` 是「`p` 这个指针自己也易变」。
寄存器地址通常写前一种，`plain_idr` 与 `vol_idr` 的区别就在指向物上。

**这份程序在主机上编得过、跑不了**（`0x40010808` 在主机进程里没有映射）。
所以主机侧的命令只到 `-S`，交叉编译侧只到 `-c` 加 `objdump`——
**要看的是机器码，不是运行结果**。

## 1.5 小结

> [!NOTE]
> **内存映射 I/O 是把「一个地址」当成「一个变量」来读写。**
> 地址转换决定编译器能不能编过，`volatile` 决定运行时会不会卡死。
> 库函数、结构体化的寄存器组、`__IO` 宏都只是这一句话的包装，
> 弄清包装底下的地址与限定符，才能在出问题时知道该看哪里。

---

# 第 2 节 `volatile` 到底阻止了什么

## 2.1 两份 x86-64 汇编

用 `gcc -std=c23 -O2 -S poll_o2.c` 生成汇编。**下面是节选**，
去掉了 `.seh_*` 之类的伪指令与对齐填充：

`实测数据`
`Assembly`

```asm
wait_plain:                       wait_volatile:
	movq	plain_idr(%rip), %rax     	movq	vol_idr(%rip), %rdx
	testb	$1, (%rax)
	je	.L3
	ret
.L3:
	jmp	.L3                              .L6:
	                                    	movl	(%rdx), %eax
	                                    	testb	$1, %al
	                                    	je	.L6
	                                    	ret
```

**左边这份永远等不到硬件。** 它做了三件事：
把 `plain_idr` 这个指针读进 `%rax`（一次）、
用它寻址读一次内存（`testb $1, (%rax)`）、
不满足条件就跳到 `.L3`，
而 `.L3` 的全部内容是 `jmp .L3`——**跳回自己，中间没有再读内存**。

**右边这份每次循环都重新读**：`.L6` 里的 `movl (%rdx), %eax`
就是「从 `%rdx` 指向的地址取一个 32 位数」，
`je .L6` 跳回标签后又会执行这条 `movl`。

编译器的推理是：**循环体里没有写 `plain_idr` 指向的对象，
也没有调用可能改它的函数，因此这个值在循环中不变。**
既然第一次读到 0，就永远读到 0，于是把「读一次」提到了循环外面，
循环本身退化成一个空转的跳转。

> [!IMPORTANT]
> **`volatile` 阻止的不是「优化」，而是「跨越访问的推理」。**
> 它要求编译器对每次访问都真的发出读写指令，并且不把两次访问合并成一次。
> 循环之外的优化——寄存器分配、常量折叠、内联——照常进行。

## 2.2 同一份源码在 Cortex-M 上

换交叉编译器，结论不变。**`arm-none-eabi-gcc` 13.2.1 加 `-O2`，
`objdump` 的节选**：

`实测数据`
`Assembly`

```asm
00000000 <wait_plain>:                    00000014 <wait_volatile>:
   0:	4b03      	ldr	r3, [pc, #12]     14:	4b02      	ldr	r3, [pc, #8]
   2:	681b      	ldr	r3, [r3, #0]      16:	685a      	ldr	r2, [r3, #4]
   4:	681b      	ldr	r3, [r3, #0]      18:	6813      	ldr	r3, [r2, #0]
   6:	07db      	lsls	r3, r3, #31      1a:	07db      	lsls	r3, r3, #31
   8:	d400      	bmi.n	c <...>           1c:	d5fc      	bpl.n	18 <...>
   a:	e7fe      	b.n	a <...>           1e:	4770      	bx	lr
   c:	4770      	bx	lr
```

**两条 `ldr` 各有分工**：第一条从字面量池取出指针的值（`plain_idr` 里存着 `0x40010808`），
第二条才是**真正读寄存器**。左边只读了一次，`b.n a` 就是「跳回自己」，
在 ARM 汇编里写成 `b.n a`（`a` 是它自己的地址）；
右边把 `ldr r3, [r2, #0]` 放在标签 `18` 处，`bpl.n 18` 跳回去就再读一次。

**`lsls r3, r3, #31` 是一次左移**：把 `r3` 的最低位挤到最高位，
于是「最低位是不是 1」变成「符号标志位是不是 1」，
紧接着 `bmi.n`（负则跳）或 `bpl.n`（非负则跳）就能直接判断，
省掉一条比较指令。**这是位运算在指令层面的常见用法**，不涉及 `volatile` 的语义。

> [!TIP]
> **看反汇编时，找「循环标签里有没有 `ldr`/`mov`」比读全部指令快。**
> 有读内存的指令，说明每次循环都去问了硬件；
> 只有跳转，说明编译器把这次询问省掉了。

## 2.3 真机上的同一段代码：内存改了，程序不知道

反汇编说明编译器把那次读去掉了。**在真板上这件事可以做成一次可复现的死锁**：
程序在 `while (g_flag == 0u) { }` 里等一个标志位，标志位由**调试器直接写内存**置 1，
当作「外设就绪」。两个版本**只差 `volatile` 一个关键字**——
同一个源文件，加 `-DNO_VOLATILE` 编另一份。

`实测数据`
`C`

```c
/* 真板上的 volatile 对照（下面是节选，取自实测用的 board_probe3.c）
   编译（在 WSL 里，arm-none-eabi-gcc 13.2.1）：
     带 volatile：arm-none-eabi-gcc -mcpu=cortex-m3 -mthumb -O2 -g -nostartfiles \
       --specs=nosys.specs -T STM32F103C8_FLASH.ld startup_stm32f103xe.s \
       board_probe3.c -o flag_v.elf
     去掉 volatile（其余一字不改）：同一条命令再加 -DNO_VOLATILE，输出 flag_nv.elf
   运行（Windows 侧 openocd 0.12.0 + CMSIS-DAP 调试器）：
     openocd -f interface/cmsis-dap.cfg -f target/stm32f1x.cfg \
       -c "init" -c "reset halt" -c "flash write_image erase flag_v.elf" \
       -c "reset run" -c "sleep 1000" -c "mww 0x20000020 1" -c "sleep 1000" \
       -c "halt" -c "mdw 0x20000000 4" -c "reg pc" -c "shutdown"
   mww 一句就是「外设就绪」：调试器把 g_flag 所在的地址写成 1 */
#include <stdint.h>

#ifdef NO_VOLATILE
uint32_t g_flag;                     /* 编译器认为没人会改它 */
#else
volatile uint32_t g_flag;            /* 正确写法 */
#endif

volatile uint32_t g_results[8];      /* 结果区：调试器读这一块 */

int main(void) {
    /* …… 省略：DWT 周期计数器的初始化、以及末尾用 semihosting 打印结果的部分 …… */
    g_results[0] = 0;                       /* 出不来就一直是 0 */
    {
        uint32_t t0 = DWT_CYCCNT;
        while (g_flag == 0u) { }            /* ← 不带 volatile 时，这里编译成死循环 */
        g_results[0] = 1;                   /* 走出来了才写这一句 */
        g_results[1] = DWT_CYCCNT - t0;     /* 等了多久（周期） */
    }
    for (;;) { }
}
```

`实测数据`

| 版本（`-O2`） | 调试器写入之后 | 程序反应 | 结束时的 PC |
|---|---|---|---|
| 带 `volatile` | 内存 `0x20000020` = 1 | **读到了，跳出循环**：`g_results[0] = 1`，等了 **7,472,238 周期** | `0x0800049e`（已到循环之后） |
| 去掉 `volatile` | 内存 `0x20000020` = 1 | **完全没反应**：`g_results[0]` 仍是 **0** | `0x0800032c`（**卡在循环里**） |

**上表是在 STM32F103C8（Cortex-M3、HSI 8 MHz，1 周期 = 125 ns）上实测的。**
`7,472,238` 个周期约合 0.93 秒，那是调试器两次操作之间的间隔，不是硬件延迟；
**这一组的重点是右边那一行**：内存里的值确实变成了 1，程序的 PC 却一直停在循环里。

**同一份实验的反汇编对照，两边也只差那个关键字**（两份 ELF 各自的 `objdump` 节选）：

`实测数据`
`Assembly`

```asm
; 带 volatile（flag_v.elf，-O2）：每次都真的去读
08000322:	f8d3 8004 	ldr.w	r8, [r3, #4]
08000328:	6a23      	ldr	r3, [r4, #32]          ← 循环标签：读 g_flag（0x20000020）
0800032a:	2b00      	cmp	r3, #0
0800032c:	d0fc      	beq.n	8000328 <main+0x38>    ← 为 0 就跳回去再读

; 去掉 volatile（flag_nv.elf，-O2）：只读一次
08000306:	6a25      	ldr	r5, [r4, #32]          ← 整个 main 里唯一一次读 g_flag
0800032a:	b905      	cbnz	r5, 800032e <main+0x3e>
0800032c:	e7fe      	b.n	800032c <main+0x3c>    ← 跳到自身：死循环
```

**这就是第 2.1 小节里那个 `.L3: jmp .L3` 的真机版本。**
QEMU 里能看到的指令序列，在真硅片上表现为「调试器改了内存、程序毫无反应」——
没有报错、没有异常、没有 HardFault，只有一个停在原地的 PC。
**两边的地址也对得上**：不带 `volatile` 那一版，调试器读回来的 PC 是 `0x0800032c`，
正是上面那条 `b.n 800032c` 自己。

> [!CAUTION]
> **少了 `volatile` 的故障形态是「静默不动」，不是崩溃。**
> 它不触发任何错误处理，看门狗也未必救得回来：
> 如果这个循环里还喂着狗，程序会一直「正常地」卡在那里。

## 2.4 让 Cortex-M 打印出来：`semihosting`

后面几节的实测要在 QEMU 与真板上各跑一遍，两边都需要一条输出通道。
**串口两边都用不上**：QEMU 的近似板外设与真实芯片不同，打不出字；
真板是一块最小系统板，只有 MCU 与晶振，没有引出可接的串口。
因此统一用 **semihosting**：ARM 留的两条调试指令，
`r0` 放功能号、`r1` 放参数、`bkpt 0xAB` 陷入调试器。
QEMU 加 `-semihosting-config enable=on,target=native`，
OpenOCD 加 `arm semihosting enable`，两边的输出都会接到宿主机的标准输出。
**这是最小板上唯一的「打印」通道。**

包成一个头文件，主机与目标机各走一条路：

`C`

```c
/* semihosting.h    Cortex-M 侧的输出通道：bkpt 0xAB + SYS_WRITE0，QEMU 直接接到 stdout。
   主机上退化为 stdout，方便先在本机检查逻辑（两条通道都只要一个 sh_puts 接口）。 */
#ifndef SEMIHOSTING_H
#define SEMIHOSTING_H

#include <stdint.h>

#if defined(__arm__)
/* Cortex-M：ARM 的 semihosting 调用约定——r0 放功能号，r1 放参数，bkpt 0xAB 陷入调试器。
   三条指令写在一个 asm 块里，r0/r1 由这里自己设置，不依赖编译器的寄存器分配。 */
static void sh_puts(const char *s)
{
    __asm__ volatile ("mov r1, %0\n\t"     /* 参数：字符串地址 */
                      "movs r0, #4\n\t"    /* 功能号 0x04：SYS_WRITE0 */
                      "bkpt 0xAB"
                      : : "r"(s) : "r0", "r1", "memory");
}
#else
#include <stdio.h>
static void sh_puts(const char *s) { fputs(s, stdout); }
#endif

/* 手写十进制与十六进制：不依赖 printf，两端输出一致 */
static void sh_dec32(uint32_t v)
{
    char buf[11];
    int i = 10;
    buf[i] = '\0';
    do { buf[--i] = (char)('0' + (v % 10u)); v /= 10u; } while (v != 0u);
    sh_puts(&buf[i]);
}

static void sh_hex32(uint32_t v)
{
    static const char d[] = "0123456789abcdef";
    char out[9];
    for (int i = 0; i < 8; ++i) { out[i] = d[(v >> (28 - 4 * i)) & 0xFu]; }
    out[8] = '\0';
    sh_puts("0x");
    sh_puts(out);
}

#endif /* SEMIHOSTING_H */
```

**头文件里的 `#if defined(__arm__)` 是关键**：目标机走 semihosting，
主机走 `fputs`。**同一份源码两边都能编译**，
主机上可以先验证逻辑对不对，再交叉编译上板。

**汇编块里的三条指令连在一起写，是有原因的**：
只写 `register uint32_t r0 __asm__("r0") = 4;` 这种局部寄存器变量，
在 `-O2` 下被内联多次之后可能失效（实测时 `r0` 里出现了别的值，
semihosting 报 `Unsupported SemiHosting SWI 0xdeadbeef`）。
**把「设置 `r0`/`r1`」交给 `asm` 块自己做，就不依赖编译器的寄存器分配。**

> [!WARNING]
> **裸机上没有 `printf`。** newlib 的 `printf` 需要 `_write` 之类的系统调用桩，
> 桩没接上时它返回 0 且什么都不输出（本节运行时 `write` 与 `printf` 都只返回 0，没有报错）。
> 嵌入式程序要么走 semihosting，要么自己接串口，要么手写格式化。

## 2.5 运行期证据一：延时循环被整个删掉

汇编是「编译器做了什么」的直接证据，但还可以从运行时间上看。
**同一个空循环，计数器加不加 `volatile`，耗时不同**：

`C`

```c
/* delay_o2.c    编译：gcc -std=c23 -O2 delay_o2.c -o delay_o2 */
#include <stdio.h>
#include <time.h>

#define N 300000000L   /* 空循环次数：够大，耗时能测出来 */

static void spin_plain(void)      { for (long i = 0; i < N; ++i) { } }
static void spin_volatile(void)   { for (volatile long i = 0; i < N; ++i) { } }

static double ms_since(clock_t t0)
{
    return (double)(clock() - t0) * 1000.0 / (double)CLOCKS_PER_SEC;
}

int main(void)
{
    clock_t t0 = clock();
    spin_plain();
    double a = ms_since(t0);

    t0 = clock();
    spin_volatile();
    double b = ms_since(t0);

    printf("N = %ld\n", N);
    printf("普通计数器： %.1f ms\n", a);
    printf("volatile 计数器：%.1f ms\n", b);
    return 0;
}
```

`实测数据`
`Text`

```text
N = 300000000
普通计数器： 0.0 ms
volatile 计数器：58.0 ms
```

连跑三次，普通计数器都是 `0.0 ms`，`volatile` 版是 `58.0`／`57.0`／`57.0 ms`
（x86-64，Windows 侧 `gcc` 15.2.0）。
**`0.0 ms` 不是「很快」，而是「这段代码不存在」**：
`spin_plain` 的循环体没有可观察的副作用，整个循环被删掉了。

**`volatile` 版的机器码是记忆体往返**：

`实测数据`
`Assembly`

```asm
.L3:
	movl	44(%rsp), %eax     读计数器
	addl	$1, %eax
	movl	%eax, 44(%rsp)     写回计数器
	movl	44(%rsp), %eax     再读回来比较
	cmpl	$299999999, %eax
	jle	.L3
```

**每次迭代都要经过栈上的那个 `long`**，这正是「每次访问都真的发生」的样子。
**要在单片机上写延时，只能用这种写法，或者改用定时器**——
前者占用 CPU，后者才让 CPU 去干别的事。

## 2.6 运行期证据二：数节拍

主机上的 `clock()` 粒度粗，只能看量级。**在 Cortex-M 上可以用 SysTick 数节拍**：
`SYST_RVR` 是重装值、`SYST_CVR` 是当前值（向下计数）、`SYST_CSR` 打开计数。
**读两次 `SYST_CVR` 的差就是这段代码消耗的节拍数**。

`C`

```c
/* arm_delay.c    主机上只编译：gcc -std=c23 -c arm_delay.c -o arm_delay.o
                  （寄存器地址在主机上不存在）
                  交叉编译 + 运行见下面 QEMU 命令 */
#include <stdint.h>
#include "semihosting.h"

/* Cortex-M 的系统节拍定时器：地址固定在系统控制区，与具体芯片无关 */
#define SYST_CSR  (*(volatile uint32_t *)0xE000E010u)   /* 控制与状态 */
#define SYST_RVR  (*(volatile uint32_t *)0xE000E014u)   /* 重装值 */
#define SYST_CVR  (*(volatile uint32_t *)0xE000E018u)   /* 当前值，向下计数 */

#define N 100000L

static void spin_plain(void)    { for (long i = 0; i < N; ++i) { } }
static void spin_volatile(void) { for (volatile long i = 0; i < N; ++i) { } }

int main(void)
{
    SYST_RVR  = 0x00FFFFFFu;      /* 24 位，减到 0 再重装 */
    SYST_CVR  = 0u;
    SYST_CSR  = 5u;               /* ENABLE | CLKSOURCE：用处理器时钟计数 */

    uint32_t t0 = SYST_CVR;
    spin_plain();
    uint32_t t1 = SYST_CVR;
    spin_volatile();
    uint32_t t2 = SYST_CVR;

    sh_puts("N = "); sh_dec32((uint32_t)N); sh_puts("\r\n");
    sh_puts("普通计数器   消耗的节拍："); sh_dec32((t0 - t1) & 0x00FFFFFFu); sh_puts("\r\n");
    sh_puts("volatile 计数器消耗的节拍："); sh_dec32((t1 - t2) & 0x00FFFFFFu); sh_puts("\r\n");
    return 0;
}

/* 真实的启动文件会调用这两个符号，这里给空实现 */
void SystemInit(void) { }
void _init(void) { }
```

**`& 0x00FFFFFF` 不能省**：`SYST_CVR` 只有 24 位，
从 `0x00000010` 减到 `0x00FFFFF0` 时，32 位减法会得到 `0xFF000020` 这种带借位的值。
先按 24 位掩码再比较，读数才正确。

交叉编译与运行（用 `A-教学素材` 里真实的链接脚本与启动文件）：

`Bash`

```bash
arm-none-eabi-gcc -mcpu=cortex-m3 -mthumb -nostartfiles --specs=nosys.specs -O2 \
  -T STM32F103XX_FLASH.ld startup_stm32f103xe.s arm_delay.c -o arm_delay.elf
qemu-system-arm -M netduinoplus2 -kernel arm_delay.elf -nographic -icount shift=0 \
  -semihosting-config enable=on,target=native
```

`实测数据`
`Text`

```text
N = 100000
普通计数器   消耗的节拍：0
volatile 计数器消耗的节拍：100802
```

**`0` 与 `100802`，两次运行完全相同**。
`-icount shift=0` 让 QEMU 按「一条指令一个虚拟纳秒」推进时间，
SysTick 的计数于是只由指令序列决定，结果可复现。
**去掉 `-icount` 之后计数由宿主机时间驱动**，同一份程序两次跑出来的差别很大
（本节两次运行是 `1412` 与 `44948`，中断次数也随之变化），
数字只能看量级——**要拿可复现的节拍数，就加 `-icount`**。

**真板上的同一段代码见下表。** 上面的数字全部来自 QEMU；
真板的节拍要用内核自带的周期计数器 `DWT_CYCCNT` 去数，
而且 flash 取指的等待周期会算进去，两组数字不会相同。

`实测数据`

| 测什么 | QEMU（`netduinoplus2`，`-icount shift=0`） | 真板 STM32F103C8（`DWT_CYCCNT`） |
|---|---|---|
| 普通计数器 100000 次（循环整个被删掉） | 0 节拍 | 同左：循环不存在，没有节拍可数 |
| `volatile` 计数器 100000 次 | 100802 节拍 | 随 flash 等待周期与时钟配置变化 |
| `volatile` 读一次外设寄存器（`GPIOA_IDR`） | 由指令序列决定 | **9 周期**（1.125 µs） |
| `volatile` 读一次 RAM 变量 | 由指令序列决定 | **6 周期**（0.75 µs） |

**后两行是在 STM32F103C8（Cortex-M3、HSI 8 MHz，1 周期 = 125 ns）上实测的**，
做法是连读 1000 次取平均，与本节 QEMU 的办法一样。
**外设寄存器比 RAM 慢 3 个周期**：前者要过 APB 总线，后者就在内核旁边。

**能外推与不能外推的分界在这里**：指令序列与内核有关、与板子无关，
所以「少了 `volatile` 循环被删掉」这条结论在真板与 QEMU 上一样成立；
节拍的绝对值则取决于 flash 等待周期与时钟树配置，只能上板测。

> [!TIP]
> **`netduinoplus2` 是 Cortex-M4 的近似板**，flash／RAM 的起始地址与 STM32F103 一致，
> 因此链接脚本可以通用；**但它的 RAM 是 128 KiB，真板只有 20 KiB**，
> 内存大小、栈顶这些数字要以真板为准。
> **板级外设（串口、GPIO、定时器）的行为也与真实芯片不同**，
> 凡涉及具体外设的结论一律要以上板为准。
> 本章只用它验证与机型无关的部分：指令序列、`volatile`、位带、中断。

## 2.7 哪些地方必须写 `volatile`

`实测数据`

| 场合 | 要不要 `volatile` | 理由 |
|---|---|---|
| 内存映射寄存器（`IDR`、`SR`、`DR`） | **要** | 值由硬件改，编译器看不见 |
| 中断服务函数与主循环共享的变量 | **要** | 中断在编译器看不见的地方改它 |
| DMA 的目标缓冲区 | **要**（并配合屏障） | 数据由 DMA 控制器写 |
| 多任务之间的共享变量 | 不够 | `volatile` 不解决原子性与顺序，用原子量或锁 |
| 只在本函数里用的局部变量 | 不要 | 没人从外面改它 |
| 传给别的函数的普通参数 | 不要 | 加上去只会挡住优化 |

**两个常写错的地方**：

**一、把 `volatile` 加在指针上而不是指向物上。**
`volatile uint32_t *p` 才对；`uint32_t *volatile p` 说的是「指针自己易变」，
寄存器内容仍然会被缓存。

**二、强制转换时丢掉限定符。**
`(uint32_t *)0x40010808u` 这种写法把 `volatile` 丢了，
编译器不会报错，只会安静地把循环优化掉。
**限定符在强制转换里丢失，是这类 bug 最隐蔽的来源。**

`C`

```c
/* cast_drop.c   只编译不运行：gcc -std=c23 -O2 -S cast_drop.c -o cast_drop.s */
#include <stdint.h>

#define IDR_V   (*(volatile uint32_t *)0x40010808u)   /* 正确 */
#define IDR_P   (*(uint32_t *)0x40010808u)            /* 少了 volatile */

void wait_good(void) { while ((IDR_V & 1u) == 0u) { } }
void wait_bad(void)  { while ((IDR_P & 1u) == 0u) { } }
```

**两个宏长得几乎一样，编出来的循环一个有 `movl (%rax), %eax`，一个没有。**
区别只在 `volatile` 这个词上。

## 2.8 `volatile` 做不到的四件事

> [!CAUTION]
> **`volatile` 只保证「访问真的发生」，不保证「访问是一个整体」，也不保证顺序。**
> 它不能替代原子量、临界区与内存屏障。下面的第 3 节给出实测。

| 问题 | `volatile` 能解决吗 |
|---|---|
| 编译器把循环里的读优化掉 | **能** |
| 「读—改—写」被中断打断 | **不能**（第 3 节） |
| 两个变量之间的先后顺序 | **不能**（编译器与 CPU 都可能重排，见《07-更底层/10-中断、并发与内存序.md》第 5 节） |
| 多核之间的可见性 | **不能**（需要屏障或原子指令） |

**这张表与《01-编译器/03-嵌入式与交叉编译.md》第 4.5 小节的表是同一结论**，
本章第 3 节把它变成可复现的数字。

---

# 第 3 节 `volatile` 不等于原子

## 3.1 一句源码，三条指令

`g_counter = g_counter + 1u;` 这一句在源码上是一次操作，
在 Cortex-M 上编出来是三条指令（取自下面程序的真实反汇编）：

`实测数据`
`Assembly`

```asm
 800030e:	686b      	ldr	r3, [r5, #4]     读 g_shared 到 r3
 8000310:	3c01      	subs	r4, #1           循环计数减一
 8000312:	f103 0301 	add.w	r3, r3, #1       加一
 8000316:	606b      	str	r3, [r5, #4]     写回 g_shared
 8000318:	d1f9      	bne.n	800030e <main+0x16>
```

**读与写之间隔着一条 `subs`**，那就是中断可以插进来的窗口。
**中断服务函数里的自增也是同一副样子**：

`实测数据`
`Assembly`

```asm
08000240 <SysTick_Handler>:
 8000240:	4b03      	ldr	r3, [pc, #12]
 8000242:	681a      	ldr	r2, [r3, #0]
 8000244:	3201      	adds	r2, #1
 8000246:	601a      	str	r2, [r3, #0]     写回 g_isr
 8000248:	685a      	ldr	r2, [r3, #4]
 800024a:	3201      	adds	r2, #1
 800024c:	605a      	str	r2, [r3, #4]     写回 g_shared
```

**如果中断正好落在主循环的 `ldr` 与 `str` 之间**：
主循环读到的旧值放在 `r3` 里，
中断进来把内存里的值加了 1 并写回，
中断返回后主循环再把 `r3 + 1` 写回去——**中断那一次自增被覆盖掉了**。
这就是「丢更新」。

**`volatile` 对此毫无办法**：它保证 `ldr` 与 `str` 都真的发生，
但这两条指令之间能不能被打断，不归它管。

## 3.2 真跑：一秒钟丢了一千次

下面这个程序让 SysTick 每 100 个时钟周期中断一次，
主循环与中断服务函数各自对同一个 `volatile` 变量做自增，
最后把「应得的数」与「实得的数」打出来。

`C`

```c
/* lost_update.c   主机上只编译：gcc -std=c23 -c lost_update.c -o lost_update.o
                   （0xE000E010 之类的地址在主机进程里没有映射）
                   Cortex-M 侧（在 WSL 里交叉编译，QEMU 里跑）：
   arm-none-eabi-gcc -mcpu=cortex-m3 -mthumb -nostartfiles --specs=nosys.specs -O2 \
     -T STM32F103XX_FLASH.ld startup_stm32f103xe.s lost_update.c -o lost_update.elf
   qemu-system-arm -M netduinoplus2 -kernel lost_update.elf -nographic -icount shift=0 \
     -semihosting-config enable=on,target=native
   volatile 保证每次都真的读写，但不保证「读—改—写」是一个整体 */
#include <stdint.h>
#include "semihosting.h"

#define SYST_CSR (*(volatile uint32_t *)0xE000E010u)   /* ENABLE | TICKINT | CLKSOURCE */
#define SYST_RVR (*(volatile uint32_t *)0xE000E014u)
#define SYST_CVR (*(volatile uint32_t *)0xE000E018u)

#define LOOPS 200000u

static volatile uint32_t g_shared;     /* 主循环与中断都动它 */
static volatile uint32_t g_isr;        /* 只记中断进了多少次 */

/* 覆盖启动文件里的弱定义 */
void SysTick_Handler(void)
{
    g_isr = g_isr + 1u;
    g_shared = g_shared + 1u;          /* 也是读—改—写 */
}

int main(void)
{
    SYST_RVR = 100u;                   /* 每 100 个时钟周期中断一次 */
    SYST_CVR = 0u;
    SYST_CSR = 7u;

    for (uint32_t i = 0; i < LOOPS; ++i) {
        g_shared = g_shared + 1u;      /* 源码上是一句话，机器码上是三条 */
    }

    SYST_CSR = 0u;

    uint32_t expect = LOOPS + g_isr;
    sh_puts("主循环次数      ："); sh_dec32(LOOPS);            sh_puts("\r\n");
    sh_puts("中断进入次数    ："); sh_dec32(g_isr);            sh_puts("\r\n");
    sh_puts("应得 g_shared   ："); sh_dec32(expect);           sh_puts("\r\n");
    sh_puts("实得 g_shared   ："); sh_dec32(g_shared);         sh_puts("\r\n");
    sh_puts("丢掉的更新      ："); sh_dec32(expect - g_shared); sh_puts("\r\n");
    return 0;
}

void SystemInit(void) { }
void _init(void) { }
```

**账是这么算的**：主循环做 200000 次自增，中断进一次做 1 次自增，
所以两个计数器加起来应当是 `200000 + g_isr`。
`g_isr` 只被中断改，而 SysTick 中断不会嵌套自己，因此它是准的。

`实测数据`
`Text`

```text
主循环次数      ：200000
中断进入次数    ：1686
应得 g_shared   ：201686
实得 g_shared   ：200675
丢掉的更新      ：1011
```

**1686 次中断，丢了 1011 次更新**——丢掉的比例超过一半，
因为主循环每次迭代只有三条指令，中断落进那个窗口的概率很高。
**加了 `-icount shift=0` 之后，两次运行的数字完全相同**，
这组数据是可复现的，不是「看运气」。

**去掉 `-icount` 时同一份程序只丢了 0 次**（多次运行中断只进了 20 到 33 次，
随宿主机负载变化，因为 QEMU 的虚拟时钟由宿主机时间驱动，中断频率远低于指令频率）。
**「跑一次没出问题」证明不了代码是对的**：
一个只在特定时序下出现的丢更新，靠试运行是抓不住的。

**真板上的同一段代码见下表。** 真板的中断频率由 `SYST_RVR` 与内核时钟共同决定
（这份实测里 SysTick 每 1000 个周期中断一次），落进「读—改—写」窗口的概率也随之改变；
对照的那一版把自增换成原子操作，其余不动。

`实测数据`

| 版本 | 期望值 | 实测值 | 差别 |
|---|---|---|---|
| 普通 `volatile` 计数器（中断与主循环都自增） | 201459（= 200000 + 1459 次中断） | **200000** | **丢了 1459 次更新：1459 次中断的增量全部丢失** |
| `__atomic_fetch_add`（`seq_cst`） | 202702（= 200000 + 2702 次中断） | **202702** | **一次没丢** |

**这是 STM32F103C8（Cortex-M3、HSI 8 MHz）上的实测**。
丢更新在真板上不是「偶尔少一两个」：**每一次中断的自增都被主循环撞掉了**，
因为主循环每次迭代只有三条指令，中断落进那个窗口的概率极高。
换成原子操作之后，`ldrex`／`strex` 的独占访问把窗口彻底关掉，一次都没丢。

> [!CAUTION]
> **`volatile` 与原子性是两件事。**
> 一个变量可以既被正确地每次都读写，又在读与写之间被中断打断。
> 中断里与主循环里同时修改的变量，必须用临界区、原子指令或 `std::atomic` 保护，
> 见《07-更底层/10-中断、并发与内存序.md》第 3 节。

## 3.3 硬件提供的原子操作：`BSRR` 与 `BRR`

**丢更新的根源是「读—改—写」三步**。
如果只改一位，可以绕开读：写 `BSRR` 的硬件只动指定的位，
其余位不受影响，因此**一条 `str` 就完成**，没有窗口。

`C`

```c
/* gpio_bits.c   只编译不运行：gcc -std=c23 -O2 -c gpio_bits.c -o gpio_bits.o
                  （这些地址在主机上不存在） */
#include <stdint.h>

/* STM32F103 的 GPIOA：基址 0x40010800 */
#define GPIOA_ODR  (*(volatile uint32_t *)0x4001080Cu)   /* 输出数据寄存器 */
#define GPIOA_BSRR (*(volatile uint32_t *)0x40010810u)   /* 置位/复位寄存器 */
#define GPIOA_BRR  (*(volatile uint32_t *)0x40010814u)   /* 复位寄存器 */

#define PIN 8u

/* 读—改—写：三条指令，中间可能被中断打断 */
void set_pin_rmw(void)   { GPIOA_ODR |= (1u << PIN); }
void clear_pin_rmw(void) { GPIOA_ODR &= ~(1u << PIN); }

/* 写 BSRR：一条指令，硬件只改那一位 */
void set_pin_bsrr(void)   { GPIOA_BSRR = (1u << PIN); }
void clear_pin_bsrr(void) { GPIOA_BRR  = (1u << PIN); }
```

`实测数据`
`Assembly`

```asm
00000000 <set_pin_rmw>:                  00000028 <set_pin_bsrr>:
   0:	4a03      	ldr	r2, [pc, #12]     28:	f44f 7280 	mov.w	r2, #256
   2:	f8d2 380c 	ldr.w	r3, [r2, #2060]   2c:	4b01      	ldr	r3, [pc, #4]
   6:	f443 7380 	orr.w	r3, r3, #256      2e:	f8c3 2810 	str.w	r2, [r3, #2064]
   a:	f8c2 380c 	str.w	r3, [r2, #2060]   32:	4770      	bx	lr
   e:	4770      	bx	lr
```

**左边是 `ldr`／`orr`／`str`，右边只有一条 `str`**（`#2060` 是 `0x80C`，
`#2064` 是 `0x810`，即 `ODR` 与 `BSRR` 相对 `GPIOA` 的偏移）。

> [!IMPORTANT]
> **硬件的原子性比软件便宜。** 只要外设提供一个「只写目标位」的寄存器，
> 就应当用它，而不是先读回来改一位再写回去。
> `BSRR` 的低 16 位置 1、高 16 位清 0（《01-编译器/03-嵌入式与交叉编译.md》第 4.3 节），
> `BRR` 只负责清 0，也是同样的思路。

**注意 `toggle` 这种操作没有对应的硬件捷径**：
`HAL_GPIO_TogglePin` 必须先读 `ODR` 再写 `BSRR`，中间照样有窗口。

**如果非要改的是内存里的一个变量**，硬件就没有捷径可走了，
只能靠内核提供的独占访问指令：Cortex-M3 有 `ldrex`／`strex`，
编译器把原子操作编成一对「独占读、独占写」，写失败就重试。
**下面是本机交叉编译出来的真实产物**：

`C`

```c
/* atomic_vs_plain.c   只编译成目标文件：gcc -std=c23 -c atomic_vs_plain.c -o atomic_vs_plain.o
   Cortex-M 侧：arm-none-eabi-gcc -mcpu=cortex-m3 -mthumb -O2 -c atomic_vs_plain.c -o atomic_vs_plain.o
                arm-none-eabi-objdump -d atomic_vs_plain.o */
#include <stdint.h>

volatile uint32_t g_plain;
volatile uint32_t g_atomic;

/* 普通自增：读—改—写三条指令，中间可以被中断打断 */
void inc_plain(void) { g_plain = g_plain + 1u; }

/* 原子自增：ldrex/strex 独占访问，失败就重试 */
void inc_atomic(void) { __atomic_fetch_add(&g_atomic, 1u, __ATOMIC_SEQ_CST); }
```

`实测数据`
`Assembly`

```asm
00000000 <inc_plain>:                    00000010 <inc_atomic>:
   0:	4a02      	ldr	r2, [pc, #8]      10:	f3bf 8f5b 	dmb	ish
   2:	6813      	ldr	r3, [r2, #0]      14:	4b05      	ldr	r3, [pc, #20]
   4:	3301      	adds	r3, #1            16:	e853 1f00 	ldrex	r1, [r3]
   6:	6013      	str	r3, [r2, #0]      1a:	3101      	adds	r1, #1
   8:	4770      	bx	lr                1c:	e843 1200 	strex	r2, r1, [r3]
                                         20:	2a00      	cmp	r2, #0
                                         22:	d1f8      	bne.n	16 <inc_atomic+0x6>
                                         24:	f3bf 8f5b 	dmb	ish
                                         28:	4770      	bx	lr
```

**自增这一步从三条指令变成了七条**（两条 `dmb`、一对 `ldrex`／`strex`、
一次比较与一个重试分支）。`dmb` 是内存屏障，保证屏障前后的访问不越过它重排；
`strex` 的返回值放在 `r2` 里，非 0 表示这期间有别人动过这块内存，于是跳回 `ldrex` 重来。
**这就是原子性的价格**：普通自增最便宜，但会丢更新；原子自增任何时刻都正确，代价是多出来的指令。

`实测数据`

| 写法 | 指令构成 | 真板 STM32F103C8 上的周期数 |
|---|---|---|
| 普通 `volatile` 变量自增 | `ldr`／`adds`／`str` 三条 | **7 周期**（0.875 µs） |
| `__atomic_fetch_add`（`seq_cst`） | 两条 `dmb` 加 `ldrex`／`strex` 与重试 | **14 周期**（1.75 µs） |

**代价正好是两倍**，多出来的七个周期就是两条屏障、独占访问与那次比较。
**这组数字来自 STM32F103C8（Cortex-M3、HSI 8 MHz）**，各连做 1000 次取平均。
**写一次 `BSRR` 要多少周期，本机没有测**：真板上没有接 LED 与示波器，
看到的是总线写而不是引脚电平，因此这一格留空。

## 3.4 小结

> [!NOTE]
> **`volatile` 管的是「访问发不发生」，原子性管的是「访问能不能被打断」。**
> 本章的实测给出两个现场：少了 `volatile` 的轮询循环在真板上永远等不到硬件
> （调试器改了内存，PC 停在循环里）；
> 加了 `volatile` 的计数器在 1459 次中断里把 1459 次更新全丢了。
> 前者靠加关键字解决，后者要靠临界区或原子操作，
> 那部分内容在《07-更底层/10-中断、并发与内存序.md》第 3 节。

---

# 第 4 节 位带：把「改一位」变成一次写

## 4.1 两个区与一条换算公式

Cortex-M3 与 M4 提供了一个叫**位带**（bit-band）的机制：
**SRAM 与外设区里每一个位，都在另一个「别名区」有一个 32 位字与之对应。**
读写那个字，就等于读写那一位，而且是硬件层面的一次访问。

`Text`

```text
        位带区（原始数据）                    别名区（每个位占一个 32 位字）
   ┌──────────────────────────┐        ┌──────────────────────────┐
   │ 0x20000000  起 1 MB      │  ───▶  │ 0x22000000  起 32 MB     │  SRAM
   │ 0x40000000  起 1 MB      │  ───▶  │ 0x42000000  起 32 MB     │  外设
   └──────────────────────────┘        └──────────────────────────┘
       一个字节 8 位                       每一位 → 一个字（4 字节，只用最低位）
```

**换算公式**（以 SRAM 为例）：

`Text`

```text
别名地址 = 0x22000000 + (字节地址 - 0x20000000) × 32 + 位号 × 4
                        └── 每个字节有 8 个位，8 × 4 字节 = 32 ──┘
```

**为什么是 32**：一个字节的 8 个位各占一个 32 位字，
`8 × 4 = 32` 个字节的别名空间。因此别名区是位带区的 32 倍大。

`C`

```c
/* bitband.c   主机上只编译：gcc -std=c23 -c bitband.c -o bitband.o
               （别名区地址在主机进程里没有映射）
               Cortex-M 侧（在 WSL 里交叉编译，QEMU 里跑）：
   arm-none-eabi-gcc -mcpu=cortex-m3 -mthumb -nostartfiles --specs=nosys.specs \
     -T STM32F103XX_FLASH.ld startup_stm32f103xe.s bitband.c -o bitband.elf
   qemu-system-arm -M netduinoplus2 -kernel bitband.elf -nographic \
     -semihosting-config enable=on,target=native */
#include <stdint.h>
#include "semihosting.h"

/* SRAM 位带别名：0x20000000 起 1 MB 里的每一个位，在 0x22000000 起各有一个 32 位字与之对应
   别名地址 = 0x22000000 + (字节地址 - 0x20000000) * 32 + 位号 * 4 */
static volatile uint32_t *alias_of(const volatile uint32_t *word, unsigned bit)
{
    uint32_t addr = (uint32_t)(uintptr_t)word;
    return (volatile uint32_t *)(uintptr_t)(0x22000000u + (addr - 0x20000000u) * 32u + bit * 4u);
}

static volatile uint32_t g_reg;      /* 把它当成一个「寄存器」 */

int main(void)
{
    volatile uint32_t *a5 = alias_of(&g_reg, 5);

    sh_puts("g_reg 的地址       ："); sh_hex32((uint32_t)(uintptr_t)&g_reg); sh_puts("\r\n");
    sh_puts("第 5 位的别名地址   ："); sh_hex32((uint32_t)(uintptr_t)a5);    sh_puts("\r\n");

    g_reg = 0;
    *a5 = 1u;                                   /* 只写一个位 */
    sh_puts("写别名之后 g_reg   ："); sh_hex32(g_reg); sh_puts("\r\n");

    *a5 = 0u;
    sh_puts("清别名之后 g_reg   ："); sh_hex32(g_reg); sh_puts("\r\n");

    g_reg = 0xFFFFFFFFu;
    sh_puts("全 1 时读别名第 5 位："); sh_dec32(*a5); sh_puts("\r\n");
    return 0;
}

/* 真实启动文件会调用它们 */
void SystemInit(void) { }
void _init(void) { }
```

## 4.2 QEMU 里的实测

`实测数据`
`Text`

```text
g_reg 的地址       ：0x20000000
第 5 位的别名地址   ：0x22000014
写别名之后 g_reg   ：0x00000020
清别名之后 g_reg   ：0x00000000
全 1 时读别名第 5 位：1
```

**验算别名地址**：`g_reg` 在 `0x20000000`，字节偏移为 0，
位号 5 → `0x22000000 + 0 * 32 + 5 * 4 = 0x22000014`。
**写它得到 `0x00000020`**，正是 `1 << 5`；
**写 0 之后整个字归零**；**读全 1 的第 5 位得到 1**。
QEMU 的 `netduinoplus2` 实现了这套地址映射，因此这组数字来自真实执行。

**位带最实用的地方是把「读—改—写」换成一次写**：

`C`

```c
/* bitband_use.c   只编译不运行：gcc -std=c23 -O2 -c bitband_use.c -o bitband_use.o */
#include <stdint.h>

/* 外设位带区：0x42000000 + (地址 - 0x40000000) * 32 + 位号 * 4 */
#define BB_PERIPH(addr, bit) \
    (*(volatile uint32_t *)(0x42000000u + ((uint32_t)(addr) - 0x40000000u) * 32u + (uint32_t)(bit) * 4u))

#define LED_ODR  0x4001080Cu
#define LED_PIN  8u

void led_off_rmw(void) { *(volatile uint32_t *)LED_ODR &= ~(1u << LED_PIN); }  /* 读—改—写 */
void led_off_bb(void)  { BB_PERIPH(LED_ODR, LED_PIN) = 0u; }                  /* 一次写 */
```

**两次写之间的差别与 `BSRR` 那一节是同一个道理**：
`led_off_bb` 只发一条 `str`，中断插进来也不会丢更新。

## 4.3 哪些芯片有，为什么现在少用

`文档`

> "The Cortex-M3 and Cortex-M4 processors support bit-band accesses to the SRAM
> and peripheral memory regions."
>
> —— Arm Cortex-M 技术参考手册中关于位带的说明（各版本章节号不同，以手头版本为准）

**用之前先查手册**：这些地址在别名区里，编译器不会替你检查，
写一个不存在的别名地址不会报错，只会写进一块没人看的内存。

`待确认`

**Cortex-M0、M0+ 与 Cortex-M7 没有位带区**这一条，以具体型号的技术参考手册为准：
本机没有 M0／M0+／M7 的编译目标，QEMU 的 `netduinoplus2` 又是 Cortex-M4 的近似板，
用它验不了「别的内核没有位带」这件事。

> [!TIP]
> **位带解决的是「单个位的原子写」，不是「更快」。**
> 它把一次读改写换成一次写，代价是别名区占用的地址空间是原来的 32 倍。
> 外设已经提供 `BSRR` 这类寄存器时，优先用外设自己的寄存器；
> 位带主要用于 SRAM 里的标志位。

---

# 第 5 节 位域：布局、位序与四个陷阱

## 5.1 `sizeof` 的实测对照

位域的语法在《04-语法/09-结构体、联合体与 enum.md》第 4 节讲过，
结论是「布局由实现决定」。**这里的「实现」有两个层面**：
编译器的分配策略，以及目标 ABI 的规则。
同一份源码在 x86-64 与 Cortex-M 上各编一次，大小就能看出差别。

`C`

```c
/* bf_variants.c   编译：gcc -std=c23 -O2 bf_variants.c -o bf_variants
                   交叉编译（Cortex-M）+ QEMU 运行的命令见本节末尾 */
#include <stdint.h>
#include <string.h>
#include "semihosting.h"

struct v1 { unsigned a : 1;  unsigned b : 3;  unsigned c : 4;  unsigned d : 8; };
struct v2 { unsigned char a : 4;  unsigned char b : 4; };
struct v3 { unsigned a : 20; unsigned b : 20; };
struct v4 { unsigned int a : 1; unsigned char b : 1; };
struct v5 { unsigned short a : 9; unsigned short b : 9; };
struct v6 { unsigned a : 24; unsigned b : 24; };
struct v7 { unsigned long long a : 33; };
struct v8 { int a : 1; };                       /* 有符号的位域 */
struct v9 { unsigned a : 3; unsigned : 0; unsigned b : 3; };   /* 零宽位域 */

static void show(const char *name, unsigned size, unsigned align)
{
    sh_puts(name);
    sh_puts(" sizeof="); sh_dec32(size);
    sh_puts(" align=");  sh_dec32(align);
    sh_puts("\r\n");
}

int main(void)
{
    show("v1  1+3+4+8 位    ", (unsigned)sizeof(struct v1), (unsigned)_Alignof(struct v1));
    show("v2  char 型 4+4   ", (unsigned)sizeof(struct v2), (unsigned)_Alignof(struct v2));
    show("v3  20+20 位      ", (unsigned)sizeof(struct v3), (unsigned)_Alignof(struct v3));
    show("v4  int 1 + char 1", (unsigned)sizeof(struct v4), (unsigned)_Alignof(struct v4));
    show("v5  short 型 9+9  ", (unsigned)sizeof(struct v5), (unsigned)_Alignof(struct v5));
    show("v6  24+24 位      ", (unsigned)sizeof(struct v6), (unsigned)_Alignof(struct v6));
    show("v7  long long : 33", (unsigned)sizeof(struct v7), (unsigned)_Alignof(struct v7));
    show("v8  int a : 1     ", (unsigned)sizeof(struct v8), (unsigned)_Alignof(struct v8));
    show("v9  3 位 + 零宽 + 3", (unsigned)sizeof(struct v9), (unsigned)_Alignof(struct v9));

    /* 有符号位域读出来是什么 */
    struct v8 s;
    memset(&s, 0, sizeof s);
    s.a = 1;
    sh_puts("s.a 写 1，读回来 = "); sh_dec32((uint32_t)(int32_t)s.a); sh_puts("\r\n");

    /* 超范围赋值：静默截断 */
    struct v1 f;
    memset(&f, 0, sizeof f);
    f.b = 9;                                   /* 只有 3 位，9 = 0b1001 放不下 */
    sh_puts("3 位字段写 9，读回来 = "); sh_dec32(f.b); sh_puts("\r\n");
    return 0;
}

void SystemInit(void) { }
void _init(void) { }
```

`实测数据`

| 变体 | x86-64（Windows，`gcc` 15.2.0） | Cortex-M3（`arm-none-eabi-gcc` 13.2.1） |
|---|---|---|
| v1 `1+3+4+8` 位 | `sizeof=4` `align=4` | `sizeof=4` `align=4` |
| v2 `char` 型 `4+4` | `sizeof=1` `align=1` | `sizeof=1` `align=1` |
| v3 `20+20` 位 | `sizeof=8` `align=4` | `sizeof=8` `align=4` |
| **v4 `int:1` + `char:1`** | **`sizeof=8` `align=4`** | **`sizeof=4` `align=4`** |
| v5 `short` 型 `9+9` | `sizeof=4` `align=2` | `sizeof=4` `align=2` |
| v6 `24+24` 位 | `sizeof=8` `align=4` | `sizeof=8` `align=4` |
| v7 `unsigned long long : 33` | `sizeof=8` `align=8` | `sizeof=8` `align=8` |
| v8 `int a : 1` | `sizeof=4` `align=4` | `sizeof=4` `align=4` |
| v9 `3` 位 + 零宽 + `3` 位 | `sizeof=8` `align=4` | `sizeof=8` `align=4` |

**除了 v4，两边完全一致**。v4 里两个位域的类型不同
（`unsigned int` 与 `unsigned char`），
**x86-64 的规则是「换类型就换存储单元」**，于是两个位域各占一个 4 字节单元、
结构体被对齐到 8 字节；**ARM 的 AAPCS 把相邻的不同类型位域挤进同一个单元**，
于是 1 位加 1 位只占 4 字节。
**同一个结构体，换一颗芯片就多了 4 字节**——这就是「布局由实现决定」的实际后果。

**这一格差异不是理论上的可能性，而是本机实测**：
交叉编译到 Cortex-M3 后 `sizeof(struct v4)` 是 4，
本机 x86-64 上是 8。**把位域结构体直接写进通信协议或存进 flash，
换编译器或换架构就会对不上。**

## 5.2 内存里的位序

`sizeof` 只说明占多少字节，不说明哪一位落在哪个位置。
把结构体清零后按字段赋值，再把原始字节打出来，就能看清排布：

`C`

```c
/* bitfield.c   编译：gcc -std=c23 bitfield.c -o bitfield
                交叉编译（Cortex-M）：arm-none-eabi-gcc -mcpu=cortex-m3 -mthumb \
                  -nostartfiles --specs=nosys.specs -T STM32F103XX_FLASH.ld \
                  startup_stm32f103xe.s bitfield.c -o bitfield.elf */
#include <stdint.h>
#include <string.h>
#include "semihosting.h"

/* 1 + 3 + 4 = 8 位，正好一个字节；再加 8 位，看编译器怎么排 */
struct flags {
    unsigned a : 1;
    unsigned b : 3;
    unsigned c : 4;
    unsigned d : 8;
};

static void put_byte(unsigned v)
{
    static const char d[] = "0123456789abcdef";
    char s[3];
    s[0] = d[(v >> 4) & 0xFu];
    s[1] = d[v & 0xFu];
    s[2] = '\0';
    sh_puts(s);
}

static void dump(const char *tag, const void *p, unsigned n)
{
    const unsigned char *b = (const unsigned char *)p;
    sh_puts(tag);
    for (unsigned i = 0; i < n; ++i) {
        sh_puts(" ");
        put_byte(b[i]);
    }
    sh_puts("\r\n");
}

int main(void)
{
    struct flags f;

    sh_puts("sizeof(struct flags) = ");
    sh_dec32((uint32_t)sizeof f);
    sh_puts("\r\n");

    memset(&f, 0, sizeof f);
    f.a = 1; f.b = 2; f.c = 3; f.d = 4;
    dump("a=1 b=2 c=3 d=4 ：", &f, (unsigned)sizeof f);

    memset(&f, 0xFF, sizeof f);
    dump("全部写 0xFF     ：", &f, (unsigned)sizeof f);

    f.a = 0;
    dump("再把 a 清 0     ：", &f, (unsigned)sizeof f);
    return 0;
}

/* 真实启动文件会调用它们 */
void SystemInit(void) { }
void _init(void) { }
```

`实测数据`
`Text`

```text
sizeof(struct flags) = 4
a=1 b=2 c=3 d=4 ： 35 04 00 00
全部写 0xFF     ： ff ff ff ff
再把 a 清 0     ： fe ff ff ff
```

**`35` 这个字节把排布说清楚了**：`0x35 = 0b0011_0101`。

`Text`

```text
 bit7  bit6  bit5  bit4   bit3  bit2  bit1  bit0
   0     0     1     1       0     1     0     1
   └──── c = 3 ────┘       └─ b = 2 ─┘   └ a = 1
```

**三个字段从小端方向依次排进低位**：`a` 占 bit0，`b` 占 bit1–3，`c` 占 bit4–7，
`d` 落到下一个字节（`04`）。
**Cortex-M3 上跑出来的字节完全相同**（同一份源码交叉编译后在 QEMU 里运行，
输出与主机一致）。

**换一个字节序，排布就变了。** 用同一份源码编一个大端目标文件，
把常量放进 `.rodata` 再看文件里的字节：

`C`

```c
/* bf_bytes.c   只编译成目标文件：gcc -std=c23 -c bf_bytes.c -o bf_bytes.o
                 （没有 main，只看常量在文件里的字节排布）
                 Cortex-M 侧（在 WSL 里交叉编译 + objdump）：
   arm-none-eabi-gcc -mcpu=cortex-m3 -mthumb -c bf_bytes.c -o bf_le.o
   arm-none-eabi-gcc -mbig-endian -mcpu=cortex-m3 -mthumb -c bf_bytes.c -o bf_be.o
   arm-none-eabi-objdump -s -j .rodata bf_le.o
   arm-none-eabi-objdump -s -j .rodata bf_be.o */

typedef unsigned int u32;

struct flags {                     /* 1 + 3 + 4 + 8 = 16 位 */
    u32 a : 1;
    u32 b : 3;
    u32 c : 4;
    u32 d : 8;
};

/* 放在 .rodata 里的常量：对象文件里就是内存里的字节顺序 */
const struct flags g_flags = { 1u, 2u, 3u, 4u };
const u32          g_word  = 0x11223344u;
```

`实测数据`
`Text`

```text
--- 小端对象（elf32-littlearm）
 0000 35040000 44332211

--- 大端对象（elf32-bigarm）
 0000 a3040000 11223344
```

**两处都反了**：
`g_flags` 从小端的 `35 04` 变成大端的 `a3 04`
（`a = 1` 从字节的最低位挪到了最高位，
`0xA3 = 0b1010_0011`，最高位是 `a`，接着三位是 `b`）；
`g_word` 从 `44 33 22 11` 变成 `11 22 33 44`。
**同一个编译器、同一份源码，只加了 `-mbig-endian`。**
字节序本身的细节在《07-更底层/04-字节序与数据表示.md》章节。

> [!CAUTION]
> **不要把位域结构体直接当成线格式。** 位序、填充、单元大小
> 三样都由实现决定，跨编译器、跨架构、跨字节序都可能不同。
> 通信协议与文件格式应当用显式的移位与掩码读写字节数组，
> 位域只用在「同一套工具链内部表示寄存器」这种场合。

## 5.3 四个陷阱

**陷阱一：有符号的位域会带符号扩展。**
`struct { int a : 1; }` 里写 `1`，读回来是 `-1`：

`实测数据`
`Text`

```text
s.a 写 1，读回来 = 4294967295
```

**同样一个位，用 `unsigned` 声明读回来是 1，用 `int` 声明读回来是 -1。**
`4294967295` 是把它按 32 位无符号打印的结果，即 `0xFFFFFFFF`。
**位域的类型决定读出来之后要不要扩展到整个类型的宽度**，
这一点在解析硬件寄存器时经常出错。

**陷阱二：超出位宽的值被静默截断，只有一个警告。**

`实测数据`
`Text`

```text
bf_variants.c: 在函数 'main' 中:
bf_variants.c:46:11: warning: unsigned conversion from 'int' to 'unsigned char:3'
changes value from '9' to '1' [-Woverflow]
   46 |     f.b = 9;
      |           ^
3 位字段写 9，读回来 = 1
```

**`f.b = 9` 只保留最低三位**：`9 = 0b1001`，三位放不下，于是变成 `1`。
编译器给了 `-Woverflow` 警告，但**默认不是错误**，
不加 `-Wall` 甚至看不到。**要把它变成编译错误，用 `-Werror=overflow`。**

**陷阱三：位域没有地址。**
`&f.a` 编译不过，「指向位域的指针」不存在，
因此**位域不能直接传给 `scanf`／`printf`／任何需要地址的接口**。

**陷阱四：`volatile` 位域不能保证一次访问一位。**
`volatile` 结构体里的位域，编译器仍会按整个存储单元读写，
一次改一个 `volatile` 位域可能变成「读整个单元、改一位、写回整个单元」，
**在寄存器上这是灾难**（会改到同一单元里其他位）。
**寄存器请用位带或 `BSRR` 一类硬件机制，不要用 `volatile` 位域。**

## 5.4 替代写法

**位域在两类场合仍然好用**：同一工具链内部的寄存器描述、
以及不需要跨平台的配置字。**跨边界的地方一律用显式位运算**：

`C`

```c
/* bit_manual.c   编译：gcc -std=c23 bit_manual.c -o bit_manual */
#include <stdio.h>
#include <stdint.h>

/* 相同的一张位表，用移位与掩码写出来：布局完全由自己控制 */
#define FIELD_MODE_POS   16u
#define FIELD_MODE_MASK  0x3FFFu
#define FIELD_IRQ_POS    30u
#define FIELD_EN_POS     31u

static uint32_t pack(uint32_t mode, int irq, int en)
{
    uint32_t v = 0;
    v |= (mode & FIELD_MODE_MASK) << FIELD_MODE_POS;   /* 先掩码再移位 */
    v |= (uint32_t)(irq & 1) << FIELD_IRQ_POS;
    v |= (uint32_t)(en & 1) << FIELD_EN_POS;
    return v;
}

static uint32_t get_mode(uint32_t v)
{
    return (v >> FIELD_MODE_POS) & FIELD_MODE_MASK;    /* 先移位再掩码 */
}

int main(void)
{
    uint32_t reg = pack(3u, 1, 1);
    printf("pack 之后 = 0x%08X\n", reg);
    printf("取回的 mode = %u\n", get_mode(reg));
    return 0;
}
```

**两条书写规矩**：**写入时先掩码再移位**（防止高位溢出污染别的字段），
**读出时先移位再掩码**。
**方向搞反就会出现「改了 A 字段结果 B 字段也变了」**，
而这类错误在代码审查里不容易看出来。

---

# 第 6 节 `register` 关键字为什么没用了

## 6.1 它的承诺，与编译器的回答

`register` 出现在 C 的存储类说明符里，字面意思是「把这个变量放在寄存器里」。
**标准从一开始就把这句话写成建议，而不是命令**：

`文档`

> "A declaration of an identifier for an object with storage-class specifier register
> suggests that access to the object be as fast as possible. The extent to which such
> suggestions are effective is implementation-defined."
>
> —— N3220 §6.7.2/12

**「implementation-defined」意味着编译器可以完全不理它**，
而今天的编译器确实完全不理：

`C`

```c
/* reg_kw.c   只生成汇编：gcc -std=c23 -O2 -S reg_kw.c -o reg_kw.s */
#include <stdio.h>

/* 两个函数只差一个 register，看编译器给出的机器码是否相同 */
int with_register(int n)
{
    register int i;
    int sum = 0;
    for (i = 0; i < n; ++i) {
        sum += i * 3;
    }
    return sum;
}

int without_register(int n)
{
    int i;
    int sum = 0;
    for (i = 0; i < n; ++i) {
        sum += i * 3;
    }
    return sum;
}

int main(void)
{
    printf("%d %d\n", with_register(10), without_register(10));
    return 0;
}
```

`实测数据`
`Assembly`

```asm
with_register:                    without_register:
	testl	%ecx, %ecx               	testl	%ecx, %ecx
	movl	%ecx, %r8d               	movl	%ecx, %r8d
	jle	.L4                      	jle	.L15
	leal	(%rcx,%rcx,2), %ecx      	leal	(%rcx,%rcx,2), %ecx
	xorl	%eax, %eax               	xorl	%eax, %eax
	xorl	%edx, %edx               	xorl	%edx, %edx
	andl	$1, %r8d                 	andl	$1, %r8d
	je	.L3                      	je	.L14
	movl	$3, %eax                 	movl	$3, %eax
	cmpl	%ecx, %eax               	cmpl	%ecx, %eax
	je	.L1                      	je	.L12
```

**两份机器码逐条相同，只有跳转标签的编号不同。**
`-O2` 下 `i` 本来就住在寄存器里，
而「哪个变量住哪个寄存器」由寄存器分配算法决定，
**关键字给不出任何额外信息**。`-O0` 下也一样：两个函数都不做寄存器分配。

## 6.2 它剩下的唯一效果：不许取地址

`register` 在 C 里仍然留着一条硬约束：

`C`

```c
/* reg_addr.c   编译（C23）：gcc -std=c23 reg_addr.c -o reg_addr （失败） */
#include <stdio.h>

int main(void)
{
    register int x = 1;
    int *p = &x;              /* register 变量取地址：C 里不允许 */
    printf("%d\n", *p);
    return 0;
}
```

`实测数据`
`Text`

```text
reg_addr.c: 在函数 'main' 中:
reg_addr.c:7:5: error: address of register variable 'x' requested
    7 |     int *p = &x;
      |     ^~~
```

**理由是自洽的**：一个变量如果真的只存在于寄存器里，它就没有内存地址。
标准把这句话写进了脚注：

`文档`

> "The implementation can treat any register declaration simply as an auto declaration.
> However, whether or not addressable storage is used, the address of any part of an
> object declared with storage-class specifier register cannot be computed, either
> explicitly (by use of the unary & operator ...) or implicitly (...)."
>
> —— N3220 §6.7.2 脚注 128

**C++ 走得更远：直接把 `register` 从存储类说明符里删掉了。**

`文档`

> "Change: Removal of register storage-class-specifier.
> Rationale: Enable repurposing of deprecated keyword in future revisions of this International Standard.
> Effect on original feature: A valid C++ 2014 declaration utilizing the register
> storage-class-specifier is ill-formed in this International Standard.
> The specifier can simply be removed to retain the original meaning."
>
> —— N4659 §C.4.3（Clause 10: declarations）

**实测表现是「默认只警告，加上 `-pedantic-errors` 才是错误」**：

`C++`

```cpp
// reg_kw.cpp   编译（C++17）：g++ -std=c++17 reg_kw.cpp -o reg_kw    （默认只警告，不报错）
//              想让它变成错误：g++ -std=c++17 -pedantic-errors reg_kw.cpp -o reg_kw
int with_register()
{
    register int i = 0;       // C++17 起 register 不再是存储类说明符
    return i;
}

int main() { return with_register(); }
```

`实测数据`
`Text`

```text
默认：        warning: ISO C++17 does not allow 'register' storage class specifier [-Wregister]
-pedantic-errors：error: ISO C++17 does not allow 'register' storage class specifier [-Wregister]
```

**「ill-formed 但只给警告」是编译器对旧代码的宽容**，
不是标准允许。**新写的 C++ 代码里不要出现 `register`。**

## 6.3 今天该怎么表达「我想让它快」

**交给优化器，然后用汇编验证。** 编译器在 `-O2` 下做的事比手写关键字多得多：
把循环变量放进寄存器、把常量折叠掉、把函数内联、把不必要的一次访存删掉。

`实测数据`

| 想达到的目的 | 老写法 | 今天的做法 |
|---|---|---|
| 变量放在寄存器里 | `register int i` | 什么都不写，开 `-O2`，看反汇编确认 |
| 告诉编译器两个指针不重叠 | 无 | C 用 `restrict`，C++ 用引用或 `__restrict`（扩展，见《07-更底层/12-编译器扩展与未定义行为.md》章节） |
| 保证每次都真的读写内存 | `volatile` | 仍是 `volatile`（本章主题） |
| 让一次读改写不可打断 | 无 | 原子量、临界区、硬件原子寄存器 |
| 让函数一定被内联 | 无 | `inline` 只是建议；强制内联是编译器扩展 |

**`restrict` 是 C 独有的限定符**，《04-语法/02-数据类型与类型系统.md》第 3.3 小节讲过它的含义；
**C++ 没有 `restrict`**，需要用编译器扩展的 `__restrict` 或改用引用与 `const`。

---

# 第 7 节 速查表

`实测数据`

| 常用件 | 一句话用途 | 典型坑 |
|---|---|---|
| `*(volatile uint32_t *)0x4001080C` | 访问内存映射寄存器 | 强转里丢掉 `volatile`：编得过、跑起来死循环 |
| `volatile uint32_t *p` | 指向的寄存器每次都要真读 | 写成 `uint32_t *volatile p` 只保护了指针自己 |
| `__I` / `__O` / `__IO`（CMSIS） | 用 `const` 与 `volatile` 表达寄存器权限 | 只当装饰看，自己写时忘了加 `volatile` |
| `-O2 -S` + 读反汇编 | 确认循环里还有没有那条读内存的指令 | 只看 `-O0` 的汇编，看不到优化后的样子 |
| `-icount shift=0`（QEMU） | 让虚拟时间只由指令序列决定，结果可复现 | 不加时数字随宿主机负载变化，不能当结论 |
| SysTick（`0xE000E010`） | 数一段代码消耗的节拍 | `SYST_CVR` 只有 24 位，做差之后要按 24 位掩码 |
| semihosting（`bkpt 0xAB`） | 裸机上不接串口就打印 | 三条指令要写在同一个 `asm` 块里，别依赖寄存器变量 |
| `BSRR` / `BRR` | 只改一位的一次写，硬件保证原子 | `toggle` 仍需先读 `ODR`，照样有窗口 |
| 位带别名（`0x22000000` / `0x42000000`） | 把「改一位」变成一次写 | M0／M0+／M7 没有位带；地址算错不会报错 |
| `struct` 位域 | 按手册的位划分声明寄存器 | 跨编译器／跨架构 `sizeof` 会变（实测 v4：8 与 4） |
| `_Alignof` + `sizeof` 实测 | 看清位域结构体到底占多少字节 | 拿它当线格式：换台机器就对不上 |
| 显式「先掩码再移位」写入 | 跨平台的位字段读写 | 写入顺序写反：改了 A 字段 B 字段也变 |
| `volatile` 计数器 + 中断 | 主循环与中断共享数据 | **丢更新**（QEMU 里 1686 次中断丢 1011 次；真板上 1459 次中断全丢） |
| `__atomic_fetch_add` | 让一次自增不可打断 | 比普通自增贵一倍（真板 7 周期与 14 周期） |
| `DWT_CYCCNT`（真板数节拍） | 用内核周期计数器量一段代码 | `DEMCR` 的 `TRCENA` 没打开时计数器不动 |
| `mww`（OpenOCD） | 调试器直接写内存，当作「外设就绪」 | 少了 `volatile` 时改了内存程序也不知道，PC 停在循环里 |
| `register` | 已无实际作用 | C++17 起 ill-formed；C 里还不能对它取地址 |
| `restrict`（C） | 告诉编译器两个指针不重叠 | C++ 没有这个关键字 |
| `-Woverflow` / `-Werror=overflow` | 抓位域超范围赋值 | 默认只是警告，静默截断 |

---

# 附录 A 相关文档

| 文档 | 关系 |
|---|---|
| 《01-编译器/03-嵌入式与交叉编译.md》第 4 节 | **前置**：寄存器是什么、`GPIOA->ODR` 为什么是访问 `0x4001080C`、`volatile` 的最小对照 |
| 《01-编译器/03-嵌入式与交叉编译.md》第 4.5 小节 | **前置**：`volatile` 能做到与做不到的四件事（本章第 3 节把「做不到」做成实测） |
| 《04-语法/02-数据类型与类型系统.md》第 3.2 小节 | **前置**：`volatile` 作为类型限定符的语法与语义 |
| 《04-语法/02-数据类型与类型系统.md》第 3.4 小节 | **前置**：`register` 的语法 |
| 《04-语法/04-表达式与运算符.md》第 3.5 小节 | **前置**：位运算符 |
| 《04-语法/09-结构体、联合体与 enum.md》第 4 节 | **前置**：位域的写法与「布局由实现决定」 |
| 《03-构建工具链/05-优化等级.md》章节 | **前置**：`-O0`／`-O2` 分别做什么 |
| 《07-更底层/04-字节序与数据表示.md》章节 | 相关：同一个字在不同字节序下的字节排布 |
| 《07-更底层/02-对齐、填充与缓存.md》第 2 节 | **后续**：位域结构体里的填充与对齐代价 |
| 《07-更底层/10-中断、并发与内存序.md》第 3 节与第 4 节 | **后续**：临界区、原子量、内存屏障 |
| 《07-更底层/12-编译器扩展与未定义行为.md》章节 | **后续**：`__restrict`、内建函数、属性 |

**本章用到的素材与配套件**：

| 用途 | 位置 |
|---|---|
| `gpio.c`／`gpio.h` 真实代码 | [`A-教学素材/01-编译器/嵌入式/寄存器与位运算/gpio.c`](../A-教学素材/01-编译器/嵌入式/寄存器与位运算/gpio.c) |
| 真实链接脚本与启动文件 | [`A-教学素材/01-编译器/嵌入式/链接脚本与启动`](../A-教学素材/01-编译器/嵌入式/链接脚本与启动) |
| 可运行的寄存器与 `volatile` 对照 | [B-examples/07-lower-level/02-volatile-and-registers/](../B-examples/07-lower-level/02-volatile-and-registers/) |
| 动手练习：内存映射 I/O | [C-templates/07-lower-level/02-mmio-lab/](../C-templates/07-lower-level/02-mmio-lab/) |
| 一键跑 `nm`／`objdump` 等工具 | [B-examples/07-lower-level/06-binary-tools/](../B-examples/07-lower-level/06-binary-tools/) |
