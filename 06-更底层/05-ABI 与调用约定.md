# ABI 与调用约定

> **授权**：本章节属教材文档部分，采用 [CC BY-NC-ND 4.0](../LICENSE)
> 加[附加条款](../许可附加条款.md)授权：署名、非商业、禁止演绎、不允许二次分发。
> 文中引用的代码不受此限，各自保留原有许可证，详见[版权与许可说明.md](../版权与许可说明.md)。

**一个函数被调用时，参数放在哪里？** 这个问题在源码层面看不出来，
因为语言只规定「调用时参数按值传入」，不规定它们走寄存器还是走栈。
**答案由 ABI 给出**——应用二进制接口，一份「编译器之间、编译单元之间的契约」。

**契约存在的原因是分离编译。** `a.c` 里的函数被 `b.c` 调用，
两个文件可能由不同的编译器、不同的版本、甚至不同的语言编译，
它们之间唯一的共同语言就是 ABI。
**只要有一边不守约定，链接期或者运行期就出问题**：
轻则 `undefined reference`，重则参数读错、栈被写坏。

本章把三套最常见的约定摊开对照：**Windows x64**、**System V**（Linux／macOS）
与 **ARM AAPCS**（Cortex-M）。同一份源码在它们手里会编出不同的寄存器分配，
这些差别在跨平台库、FFI、回调与手写汇编的场合必须知道。
名字修饰也在本章——它是 ABI 在符号表上的表现形式，
`extern "C"` 与 `LNK2019` 都从这里解释。

---

> **约定**：标注以行内代码单独成行——`实测数据` 表示实际执行验证过，
> 复现命令随程序给出；`文档` 表示引自标准或官方资料；
> `待确认` 表示尚未验证。路径占位符（如 `<MinGW>`、`<工作区>`）的含义见《README.md》。

# 本章节定位

| 需要先知道 | 在哪 |
|---|---|
| 函数的声明与定义、参数传递 | 《04-语法/07-函数.md》第 2 节 |
| 栈帧的构成（参数、返回地址、保存的寄存器） | 《06-更底层/01-对象在哪里：栈、堆与静态区.md》章节 |
| 符号与链接、未定义符号报错 | 《03-构建工具链/04-符号与调试信息.md》章节 |
| 结构体与联合体的布局 | 《04-语法/09-结构体、联合体与 enum.md》第 1 节 |
| 交叉编译工具链的用法 | 《01-编译器/03-嵌入式与交叉编译.md》第 1.3 小节 |
| 字节序与数据表示 | 《06-更底层/04-字节序与数据表示.md》章节 |

**相邻的章节**：`06-更底层/03-寄存器、位与 volatile`（《06-更底层/03-寄存器、位与 volatile.md》章节）
讲一个地址上的对象怎么访问，本章讲对象怎么在函数之间传递。
`06-更底层/06-符号与链接属性`（《06-更底层/06-符号与链接属性.md》章节）
接着第 5 节讲符号本身。

| 节 | 讲什么 |
|---|---|
| **第 1 节** | x86-64 的两套约定怎么对照；影子空间；谁保存哪些寄存器 |
| **第 2 节** | ARM AAPCS：`r0`–`r3` 传参、`r0` 返回、被调用者保存的寄存器、真板上的调用开销 |
| **第 3 节** | 结构体按值传递的真实过程与三套阈值 |
| **第 4 节** | 软浮点、软硬混合、硬浮点的差别与混用后果 |
| **第 5 节** | `_ZN5PointC1Eii` 怎么读，`extern "C"` 为什么存在 |
| **第 6 节** | 速查表 |

---

# 第 1 节 x86-64：两套约定

## 1.1 同一份源码，两份反汇编

**Windows 与 Linux 在 64 位 x86 上用的是两套不同的 ABI**，
参数寄存器的分配顺序完全不同。用同一个文件编两次就能看出来：

`C`

```c
/* abi_args.c   只生成汇编（两套 ABI 各一份，源码完全相同）：
                 Windows x64：gcc -std=c23 -O2 -S abi_args.c -o abi_args_win.s
                 System V   ：gcc -std=c2x -O2 -S abi_args.c -o abi_args_sysv.s
               noinline 是 GCC 的扩展属性，这里只是为了让「调用」真实发生，
               否则 -O2 会把 8 个参数折叠成一个常量。 */
#include <stdio.h>

__attribute__((noinline))
int sum8(int a, int b, int c, int d, int e, int f, int g, int h)
{
    return a + b + c + d + e + f + g + h;
}

volatile int g_base = 1;

int call_sum8(void)
{
    int b = g_base;                 /* 从内存取，防止被折叠成常量 */
    return sum8(b, b + 1, b + 2, b + 3, b + 4, b + 5, b + 6, b + 7);
}

int main(void)
{
    printf("%d\n", call_sum8());
    return 0;
}
```

**被调用者看到的参数**（下面是节选，省略了与参数无关的指令）：

`实测数据`
`Assembly`

```asm
Windows x64（sum8）                 System V（sum8）
	addl	%edx, %ecx                 	addl	%esi, %edi
	addl	%r8d, %ecx                 	addl	%edx, %edi
	leal	(%rcx,%r9), %eax           	addl	%ecx, %edi
	addl	40(%rsp), %eax             	addl	%r8d, %edi
	addl	48(%rsp), %eax             	leal	(%rdi,%r9), %eax
	addl	56(%rsp), %eax             	addl	8(%rsp), %eax
	addl	64(%rsp), %eax             	addl	16(%rsp), %eax
	ret                                	ret
```

**左边第 1 个参数在 `ecx`，右边在 `edi`**：两套约定给前四个参数分配的寄存器不同。

**调用者怎么放参数**（同样是节选）：

`实测数据`
`Assembly`

```asm
Windows x64（call_sum8）                          System V（call_sum8）
	subq	$72, %rsp                                movl	g_base(%rip), %edi
	movl	g_base(%rip), %ecx                       leal	7(%rdi), %eax
	leal	7(%rcx), %eax                            leal	3(%rdi), %ecx
	leal	1(%rcx), %edx                            pushq	%rax
	movl	%eax, 56(%rsp)                           leal	6(%rdi), %eax
	leal	6(%rcx), %eax                            leal	2(%rdi), %edx
	movl	%eax, 48(%rsp)                           pushq	%rax
	leal	5(%rcx), %eax                            leal	1(%rdi), %esi
	movl	%eax, 40(%rsp)                           leal	5(%rdi), %r9d
	leal	4(%rcx), %eax                            leal	4(%rdi), %r8d
	movl	%eax, 32(%rsp)                           call	sum8
	leal	3(%rcx), %r9d                            popq	%rdx
	leal	2(%rcx), %r8d                            popq	%rcx
	call	sum8                                     ret
	addq	$72, %rsp
	ret
```

**第 5 到第 8 个参数两边都走栈**，但位置不同：
Windows 把它们放在 `32(%rsp)` 往上（**下面留了 32 字节**），
System V 直接 `pushq` 两次（**没有留空**）。
**那 32 字节就是影子空间。**

`实测数据`

| 参数序号 | Windows x64 | System V |
|---|---|---|
| 1 | `rcx` | `rdi` |
| 2 | `rdx` | `rsi` |
| 3 | `r8` | `rdx` |
| 4 | `r9` | `rcx` |
| 5 | 栈（影子空间之上） | `r8` |
| 6 | 栈 | `r9` |
| 7 | 栈 | 栈 `8(%rsp)` |
| 8 | 栈 | 栈 `16(%rsp)` |
| 返回值 | `rax`（浮点 `xmm0`） | `rax`（浮点 `xmm0`） |
| 调用者必须留出的空间 | **32 字节影子空间** | 无 |
| 谁负责清理栈参数 | 调用者 | 调用者 |

**System V 给前六个整型参数各留了一个寄存器**，
第 7 个才开始走栈；**Windows 只给前四个**，第 5 个就上栈。

## 1.2 影子空间是什么

Windows x64 规定：**调用者必须在栈上留出 32 字节的「家」，
给被调用者存放那四个寄存器参数**（`rcx`、`rdx`、`r8`、`r9` 各 8 字节）。
**这段空间由调用者分配，被调用者可以随便用，也可以不用。**

`C`

```c
/* shadow.c   看 Windows x64 的影子空间：
               -O0 下 gcc -std=c23 -O0 -S shadow.c -o shadow_O0.s
               -O2 下 gcc -std=c23 -O2 -S shadow.c -o shadow_O2.s */
#include <stdio.h>

/* 取参数的地址：编译器必须把寄存器里的参数存进影子空间 */
int takes_addr(int a)
{
    int *p = &a;
    return *p + 1;
}

/* 再调用别的函数：调用者必须为自己留出 32 字节影子空间 */
int calls_other(int x)
{
    return takes_addr(x) + (int)printf("");
}

int main(void)
{
    printf("%d\n", calls_other(1));
    return 0;
}
```

`实测数据`
`Assembly`

```asm
takes_addr（-O0，节选）          calls_other（-O0，节选）
	pushq	%rbp                    	pushq	%rbp
	movq	%rsp, %rbp              	pushq	%rbx
	subq	$16, %rsp               	subq	$40, %rsp
	movl	%ecx, 16(%rbp)          	leaq	32(%rsp), %rbp
	leaq	16(%rbp), %rax          	movl	%ecx, 32(%rbp)
	movq	%rax, -8(%rbp)          	call	takes_addr
```

**`movl %ecx, 16(%rbp)` 就是「把第 1 个参数存进影子空间」**：
`%rbp` 之上 8 字节是保存的 `%rbp`，再上 8 字节是返回地址，
因此 `16(%rbp)` 正是调用者为第 1 个参数准备的那个位置。
**`subq $40, %rsp` 里的 40 是 32 字节影子空间加 8 字节对齐**。

**`-O2` 下这段会消失**：参数被留在寄存器里，`&a` 也没有真的取地址。

`实测数据`
`Assembly`

```asm
takes_addr（-O2）
	leal	1(%rcx), %eax
	ret
```

**影子空间只在「需要把参数写回内存」时才用得上**，
但**分配它的责任永远在调用者**——
这就是为什么 Windows x64 的函数序言里常看到 `subq $0x28, %rsp`（40 字节）。

> [!IMPORTANT]
> **影子空间是「留给被调用者的固定 32 字节」，不是「参数溢出区」。**
> 第 5 个及以后的参数放在它**上面**，位置不能与它重叠。
> 写汇编或写 JIT 时算错这 32 字节，会导致参数读到别的值。

## 1.3 被调用者保存的寄存器

**两套约定都把寄存器分成两组**：

| 组 | Windows x64 | System V | 谁负责保存 |
|---|---|---|---|
| 参数与临时 | `rax`、`rcx`、`rdx`、`r8`–`r11` | `rax`、`rcx`、`rdx`、`rsi`、`rdi`、`r8`–`r11` | **调用者**（被调用者可以随便用） |
| 长期存活 | `rbx`、`rbp`、`rdi`、`rsi`、`r12`–`r15` | `rbx`、`rbp`、`r12`–`r15` | **被调用者**（用了必须先存后恢复） |
| 栈指针 | `rsp` | `rsp` | 双方约定 |
| 浮点参数 | `xmm0`–`xmm3` | `xmm0`–`xmm7` | 调用者 |

**名字不同、规则相同**：调用者保存的寄存器可以当草稿纸，
被调用者保存的寄存器**动了就要恢复**。
`rdi`／`rsi` 在 Windows 上属于后者，在 System V 上前者——
**这是两套约定最容易被忽略的一处差别**。

> [!TIP]
> **自己写汇编函数时，只需要关心「被调用者保存」那一列。**
> 用到了就在开头 `push`、返回前 `pop`，
> 其余寄存器可以随意使用，因为调用者本来就不指望它们活着。

## 1.4 小结

> [!NOTE]
> **x86-64 上有两套主流约定，参数寄存器与谁保存哪些寄存器都不同。**
> Windows x64 用 `rcx`／`rdx`／`r8`／`r9` 加 32 字节影子空间；
> System V 用 `rdi`／`rsi`／`rdx`／`rcx`／`r8`／`r9`，没有影子空间。
> 同一份源码在两边编出来的机器码不同，**这不是编译器差异，是契约差异**。

---

# 第 2 节 ARM AAPCS：寄存器传参的另一种排法

## 2.1 前四个参数进 `r0`–`r3`

**AAPCS**（ARM 架构的过程调用标准）规定：
**前四个不超过一个字的参数放 `r0`、`r1`、`r2`、`r3`，其余入栈**；
返回值放 `r0`（64 位返回值放 `r0`／`r1`）。

同一份 `abi_args.c` 交叉编译到 Cortex-M3：

`Bash`

```bash
arm-none-eabi-gcc -mcpu=cortex-m3 -mthumb -O2 -c abi_args.c -o abi_args_arm.o
arm-none-eabi-objdump -d abi_args_arm.o
```

`实测数据`
`Assembly`

```asm
00000000 <sum8>:
   0:	4408      	add	r0, r1          r0 = a + b
   2:	4410      	add	r0, r2          + c
   4:	4418      	add	r0, r3          + d
   6:	9b00      	ldr	r3, [sp, #0]    第 5 个参数在栈顶
   8:	e9dd 1201 	ldrd	r1, r2, [sp, #4] 第 6、7 个：一次读两个字
   c:	4418      	add	r0, r3
   e:	4408      	add	r0, r1
  10:	9b03      	ldr	r3, [sp, #12]   第 8 个
  12:	4410      	add	r0, r2
  14:	4418      	add	r0, r3
  16:	4770      	bx	lr             返回：结果已经在 r0 里
```

**前四个参数在 `r0`–`r3` 里直接相加，一个字节的栈都没用**；
第 5 个参数起从 `[sp, #0]` 开始。
**`ldrd r1, r2, [sp, #4]` 一次读两个字**（64 位读），
两个参数一趟取回，这是 Thumb-2 才有的指令。

**调用者一侧同样能看出约定**：

`实测数据`
`Assembly`

```asm
00000018 <call_sum8>:
  18:	b510      	push	{r4, lr}         保存被调用者保存的寄存器与返回地址
  1a:	4b09      	ldr	r3, [pc, #36]
  1c:	b084      	sub	sp, #16           给四个栈参数腾地方
  1e:	6818      	ldr	r0, [r3, #0]      r0 = b        → 第 1 个参数
  20:	1dc4      	adds	r4, r0, #7        r4 = b+7      → 第 8 个（要入栈）
  22:	1d81      	adds	r1, r0, #6        r1 = b+6      → 第 7 个（要入栈）
  24:	1d42      	adds	r2, r0, #5        r2 = b+5      → 第 6 个（要入栈）
  26:	1d03      	adds	r3, r0, #4        r3 = b+4      → 第 5 个（要入栈）
  28:	e9cd 3200 	strd	r3, r2, [sp]      存 [sp]=b+4、[sp+4]=b+5
  2c:	e9cd 1402 	strd	r1, r4, [sp, #8]  存 [sp+8]=b+6、[sp+12]=b+7
  30:	1cc3      	adds	r3, r0, #3        r3 = b+3      → 第 4 个参数
  32:	1c82      	adds	r2, r0, #2        r2 = b+2      → 第 3 个参数
  34:	1c41      	adds	r1, r0, #1        r1 = b+1      → 第 2 个参数
  36:	f7ff fffe 	bl	0 <sum8>          调用：返回地址进 lr
  3a:	b004      	add	sp, #16           收回栈参数
  3c:	bd10      	pop	{r4, pc}          恢复 r4，返回地址进 pc
```

**`r3` 先装第 5 个参数（入栈），再回头装第 4 个参数。**
这不是编译器搞错了顺序，而是**寄存器不够用**：
要入栈的四个值必须先算出来、存下去，才能把 `r1`–`r3` 腾给真正属于它们的第 2 到第 4 个参数。
**读反汇编时不能按「寄存器出现的先后」去猜参数顺序，
要按最终的值落到哪里去看**——这里 `[sp]` 到 `[sp+12]` 依次是第 5 到第 8 个参数，
与被调用者那一边的 `[sp, #0]`、`[sp, #4]`、`[sp, #12]` 对得上。

**`pop {r4, pc}` 是 ARM 上最常见的返回写法**：
把返回地址直接弹进 `pc`（程序计数器），一条指令完成「恢复寄存器 + 返回」。
**`bl`（branch with link）把下一条指令的地址放进 `lr`（`r14`）**，
这就是返回地址的来源。

**这套约定在真板上要花多少周期**，可以用内核自带的周期计数器 `DWT_CYCCNT` 数出来：

`实测数据`

| 测什么（`-O2`，各做 1000 次取平均） | 真板 STM32F103C8 上的周期数 |
|---|---|
| 空函数调用（`noinline`） | **8** |
| 4 个参数（全走 `r0`–`r3`） | **11** |
| 6 个参数（后两个走栈） | **11** |
| 关中断再开（`cpsid i` 与 `cpsie i`） | **6** |

**上表是在 STM32F103C8（Cortex-M3、HSI 8 MHz，1 周期 = 125 ns）上实测的。**
**空函数调用也要 8 个周期**：`bl` 写返回地址、函数序言与 `bx lr` 各自都要占时间，
**在 MCU 上没有「零成本抽象」这回事**——把一个小操作包成函数，就按这个价格付。
**4 个参数与 6 个参数在这次测量里都是 11 个周期**：
多出来的两个参数由调用者存进栈、被调用者再读回来，指令数增加得有限，
摊到 1000 次平均之后落在同一个整数上。
**关中断再开只要 6 个周期**，临界区在 MCU 上比在通用处理器上便宜得多，
这条数字在《06-更底层/10-中断、并发与内存序.md》第 3.1 小节里还要用到。

## 2.2 被调用者保存的寄存器

**AAPCS 把 `r4`–`r11` 划为被调用者保存**（`r12`／`ip` 是临时寄存器，`r13`–`r15` 是 `sp`／`lr`／`pc`）。
**一个函数如果用到了 `r4`–`r11`，必须在入口保存、出口恢复。**

`C`

```c
/* callee_saved.c   只编译成目标文件：arm-none-eabi-gcc -mcpu=cortex-m3 -mthumb -O2 -c callee_saved.c -o callee_saved.o
                    （看被调用者保存的寄存器；再反汇编：arm-none-eabi-objdump -d callee_saved.o） */
extern int helper(int);

/* 累加器必须活过 helper 的调用，因此它会占一个「被调用者保存」的寄存器 */
int uses_callee_saved(int n)
{
    int acc = 0;
    for (int i = 0; i < n; ++i) {
        acc += helper(i);
    }
    return acc;
}
```

`实测数据`
`Assembly`

```asm
00000000 <uses_callee_saved>:
   0:	b570      	push	{r4, r5, r6, lr}   ← 入口：保存三个被调用者保存的寄存器
   2:	1e06      	subs	r6, r0, #0
   4:	dd0a      	ble.n	1c <...>
   6:	2400      	movs	r4, #0            r4 = i
   8:	4625      	mov	r5, r4            r5 = acc
   a:	4620      	mov	r0, r4
   c:	f7ff fffe 	bl	0 <helper>        调用：helper 可以随便用 r0-r3、r12
  10:	3401      	adds	r4, #1
  12:	42a6      	cmp	r6, r4
  14:	4405      	add	r5, r0            acc 累加：r5 活过了上面的调用
  16:	d1f8      	bne.n	a <...>
  18:	4628      	mov	r0, r5
  1a:	bd70      	pop	{r4, r5, r6, pc}  ← 出口：恢复并返回
```

**`r5` 里的 `acc` 与 `r4` 里的 `i` 都活过了 `bl helper`**。
它们能活着，靠的不是 `helper` 的善意，而是**这条约定**：
`helper` 要用 `r4`／`r5` 就必须自己先存后恢复。
**`push`／`pop` 成对出现，是这类函数的标准形状。**

> [!IMPORTANT]
> **「调用者保存」与「被调用者保存」是一份分工协议，
> 不是硬件强制。** 硬件不会阻止 `helper` 破坏 `r5`，
> 破坏之后出错的地方会离现场很远。
> 自己写汇编或写中断服务函数时，这条协议必须自己遵守。

## 2.3 小结

> [!NOTE]
> **ARM 的约定比 x86-64 简单**：前四个字参数进 `r0`–`r3`，其余入栈，
> 返回值在 `r0`，`r4`–`r11` 由被调用者保存。
> `bl` 负责把返回地址放进 `lr`，`pop {..., pc}` 负责返回。
> 大结构体（超过 4 字节）改成传地址，见第 3 节。

---

# 第 3 节 结构体按值传递：三套阈值

## 3.1 把调用者与被调用者分开

**结构体按值传递是 ABI 里最复杂的一条**，因为「一个结构体」在寄存器里放不下。
**最容易看清它的场合是两个翻译单元之间**——同一个文件里，
编译器会把结构体拆成字段直接优化掉（本机 `-O2` 下 `take_s8` 只剩一条 `bx lr`），
反而看不到约定。

`C`

```c
/* abi_struct_callee.c   只编译成目标文件，gcc -std=c23 -O2 -c abi_struct_callee.c -o callee.o
                         （不是完整程序：ABI 体现在两个翻译单元之间）
                         交叉编译：arm-none-eabi-gcc -mcpu=cortex-m3 -mthumb -O2 \
                           -c abi_struct_callee.c -o callee_arm.o */
typedef struct { char c[3]; } S3;
typedef struct { int  a;    } S4;
typedef struct { int  a, b; } S8;
typedef struct { int  a, b, c; } S12;
typedef struct { int  a, b, c, d; } S16;
typedef struct { int  a[6]; } S24;

/* 每个函数只负责把按值传进来的结构体读出来用 */
int take_s3(S3 s)   { return s.c[0]; }
int take_s4(S4 s)   { return s.a; }
int take_s8(S8 s)   { return s.a + s.b; }
int take_s12(S12 s) { return s.a + s.b + s.c; }
int take_s16(S16 s) { return s.a + s.b + s.c + s.d; }
int take_s24(S24 s) { return s.a[0] + s.a[5]; }
```

`C`

```c
/* abi_struct_caller.c   只编译成目标文件，gcc -std=c23 -O2 -c abi_struct_caller.c -o caller.o
                         （与 callee 是两个翻译单元）
                         交叉编译：arm-none-eabi-gcc -mcpu=cortex-m3 -mthumb -O2 \
                           -c abi_struct_caller.c -o caller_arm.o */
typedef struct { char c[3]; } S3;
typedef struct { int  a;    } S4;
typedef struct { int  a, b; } S8;
typedef struct { int  a, b, c; } S12;
typedef struct { int  a, b, c, d; } S16;
typedef struct { int  a[6]; } S24;

int take_s3(S3);
int take_s4(S4);
int take_s8(S8);
int take_s12(S12);
int take_s16(S16);
int take_s24(S24);

volatile int g[8] = { 1, 2, 3, 4, 5, 6, 7, 8 };

/* 调用处是「把值放进寄存器」还是「把地址放进寄存器」，由 ABI 的大小阈值决定 */
int call_s3(void)  { S3  s; s.c[0] = (char)g[0]; return take_s3(s); }
int call_s4(void)  { S4  s; s.a = g[0]; return take_s4(s); }
int call_s8(void)  { S8  s; s.a = g[0]; s.b = g[1]; return take_s8(s); }
int call_s12(void) { S12 s; s.a = g[0]; s.b = g[1]; s.c = g[2]; return take_s12(s); }
int call_s16(void) { S16 s; s.a = g[0]; s.b = g[1]; s.c = g[2]; s.d = g[3]; return take_s16(s); }
int call_s24(void) { S24 s; for (int i = 0; i < 6; ++i) { s.a[i] = g[i]; } return take_s24(s); }
```

**被调用者的第一条指令就能看出「拿到的是值还是地址」**：

`实测数据`
`Assembly`

```asm
Windows x64                          System V
take_s3：  movsbl (%rcx),%eax        take_s3：  movsbl %dil,%eax
take_s4：  mov    %ecx,%eax          take_s4：  mov    %edi,%eax
take_s8：  mov    %rcx,%rax          take_s8：  mov    %rdi,%rax
           shr    $0x20,%rax                    shr    $0x20,%rax
take_s12： mov    0x4(%rcx),%eax     take_s12： add    %esi,%eax
take_s16： mov    (%rcx),%rax        take_s16： add    %esi,%eax
           mov    0x8(%rcx),%rdx                 sar    $0x20,%rsi
take_s24： mov    (%rcx),%eax        take_s24： mov    0x1c(%rsp),%eax
```

**括号里带 `(%rcx)` 说明 `rcx` 是地址**（结构体被复制到某个地方，只传了地址）；
**直接用寄存器名说明拿到的是值**。
`take_s12` 分成两种写法，正是两套约定的分界：
Windows 传地址，System V 把它拆成两个八字节当成两个整数参数。

**调用处的证据更直白**——传地址时要先 `lea`：

`实测数据`
`Assembly`

```asm
Windows x64                          System V
call_s12：sub  $0x48,%rsp            call_s24：sub  $0x28,%rsp
          lea  0x20(%rsp),%rcx                 mov  %rsp,%rcx
call_s24：sub  $0x68,%rsp                      mov  (%rsi,%rdx,4),%edx
          lea  0x40(%rsp),%rdx
```

**`lea 0x20(%rsp), %rcx` 就是「在栈上开一块地方，把结构体拷进去，把地址传过去」**。
`Windows` 从 12 字节起就这么做，`System V` 要到超过 16 字节才开始。

**ARM 的规则又不一样：超过 4 字节就传地址。**

`实测数据`
`Assembly`

```asm
call_s4：  ldr    r0, [r3, #0]          3 字节与 4 字节：值直接进 r0
call_s8：  sub    sp, #8                8 字节：先在栈上开 8 字节
           strd   r2, r3, [sp]          把两个字存进去
           add    r3, sp, #8            r3 = 那块地方的地址
call_s12： sub    sp, #16               12 字节：同样先复制到栈上
           strd   r1, r2, [sp, #4]
           str    r3, [sp, #12]
```

`实测数据`

| 结构体大小 | Windows x64 | System V | ARM AAPCS |
|---|---|---|---|
| 3 字节 | 传地址 | 传值（`dil`） | 传值（`r0`） |
| 4 字节 | 传值（`ecx`） | 传值（`edi`） | 传值（`r0`） |
| 8 字节 | 传值（`rcx`） | 传值（`rdi`） | **传地址** |
| 12 字节 | **传地址** | 传值（`rdi` + `esi`） | 传地址 |
| 16 字节 | 传地址 | 传值（`rdi` + `rsi`） | 传地址 |
| 24 字节 | 传地址 | **内存**（栈上） | 传地址 |

**三套规则的小结**：

| ABI | 规则 |
|---|---|
| Windows x64 | 1／2／4／8 字节按整数传，**其他大小一律传隐藏指针** |
| System V | 按「八字节」分类，**总共不超过 16 字节就用寄存器，超过就整体进内存** |
| ARM AAPCS | **不超过 4 字节传值，超过就传指向副本的指针** |

## 3.2 为什么大结构体改成传地址

**按值传递的语义要求被调用者拿到一份自己的副本**，
因此无论 ABI 选择哪种方式，**拷贝都会发生**：
小的直接放进寄存器（不占栈），大的就必须在栈上或内存里复制一份，
再把地址传过去。

**这带来一个反直觉的结论**：**结构体按值传参并不比传指针「更高效」，
它只是语义更简单**。一份 24 字节的结构体按值传，System V 会在栈上复制 24 字节；
传 `const S24 *` 则只传一个地址。
**C++ 里的 `const T &` 参数之所以常见，根源就在这里**。

> [!WARNING]
> **两个翻译单元里的结构体定义必须完全一致。**
> 若一个文件里是 `struct P { int x, y; }`，另一个文件里多了一个字段，
> **C 语言不会报错、链接器也看不出来**——
> 一方按 8 字节传值，另一方按指针读，结果就是读到垃圾。
> **C++ 的名字修饰会把类型编进符号名，多数情况下能在链接期挡住这类错误**（第 5 节），
> 这也是混编时优先用 C++ 编译接口的一个理由。

## 3.3 小结

> [!NOTE]
> **结构体按值的传递方式由大小阈值决定，三套 ABI 的阈值不同**：
> Windows x64 只认 1／2／4／8 字节，System V 认到 16 字节，
> ARM AAPCS 只认 4 字节。
> 超过阈值时**语义不变、实现变成「复制一份再传地址」**，
> 因此大对象按值传递的代价是真实的拷贝。

---

# 第 4 节 软浮点与硬浮点：EABI 的三个选项

## 4.1 三个选项与三份反汇编

**Cortex-M3 没有浮点单元，Cortex-M4F 有。** 编译器因此提供三种浮点约定：

| 选项 | 参数放哪 | 运算用什么 | 适用 |
|---|---|---|---|
| `-mfloat-abi=soft` | 整数寄存器 `r0`–`r3` | 调库函数（`__aeabi_fadd`） | 没有 FPU |
| `-mfloat-abi=softfp` | 整数寄存器 `r0`–`r3` | FPU 指令 | 有 FPU，但要与软浮点代码兼容 |
| `-mfloat-abi=hard` | **浮点寄存器 `s0`–`s15`** | FPU 指令 | 全工程都有 FPU |

`C`

```c
/* float_abi.c   三种浮点 ABI 各编一次，比较指令：
                  arm-none-eabi-gcc -mcpu=cortex-m4 -mfpu=fpv4-sp-d16 -mfloat-abi=soft \
                    -O2 -c float_abi.c -o f_soft.o
                  arm-none-eabi-gcc -mcpu=cortex-m4 -mfpu=fpv4-sp-d16 -mfloat-abi=softfp \
                    -O2 -c float_abi.c -o f_softfp.o
                  arm-none-eabi-gcc -mcpu=cortex-m4 -mfpu=fpv4-sp-d16 -mfloat-abi=hard \
                    -O2 -c float_abi.c -o f_hard.o
                  arm-none-eabi-objdump -d f_hard.o
                  arm-none-eabi-readelf -A f_hard.o */

float fadd(float a, float b)      { return a + b; }
float fmul(float a, float b)      { return a * b; }
double dadd(double a, double b)   { return a + b; }
```

`实测数据`
`Assembly`

```asm
--- soft（-mfloat-abi=soft）
00000000 <fadd>:
   0:	b508      	push	{r3, lr}
   2:	f7ff fffe 	bl	0 <__aeabi_fadd>    参数在 r0/r1，调用库函数
   6:	bd08      	pop	{r3, pc}

--- softfp（-mfloat-abi=softfp）
00000000 <fadd>:
   0:	ee07 0a10 	vmov	s14, r0             从整数寄存器搬到浮点寄存器
   4:	ee07 1a90 	vmov	s15, r1
   8:	ee77 7a27 	vadd.f32	s15, s14, s15   运算是硬件指令
   c:	ee17 0a90 	vmov	r0, s15             结果搬回整数寄存器
  10:	4770      	bx	lr

--- hard（-mfloat-abi=hard）
00000000 <fadd>:
   0:	ee30 0a20 	vadd.f32	s0, s0, s1         参数与结果都在浮点寄存器
   4:	4770      	bx	lr
```

**三份代码的接口完全不同**：
`soft` 的参数在 `r0`／`r1`，要调一个库函数；
`softfp` 的参数仍在 `r0`／`r1`，但多了四次寄存器搬运；
`hard` 的参数直接在 `s0`／`s1`，一条指令做完。

**这块真板（STM32F103C8）是 Cortex-M3，没有浮点单元**，
因此它上面的浮点代码只能走 `soft` 那一栏：参数进 `r0`／`r1`，一次加法要 `bl __aeabi_fadd`。
**选型时这条差别比主频更值得看**：同一段浮点算法，M3 上是库函数调用，M4F 上是一条 `vadd.f32`。

**对象文件里记着用哪种约定**：

`Bash`

```bash
arm-none-eabi-readelf -A f_hard.o
```

`实测数据`
`Text`

```text
--- soft
  Tag_CPU_arch: v7E-M

--- softfp
  Tag_CPU_arch: v7E-M
  Tag_FP_arch: VFPv4-D16

--- hard
  Tag_CPU_arch: v7E-M
  Tag_FP_arch: VFPv4-D16
  Tag_ABI_VFP_args: VFP registers
```

**`Tag_ABI_VFP_args: VFP registers` 这一行就是「我按硬浮点约定传参」的声明**，
链接器会拿它去比对（下一小节）。
`softfp` 与 `soft` 都没有这一行，因此**它们的接口是兼容的**——
`softfp` 只是把运算换成了 FPU 指令。

## 4.2 混用会怎样：链接期直接报错

**这是链接器少数会主动检查 ABI 属性的地方。**
把调用方与被调用方编成不同的浮点约定：

`实测数据`
`Text`

```text
--- 两个都是 hard：链接成功
--- 调用方 hard + 被调用方 softfp：
arm-none-eabi-ld: error: bad.o uses VFP register arguments, f_softfp.o does not
arm-none-eabi-ld: failed to merge target specific data of file f_softfp.o

--- 调用方 softfp + 被调用方 hard：
arm-none-eabi-ld: error: f_hard.o uses VFP register arguments, bad2.o does not
arm-none-eabi-ld: failed to merge target specific data of file f_hard.o
```

**错误信息说的就是「一边用 VFP 寄存器传参，另一边不用」**，
与 `Tag_ABI_VFP_args` 完全对应。
**这类错误好在发生在链接期，而不是运行期**——
浮点参数读错的表现往往是「结果莫名其妙」，极难定位。

> [!CAUTION]
> **`-mfloat-abi` 对整个工程必须统一，包括第三方库。**
> 只编译过软浮点的静态库，在硬浮点工程里链接会直接失败；
> 若第三方库是二进制形式且没有这个属性，就要标 `待确认` 去问作者。
> **这一条也是构建配置里最容易被忽略的一项**：它不在源码里，只在编译选项里。

## 4.3 小结

> [!NOTE]
> **软浮点与硬浮点的差别不只是「快慢」，而是「参数走哪个寄存器」**，
> 因此它是一个接口约定，不是优化选项。
> 对象文件用 `Tag_ABI_VFP_args` 记录这个约定，
> 链接器会拒绝混用——**这是 ABI 检查发挥作用的一个正面例子**。

---

# 第 5 节 名字修饰：ABI 在符号表上的样子

## 5.1 `_ZN5PointC1Eii` 逐个字段拆开

**C 的函数名就是符号名**，一个 `add` 只能有一个。
**C++ 允许重载、命名空间、类成员、模板**，
因此编译器必须把「是哪个 `add`」编进符号里，这个过程叫**名字修饰**（name mangling）。

一份真实的符号清单：

`C++`

```cpp
// mangling.cpp   只编译成目标文件，再看符号：g++ -std=c++17 -c mangling.cpp -o mangling.o
#include <cstddef>

struct Point {
    int x, y;
    Point(int a, int b);            // 构造函数
    int sum() const;                // 成员函数
};

Point::Point(int a, int b) : x(a), y(b) { }
int Point::sum() const { return x + y; }

int add(int a, int b) { return a + b; }          // 自由函数
double add(double a, double b) { return a + b; } // 重载
void take_ptr(Point *p) { (void)p; }             // 指针参数
void take_ref(Point &p) { (void)p; }             // 引用参数
void take_const_ref(const Point &p) { (void)p; } // const 引用

namespace ns {
    int value = 0;
    void func() { }
}

template <typename T>
T twice(T v) { return v + v; }

template int    twice<int>(int);
template double twice<double>(double);

extern "C" int c_style(int x) { return x + 1; }  // C 链接：不加修饰
```

`Bash`

```bash
arm-none-eabi-g++ -std=c++17 -mcpu=cortex-m3 -mthumb -O2 -c mangling.cpp -o mangling_arm.o
arm-none-eabi-nm mangling_arm.o
arm-none-eabi-nm mangling_arm.o | awk '{print $3}' | arm-none-eabi-c++filt
```

`实测数据`
`Text`

```text
00000000 T _ZN5PointC1Eii          → Point::Point(int, int)
00000000 T _ZN5PointC2Eii          → Point::Point(int, int)
00000008 T _ZNK5Point3sumEv        → Point::sum() const
00000010 T _Z3addii                → add(int, int)
00000014 T _Z3adddd                → add(double, double)
0000001c T _Z8take_ptrP5Point      → take_ptr(Point*)
00000020 T _Z8take_refR5Point      → take_ref(Point&)
00000024 T _Z14take_const_refRK5Point → take_const_ref(Point const&)
00000028 T _ZN2ns4funcEv           → ns::func()
00000000 B _ZN2ns5valueE           → ns::value
00000000 W _Z5twiceIiET_S0_        → int twice<int>(int)
00000000 W _Z5twiceIdET_S0_        → double twice<double>(double)
0000002c T c_style                 → c_style（extern "C"，不加修饰）
```

**`_ZN5PointC1Eii` 的每个字段**：

`Text`

```text
_Z      N     5Point   C1     E     i     i
│       │     │        │      │     │     └─ 第二个参数：int
│       │     │        │      │     └─────── 第一个参数：int
│       │     │        │      └───────────── 嵌套名结束（N 与 E 成对）
│       │     │        └──────────────────── 构造函数（C1 = 完整对象构造函数）
│       │     └───────────────────────────── 名字长度 5，内容是 Point
│       └─────────────────────────────────── 接下来是一个嵌套名（N）
└─────────────────────────────────────────── 这是 Itanium C++ ABI 的修饰名
```

**几个反复出现的编码**：

`实测数据`

| 片段 | 含义 | 例子 |
|---|---|---|
| `_Z` | 修饰名的开头 | 所有名字 |
| `N` … `E` | 嵌套名（类、命名空间） | `_ZN2ns4funcEv` |
| 数字 + 名字 | 长度 + 标识符本身 | `5Point`、`3sum`、`14take_const_ref` |
| `i`／`d`／`v`／`b` | `int`／`double`／`void`／`bool` | `_Z3addii` |
| `P` 前缀 | 指针 | `P5Point` = `Point *` |
| `R` 前缀 | 引用 | `R5Point` = `Point &` |
| `RK` | const 引用 | `RK5Point` = `Point const &` |
| `K`（在名字后） | const 成员函数 | `_ZNK5Point3sumEv` |
| `C1`／`C2`／`D1` | 构造函数（完整／基）／析构函数 | `_ZN5PointC1Eii` |
| `I` … `E` | 模板实参表 | `_Z5twiceIiE...` |

**为什么构造函数有两个**（`C1` 与 `C2`）：C++ 的构造函数在
「构造一个完整对象」与「构造一个基类子对象」两种场合语义略有差别，
Itanium ABI 因此给了两个符号，多数情况下它们指向同一段代码。
**这是 ABI 层可见、源码层看不见的一处细节。**

**`_ZNK5Point3sumEv` 比 `_ZN5PointC1Eii` 多了一个 `K`**：
它表示这是 `const` 成员函数。
**`const` 会影响符号名**，因此「在头文件里给成员函数加了个 `const`」
会让所有调用点重新链接——这是改接口时的一个实际后果。

## 5.2 C 与 C++ 的差别，以及 `extern "C"` 为什么存在

**C 的符号就是名字本身**，`int c_func(int)` 在符号表里叫 `c_func`；
**C++ 编译同一个声明会得到 `_Z6c_funci`**。
两者对不上，链接就失败。

**标准把这件事叫「语言链接」（language linkage），并且明说它包含哪些内容**：

`文档`

> "All function types, function names with external linkage, and variable names with external
> linkage have a language linkage. [Note: Some of the properties associated with an entity with
> language linkage are specific to each implementation and are not described here. For example,
> a particular language linkage may be associated with a particular form of representing names
> of objects and functions with external linkage, or with a particular calling convention,
> etc.—end note] The default language linkage of all function types, function names, and
> variable names is C++ language linkage. Two function types with different language linkages
> are distinct types even if they are otherwise identical."
>
> —— N4659 §10.5/1

**「a particular form of representing names」就是名字修饰，
「a particular calling convention」就是调用约定**——
标准把这两件事都留给实现，因此它们由 ABI 规定，而不是由语言规定。
**这也解释了为什么同一个 C++ 程序在 GCC 与 MSVC 下符号名完全不同而两家都合规。**

`extern "C"` 的作用是「**这个声明按 C 的规则修饰**」——
它只影响符号名与调用约定，不影响函数体的写法：

`实测数据`
`Text`

```text
不加修饰的 C 函数：      c_func
C++ 编译器生成的名字：   _Z6c_funci
extern "C" 之后：        c_style
```

**系统头文件里到处是这对大括号**，原因就在这里：

`文档`

> "Every implementation shall provide for linkage to functions written in the C programming
> language, \"C\", and linkage to C++ functions, \"C++\"."
>
> —— N4659 §10.5/3

**标准要求的只有这一句**：必须提供通往 C 函数的链接。
**至于 `extern "C"` 的名字长什么样、参数走哪个寄存器，标准不管**，
由各家的 ABI 补齐——这正是第 5.4 小节两套修饰体系的来源。

`C`

```c
/* gpio.h（A-教学素材，原文节选） */
#ifdef __cplusplus
extern "C" {
#endif

void MX_GPIO_Init(void);

#ifdef __cplusplus
}
#endif
```

**`__cplusplus` 只在 C++ 编译器里有定义**，
因此这段头文件被 C 编译器读到时，那两行 `extern "C"` 根本不存在——
**同一份头文件同时服务两种语言**。

## 5.3 写错了会怎样：链接期的真实报错

**在 C++ 里调用 C 函数而忘了 `extern "C"`**，两边都在，但符号名对不上。

`C`

```c
/* clib.c   只编译成目标文件：gcc -std=c23 -c clib.c -o clib.o */
int c_func(int x)
{
    return x + 1;
}
```

`C++`

```cpp
// cpp_main.cpp   编译并链接（失败）：g++ -std=c++17 cpp_main.cpp clib.o -o cpp_main （失败）
//                修法：把声明改成 extern "C" int c_func(int);
int c_func(int);            // C++ 编译器按 C++ 规则修饰这个名字

int main()
{
    return c_func(1);
}
```

**GCC 的报错原文**：

`实测数据`
`Text`

```text
ld.exe: cpp_main.o:(.text+0x13): undefined reference to `c_func(int)'
collect2.exe: error: ld returned 1 exit status
```

**报错里写的是 `c_func(int)`，那是反解之后的样子**。
目标文件里真正找的符号是：

`实测数据`
`Text`

```text
nm cpp_main.o  →                 U _Z6c_funci
nm clib.o      → 0000000000000000 T c_func
```

**一个要 `_Z6c_funci`，一个只提供 `c_func`**，链接器只能报错。

**MSVC 的报错更啰嗦，但把两个名字都摆出来了**：

`实测数据`
`Text`

```text
msmain.obj : error LNK2019: 无法解析的外部符号 "int __cdecl c_func(int)"
             (?c_func@@YAHH@Z)，函数 main 中引用了该符号
  已定义且可能匹配的符号上的提示:
    c_func
ms.exe : fatal error LNK1120: 1 个无法解析的外部命令
```

**`LNK2019` 就是 MSVC 的「未解析外部符号」**，
`?c_func@@YAHH@Z` 是它自己的修饰名（下一小节）。
**最后那行提示 `c_func` 是链接器在帮着找近似符号**，
它已经指出了问题：两边差的就是修饰。

> [!TIP]
> **看到 `LNK2019` 或 `undefined reference` 时，先用 `nm` 把两边的符号名都打出来。**
> 「差一个 `extern "C"`」「差一个 `const`」「差一个命名空间」
> 都在符号名上一眼可见，比读源码快得多。

## 5.4 两套修饰体系

**C++ 没有规定用哪种修饰方案**，因此不同工具链各有一套。
同一份 C++ 源码在 GCC 与 MSVC 下编出来的名字完全不同：

`实测数据`

| 源码 | GCC／Clang（Itanium ABI） | MSVC |
|---|---|---|
| `int add(int, int)` | `_Z3addii` | `?add@@YAHHH@Z` |
| `double add(double, double)` | `_Z3adddd` | `?add@@YANNN@Z` |
| `void take_ptr(Point *)` | `_Z8take_ptrP5Point` | `?take_ptr@@YAXPEAUPoint@@@Z` |
| `ns::func()` | `_ZN2ns4funcEv` | `?func@ns@@YAXXZ` |
| `extern "C" int c_style(int)` | `c_style` | `c_style` |
| 未加 `extern "C"` 的 C 函数调用 | `_Z6c_funci` | `?c_func@@YAHH@Z` |

**两边唯一的共同点是 `extern "C"` 的名字**——
这正好说明 `extern "C"` 的用处：**它是跨编译器、跨语言的公共符号格式**。
**MSVC 与 GCC 编译的 C++ 目标文件不能直接互相链接**，
要跨工具链就必须把接口收窄成 C 风格。

> [!IMPORTANT]
> **名字修饰是 ABI 的一部分，不是编译器的「内部实现」。**
> 它决定了库的二进制兼容性：给成员函数加一个 `const`、
> 给命名空间改个名字、把参数类型从 `int` 改成 `long`，
> 都会让符号名变化，**旧的目标文件就链接不上了**。

## 5.5 小结

> [!NOTE]
> **名字修饰把「类型信息」编进符号名**，因此链接器能在多数情况下挡住类型不匹配；
> 代价是名字不可读、跨编译器不兼容。
> **`extern "C"` 是唯一的公共地带**，系统头文件用它同时服务 C 与 C++。
> 出现 `LNK2019`／`undefined reference` 时，先 `nm` 两边对照符号名。

---

# 第 6 节 速查表

`实测数据`

| 常用件 | 一句话用途 | 典型坑 |
|---|---|---|
| `gcc -S` 同一份源码编两次 | 对照两套 ABI 的寄存器分配 | 只在自己平台上编过，以为 ABI 是唯一的 |
| 影子空间（Windows x64，32 字节） | 给被调用者存四个寄存器参数 | 忘了留：栈参数与它重叠，参数读错 |
| `rdi`／`rsi` | System V 的前两个参数；Windows 上是被调用者保存 | 按 Windows 的习惯认为它们可以随便用 |
| `r0`–`r3`、`r0` 返回 | ARM AAPCS 的传参与返回 | 第 5 个参数起在栈上，位置要算对 |
| 真板上一次空调用 | **8 周期**（Cortex-M3、HSI 8 MHz） | 把函数调用当成免费的：循环里每个小操作都包一层函数 |
| 4 参数与 6 参数的调用 | 都是 **11 周期** | 以为多两个栈参数会明显变慢：这次测量里看不出差别 |
| `r4`–`r11`（ARM）被调用者保存 | 用了要 `push`／`pop` | 中断服务函数里破坏它们：主程序出错 |
| `pop {r4, pc}` | ARM 上一条指令完成恢复与返回 | 忘了恢复，返回地址错 |
| `bl` / `bx lr` | 调用与返回 | 在 `bl` 与返回之间改动 `lr` 会丢返回地址 |
| 结构体按值传参 | 超过阈值自动变成传地址 | 以为「按值」等于「高效」，实际发生拷贝 |
| Windows x64 结构体阈值 | 只认 1／2／4／8 字节 | 3 字节结构体也走指针（本机反汇编所见） |
| System V 结构体阈值 | 16 字节以内拆成八字节用寄存器 | 超过 16 字节改走栈，位置从 `8(%rsp)` 起 |
| ARM 结构体阈值 | **超过 4 字节就传地址** | 8 字节结构体的参数是一个地址，不是两个寄存器 |
| `-mfloat-abi=soft/softfp/hard` | 三种浮点约定 | 参数寄存器不同；混用链接直接失败 |
| `readelf -A` | 看对象的 ABI 属性 | 忽略 `Tag_ABI_VFP_args` 就查不出混用 |
| `nm` + `c++filt` | 看修饰名与反解 | 只看反解后的名字，看不到链接器找的是哪个符号 |
| `extern "C"` | 让 C++ 按 C 的规则修饰名字 | 只加在定义处、忘了头文件：还是对不上 |
| `__cplusplus` 守卫 | 一份头文件同时服务 C 与 C++ | 守卫写反，C 编译器看到 `extern "C"` 就报错 |
| `LNK2019` / `undefined reference` | 未解析外部符号 | 只改源码不 `nm`：找不到「差在哪个修饰上」 |
| Itanium 与 MSVC 修饰名 | 两套体系 | 跨工具链直接链 C++ 目标文件：名字对不上 |

---

# 附录 A 相关文档

| 文档 | 关系 |
|---|---|
| 《04-语法/07-函数.md》第 2 节 | **前置**：声明与定义、参数传递 |
| 《04-语法/09-结构体、联合体与 enum.md》第 1 节 | **前置**：结构体的布局与大小 |
| 《03-构建工具链/04-符号与调试信息.md》章节 | **前置**：符号表、`nm`、未定义符号 |
| 《01-编译器/03-嵌入式与交叉编译.md》第 1.3 小节 | **前置**：交叉编译工具链 |
| 《06-更底层/04-字节序与数据表示.md》章节 | **前置**：同一份数据的字节表示 |
| 《06-更底层/03-寄存器、位与 volatile.md》章节 | **相邻**：寄存器与内存映射 I/O |
| 《06-更底层/README.md》章节 | 板块入口：两类读者两条线、本板块的阅读顺序 |
| 《06-更底层/01-对象在哪里：栈、堆与静态区.md》章节 | **前置**：栈帧的构成 |
| 《06-更底层/06-符号与链接属性.md》章节 | **后续**：符号的链接属性、弱符号、`extern "C"` 混编 |
| 《06-更底层/09-C++ 对象布局与它的硬件代价.md》章节 | **后续**：`this` 指针怎么传、虚函数的调用约定 |
| 《06-更底层/12-编译器扩展与未定义行为.md》章节 | **后续**：`__attribute__`、`__declspec` 与调用约定扩展 |

**本章用到的配套件**：

| 用途 | 位置 |
|---|---|
| `extern "C"` 与静态库的混编示例 | [B-examples/06-lower-level/04-symbols-and-linking/](../B-examples/06-lower-level/04-symbols-and-linking/) |
| 动手练习：静态库与调用约定 | [C-templates/06-lower-level/04-static-lib/](../C-templates/06-lower-level/04-static-lib/) |
| 真实链接脚本与启动文件（交叉编译用） | [`A-教学素材/01-编译器/嵌入式/链接脚本与启动`](../A-教学素材/01-编译器/嵌入式/链接脚本与启动) |