# 常量与 const

> **授权**：本章节属教材文档部分，采用 [CC BY-NC-ND 4.0](../LICENSE)
> 加[附加条款](../许可附加条款.md)授权：署名、非商业、禁止演绎、不允许二次分发。
> 文中引用的代码不受此限，各自保留原有许可证，详见[版权与许可说明.md](../版权与许可说明.md)。

**程序里的数据分两类：会变的，和不该变的。**

**不该变的东西如果只写在注释里，改了也没有人拦。**
`const` 把这件事写进类型，交给编译器执行：
**不通过这个名字修改它。**

**它同时改善两件事。**

| 改善 | 说明 |
|---|---|
| **防错** | 自己几个月后改错、别人接手时改错，编译器直接报错，而不是留下一个难查的 bug |
| **接口** | `void print(const char *s)` 这一行声明里就写明了「`s` 指向的内容不会被改动」，读代码的人不必翻实现 |

**代价是零**：`const` 只在编译期起作用，生成的机器码里不留痕迹。

**但围绕它的误解不少**：
「`const` 就是放进只读内存」、
「`const` 就是编译期常量」、
「`const int *p` 与 `int * const p` 差不多」。
**按这些说法写代码，会在需要保护的地方保护不了，在需要改值的地方改不动。**

---

> **约定**：标注以行内代码单独成行——`实测数据` 表示实际执行验证过，
> 完整数据与复现命令见附录 A；`文档` 表示引自标准或官方资料；
> `待确认` 表示尚未验证。路径占位符的含义见《README.md》。

## 本章节定位

| 需要先知道 | 在哪 |
|---|---|
| 类型与类型限定符 | 《04-语法/02-数据类型与类型系统.md》第 3 节 |
| 左值与右值 | 《04-语法/04-表达式与运算符.md》第 2.2 小节 |
| 数组名在表达式里会退化 | 《04-语法/08-数组、指针与引用.md》第 2.2 小节 |
| 声明的读法 | 《04-语法/08-数组、指针与引用.md》第 4.3 小节 |
| 作用域、生存期与链接 | 《04-语法/11-作用域、生存期与链接.md》第 1 节 |

**相邻的章节**：`02-数据类型与类型系统`（限定符的全家）、
`11-作用域、生存期与链接`（`const` 全局变量的链接属性）、
`12-编译期能力`（C++ 的 `constexpr` 与真正的编译期常量）。

**本章节的实测环境**：Windows 侧 `gcc` / `g++` 15.2.0（MinGW-w64），
Linux 侧 `gcc` / `g++` 13.3.0。两边的结论一致，措辞差异见附录 A.1。

---

# 第 1 节 常量要解决什么问题

## 1.1 「说好了不改」需要机制来保证

**下面两个名字表达的是同一件事，约束力却完全不同。**

`C`

```c
/* two_kinds.c    编译：gcc -std=c23 two_kinds.c -o two_kinds */
#include <stdio.h>

#define MAX_USERS_MACRO 100     /* 宏：预处理阶段文本替换，没有类型 */
static const int max_users = 100;   /* const 对象：有类型，编译器保证不通过这个名字修改 */
static int limit = 100;             /* 普通对象：谁都能改，注释说了不算 */

int main(void) {
    limit = 200;            /* 编译器不管：注释里写的「不要改」它看不见 */
    printf("limit = %d\n", limit);
    /* max_users = 200;     这一行会编译失败：assignment of read-only variable 'max_users' */
    return 0;
}
```

**`static` 在这里只影响链接属性**（见《04-语法/11-作用域、生存期与链接.md》第 4 节），
与 `const` 无关：把 `static` 去掉，`max_users = 200;` 一样会失败。

**三种「起名字」的手段，能力并不相同。**

| 手段 | 有类型 | 遵守作用域 | 能取地址 | 能当数组长度 |
|---|---|---|---|---|
| 宏 `#define` | **没有**，纯文本替换 | 从定义处到文件尾 | 不能 | 能（见《04-语法/14-预处理器.md》第 2 节） |
| `const` 对象 | 有 | 遵守 | 能 | **C 里不能，C++ 里看初始化方式**（第 6 节实测） |
| 枚举值 `enum { N = 100 }` | 有（`int`） | 遵守 | 不能 | 能（第 6 节实测） |
| `constexpr` | 有 | 遵守 | 能 | 能（见《04-语法/12-编译期能力.md》第 2 节） |

> [!IMPORTANT]
> **`const` 管的是一件事：不许通过这个名字写它。**
> 它**不**保证这个值在编译期已知，也**不**保证它放在只读内存里。
> 前一句见《04-语法/12-编译期能力.md》第 2 节，后一句是第 2 节的内容。

## 1.2 接口里的承诺

**函数声明是调用者能看到的全部信息。**

`C`

```c
/* two_apis.c    编译：gcc -std=c23 two_apis.c -o two_apis */
#include <stdio.h>

/* 写法一：参数是 const char *。声明里就写明「不改 s 指向的内容」 */
static int len_ok(const char *s) {
    int n = 0;
    while (s[n] != 0) ++n;
    return n;
}

/* 写法二：参数是 char *。声明里没有这个承诺 */
static int len_plain(char *s) {
    int n = 0;
    while (s[n] != 0) ++n;
    return n;               /* 实现里也没改，但调用者看不出来 */
}

int main(void) {
    const char *a = "abcd";
    char b[] = "xyz";

    printf("len_ok(a) = %d, len_ok(b) = %d, len_ok(字面量) = %d\n",
           len_ok(a), len_ok(b), len_ok("hello"));
    printf("len_plain(b) = %d\n", len_plain(b));
    /* printf("%d\n", len_plain(a));      这一行在 C 里会警告，在 C++ 里会失败 */
    return 0;
}
```

`实测数据`
`Text`

```text
len_ok(a) = 4, len_ok(b) = 3, len_ok(字面量) = 5
len_plain(b) = 3
```

**写法二少了一个 `const`，代价不只是「没写清楚」**：
它连 `const char *` 的实参和字符串字面量都收不下——
**C 里编译器为此发出警告（加 `-Werror` 即失败），C++ 里直接拒绝编译。**

`C`

```c
/* discard_arg.c    编译：gcc -std=c23 -Wall discard_arg.c -o discard_arg
 * C 里只是警告，编译与运行都正常：passing argument 1 of 'my_len' discards
 * 'const' qualifier from pointer target type [-Wdiscarded-qualifiers]
 */
#include <stdio.h>

static int my_len(char *s) {        /* 参数少了 const */
    int n = 0;
    while (s[n] != 0) ++n;
    return n;
}

int main(void) {
    const char *a = "abcd";
    printf("%d %d\n", my_len(a), my_len("hello"));
    return 0;
}
```

`C++`

```cpp
// discard_arg.cpp    编译：g++ -std=c++17 -Wall discard_arg.cpp -o discard_arg （失败）
// C++ 里是错误：invalid conversion from 'const char*' to 'char*' [-fpermissive]
#include <cstdio>

static int my_len(char *s) {        // 参数少了 const
    int n = 0;
    while (s[n] != 0) ++n;
    return n;
}

int main() {
    const char *a = "abcd";
    std::printf("%d\n", my_len(a));
    return 0;
}
```

**同一条规则，C 给警告，C++ 给错误**——这类分歧在本章第 6 节汇总。

`实测数据`
`Text`

```text
# C：编译退出码 0（只有警告），运行退出码 0，输出
4 5

# C：把 -Wall -Werror 一起加上，编译退出码 1
cc1.exe: all warnings being treated as errors
# Linux 的措辞相同：cc1: all warnings being treated as errors

# C++：编译失败，退出码 1
error: invalid conversion from 'const char*' to 'char*' [-fpermissive]
```

> [!TIP]
> **只要函数不改这个指针指向的内容，参数就写成 `const T *`。**
> 这样声明与实现一致，调用者能传的东西也更多：
> 字面量、`const` 对象、普通对象都收得下。

## 1.3 不加 `const` 的两种后果

**后果一：想保护的东西保护不了。**

**后果二：能接受的实参变少，接口白白变窄。**

| 场景 | 参数没有 `const` | 参数有 `const` |
|---|---|---|
| 传字符串字面量 | C 警告、C++ **编译失败** | 收得下 |
| 传 `const` 对象 | C 警告、C++ **编译失败** | 收得下 |
| 传普通对象 | 收得下 | 收得下 |

**C 侧的「警告」在两处会变成麻烦**：
一是加上 `-Werror` 之后编译直接失败，
二是把 `const int *` 传给 `int *` 参数这种写法本身就在放弃保护。

**这条差异让「加 `const`」不再是风格偏好，而是能不能用的问题。**

---

# 第 2 节 `const` 的准确含义

## 2.1 被限制的是访问路径，不是内存

**先说结论。**

> [!IMPORTANT]
> **`const` 修饰的是「通过这个名字的访问路径」，不是「这块内存在物理上只读」。**
> 同一个对象可以有一条 `const` 的路径和一条可写的路径，
> 通过可写路径修改它是**完全合法**的。

**标准把这条写得很直接。**

`文档`

> "A pointer or reference to a cv-qualified type need not actually point or refer
> to a cv-qualified object, but it is treated as if it does; a const-qualified
> access path cannot be used to modify an object even if the object referenced is
> a non-const object and can be modified through some other access path."
>
> —— N4659 §10.1.7.1/3

**真正被禁止的是「改写一个 `const` 对象」**，
而与「访问路径」无关——路径上有没有 `const` 都可以改一个非 `const` 对象。

`文档`

> "If an attempt is made to modify an object defined with a const-qualified type
> through use of an lvalue with non-const-qualified type, the behavior is undefined."
>
> —— N3220 §6.7.4.1/7

`文档`

> "Except that any class member declared mutable (10.1.1) can be modified, any
> attempt to modify a const object during its lifetime (6.8) results in
> undefined behavior."
>
> —— N4659 §10.1.7.1/4

**两句话合起来就是本节的全部内容**：

| 情况 | 合法吗 |
|---|---|
| 对象本身**不是** `const`，路径上有 `const`，通过别的路径改它 | **合法** |
| 对象本身**是** `const`，通过强制转换绕开 `const` 去改它 | **未定义行为** |

**两种情况的代码与结果见 2.4 与 2.5。**

## 2.2 全局 `const` 落在只读段

**「`const` 放在只读内存里」这个说法从哪里来**：
在 GCC 这类实现上，**文件作用域的 `const` 对象确实会被放进只读段**，
于是写它会立刻崩。**这是实现的选择，不是标准的要求。**

`C`

```c
/* sections.c    编译：gcc -std=c23 -O0 sections.c -o sections
                  看符号：gcc -std=c23 -O0 -c sections.c -o sections.o && nm sections.o */
#include <stdio.h>
#include <stdlib.h>

const int g_const = 42;         /* 全局 const 对象 */
int g_var = 43;                 /* 普通全局对象，可写 */
const char g_str[] = "hello";   /* 全局 const 数组 */

int main(void) {
    const int l_const = 7;      /* 自动存储期的局部 const：在栈上 */
    int l_var = 8;
    static const int s_const = 3;   /* 静态存储期的局部 const */
    int *heap = malloc(sizeof(int));

    printf("&main    = %p\n", (void *)&main);
    printf("&g_const = %p\n", (void *)&g_const);
    printf("&g_str   = %p\n", (void *)g_str);
    printf("&s_const = %p\n", (void *)&s_const);
    printf("&g_var   = %p\n", (void *)&g_var);
    printf("&l_const = %p\n", (void *)&l_const);
    printf("&l_var   = %p\n", (void *)&l_var);
    printf("heap     = %p\n", (void *)heap);
    free(heap);
    return 0;
}
```

**先看符号表里的类型字母。**

`实测数据`
`Text`

```text
# Windows（gcc 15.2.0，nm sections.o，只列与本节有关的符号）
0000000000000000 R g_const          ← 大写 R：只读数据
0000000000000004 R g_str
0000000000000000 D g_var            ← 大写 D：可写数据
0000000000000000 T main
0000000000000084 r s_const.0        ← 小写 r：只读数据，且是内部链接

# Linux（gcc 13.3.0，nm sections.o）
0000000000000000 R g_const
0000000000000004 R g_str
0000000000000000 D g_var
0000000000000000 T main
0000000000000084 r s_const.0
```

**两个平台的符号类型逐条相同。**
`nm` 的字母含义（`R` 只读数据、`D` 可写数据、`T` 代码、小写表示内部链接）
见《04-语法/11-作用域、生存期与链接.md》第 4.5 小节。

**再看段的划分，两边只是名字不同。**

`实测数据`
`Text`

```text
# Windows（objdump -h sections.o，节选）
  1 .data         00000010  ...  CONTENTS, ALLOC, LOAD, DATA
  3 .rdata        00000090  ...  CONTENTS, ALLOC, LOAD, READONLY, DATA

# Linux（readelf -S sections.o，节选）
  [ 3] .data      PROGBITS  ...  0000000000000004  WA      ← W 表示可写
  [ 5] .rodata    PROGBITS  ...  0000000000000088  A       ← 没有 W
```

**同一份数据，放在两个段里**：
`g_var` 在 `.data`（ELF）或 `.data`（PE，属性是 `DATA`），
`g_const` 与 `g_str` 在 `.rodata`（ELF）或 `.rdata`（PE）。
**`g_var` 那个段带 `W`，`g_const` 那个段不带。**

**运行期的地址把这件事显示得更清楚。**

`实测数据`

| 对象 | Windows（MinGW x64） | Linux（WSL x86-64） | 落在哪 |
|---|---|---|---|
| `&main` | `00007ff79f961440` | `0x560db79891a9` | 代码段 |
| `&g_const` | `00007ff79f96a000` | `0x560db798a004` | 只读段 |
| `&g_str` | `00007ff79f96a004` | `0x560db798a008` | 只读段 |
| `&s_const`（静态局部） | `00007ff79f96a084` | `0x560db798a088` | 只读段 |
| `&g_var` | `00007ff79f969000` | `0x560db798c010` | 可写数据段 |
| `heap`（`malloc`） | `00000163504b2420` | `0x560db8c6f2a0` | 堆 |
| `&l_const`（局部） | `0000005f853ffa54` | `0x7ffff2bfb408` | 栈 |
| `&l_var`（局部） | `0000005f853ffa50` | `0x7ffff2bfb40c` | 栈 |

**地址每次运行都不同**（这是地址空间随机化的结果），
**要看的是相对位置**：`g_const` 与 `g_str` 紧挨着，与 `main` 在同一片低地址区；
`l_const` 与 `l_var` 紧挨着，落在另一端的栈区。

**这一列里最关键的一行是 `s_const`**：
它与 `l_const` 都带 `const`，却落在完全不同的段里。

**决定它放哪里的不是 `const`，而是存储期**
（存储期见《04-语法/11-作用域、生存期与链接.md》第 3 节）。

`文档`

> "The implementation can place a const object that is not volatile in a
> read-only region of storage. Moreover, the implementation does not have to
> allocate storage for such an object if its address is never used."
>
> —— N3220 §6.7.4.1 脚注 148

**标准用的词是 "can"**：
实现**可以**把 `const` 对象放进只读存储，**也可以不**。
在后半句那种「地址从未被使用」的情形里，编译器甚至不必为它分配存储——
这正好解释了 2.4 里那个「改了却没变」的现象。

## 2.3 局部 `const` 就在栈上

**自动存储期的 `const` 对象与同函数的普通局部变量是紧邻的。**

`实测数据`
`Text`

```text
# Windows                          # Linux
&l_const = 0000005f853ffa54         &l_const = 0x7ffff2bfb408
&l_var   = 0000005f853ffa50         &l_var   = 0x7ffff2bfb40c
```

**两者相差 4 字节**，正好是一个 `int` 的宽度，说明它们同在一块可写的栈内存上。
**这块内存上没有任何「只读」标记**，所以对它的写入不会触发硬件异常——
**但仍然是未定义行为**（见 2.4）。

> [!WARNING]
> **不要根据地址判断一个对象能不能写。**
> 局部 `const` 的地址在栈上，写进去不会崩；
> 全局 `const` 的地址在只读段，写进去会崩。
> **两种情况下这个写操作都是未定义行为**，只是表现不同。

## 2.4 强转写进去会怎样

**全局 `const` 在只读段上，写它会直接被硬件拦住。**

`C`

```c
/* write_global.c    编译：gcc -std=c23 -O0 write_global.c -o write_global */
#include <stdio.h>

const int g_const = 42;         /* 位于只读段 */

int main(void) {
    printf("before: g_const = %d\n", g_const);
    fflush(stdout);

    int *p = (int *)&g_const;   /* 强制转换去掉 const */
    *p = 99;                    /* 往只读段里写 */

    printf("after:  g_const = %d, *p = %d\n", g_const, *p);
    return 0;
}
```

`实测数据`
`Text`

```text
# Windows（gcc 15.2.0）
before: g_const = 42
运行退出码 3221225477 = 0xC0000005      ← 访问冲突

# Linux（gcc 13.3.0）
before: g_const = 42
Segmentation fault，运行退出码 139      ← 128 + 11（SIGSEGV）
```

**「before」那一行打印出来了，「after」那一行没有**：
程序在 `*p = 99;` 上就结束了。
**两个平台的崩溃码都是「访问了没有写权限的内存」。**

**局部 `const` 的情形完全不同：写操作会「成功」。**

`C`

```c
/* write_local.c    编译：gcc -std=c23 -O0 write_local.c -o write_local */
#include <stdio.h>

int main(void) {
    const int l_const = 7;      /* 自动存储期：对象在栈上 */

    int *p = (int *)&l_const;
    *p = 99;

    printf("l_const = %d, *p = %d\n", l_const, *p);
    return 0;
}
```

`实测数据`
`Text`

```text
# Windows，-O0               # Windows，-O2
l_const = 99, *p = 99        l_const = 7, *p = 99

# Linux，-O0                 # Linux，-O2
l_const = 99, *p = 99        l_const = 7, *p = 99
```

**同一份源码，同一个编译器，只换优化等级，结果就不同。**

| 优化等级 | 打印出来的东西 | 为什么 |
|---|---|---|
| `-O0` | `l_const = 99` | 每次读 `l_const` 都去栈上取，取到的就是被改写后的 99 |
| `-O2` | `l_const = 7` | 编译器知道它是 `const`，把 7 直接填进了 `printf` 的参数里，**根本没读内存** |

**C++ 在 `-O0` 下就给出了 `-O2` 的结果。**

`C++`

```cpp
// write_local.cpp    编译：g++ -std=c++17 -O0 write_local.cpp -o write_local
#include <cstdio>

int main() {
    const int l_const = 7;

    int *p = (int *)&l_const;
    *p = 99;

    std::printf("l_const = %d, *p = %d\n", l_const, *p);
    return 0;
}
```

`实测数据`
`Text`

```text
l_const = 7, *p = 99
```

**同一块内存，两种读法得到不同的值。**
原因是 C++ 里常量初始化的 `const int` 本身就是常量表达式，
编译器在 `-O0` 也有权把它折叠成字面量——
**这个灵活性正是「修改 `const` 对象是未定义行为」所允许的**。

> [!CAUTION]
> **「改了没崩」不等于「改成功了」。**
> `-O0` 下打印 99、`-O2` 下打印 7，是同一份未定义行为在两种编译方式下的两种表现。
> **未定义行为的意思是：标准不规定结果**，
> 于是编译器可以按「这个对象不会变」来优化，得到的程序与写代码的人想的不是一回事。
> 全局 `const` 上的同一次写入会直接崩，只是这个自由度的另一种表现。

## 2.5 什么时候可以绕过 `const`

**有一种情形是合法的**：对象本身不是 `const`，只是你手上的路径带 `const`。

`C`

```c
/* cast_ok.c    编译：gcc -std=c23 -O0 cast_ok.c -o cast_ok */
#include <stdio.h>

int main(void) {
    int a = 1;
    const int *pc = &a;         /* 路径带 const，对象本身可写 */

    a = 20;                     /* 直接用可写的名字改：合法 */
    printf("a = 20 之后 *pc = %d\n", *pc);

    *(int *)pc = 30;            /* 通过转换后的路径改：同样合法 */
    printf("*(int *)pc = 30 之后 a = %d\n", a);
    return 0;
}
```

`实测数据`
`Text`

```text
a = 20 之后 *pc = 20
*(int *)pc = 30 之后 a = 30
```

**这段代码完全符合标准**：被改的对象 `a` 是 `int`，不是 `const int`。
**区别只在「有没有一个 `const` 对象被改写」**——
这正是 2.1 里两条标准原文的分界。

> [!NOTE]
> **第 2 节小结。**
> `const` 是关于**名字与访问路径**的约束，由编译器检查；
> 「放进只读段」是**实现的选择**，标准只用了 "can" 这个词。
> 全局 `const` 与静态局部 `const` 在 GCC 上确实进了只读段（写它会崩），
> 而自动存储期的局部 `const` 在栈上（写它可能「成功」）。
> **无论哪一种，改写 `const` 对象都是未定义行为。**

---

# 第 3 节 `const` 与指针

## 3.1 三种组合

**`const` 与指针相遇时有三种写法，含义各不相同。**

| 写法 | `*p` 能写吗 | `p` 能改写吗 | 术语 |
|---|---|---|---|
| `const int *p` | **不能** | 能 | 底层 `const`（被指向的东西是 `const`） |
| `int * const p` | 能 | **不能** | 顶层 `const`（指针自己 `const`） |
| `const int * const p` | **不能** | **不能** | 两者都有 |

**写成 `int const *p` 与 `const int *p` 是同一件事**：
`const` 修饰的是它左边紧邻的类型；左边没有东西时，修饰右边紧邻的类型。
两种写法并存，本教材统一用 `const int *p`。

## 3.2 读法

**从变量名出发，先看右边，再看左边。**
这条规则对数组、指针、函数指针都适用（见《04-语法/08-数组、指针与引用.md》第 4.3 小节）。

`Text`

```text
   const int * p          读作：p 是一个指针，指向 const int
         ↑     │                 └─ 右边是 *，说明 p 是指针
         └─────┘                 左边是 const int，说明指向的东西不能改
         指向的东西不能改

   int * const p          读作：p 是一个 const 指针，指向 int
             ↑   │              左边是 * const，说明 p 自己不能改
             └───┘              再往左是 int，说明指向的东西能改
         p 自己不能改

   const int * const p    两边都有：p 不能改，*p 也不能改
```

**一句话版本**：`const` 在 `*` 左边就锁住「指向的东西」，在 `*` 右边就锁住「指针自己」。

> [!TIP]
> **`const int *p` 里的 `p` 本身不是 `const`**，所以它可以指向别处；
> **`int * const p` 里的 `p` 本身是 `const`**，所以定义时就必须给初值
> （C++ 编译器的要求见第 6.4 小节）。

## 3.3 每种组合能做什么

**先看四种「不行」的写法，报错原文照抄。**

`C`

```c
/* err_target_const.c    编译：gcc -std=c23 err_target_const.c -o err_target_const （失败） */
int main(void) {
    const int a = 1;
    const int *p = &a;      /* 底层 const：*p 不能改 */
    *p = 5;
    return 0;
}
```

`实测数据`
`Text`

```text
err_target_const.c: In function 'main':
err_target_const.c:5:8: error: assignment of read-only location '*p'
    5 |     *p = 5;
      |        ^
```

`C`

```c
/* err_const_ptr_assign.c    编译：gcc -std=c23 err_const_ptr_assign.c -o err_const_ptr_assign （失败） */
int main(void) {
    int a = 1, b = 2;
    int * const p = &a;     /* 顶层 const：p 本身不能改 */
    p = &b;
    return 0;
}
```

`实测数据`
`Text`

```text
err_const_ptr_assign.c: In function 'main':
err_const_ptr_assign.c:5:7: error: assignment of read-only variable 'p'
    5 |     p = &b;
      |       ^
```

**自增也是修改 `p`，报错的措辞换成了 `increment`。**

`C`

```c
/* err_const_ptr_inc.c    编译：gcc -std=c23 err_const_ptr_inc.c -o err_const_ptr_inc （失败） */
int main(void) {
    int a[3] = {1, 2, 3};
    int * const p = a;      /* 顶层 const：p 本身不能改 */
    p++;                    /* 自增也是修改 p */
    return 0;
}
```

`实测数据`
`Text`

```text
err_const_ptr_inc.c: In function 'main':
err_const_ptr_inc.c:5:6: error: increment of read-only variable 'p'
    5 |     p++;                    /* 自增也是修改 p */
      |      ^~
```

**两种 `const` 都有时，两条错误一次报出。**

`C`

```c
/* err_both_const.c    编译：gcc -std=c23 err_both_const.c -o err_both_const （失败） */
int main(void) {
    const int a = 1, b = 2;
    const int * const p = &a;   /* 两层 const */
    *p = 5;
    p = &b;
    return 0;
}
```

`实测数据`
`Text`

```text
err_both_const.c: In function 'main':
err_both_const.c:5:8: error: assignment of read-only location '*(const int *)p'
    5 |     *p = 5;
      |        ^
err_both_const.c:6:7: error: assignment of read-only variable 'p'
    6 |     p = &b;
      |       ^
```

**四份源码里的 `const` 位置不同，报错的关键词也跟着变**：

| 失败写法 | 报错关键词 | 读作 |
|---|---|---|
| `*p = 5`（`const int *p`） | `read-only location '*p'` | 位置只读 |
| `p = &b`（`int * const p`） | `read-only variable 'p'` | 变量只读 |
| `p++`（`int * const p`） | `increment of read-only variable 'p'` | 变量的自增被拒 |
| 两者都有 | 上面两条同时出现 | 位置与变量都只读 |

**同一段源码交给 C++，措辞几乎一致。**

`实测数据`
`Text`

```text
# g++ -std=c++17（同一份源码的 .cpp 版）
err_target_const.c:5:8: error: assignment of read-only location '* p'
err_const_ptr_assign.c:5:7: error: assignment of read-only variable 'p'
err_both_const.c:5:8: error: assignment of read-only location '*(const int*)p'
err_both_const.c:6:7: error: assignment of read-only variable 'p'
```

**差别只在语法细节的书写方式**（C++ 写成 `* p`、`const int*`），
**关键词 `read-only location` 与 `read-only variable` 两门语言相同。**

**再看允许的操作，全部集中在同一份源码里。**

`C`

```c
/* ptr_combos.c    编译：gcc -std=c23 -O0 ptr_combos.c -o ptr_combos */
#include <stdio.h>

int main(void) {
    int a = 1, b = 2;

    const int *p = &a;          /* 底层 const：*p 不能改，p 能改 */
    printf("*p = %d\n", *p);    /* 读是允许的 */
    p = &b;                     /* 改 p 本身：允许 */
    printf("p = &b 之后 *p = %d\n", *p);

    int * const q = &a;         /* 顶层 const：*q 能改，q 不能改 */
    *q = 10;                    /* 改 q 指向的对象：允许 */
    printf("*q = 10 之后 a = %d\n", a);

    const int * const r = &b;   /* 两层都有：只能读 */
    printf("*r = %d\n", *r);
    return 0;
}
```

`实测数据`
`Text`

```text
*p = 1
p = &b 之后 *p = 2
*q = 10 之后 a = 10
*r = 2
```

**每一行的对应关系**：
第一行是「底层 `const` 允许读」，
第二行是「底层 `const` 允许改指针本身」，
第三行是「顶层 `const` 允许改指向的对象」，
第四行是「两层 `const` 仍然允许读」。

## 3.4 对照表

**把三种组合放进四个场景里对照。**

| 写法 | `*p = 1` | `p = &other` | `p++` | 读 `*p` |
|---|---|---|---|---|
| `int *p` | 允许 | 允许 | 允许 | 允许 |
| `const int *p` | **报错** | 允许 | 允许 | 允许 |
| `int * const p` | 允许 | **报错** | **报错** | 允许 |
| `const int * const p` | **报错** | **报错** | **报错** | 允许 |

**这四条组合在函数参数上还会派生出一条实用规则。**

| 函数参数写成 | 能接收 `int *` | 能接收 `const int *` |
|---|---|---|
| `void f(int *p)` | 能 | **C 警告、C++ 报错**（丢失 `const`） |
| `void f(const int *p)` | 能（加限定是允许的） | 能 |

**加 `const` 是安全的，去掉 `const` 是危险的**：
从 `int *` 到 `const int *` 可以隐式转换，
反过来必须写强制转换，而 `const` 在 C 里只给警告、在 C++ 里直接报错
（原文见第 6.3 小节）。

## 3.5 顶层 `const` 与底层 `const`

**`const` 加在对象本身上叫顶层 `const`，加在被指向的类型上叫底层 `const`。**

`Text`

```text
   int * const p        顶层 const：p 这个对象自己不能改
   const int *p         底层 const：p 指向的类型不能改
   const int * const p  两者都有
```

**这个区分在函数类型上有一个直接后果**：
**顶层 `const` 会被丢掉，底层 `const` 会保留。**

`文档`

> "After producing the list of parameter types, any top-level cv-qualifiers
> modifying a parameter type are deleted when forming the function type."
>
> —— N4659 §11.3.5/5

`文档`

> "...each parameter declared with qualified type is taken as having the
> unqualified version of its declared type."
>
> —— N3220 §6.7.7.4/14

**C 与 C++ 在这条规则上一致**，本机两个编译器都不给任何诊断。

`C`

```c
/* param_toplevel.c    编译：gcc -std=c23 -Wall param_toplevel.c -o param_toplevel */
#include <stdio.h>

int twice(const int x);                 /* 顶层 const */
int twice(int x) { return 2 * x; }      /* 同一个函数，不是重复定义 */

int main(void) {
    printf("twice(21) = %d\n", twice(21));
    return 0;
}
```

`C++`

```cpp
// param_toplevel.cpp    编译：g++ -std=c++17 param_toplevel.cpp -o param_toplevel
#include <cstdio>

int twice(const int x);                 // 顶层 const
int twice(int x) { return 2 * x; }      // 同一个函数，不是重载

int main() {
    std::printf("twice(21) = %d\n", twice(21));
    return 0;
}
```

`实测数据`
`Text`

```text
# 两份源码都编译通过（-Wall 也没有警告）
twice(21) = 42
```

**底层 `const` 则真的进入了类型**：
`const int *` 与 `int *` 是两个不同的参数类型。

`C`

```c
/* param_lowlevel.c    编译：gcc -std=c23 param_lowlevel.c -o param_lowlevel （失败）
 * gcc 报错：error: conflicting types for 'f'; have 'int(int *)'
 *           note: previous declaration of 'f' with type 'int(const int *)'
 */
int f(const int *p);                    /* 底层 const：类型不同 */
int f(int *p) { return p ? *p : 0; }
int main(void) { return f((int *)0); }
```

**同一对声明在 C++ 里能编过**，因为它们是两个不同的重载
（重载见《04-语法/07-函数.md》第 5.3 小节，类的部分见【待补：05-类与面向对象/】）。

> [!WARNING]
> **参数上的顶层 `const` 对调用者没有任何影响。**
> `void f(const int x)` 与 `void f(int x)` 是同一个函数，
> 声明里写与不写，调用者看到的类型完全相同。
> **想让调用者受益，要加的是底层 `const`（`const T *`）或 `const T &`。**

---

# 第 4 节 `const` 与数组

## 4.1 数组元素是 `const` 时

**`const int a[3]` 里的 `const` 修饰的是元素。**
在 C 与 C++ 里，数组不是可修改的左值整体，元素才是——
因此 `a[0] = 9` 会被拦住，`a` 也不能整体赋值。

`C`

```c
/* arr_const.c    编译：gcc -std=c23 arr_const.c -o arr_const （失败） */
int main(void) {
    const int a[3] = {1, 2, 3};   /* 数组元素是 const */
    a[0] = 9;
    return 0;
}
```

`实测数据`
`Text`

```text
arr_const.c: In function 'main':
arr_const.c:4:10: error: assignment of read-only location 'a[0]'
    4 |     a[0] = 9;
      |          ^
```

**C++ 的措辞相同**（`read-only location 'a[0]'`）。

**数组元素被锁住之后，整块内存就是只读的**：
这与 2.2 里的全局 `const` 对象一样，写法上会直接崩。

## 4.2 数组名本来就不可赋值

**数组不能整体赋值，与 `const` 无关。**

`C`

```c
/* arr_assign.c    编译：gcc -std=c23 arr_assign.c -o arr_assign （失败） */
int main(void) {
    int a[3] = {1, 2, 3};
    int b[3] = {4, 5, 6};
    a = b;                  /* 数组名不是可修改的左值 */
    return 0;
}
```

`实测数据`
`Text`

```text
arr_assign.c: In function 'main':
arr_assign.c:5:7: error: assignment to expression with array type
    5 |     a = b;                  /* 数组名不是可修改的左值 */
      |       ^
```

**注意报错里没有出现 `const`**：即使两个数组都不是 `const`，这一行一样失败。
原因是数组名在表达式里退化成指向首元素的指针，退化之后只剩一个地址，
没有「一整块」可以赋（见《04-语法/08-数组、指针与引用.md》第 2.3 小节）。

> [!IMPORTANT]
> **不要把「数组不能赋值」记成 `const` 的功劳。**
> 它是数组退化这条规则的推论，与 `const` 是两件事。

## 4.3 `main` 的 `argv`

**标准写出来的第二种形式是 `char *argv[]`。**

`文档`

> "It shall be defined with a return type of int and with no parameters:
> `int main (void) { /* ... */ }` or with two parameters (referred to here as
> argc and argv, though any names may be used, as they are local to the
> function in which they are declared): `int main (int argc, char *argv[]) { /* ... */ }`
> or equivalent; or in some other implementation-defined manner."
>
> —— N3220 §5.1.2.3.2/1

**在这个形式上可以再加 `const`，而加在哪一层决定了锁住什么。**

`C`

```c
/* argv_const.c    编译：gcc -std=c23 -Wall -Wextra argv_const.c -o argv_const */
#include <stdio.h>

/* 元素写成 const 指针：参数类型是 char *const * */
int main(int argc, char *const argv[]) {
    for (int i = 0; i < argc; ++i) {
        printf("argv[%d] = %s\n", i, argv[i]);
    }
    return 0;
}
```

`实测数据`
`Text`

```text
# Windows 与 Linux 的 gcc 都编译通过，-Wall -Wextra 没有任何诊断
```

**再给被指向的字符也加 `const`，gcc 同样接受。**

`C`

```c
/* argv_deep_const.c    编译：gcc -std=c23 -Wall -Wextra argv_deep_const.c -o argv_deep_const */
#include <stdio.h>

/* 参数类型变成 const char *const * */
int main(int argc, const char *const argv[]) {
    printf("argc = %d, argv[0] = %s\n", argc, argv[0]);
    return 0;
}
```

`实测数据`
`Text`

```text
argc = 3, argv[0] = ...（程序自身的路径）
```

**两种加法的效果不同，而且加了之后就写不回去了。**

| 写法 | `argv` 数组里的指针 | 指针指向的字符 |
|---|---|---|
| `char *argv[]` | 可改 | 可改 |
| `char *const argv[]` | **不可改** | 可改 |
| `const char *const argv[]` | **不可改** | **不可改** |

`C`

```c
/* argv_element.c    编译：gcc -std=c23 argv_element.c -o argv_element （失败） */
int main(int argc, char *const argv[]) {
    if (argc > 0) {
        argv[0] = "x";      /* 元素是 const 指针，不能改 */
    }
    return 0;
}
```

`实测数据`
`Text`

```text
argv_element.c: In function 'main':
argv_element.c:4:17: error: assignment of read-only location '*argv'
    4 |         argv[0] = "x";      /* 元素是 const 指针，不能改 */
      |                 ^
```

**`argv` 是数组名，`argv[0]` 就是 `*argv`**，所以报错指向 `*argv`。
**这一条正是 3.1 那张表在真实代码里的样子**：
`char *const argv[]` 是「数组元素是顶层 `const` 的指针」，
`const char *const argv[]` 再叠一层底层 `const`。

> [!WARNING]
> **标准只承诺 `char *argv[]` 这一种带参数的写法**，
> 其余形式属于 "in some other implementation-defined manner"。
> `const char *const argv[]` 在 `gcc` 15.2.0 与 13.3.0 上都能通过且没有警告，
> **但这属于实现的额外允许**；
> 更重要的是，标准要求「`argv` 指向的字符串可以被程序修改」，
> 把它们锁成 `const` 之后，这一段能力就自己放弃了。
> **需要改 `argv` 或它指向的字符时，不要加这两层 `const`。**

## 4.4 数组参数上的 `const`

**`const int a[3]` 作为参数时，数组那一层会先退化成指针。**

`C`

```c
/* arr_param.c    编译：gcc -std=c23 arr_param.c -o arr_param */
#include <stdio.h>

/* 下面三种写法是同一个函数类型：const int * */
static int sum3(const int a[3])         { return a[0] + a[1] + a[2]; }
static int sum_ptr(const int *a)        { return a[0] + a[1] + a[2]; }
static int sum_sugar(const int a[])     { return a[0] + a[1] + a[2]; }

int main(void) {
    const int v[3] = {1, 2, 3};
    int w[3] = {4, 5, 6};
    printf("%d %d %d\n", sum3(v), sum_ptr(v), sum_sugar(v));
    printf("%d\n", sum_ptr(w));     /* 普通数组也能传：加限定是允许的 */
    return 0;
}
```

`实测数据`
`Text`

```text
6 6 6
15
```

**方括号里的 3 不起作用**，它既不检查长度，也不影响参数类型
（见《04-语法/08-数组、指针与引用.md》第 2.5 小节）。
**真正起作用的是那个 `const`**：它让函数能接收 `const int *`，
也让读代码的人知道这个数组不会被改。

---

# 第 5 节 `const` 与引用（C++）

## 5.1 普通引用必须绑定到对象

**C++ 的引用必须绑定到一个左值**（左值见《04-语法/04-表达式与运算符.md》第 2.2 小节）。
字面量与临时对象都是右值，因此不能用普通引用绑定。

`C++`

```cpp
// ref_err.cpp    编译：g++ -std=c++17 ref_err.cpp -o ref_err （失败）
int main() {
    int &r = 5;             // 普通引用不能绑定右值
    return r;
}
```

`实测数据`
`Text`

```text
ref_err.cpp: In function 'int main()':
ref_err.cpp:3:14: error: cannot bind non-const lvalue reference of type 'int&'
to an rvalue of type 'int'
    3 |     int &r = 5;             // 普通引用不能绑定右值
      |              ^
```

**标准对这条的措辞是「要么是 `const` 左值引用，要么是右值引用」。**

`文档`

> "Otherwise, the reference shall be an lvalue reference to a non-volatile const
> type (i.e., cv1 shall be const), or the reference shall be an rvalue reference."
>
> —— N4659 §11.6.3/5.2

**同一段标准给的例子正是上面这份源码。**

`文档`

> ```text
> double& rd2 = 2.0;      // error: not an lvalue and reference not const
> int i = 2;
> double& rd3 = i;        // error: type mismatch and reference not const
> ```
>
> —— N4659 §11.6.3/5.2

## 5.2 `const T&` 能绑定什么

**加上 `const` 之后，三种实参都能绑定。**

`C++`

```cpp
// ref_bind.cpp    编译：g++ -std=c++17 ref_bind.cpp -o ref_bind
#include <cstdio>
#include <string>

int main() {
    const int &r = 5;               // 绑定字面量
    std::printf("r = %d\n", r);

    int a = 1;
    const int &cr = a;              // 绑定普通变量：a 本身仍可改
    a = 2;
    std::printf("cr = %d\n", cr);

    const std::string &s = std::string("临时对象");   // 绑定临时对象
    std::printf("s = %s, 长度 %zu\n", s.c_str(), s.size());
    return 0;
}
```

`实测数据`
`Text`

```text
r = 5
cr = 2
s = 临时对象, 长度 12
```

**三行的含义各不相同**：

| 写法 | 绑定了什么 | 之后能读到什么 |
|---|---|---|
| `const int &r = 5;` | 一个临时对象，值是 5 | 5 |
| `const int &cr = a;` | 对象 `a` 本身 | 后来 `a = 2`，因此读到 2 |
| `const std::string &s = std::string(...)` | 一个临时对象 | 临时对象仍然可用 |

**第二行说明 `const` 引用不表示对象是 `const`**：
`a` 不是 `const` 对象，通过 `cr` 只是不能写它，通过 `a` 照样能写。
这与第 2.1 小节的两条标准原文完全一致——
**`const` 限定的是访问路径。**

## 5.3 临时对象的生存期被延长了

**`const std::string &s = std::string("临时对象");` 这一行里，
右边的临时对象本该在这一行结束时销毁**，
但绑定到 `const` 引用之后，它的生存期被延长到与引用一样长。

`文档`

> "The temporary to which the reference is bound or the temporary that is the
> complete object of a subobject to which the reference is bound persists for
> the lifetime of the reference except: ..."
>
> —— N4659 §15.2/6

**上一步打印出的长度 12 就是这条规则的证据**：
`s.size()` 得到 12，说明那一行之后的临时对象仍然存在。

> [!WARNING]
> **延长只对「直接绑定」有效。**
> N4659 §15.2/6 紧接着列了三种例外，
> 其中包括「绑定到函数返回值上的临时对象不会被延长」。
> **返回 `const T&` 指向一个局部对象或临时对象，会得到悬垂引用**——
> 这类写法属于【待补：05-类与面向对象/】里要展开的内容。

## 5.4 传参不拷贝：`const T&` 最常见的用途

**把一个对象传给 `const T&` 参数，函数里拿到的是原对象，不是副本。**

`C++`

```cpp
// ref_nocopy.cpp    编译：g++ -std=c++17 -O0 ref_nocopy.cpp -o ref_nocopy
#include <cstdio>

struct Big { int data[64]; };       // 256 字节

void by_value(Big b) {              // 传值：复制一份
    std::printf("by_value : &b = %p\n", (void *)&b);
}

void by_cref(const Big &b) {        // 传 const 引用：不复制
    std::printf("by_cref  : &b = %p\n", (void *)&b);
}

int main() {
    Big x{};
    std::printf("sizeof(Big) = %zu\n", sizeof(Big));
    std::printf("main     : &x = %p\n", (void *)&x);
    by_value(x);
    by_cref(x);
    return 0;
}
```

`实测数据`
`Text`

```text
sizeof(Big) = 256
main     : &x = 0000006e50fffbb0
by_value : &b = 0000006e50fffab0      ← 与 &x 不同：这是副本
by_cref  : &b = 0000006e50fffbb0      ← 与 &x 相同：没有副本
```

**两个地址的关系正好说明问题**：
`by_value` 里的 `b` 在栈上另一个位置，差了 256 字节，正好是 `sizeof(Big)`；
`by_cref` 里的 `b` 就是 `x` 本身。
**传值要复制 256 字节，传 `const` 引用一个字节都不复制。**

**这条与「按值传递」的规则是一致的**：
形参是函数自己的局部变量，传值就是拷贝
（见《04-语法/07-函数.md》第 3 节）。

> [!TIP]
> **只读的大对象一律传 `const T&`。**
> 它同时做到两件事：不拷贝，且函数里改不动。
> 小的标量（`int`、`double`、指针）按值传就好，引用反而多一层间接。

## 5.5 `const` 引用的其他用处

**一、返回值上不要用 `const T&` 指局部对象**（悬垂，见 5.3 的警告）。

**二、`const T&` 参数能接受更多实参**：临时对象、字面量、`const` 对象、普通对象都收得下。
普通引用 `T&` 只能接受可修改的左值。

**三、成员函数上的 `const`**（写在函数后面，表示不修改对象）
属于《05-类与面向对象/02-类是一种类型.md》第 2.3 小节。

---

# 第 6 节 C 与 C++ 的分歧

**`const` 是两门语言都有的限定符，规则也大体相同。
分歧集中在四处，每一处都有实际后果。**

## 6.1 分歧一：`const` 变量是不是常量表达式

**在 C 里，`const` 变量是「只读变量」，不是常量表达式。**

`C`

```c
/* vla.c    编译：gcc -std=c23 -O0 -Wvla vla.c -o vla */
#include <stdio.h>

int main(void) {
    const int n = 5;        /* C 里这不是常量表达式 */
    int a[n];               /* 于是 a 是变长数组 */
    enum { M = 5 };         /* 枚举常量才是常量表达式 */
    int b[M];               /* 定长数组：没有警告 */

    printf("sizeof(a) = %zu, sizeof(b) = %zu\n", sizeof(a), sizeof(b));
    return 0;
}
```

`实测数据`
`Text`

```text
vla.c: In function 'main':
vla.c:6:5: warning: ISO C90 forbids variable length array 'a' [-Wvla]
    6 |     int a[n];               /* 于是 a 是变长数组 */
      |     ^~~
sizeof(a) = 20, sizeof(b) = 20
```

**警告说明 `a` 是变长数组（VLA）**——它的大小在运行期才知道。
两份数组打印出的 `sizeof` 相同，是因为 `n` 恰好等于 `M`，
**但 `a` 的那一次是在运行期算出来的。**

**文件作用域里这一点会变成硬错误。**

`C`

```c
/* vla_file.c    编译：gcc -std=c23 -c vla_file.c -o vla_file.o （失败） */
const int n = 5;
int a[n];               /* 文件作用域的数组长度必须是常量表达式 */
```

`实测数据`
`Text`

```text
vla_file.c:3:5: error: variably modified 'a' at file scope
    3 | int a[n];               /* 文件作用域的数组长度必须是常量表达式 */
      |     ^
```

**在 C++ 里，同样的写法是合法的常量表达式。**

`C++`

```cpp
// vla_ok.cpp    编译：g++ -std=c++17 vla_ok.cpp -o vla_ok
#include <cstdio>

int main() {
    const int n = 5;        // 常量初始化 → 常量表达式
    int a[n];               // 合法：数组长度是常量
    std::printf("sizeof(a) = %zu\n", sizeof(a));
    return 0;
}
```

`实测数据`
`Text`

```text
sizeof(a) = 20
```

**划界的条件不是 `const`，而是初始化方式。**

`C++`

```cpp
// vla_err.cpp    编译：g++ -std=c++17 -pedantic-errors vla_err.cpp -o vla_err （失败）
int get() { return 5; }

int main() {
    const int m = get();    // 运行期初始化 → 不是常量表达式
    int b[m];               // 不能当数组长度
    return b[0];
}
```

`实测数据`
`Text`

```text
# 默认：g++ 15.2.0 与 13.3.0 都不给任何诊断，把它当扩展接受
# 加 -Wvla：
warning: variable length array 'b' is used [-Wvla]
# 加 -pedantic-errors：
error: ISO C++ forbids variable length array 'b' [-Wvla]
```

**同一个 `const`，常量初始化的能当数组长度，运行期初始化的不能。**
`const` 只保证「不通过这个名字改」，不保证「值在编译期已知」——
这条边界在《04-语法/12-编译期能力.md》第 2 节展开，
需要真正的编译期常量时用那里的 `constexpr`（C++ 里对象与函数都能用，
C23 只把它给了对象）。

> [!IMPORTANT]
> **C 与 C++ 在这件事上的差距是语言设计的结果**：
> C 里 `const` 只是限定符，常量表达式另有来源（枚举、宏、`sizeof`）；
> C++ 里常量初始化的 `const` 本身就是常量表达式。
> C 里的手段见《04-语法/12-编译期能力.md》第 6 节。

## 6.2 分歧二：文件作用域 `const` 的链接属性

**这是两门语言在链接层面最实际的一处分歧。**

`C`

```c
/* link_c.c    编译：gcc -std=c23 -c link_c.c -o link_c.o */
const int cval = 5;
```

`C++`

```cpp
// link_cpp.cpp    编译：g++ -std=c++17 -c link_cpp.cpp -o link_cpp.o
const int cval = 5;
```

**看符号表就一目了然。**

`实测数据`
`Text`

```text
# C（gcc 15.2.0 / 13.3.0 结果相同）
0000000000000000 R cval        ← 大写 R：外部链接

# C++（g++ 15.2.0 / 13.3.0 结果相同）
0000000000000000 r _ZL4cval    ← 小写 r，符号名里的 L 表示 local：内部链接
```

**两个文件一起编时，差异变成能不能连上。**

`C`

```c
/* link_use.c    编译：gcc -std=c23 -c link_use.c -o link_use.o
 * 链接要带上 link_c.c，命令见下面的 Bash 块 */
#include <stdio.h>
extern const int cval;
int main(void) {
    printf("cval = %d\n", cval);
    return 0;
}
```

`C++`

```cpp
// link_use.cpp    编译：g++ -std=c++17 -c link_use.cpp -o link_use.o
// 链接要带上 link_cpp.cpp，两文件一起编会失败（见下）
#include <cstdio>
extern const int cval;
int main() {
    std::printf("cval = %d\n", cval);
    return 0;
}
```

`实测数据`
`Bash`

```bash
# C：两个文件一起编，通过
gcc -std=c23 link_use.c link_c.c -o link_use_c && ./link_use_c
# cval = 5

# C++：两个文件一起编，链接失败
g++ -std=c++17 link_use.cpp link_cpp.cpp -o link_use_cpp
# undefined reference to `cval'
# collect2: error: ld returned 1 exit status
```

**标准原文给了这条差异的依据。**

`文档`

> "If the declaration of an identifier for an object has file scope and does not
> contain the storage-class specifier static or constexpr, its linkage is external."
>
> —— N3220 §6.2.2/5

`文档`

> "A name having namespace scope (6.3.6) has internal linkage if it is the name of
> ... a non-inline variable of non-volatile const-qualified type that is neither
> explicitly declared extern nor previously declared to have external linkage"
>
> —— N4659 §6.5/3.2

**C 的规则里没有 `const` 这个词**，所以它是外部链接；
**C++ 的规则里点名了 `const`**，所以它是内部链接。

**想让 C++ 里的 `const` 全局变量被别的文件用到，加 `extern`。**

`C++`

```cpp
// link_fixed.cpp    编译：g++ -std=c++17 -c link_fixed.cpp -o link_fixed.o
// 链接要带上 link_use.cpp，命令见下
extern const int cval = 5;      // 显式 extern：外部链接
```

`实测数据`
`Bash`

```bash
g++ -std=c++17 link_use.cpp link_fixed.cpp -o link_fixed && ./link_fixed
# cval = 5
```

**工程上的后果**：头文件里写 `const int N = 5;`
在 C++ 里是安全的（每个文件各一份），在 C 里会重复定义。
**完整讨论见《04-语法/11-作用域、生存期与链接.md》第 5 节**，
底层的链接规则见《01-编译器/00-语言的实现.md》第 6.4 小节。

## 6.3 分歧三：丢掉 `const` 限定

**把 `const int *` 赋给 `int *`，两门语言的处置不同。**

`C`

```c
/* discard.c    编译：gcc -std=c23 -Wall discard.c -o discard */
#include <stdio.h>

int main(void) {
    const int a = 1;
    const int *p = &a;
    int *q = p;             /* C 里只是警告 */
    printf("*q = %d\n", *q);
    return 0;
}
```

`实测数据`
`Text`

```text
discard.c: In function 'main':
discard.c:7:14: warning: initialization discards 'const' qualifier from pointer
target type [-Wdiscarded-qualifiers]
    7 |     int *q = p;             /* C 里只是警告 */
      |              ^
*q = 1
```

**C++ 里这是错误。**

`C++`

```cpp
// discard.cpp    编译：g++ -std=c++17 discard.cpp -o discard （失败）
int main() {
    const int a = 1;
    const int *p = &a;
    int *q = p;             // C++ 里是错误
    return *q;
}
```

`实测数据`
`Text`

```text
discard.cpp: In function 'int main()':
discard.cpp:5:14: error: invalid conversion from 'const int*' to 'int*' [-fpermissive]
    5 |     int *q = p;             // C++ 里是错误
      |              ^
      |              const int*
```

**第 1.2 小节里那份「参数少写 `const`」的源码是同一件事的另一面**：
C 给 `warning: passing argument 1 of ... discards 'const' qualifier`，
C++ 给 `error: invalid conversion from 'const char*' to 'char*'`。

> [!WARNING]
> **C 只给警告，不代表这样写没关系。**
> `int *q = p;` 之后，`q` 就是一条没有 `const` 的访问路径。
> 如果 `q` 指向的正好是一个 `const` 对象，
> 通过它写入就是第 2.1 小节里那条未定义行为。
> **把 `-Wall -Werror` 一起加上，这条警告就变成编译错误**：
> C 侧的编译退出码从 0 变成 1，输出 `all warnings being treated as errors`。

## 6.4 分歧四：`const` 对象必须有初始值

**C++ 要求 `const` 对象在定义时就有初值，C 不要求。**

`C`

```c
/* noinit.c    编译：gcc -std=c23 -Wall noinit.c -o noinit */
int main(void) {
    int * const p;          /* C 里只是「没初始化」，还能编过 */
    return p != 0;
}
```

`实测数据`
`Text`

```text
noinit.c: In function 'main':
noinit.c:4:14: warning: 'p' is used uninitialized [-Wuninitialized]
    4 |     return p != 0;
      |            ~~^~~~
noinit.c:3:17: note: 'p' was declared here
    3 |     int * const p;          /* C 里只是「没初始化」，还能编过 */
      |                 ^
```

**C++ 里这不是警告，是错误。**

`C++`

```cpp
// noinit.cpp    编译：g++ -std=c++17 noinit.cpp -o noinit （失败）
int main() {
    int * const p;          // C++ 里必须有初始值
    return p != 0;
}
```

`实测数据`
`Text`

```text
noinit.cpp: In function 'int main()':
noinit.cpp:3:17: error: uninitialized 'const p' [-fpermissive]
    3 |     int * const p;          // C++ 里必须有初始值
      |                 ^
```

**标准的依据。**

`文档`

> "As described in 11.6, the definition of an object or subobject of
> const-qualified type must specify an initializer or be subject to
> default-initialization."
>
> —— N4659 §10.1.7.1/2

**C++ 这样规定是有道理的**：`const` 对象不能赋值，若定义时也不给初值，
它永远不可能有一个有意义的值。
初始化与赋值的区别见《04-语法/05-初始化.md》第 1 节。

## 6.5 四处分歧汇总

| 分歧 | C | C++ | 在本章哪里 |
|---|---|---|---|
| `const int n = 5;` 能否当数组长度 | **不能**，`n` 是只读变量，`int a[n]` 是 VLA | **能**（常量初始化时） | 6.1 |
| 文件作用域 `const` 的链接属性 | **外部链接**，`nm` 显示 `R` | **内部链接**，`nm` 显示 `r _ZL...` | 6.2 |
| 把 `const int *` 给 `int *` | **警告** | **错误** | 6.3 |
| `const` 对象必须有初值 | 不要求 | **要求** | 6.4 |

**这四处都属于同一类现象**：同一段源码在两门语言里行为不同，
是《00-写在最初/01-C与C++.md》第 2.3.4 小节所讲的那一类差异。

---

# 第 7 节 写代码时的用法

## 7.1 能加 `const` 的参数都加上

**理由已经在第 1 节与第 5 节给过，这里集中成一张对照表。**

| 场合 | 建议写法 | 好处 |
|---|---|---|
| 函数只读一个指针指向的内容 | `const T *p` | 声明即承诺；能接收字面量与 `const` 实参 |
| 函数只读一个大对象 | `const T &x`（C++） | 不拷贝，且函数里改不动 |
| 函数要改这个对象 | `T *p` 或 `T &x` | 调用者从声明就能看出「这里会被改」 |
| 本函数内不改的局部变量 | `const T x = ...;` | 防自己改错；对调用者不可见 |
| 类的成员函数不改对象 | 函数后面加 `const`（C++） | 见《05-类与面向对象/02-类是一种类型.md》第 2.3 小节 |

**第二条与第三条合起来构成一条接口约定**：
**看函数声明就能知道哪些参数会被改。**
这对读代码的人比注释可靠得多。

## 7.2 传参不拷贝

**C++ 的 `const T&` 参数还有一个作用：省掉一次拷贝。**

`实测数据`
`Text`

```text
sizeof(Big) = 256
main     : &x = 0000006e50fffbb0
by_value : &b = 0000006e50fffab0      ← 复制了 256 字节
by_cref  : &b = 0000006e50fffbb0      ← 一个字节都没复制
```

**判断标准**：对象的字节数明显大于一个指针（8 字节）时，传 `const T&` 更划算。
小的标量按值传，引用反而引入一层间接。

## 7.3 什么时候不要加 `const`

**三种「加了没坏处，但也没有好处」的写法。**

**一、按值参数上的顶层 `const`。**

`C++`

```cpp
// param_useless.cpp    编译：g++ -std=c++17 param_useless.cpp -o param_useless
#include <cstdio>

int twice(const int x);                 // 顶层 const：调用者看不到
int twice(int x) { return 2 * x; }      // 同一个函数

int main() {
    std::printf("twice(21) = %d\n", twice(21));
    return 0;
}
```

`实测数据`
`Text`

```text
twice(21) = 42
```

**两行声明的是同一个函数**（依据见第 3.5 小节的两条标准原文）。
把 `const` 写在值参数上只约束函数体内那一个局部副本，
**对调用者与函数类型都没有影响**。

**二、返回类型上的 `const`。**

`C`

```c
/* ret_qual.c    编译：gcc -std=c23 -Wall -Wextra ret_qual.c -o ret_qual */
const int five(void) { return 5; }      /* 返回类型上的 const 没有意义 */
int main(void) { return five(); }
```

`实测数据`
`Text`

```text
ret_qual.c:2:1: warning: type qualifiers ignored on function return type
[-Wignored-qualifiers]
    2 | const int five(void) { return 5; }      /* 返回类型上的 const 没有意义 */
      | ^~~~~
```

**C++ 给出同一条警告**（措辞完全相同）。
返回值是一个副本，限定这个副本没有意义。

**三、把 `const` 当成编译期常量用。**

**C 里 `const int n = 5;` 不能当数组长度**（第 6.1 小节的实测），
**C++ 里也只有常量初始化时才行**。
**需要编译期常量就用 `constexpr`**，见《04-语法/12-编译期能力.md》第 2 节。

## 7.4 一条判断标准

> [!TIP]
> **问一句：这个对象（或这条访问路径）在后面需要被写吗。**
> **不需要写，就加 `const`；需要写，就不加。**
> 判断错了的代价很小——编译器会在该报错的地方报错，
> 而漏掉 `const` 的代价是少了一层保护，且往往要等到出问题时才发现。

**三种加法的效果可以叠在一起看。**

`Text`

```text
   const int * p              保护「指向的东西」，适合只读参数的场景
   int * const p              保护「指针自己」，适合「这句话我只说一次」
   const int * const p        两者都保护，适合「表」这类常量结构
   const T & x          （C++）保护对象，同时不拷贝
```

> [!NOTE]
> **第 7 节小结。**
> `const` 的用法可以归结为两件事：
> **接口上，能承诺的承诺出去**（`const T *`、`const T &`）；
> **实现里，该防的防住**（局部对象、成员函数）。
> 加在值参数与返回类型上没有效果，加在数组长度这种「需要常量表达式」的场合则用错了工具。

---

# 术语表

| 术语 | 英文 | 含义 |
|---|---|---|
| 类型限定符 | Type qualifier | `const`、`volatile`、`restrict`（C）、`_Atomic`（C），见《04-语法/02-数据类型与类型系统.md》第 3 节 |
| 只读段 | Read-only section | 实现用来放只读数据的段：ELF 里是 `.rodata`，PE 里是 `.rdata` |
| 顶层 `const` | Top-level const | 对象本身是 `const`，如 `int * const p` |
| 底层 `const` | Low-level const | 被指向的类型是 `const`，如 `const int *p` |
| 访问路径 | Access path | 通过某个名字或指针访问对象的途径；`const` 限制的是它 |
| 常量表达式 | Constant expression | 编译期就能求值的表达式，见《04-语法/12-编译期能力.md》第 1 节 |
| 链接属性 | Linkage | 名字能否被别的翻译单元引用，见《04-语法/11-作用域、生存期与链接.md》第 4 节 |
| `const` 引用 | Reference to const | 不能通过它修改对象的引用，能绑定临时对象（C++） |
| 强制转换去掉 `const` | Casting away constness | 用强制转换绕开 `const`；若对象本身是 `const`，随后的写入是未定义行为 |

---

# 附录 A 复现本章节实测

**本附录给出每一组实测的命令与输出。**
**与正文完全相同的源码不再重抄**，按下表回到对应小节复制即可；
表中没有列出的文件，源码就在本附录对应的那一小节里。

| 文件 | 源码在哪 |
|---|---|
| `write_global.c`、`write_local.c`、`write_local.cpp` | 第 2.4 小节 |
| `ref_err.cpp` | 第 5.1 小节 |
| `ref_bind.cpp` | 第 5.2 小节 |
| `vla.c` | 第 6.1 小节 |
| `link_c.c`、`link_cpp.cpp`、`link_use.c`、`link_use.cpp`、`link_fixed.cpp` | 第 6.2 小节 |
| `discard.c`、`discard.cpp` | 第 6.3 小节 |
| `noinit.c`、`noinit.cpp` | 第 6.4 小节 |

## A.1 环境与命令差异

| 平台 | 编译器 | 版本 | C 标准选项 |
|---|---|---|---|
| Windows x64（MinGW-w64） | `gcc` / `g++` | 15.2.0 | `-std=c23` |
| Linux x86-64（WSL Ubuntu 24.04） | `gcc` / `g++` | 13.3.0 | `-std=c2x`（该版本还没有 `-std=c23`） |

**两个平台都跑过本附录的全部实验，结论一致**。
差别只有两处：

| 差别 | 说明 |
|---|---|
| 引号形状 | Linux 的 gcc 输出用弯引号 `‘p’`，Windows 用直引号 `'p'` |
| 崩溃码 | Windows 报 `0xC0000005`，Linux 报 `SIGSEGV`（退出码 139） |

**源码与脚本位置**：`临时/ops_lab/`（该目录已被 `.gitignore` 排除）。
Windows 侧由 `run_const_lab.py`、`run_const_lab2.py`、`run_const_lab3.py` 驱动，
Linux 侧由 `lab_const_linux.sh` 驱动。

`Bash`

```bash
# Windows 侧
python 临时/ops_lab/run_const_lab.py
python 临时/ops_lab/run_const_lab2.py
python 临时/ops_lab/run_const_lab3.py

# Linux 侧（显式指定发行版）
wsl -d Ubuntu -e bash "/mnt/k/C相关课程/临时/ops_lab/lab_const_linux.sh"
```

**下面的地址每次运行都不同**（地址空间随机化），
对照的是相对位置与符号类型，不是具体的数值。

## A.2 段的归属与地址

`C`

```c
/* sections.c    编译：gcc -std=c23 -O0 sections.c -o sections */
#include <stdio.h>
#include <stdlib.h>

const int g_const = 42;         /* 全局 const 对象 */
int g_var = 43;                 /* 普通全局对象，可写 */
const char g_str[] = "hello";   /* 全局 const 数组 */

int main(void) {
    const int l_const = 7;      /* 自动存储期的局部 const：在栈上 */
    int l_var = 8;
    static const int s_const = 3;   /* 静态存储期的局部 const */
    int *heap = malloc(sizeof(int));

    printf("&main    = %p\n", (void *)&main);
    printf("&g_const = %p\n", (void *)&g_const);
    printf("&g_str   = %p\n", (void *)g_str);
    printf("&s_const = %p\n", (void *)&s_const);
    printf("&g_var   = %p\n", (void *)&g_var);
    printf("&l_const = %p\n", (void *)&l_const);
    printf("&l_var   = %p\n", (void *)&l_var);
    printf("heap     = %p\n", (void *)heap);
    free(heap);
    return 0;
}
```

`Bash`

```bash
gcc -std=c23 -O0 -c sections.c -o sections.o
nm sections.o --defined-only
objdump -h sections.o            # Windows
readelf -S sections.o            # Linux
gcc -std=c23 -O0 sections.c -o sections && ./sections
```

`实测数据`
`Text`

```text
# nm sections.o（两个平台的符号类型相同）
0000000000000000 R g_const
0000000000000004 R g_str
0000000000000000 D g_var
0000000000000000 T main
0000000000000084 r s_const.0

# objdump -h sections.o（Windows，节选）
  1 .data         00000010  ...  CONTENTS, ALLOC, LOAD, DATA
  3 .rdata        00000090  ...  CONTENTS, ALLOC, LOAD, READONLY, DATA

# readelf -S sections.o（Linux，节选）
  [ 3] .data      PROGBITS  ...  0000000000000004  WA
  [ 5] .rodata    PROGBITS  ...  0000000000000088  A

# 运行输出（Windows / Linux 各一次）
&main    = 00007ff79f961440    /  0x560db79891a9
&g_const = 00007ff79f96a000    /  0x560db798a004
&g_str   = 00007ff79f96a004    /  0x560db798a008
&s_const = 00007ff79f96a084    /  0x560db798a088
&g_var   = 00007ff79f969000    /  0x560db798c010
&l_const = 0000005f853ffa54    /  0x7ffff2bfb408
&l_var   = 0000005f853ffa50    /  0x7ffff2bfb40c
heap     = 00000163504b2420    /  0x560db8c6f2a0
```

## A.3 强转写 `const` 对象

`Bash`

```bash
gcc -std=c23 -O0 write_global.c -o write_global && ./write_global; echo "exit=$?"
# Windows：exit=3221225477（0xC0000005）
# Linux：Segmentation fault，exit=139

gcc -std=c23 -O0 write_local.c -o wl_O0 && ./wl_O0
gcc -std=c23 -O2 write_local.c -o wl_O2 && ./wl_O2
g++ -std=c++17 -O0 write_local.cpp -o wl_cpp && ./wl_cpp
```

`实测数据`
`Text`

```text
# 全局 const（两个平台）
before: g_const = 42
然后进程结束：Windows 0xC0000005，Linux 139

# 局部 const，C
-O0：l_const = 99, *p = 99
-O2：l_const = 7, *p = 99

# 局部 const，C++，-O0
l_const = 7, *p = 99
```

## A.4 三种指针组合

`C`

```c
/* ptr_combos.c    编译：gcc -std=c23 -O0 ptr_combos.c -o ptr_combos */
#include <stdio.h>

int main(void) {
    int a = 1, b = 2;

    const int *p = &a;          /* 底层 const：*p 不能改，p 能改 */
    printf("*p = %d\n", *p);
    p = &b;
    printf("p = &b 之后 *p = %d\n", *p);

    int * const q = &a;         /* 顶层 const：*q 能改，q 不能改 */
    *q = 10;
    printf("*q = 10 之后 a = %d\n", a);

    const int * const r = &b;   /* 两层都有：只能读 */
    printf("*r = %d\n", *r);
    return 0;
}
```

**四个失败例子的完整源码见第 3.3 小节**，文件名分别是
`err_target_const.c`、`err_const_ptr_assign.c`、`err_const_ptr_inc.c`、`err_both_const.c`。

`Bash`

```bash
gcc -std=c23 ptr_combos.c -o ptr_combos && ./ptr_combos
gcc -std=c23 err_target_const.c -o t      # 失败
gcc -std=c23 err_const_ptr_assign.c -o t  # 失败
gcc -std=c23 err_const_ptr_inc.c -o t     # 失败
gcc -std=c23 err_both_const.c -o t        # 失败
```

`实测数据`
`Text`

```text
# ptr_combos
*p = 1
p = &b 之后 *p = 2
*q = 10 之后 a = 10
*r = 2

# 四个失败例子的报错关键词
read-only location '*p'
read-only variable 'p'
increment of read-only variable 'p'
read-only location '*(const int *)p'  +  read-only variable 'p'
```

**C++ 用同一份源码（扩展名改成 `.cpp`）得到措辞相同的报错**，
只有语法细节的写法不同（`* p`、`const int*`）。

## A.5 顶层 `const` 与函数类型

`C`

```c
/* param_toplevel.c    编译：gcc -std=c23 -Wall param_toplevel.c -o param_toplevel */
#include <stdio.h>

int twice(const int x);                 /* 顶层 const */
int twice(int x) { return 2 * x; }      /* 同一个函数 */

int main(void) {
    printf("twice(21) = %d\n", twice(21));
    return 0;
}
```

`C`

```c
/* param_lowlevel.c    编译：gcc -std=c23 param_lowlevel.c -o param_lowlevel （失败） */
int f(const int *p);                    /* 底层 const：类型不同 */
int f(int *p) { return p ? *p : 0; }
int main(void) { return f((int *)0); }
```

`C`

```c
/* ret_qual.c    编译：gcc -std=c23 -Wall -Wextra ret_qual.c -o ret_qual */
const int five(void) { return 5; }
int main(void) { return five(); }
```

`Bash`

```bash
gcc -std=c23 -Wall param_toplevel.c -o param_toplevel && ./param_toplevel
# twice(21) = 42

gcc -std=c23 param_lowlevel.c -o param_lowlevel
# error: conflicting types for 'f'; have 'int(int *)'
# note: previous declaration of 'f' with type 'int(const int *)'

gcc -std=c23 -Wall -Wextra ret_qual.c -o ret_qual
# warning: type qualifiers ignored on function return type [-Wignored-qualifiers]

g++ -std=c++17 param_toplevel.cpp -o param_toplevel && ./param_toplevel
# twice(21) = 42（顶层 const 被忽略）
g++ -std=c++17 param_lowlevel.cpp -o param_lowlevel
# 通过：两个声明在 C++ 里是两个重载
```

## A.6 `const` 与数组、`argv`

`C`

```c
/* arr_const.c    编译：gcc -std=c23 arr_const.c -o arr_const （失败） */
int main(void) {
    const int a[3] = {1, 2, 3};
    a[0] = 9;               /* error: assignment of read-only location 'a[0]' */
    return 0;
}
```

`C`

```c
/* arr_assign.c    编译：gcc -std=c23 arr_assign.c -o arr_assign （失败） */
int main(void) {
    int a[3] = {1, 2, 3};
    int b[3] = {4, 5, 6};
    a = b;                  /* error: assignment to expression with array type */
    return 0;
}
```

`C`

```c
/* argv_element.c    编译：gcc -std=c23 argv_element.c -o argv_element （失败） */
int main(int argc, char *const argv[]) {
    if (argc > 0) {
        argv[0] = "x";      /* error: assignment of read-only location '*argv' */
    }
    return 0;
}
```

`C`

```c
/* arr_param.c    编译：gcc -std=c23 arr_param.c -o arr_param */
#include <stdio.h>

static int sum3(const int a[3])      { return a[0] + a[1] + a[2]; }
static int sum_ptr(const int *a)     { return a[0] + a[1] + a[2]; }
static int sum_sugar(const int a[])  { return a[0] + a[1] + a[2]; }

int main(void) {
    const int v[3] = {1, 2, 3};
    int w[3] = {4, 5, 6};
    printf("%d %d %d\n", sum3(v), sum_ptr(v), sum_sugar(v));
    printf("%d\n", sum_ptr(w));
    return 0;
}
```

**`argv` 的三种写法都编译过且运行正常。**

`C`

```c
/* argv_forms.c    编译：gcc -std=c23 -Wall -Wextra argv_forms.c -o argv_forms */
#include <stdio.h>

/* 只列三种 form 的声明，三份源码分别编译，实测见下 */

int main(int argc, char *argv[]) {              /* 形式一：标准写法 */
    printf("argc = %d\n", argc);
    return 0;
}
```

`Bash`

```bash
gcc -std=c23 -Wall -Wextra argv_forms.c -o argv_forms && ./argv_forms A B
# argc = 3
# 把参数换成 char *const argv[] 与 const char *const argv[]：同样通过，没有警告
```

## A.7 C 与 C++ 的常量表达式

`C`

```c
/* vla_file.c    编译：gcc -std=c23 -c vla_file.c -o vla_file.o （失败） */
const int n = 5;
int a[n];               /* error: variably modified 'a' at file scope */
```

`C++`

```cpp
// vla_ok.cpp    编译：g++ -std=c++17 vla_ok.cpp -o vla_ok
#include <cstdio>

int main() {
    const int n = 5;        // 常量初始化 → 常量表达式
    int a[n];               // 合法
    std::printf("sizeof(a) = %zu\n", sizeof(a));
    return 0;
}
```

`C++`

```cpp
// vla_err.cpp    编译：g++ -std=c++17 -pedantic-errors vla_err.cpp -o vla_err （失败）
int get() { return 5; }

int main() {
    const int m = get();    // 运行期初始化 → 不是常量表达式
    int b[m];               // 默认静默，-Wvla 给警告，-pedantic-errors 给错误
    return b[0];
}
```

**C23 也引入了 `constexpr`（语法与 C++ 不同），本机 `gcc` 15.2.0 已支持。**

`C`

```c
/* c23_constexpr.c    编译：gcc -std=c23 -O0 c23_constexpr.c -o c23_constexpr */
#include <stdio.h>

int main(void) {
    constexpr int n = 5;    /* C23 起也有关键字 constexpr */
    int a[n];
    printf("sizeof(a) = %zu\n", sizeof(a));
    return 0;
}
```

`Bash`

```bash
gcc -std=c23 -O0 -Wvla vla.c -o vla && ./vla
# warning: ISO C90 forbids variable length array 'a' [-Wvla]
# sizeof(a) = 20, sizeof(b) = 20

gcc -std=c23 -c vla_file.c -o vla_file.o
# error: variably modified 'a' at file scope

g++ -std=c++17 vla_ok.cpp -o vla_ok && ./vla_ok
# sizeof(a) = 20

g++ -std=c++17 -Wvla vla_err.cpp -o vla_err
# warning: variable length array 'b' is used [-Wvla]
g++ -std=c++17 -pedantic-errors vla_err.cpp -o vla_err
# error: ISO C++ forbids variable length array 'b' [-Wvla]

gcc -std=c23 -O0 c23_constexpr.c -o c23_constexpr && ./c23_constexpr
# sizeof(a) = 20
```

## A.8 文件作用域 `const` 的链接属性

`Bash`

```bash
gcc -std=c23 -c link_c.c -o link_c.o && nm link_c.o --defined-only
# 0000000000000000 R cval

g++ -std=c++17 -c link_cpp.cpp -o link_cpp.o && nm link_cpp.o --defined-only
# 0000000000000000 r _ZL4cval

gcc -std=c23 link_use.c link_c.c -o link_use && ./link_use
# cval = 5

g++ -std=c++17 link_use.cpp link_cpp.cpp -o link_use
# undefined reference to `cval'

g++ -std=c++17 link_use.cpp link_fixed.cpp -o link_fixed && ./link_fixed
# cval = 5
```

## A.9 丢弃 `const` 限定

`C`

```c
/* discard_arg.c    编译：gcc -std=c23 -Wall discard_arg.c -o discard_arg */
#include <stdio.h>

static int my_len(char *s) {        /* 参数少了 const */
    int n = 0;
    while (s[n] != 0) ++n;
    return n;
}

int main(void) {
    const char *a = "abcd";
    printf("%d %d\n", my_len(a), my_len("hello"));
    return 0;
}
```

`Bash`

```bash
gcc -std=c23 -Wall discard.c -o discard && ./discard
# warning: initialization discards 'const' qualifier from pointer target type
# *q = 1

gcc -std=c23 -Wall discard_arg.c -o discard_arg
# warning: passing argument 1 of 'my_len' discards 'const' qualifier
#          from pointer target type [-Wdiscarded-qualifiers]

g++ -std=c++17 discard.cpp -o discard
# error: invalid conversion from 'const int*' to 'int*' [-fpermissive]

g++ -std=c++17 -Wall discard_arg.cpp -o discard_arg
# error: invalid conversion from 'const char*' to 'char*' [-fpermissive]
```

## A.10 `const` 对象必须有初值

`Bash`

```bash
gcc -std=c23 -Wall noinit.c -o noinit
# warning: 'p' is used uninitialized [-Wuninitialized]

g++ -std=c++17 noinit.cpp -o noinit
# error: uninitialized 'const p' [-fpermissive]
```

## A.11 C++ 的 `const` 引用

`C++`

```cpp
// ref_nocopy.cpp    编译：g++ -std=c++17 -O0 ref_nocopy.cpp -o ref_nocopy
#include <cstdio>

struct Big { int data[64]; };       // 256 字节

void by_value(Big b) {
    std::printf("by_value : &b = %p\n", (void *)&b);
}

void by_cref(const Big &b) {
    std::printf("by_cref  : &b = %p\n", (void *)&b);
}

int main() {
    Big x{};
    std::printf("sizeof(Big) = %zu\n", sizeof(Big));
    std::printf("main     : &x = %p\n", (void *)&x);
    by_value(x);
    by_cref(x);
    return 0;
}
```

`Bash`

```bash
g++ -std=c++17 ref_bind.cpp -o ref_bind && ./ref_bind
# r = 5
# cr = 2
# s = 临时对象, 长度 12

g++ -std=c++17 ref_err.cpp -o ref_err
# error: cannot bind non-const lvalue reference of type 'int&' to an rvalue of type 'int'

g++ -std=c++17 -O0 ref_nocopy.cpp -o ref_nocopy && ./ref_nocopy
# sizeof(Big) = 256
# main     : &x = 0000006e50fffbb0
# by_value : &b = 0000006e50fffab0
# by_cref  : &b = 0000006e50fffbb0
```

## A.12 参数里的 `const` 带来的差别

`C`

```c
/* len_const.c    编译：gcc -std=c23 -Wall len_const.c -o len_const */
#include <stdio.h>

static int my_len(const char *s) {      /* 参数带 const：三种实参都收 */
    int n = 0;
    while (s[n] != 0) ++n;
    return n;
}

int main(void) {
    const char *a = "abcd";
    char b[] = "xyz";
    printf("my_len(a) = %d, my_len(b) = %d, my_len(字面量) = %d\n",
           my_len(a), my_len(b), my_len("hello"));
    return 0;
}
```

`Bash`

```bash
gcc -std=c23 -Wall len_const.c -o len_const && ./len_const
# my_len(a) = 4, my_len(b) = 3, my_len(字面量) = 5
```

`实测数据`
`Text`

```text
# 参数不带 const 时（源码见 A.9 的 discard_arg.c）
C  ：warning: passing argument 1 of 'my_len' discards 'const' qualifier
              from pointer target type [-Wdiscarded-qualifiers]
C++：error: invalid conversion from 'const char*' to 'char*' [-fpermissive]
```

---

# 附录 B 相关文档

| 文档 | 关系 |
|---|---|
| 《04-语法/02-数据类型与类型系统.md》第 3 节 | **前置**：`const`、`volatile`、`restrict` 等限定符 |
| 《04-语法/04-表达式与运算符.md》第 2.2 小节 | **前置**：左值与右值，判断能不能绑定引用 |
| 《04-语法/08-数组、指针与引用.md》第 2.2 小节 | **前置**：数组退化，解释数组为何不能整体赋值 |
| 《04-语法/08-数组、指针与引用.md》第 3.3 小节 | **前置**：引用就是指针，底层机制 |
| 《04-语法/08-数组、指针与引用.md》第 4.3 小节 | **前置**：声明的读法 |
| 《04-语法/11-作用域、生存期与链接.md》第 5 节 | **后续**：`const` 全局变量链接属性的完整讨论 |
| 《04-语法/12-编译期能力.md》第 2 节 | **后续**：`const` 不等于编译期常量，C++ 的 `constexpr` |
| 《04-语法/12-编译期能力.md》第 6 节 | **后续**：C 里的编译期手段 |
| 《04-语法/05-初始化.md》第 1 节 | 相关：初始化与赋值的区别 |
| 《04-语法/07-函数.md》第 3 节 | 相关：参数按值传递，`const T&` 传参不拷贝的依据 |
| 《04-语法/07-函数.md》第 5.3 小节 | 相关：重载，`const int *` 与 `int *` 在 C++ 里是两个重载 |
| 《04-语法/10-字符串.md》第 2.2 小节 | 相关：字符串字面量在只读段，改它会崩 |
| 《04-语法/14-预处理器.md》第 2 节 | 相关：宏是文本替换，没有类型 |
| 《01-编译器/00-语言的实现.md》第 6.4 小节 | 相关：`const` 链接属性的符号层证据 |
| 《00-写在最初/01-C与C++.md》第 2.3.4 小节 | 相关：同一段代码在两门语言里行为不同 |
