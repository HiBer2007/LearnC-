# 结构体、联合体与 enum

> **授权**：本章节属教材文档部分，采用 [CC BY-NC-ND 4.0](../LICENSE)
> 加[附加条款](../许可附加条款.md)授权：署名、非商业、禁止演绎、不允许二次分发。
> 文中引用的代码不受此限，各自保留原有许可证，详见[版权与许可说明.md](../版权与许可说明.md)。

**数组要求所有元素同类型。可现实中经常要把不同类型的东西捆在一起**——
一个坐标有两个数，一条协议记录有长度、类型和载荷。

**结构体、联合体与 enum 就是三种「把东西捆起来」的方式**，
捆法各不相同：

| 关键字 | 捆法 |
|---|---|
| `struct` | **并排**放，各占各的位置 |
| `union` | **叠着**放，共用同一块内存 |
| `enum` | 不捆数据，**给一组整数起名字** |

---

> **约定**：标注以行内代码单独成行——`实测数据` 表示实际执行验证过，
> 完整数据与复现命令见附录 A；`文档` 表示引自标准或官方资料；
> `待确认` 表示尚未验证。路径占位符的含义见《README.md》。

## 本章节定位

| 需要先知道 | 在哪 |
|---|---|
| 类型与宽度 | 《04-语法/02-数据类型与类型系统.md》第 2.6 小节 |
| 初始化（含指定初始化器） | 《04-语法/05-初始化.md》第 5 节 |
| 指针与 `&`、`*` | 《04-语法/08-数组、指针与引用.md》第 1 节 |

**相邻的章节**：`10-字符串`（字符串常放在结构体里）、
`11-作用域、生存期与链接`（结构体的存储）。

**结构体在内存里的布局**（填充、对齐、ABI 影响）
见《01-编译器/00-语言的实现.md》第 5.5 小节，本章不重复。

---

# 第 1 节 结构体

## 1.1 定义与使用

`C`

```c
/* struct_basic.c    编译：gcc -std=c23 struct_basic.c -o struct_basic */
#include <stdio.h>

struct Point {              /* 定义一种新类型 */
    int x;
    int y;
};

int main(void) {
    struct Point p = { 1, 2 };      /* 定义变量 */
    printf("p = (%d, %d)\n", p.x, p.y);
    printf("sizeof(struct Point) = %zu\n", sizeof(struct Point));
    return 0;
}
```

**输出**：`p = (1, 2)` 与 `sizeof(struct Point) = 8`

**`struct Point` 是一个类型名**，与 `int` 的地位相同，
只是它由两个 `int` 组成。

## 1.2 C 与 C++ 的一处分歧：要不要写 `struct`

**这是两种语言在写法上最显眼的差别之一。**

`实测数据`
`C`

```c
struct Point p = { 1, 2 };      /* C：必须写 struct */
Point p = { 1, 2 };             /* C：编译失败 */
```

`C++`

```cpp
Point p = { 1, 2 };             // C++：直接写类型名
```

C23 下不写 `struct` 会报
`unknown type name 'Point'`。

> [!IMPORTANT]
> **C 里 `struct Point` 是一个整体，`Point` 单独不存在。**
> C++ 把标签名直接提升成了类型名，因此可以省略 `struct`。
> **反过来，C++ 里写 `struct Point` 也完全合法**，
> 所以「两种语言都能用」的写法是带上 `struct`。

**在 C 里想省掉 `struct`，用 `typedef`**：

`C`

```c
typedef struct Point {
    int x;
    int y;
} Point;                    /* 现在 Point 是一个类型名 */

Point p = { 1, 2 };         /* 可以这样写了 */
```

**做法与《04-语法/02-数据类型与类型系统.md》第 4 节讲的 `typedef` 三步一致**：
写一个变量声明，把变量名换成类型名，前面加 `typedef`。

## 1.3 整体赋值可以，整体比较不行

`实测数据`

| 操作 | C23 | C++17 |
|---|---|---|
| `b = a;`（整体赋值） | **通过** | **通过** |
| `a == b;`（整体比较） | **失败**：`invalid operands to binary ==` | **失败**：`no match for 'operator=='` |

**为什么赋值可以比较不行**：

| | 能不能做 | 为什么 |
|---|---|---|
| 赋值 | 能 | 编译器知道每个成员在哪，逐个复制即可 |
| 比较 | **不能** | 有填充字节，而填充字节的值是不确定的 |

> [!WARNING]
> **不要用 `memcmp` 比较两个结构体。**
> 填充字节里可能是任何值，两个「逻辑上相等」的结构体
> `memcmp` 结果可能不为 0。
> **逐个成员比较**，或者（C++ 里）自己写 `operator==`。

## 1.4 `.` 与 `->`

`实测数据`
`C`

```c
/* arrow.c    编译：gcc -std=c23 arrow.c -o arrow */
#include <stdio.h>

struct Point { int x; int y; };

int main(void) {
    struct Point p = { 1, 2 };
    struct Point *q = &p;

    printf("p.x    = %d   （对象用 .）\n", p.x);
    printf("q->x   = %d   （指针用 ->）\n", q->x);
    printf("(*q).x = %d   （-> 就是「先解引用再取成员」）\n", (*q).x);
    return 0;
}
```

**三行输出都是 1。**

**两种写法编译出来完全一样**：

`实测数据`
`C`

```c
/* arrow_same.c    编译：gcc -std=c23 -O0 -S arrow_same.c -o arrow_same.s */
struct Point { int x; int y; };

int via_arrow(struct Point *q) { return q->x; }        /* 用 -> */
int via_deref(struct Point *q) { return (*q).x; }      /* 用 (*q).x */
```

`实测数据`
`Assembly`

```asm
via_arrow:
    pushq   %rbp
    movq    %rsp, %rbp
    movq    %rcx, 16(%rbp)
    movq    16(%rbp), %rax
    movl    (%rax), %eax
    popq    %rbp
    ret
```

**`via_deref` 与它逐条相同。**

**为什么那对括号不能省**：

`Text`

```
   q->x    等价于   (*q).x
   *q.x    先算 .   得到 *(q.x)      ← 不是想要的意思
   q.x     本身就是错的
```

**两种错法报的是同一条错误**：

`实测数据`
`Text`

```text
q.x  → error: 'q' is a pointer; did you mean to use '->'?
*q.x → error: 'q' is a pointer; did you mean to use '->'?
```

**`q` 是指针，它没有成员**；而 `*q.x` 里 `.` 的优先级高于 `*`，
编译器先算 `q.x`，于是撞上同一条错误。

> [!IMPORTANT]
> **`->` 存在的唯一理由就是省掉那对括号。**
> `q->x` 与 `(*q).x` 含义相同、编译结果相同，**它没有别的作用。**

**这是语法糖的一个典型例子**（见《04-语法/01-一些基础概念.md》第 1.5 小节）。

## 1.5 成员顺序影响大小

**这一条在《01-编译器/00-语言的实现.md》第 5.5 小节有完整推导，
这里只给出实测数字。**

`C`

```c
/* padding.c    编译：gcc -std=c23 padding.c -o padding */
#include <stdio.h>
#include <stddef.h>

struct A { char c; int i; char d; };    /* 原样 */
struct B { int i; char c; char d; };    /* 大的在前 */

int main(void) {
    printf("struct A { char; int; char; } = %zu 字节\n", sizeof(struct A));
    printf("struct B { int; char; char; } = %zu 字节\n", sizeof(struct B));
    printf("A 各成员偏移: c=%zu i=%zu d=%zu\n",
           offsetof(struct A, c), offsetof(struct A, i), offsetof(struct A, d));
    return 0;
}
```

`实测数据`
`Text`

```text
struct A { char; int; char; } = 12 字节
struct B { int; char; char; } = 8 字节
A 各成员偏移: c=0 i=4 d=8
```

**成员完全相同，只是换了顺序，就差 4 字节。**

`实测数据`

| 字节偏移 | `struct A { char c; int i; char d; }` | `struct B { int i; char c; char d; }` |
|---|---|---|
| 0 | `c` | `i` 的第 1 字节 |
| 1–3 | **填充**（把 `i` 对齐到 4） | `i` 的第 2–4 字节 |
| 4 | `i` 的第 1 字节 | `c` |
| 5 | `i` 的第 2 字节 | `d` |
| 6 | `i` 的第 3 字节 | **填充**（补到 4 的倍数） |
| 7 | `i` 的第 4 字节 | **填充** |
| 8 | `d` | —— |
| 9–11 | **填充**（整个结构体补到 4 的倍数） | —— |
| 合计 | **12 字节** | **8 字节** |

**`A` 比 `B` 多出来的 4 字节全是填充**：为了让 `int` 落在 4 的倍数上，
`c` 后面垫了 3 字节；结构体整体又要补到 4 的倍数，`d` 后面再垫 3 字节。

> [!TIP]
> **结构体成员按「宽的在前、窄的在后」排列**，
> 通常能省下填充字节。
> **代价是成员顺序不再反映逻辑关系**，可读性会下降——
> 只在内存紧张时这么做，并且写清注释。

---

# 第 2 节 联合体

**联合体要解决的问题是：同一块内存，常常需要被当成不同的东西来看。**

**C 诞生的年代，内存以 KB 计。** 一个 16 位寄存器既可能是一个整数，
也可能是两个字符、四个标志位、半条指令。
**每种解释都单独存一份，内存立刻翻几倍。**
`union` 让「同一块存储的多种解释」这件事**能被明确写出来**，
而不是靠指针强转去蒙。

**它还有第二个身份：C 里表达「几种可能之一」的手段。**
一条消息可能是登录，也可能是心跳，两者不会同时出现；
把两者叠在同一块内存上，再配一个标签说明当前是哪一种
（见第 2.2 小节的「带标签的联合体」）。

> [!IMPORTANT]
> **`union` 的语义是「同一块存储的多种视图」，省空间只是它的结果。**
> 把它当成省空间的小技巧就会用错——**它的重点是「重叠」这件事本身**。

## 2.1 成员共享同一块内存

**`union` 与 `struct` 只差一件事：成员的起始偏移。**
`struct` 的成员依次排开，`union` 的成员**全部从偏移 0 开始**——
**它们互相重叠，占的是同一块内存。**

`实测数据`

| | `struct` | `union` |
|---|---|---|
| 成员的起始偏移 | 依次排开：0、4、8…… | **全部是 0** |
| 大小 | 各成员之和，再加填充 | **最大成员的大小**，再按对齐补足 |
| 成员的地址 | 各不相同 | **全都相同**，等于联合体自己的地址 |
| 给一个成员赋值 | 不影响其他成员 | **覆盖其他成员占的那几个字节** |
| 典型用途 | 把相关的数据捆在一起 | 同一块内存的多种解释、省空间 |

**下面这段程序把前四条都测了出来**——
两个类型的成员完全相同，只是一个是 `struct`、一个是 `union`：

`实测数据`
`C`

```c
/* union_share.c    编译：gcc -std=c23 union_share.c -o union_share
 * 完整文件见附录 A.7。
 */
#include <stddef.h>
#include <stdio.h>

union U  { unsigned int u; unsigned char b[4]; float f; };
struct S { unsigned int u; unsigned char b[4]; float f; };

int main(void) {
    union U v;
    struct S s;

    printf("sizeof(union U)  = %zu\n", sizeof(union U));
    printf("sizeof(struct S) = %zu\n", sizeof(struct S));

    printf("&v.u = %p, &v.b = %p, &v.f = %p\n",
           (void *)&v.u, (void *)&v.b, (void *)&v.f);
    printf("&s.u = %p, &s.b = %p, &s.f = %p\n",
           (void *)&s.u, (void *)&s.b, (void *)&s.f);

    v.u = 0x11223344;
    v.b[0] = 0xFF;                      /* 只改一个字节 */
    printf("写 v.b[0] 之后 v.u = 0x%08X\n", v.u);

    s.u = 0x11223344;
    s.b[0] = 0xFF;
    printf("写 s.b[0] 之后 s.u = 0x%08X\n", s.u);
    return 0;
}
```

`实测数据`
`Text`

```text
sizeof(union U)  = 4
sizeof(struct S) = 12

&v.u = 000000af6b9ff7cc, &v.b = 000000af6b9ff7cc, &v.f = 000000af6b9ff7cc
&s.u = 000000af6b9ff7c0, &s.b = 000000af6b9ff7c4, &s.f = 000000af6b9ff7c8

写 v.b[0] 之后 v.u = 0x112233FF
写 s.b[0] 之后 s.u = 0x11223344
```

**联合体的三个成员地址完全相同**：每个成员都从这块内存的开头算起。
**结构体的三个地址依次相差 4**：成员各占一段，互不重叠。
**每次运行的地址都不同，看的是「相同还是不同」。**

**只把 `v.b[0]` 改成一个字节，`v.u` 就跟着变了**（`0x11223344` → `0x112233FF`）——
**因为它俩本来就是同一块内存上的两个名字**。
结构体那边写 `s.b[0]`，`s.u` 一点没动。

> [!IMPORTANT]
> **「共享」的准确含义是：所有成员的起始偏移都是 0。**
> 给一个成员赋值会**覆盖**其他成员所在的那几个字节，
> **不是「各自保留一份值」。**
> **`union` 在同一时刻只有一个成员的内容是有意义的。**

## 2.2 联合体用来做什么

**用途一：让同一块内存有多种解释。**

`实测数据`
`C`

```c
/* union_basic.c    编译：gcc -std=c23 union_basic.c -o union_basic */
#include <stdio.h>

union U {
    unsigned int  u;            /* 4 字节 */
    unsigned char b[4];         /* 4 字节 */
    float         f;            /* 4 字节 */
};

int main(void) {
    union U v;
    printf("sizeof(union U) = %zu  （取最大的成员）\n", sizeof(union U));

    v.u = 0x41424344;
    printf("v.u = 0x%08X\n", v.u);
    printf("v.b = %02X %02X %02X %02X   （同一块内存，按字节看）\n",
           v.b[0], v.b[1], v.b[2], v.b[3]);

    v.f = 1.0f;
    printf("v.f = %g 时 v.u = 0x%08X   （浮点的位模式）\n", v.f, v.u);
    return 0;
}
```

`实测数据`
`Text`

```text
sizeof(union U) = 4  （取最大的成员）
v.u = 0x41424344
v.b = 44 43 42 41   （同一块内存，按字节看）
v.f = 1 时 v.u = 0x3F800000   （浮点的位模式）
```

**三件事同时看出来了**：

**第一，`union` 的大小是所有成员里最大的那个**，不是它们的和。

**第二，`v.b` 的顺序是 `44 43 42 41`，与写出来的 `0x41424344` 相反**——
这说明本机是**小端序**（低位字节放在低地址）
（字节序见《01-编译器/03-嵌入式与交叉编译.md》第 4 节）。

**第三，`1.0f` 的位模式是 `0x3F800000`**——
浮点数在内存里不是「1」这个值，而是这样一串比特
（见《04-语法/02-数据类型与类型系统.md》第 2.3 小节）。

**用途二：节省空间——同一时刻只会用到其中一个成员。**

`实测数据`
`C`

```c
/* tagged_union.c    编译：gcc -std=c23 tagged_union.c -o tagged_union */
#include <stdio.h>

struct Value {
    int type;                   /* 0 = int，1 = float */
    union {
        int   i;
        float f;
    } data;                     /* 只占最大的那个成员的空间 */
};

int main(void) {
    struct Value a = { 0, { .i = 42 } };
    struct Value b = { 1, { .f = 1.5f } };

    printf("sizeof(struct Value) = %zu\n", sizeof(struct Value));
    printf("a.type = %d, a.data.i = %d\n", a.type, a.data.i);
    printf("b.type = %d, b.data.f = %g\n", b.type, b.data.f);
    return 0;
}
```

**`type` 决定该读哪个成员**——这叫「带标签的联合体」，
是 C 里表达「几种可能之一」的常用手法。

`实测数据`
`Text`

```text
sizeof(struct Value) = 8
a.type = 0, a.data.i = 42
b.type = 1, b.data.f = 1.5
```

> [!CAUTION]
> **读联合体里「不是最后写入的那个成员」是未定义行为**（C++ 里明确规定）。
> 唯一的例外是「共同初始序列」这种特殊情况。
> **用联合体做类型双关（type punning）在 C 里是常见做法，
> 但它不属于可移植代码**——依赖字节序与对齐。
> **需要按字节看一个值，用 `memcpy` 更安全。**

## 2.3 匿名联合体

**成员可以直接访问，不用写中间那层名字。**

`实测数据`
`C`

```c
/* anonymous.c    编译：gcc -std=c23 anonymous.c -o anonymous */
#include <stdio.h>

struct Var {
    int type;
    union {                     /* 匿名联合体 */
        int   i;
        float f;
    };
};

int main(void) {
    struct Var v;
    v.type = 1;
    v.i = 42;                   /* 不用写 v.data.i */
    printf("v.i = %d\n", v.i);
    return 0;
}
```

**C23 与 C++17 都通过，输出 `v.i = 42`。**

---

# 第 3 节 `enum`

## 3.1 它解决什么问题

**没有 `enum` 的时候，一组相关的常量只能这样写**：

`C`

```c
/* define_color.c    编译：gcc -std=c23 define_color.c -o define_color */
#include <stdio.h>

#define RED   0
#define GREEN 1
#define BLUE  2

int main(void) {
    int c = GREEN;
    printf("c = %d\n", c);      /* 只有数字，没有名字 */
    return 0;
}
```

**或者干脆直接写数字。这样写有三个问题**：

| 问题 | 说明 |
|---|---|
| 常量之间没有联系 | 编译器不知道 `RED` 与 `GREEN` 属于同一组 |
| 类型上看不出意图 | `void set_color(int c)` 里那个 `int` 什么都能接 |
| 调试信息里没有名字 | 断点停下时只看到 `1`，看不到 `GREEN` |

**第三件事可以直接查**——把两种写法都加 `-g` 编译，再看调试信息：

`实测数据`
`Bash`

```bash
gcc -std=c23 -g enum_dwarf.c   -o enum_dwarf.exe
gcc -std=c23 -g define_dwarf.c -o define_dwarf.exe
objdump --dwarf=info enum_dwarf.exe   | grep -E ":( +)(RED|GREEN|BLUE|Color)$"
objdump --dwarf=info define_dwarf.exe | grep -E ":( +)(RED|GREEN|BLUE|Color)$"
```

`实测数据`
`Text`

```text
enum 版：
    DW_AT_name        : Color
    DW_AT_name        : RED
    DW_AT_name        : GREEN
    DW_AT_name        : BLUE

#define 版：
    （一个都没有）
```

**`#define` 在预处理阶段就被替换掉了，名字根本进不了目标文件**；
`enum` 会生成一个真正的**类型**，成员名与取值一起写进调试信息
（源码见附录 A.8）。

**这就是 `enum` 的意图**：给一组相关常量一个**共同的名字**，
让它们成为一个**类型**，并且**把名字保留到调试信息里**。

`实测数据`
`C`

```c
/* enum_basic.c    编译：gcc -std=c23 enum_basic.c -o enum_basic */
#include <stdio.h>

enum Color { RED, GREEN, BLUE };    /* 从 0 开始，依次加一 */

int main(void) {
    enum Color c = GREEN;
    printf("RED=%d GREEN=%d BLUE=%d\n", RED, GREEN, BLUE);
    printf("c = %d\n", c);
    return 0;
}
```

**输出**：`RED=0 GREEN=1 BLUE=2` 与 `c = 1`

**它做到了什么、没做到什么，要看清楚**：

| | 做到了 | 没做到 |
|---|---|---|
| 名字 | 一组常量有了共同的名字 `enum Color` | —— |
| 类型 | 可以写 `enum Color c`，函数签名里看得出要什么 | C 里挡不住 `int` 混进来 |
| 调试 | 调试信息里保留 `RED`、`GREEN`、`BLUE` | —— |
| 取值 | —— | **不检查**：C 里 `enum Color c = 5;` 合法 |

**「不检查取值」不是疏忽，而是 C 的取向**：
`enum` 属于**整型家族**，底层就是一个整数类型（具体用哪个由实现选），
因此可以自由地与 `int` 互转。
**C++ 保留了这个取向**（只关掉了 `int` → `enum` 的隐式转换，见第 3.2 小节），
**要真正的独立类型得用 `enum class`**（第 3.3 小节）。

**另一件常被忽略的事**：`enum` 的成员是**编译期常量**，
因此能当数组长度、能当 `case` 标签（见《04-语法/06-控制流语句.md》第 2 节）。

**可以指定值**：

`C`

```c
enum Status { OK = 200, NOT_FOUND = 404, ERROR = 500 };
enum Flags  { A = 1, B = 2, C = 4, D = 8 };     /* 当位标志用 */
```

## 3.2 C 与 C++ 的分歧

`实测数据`

| 写法 | C23 | C++17 |
|---|---|---|
| `enum E e = A; int x = e;`（枚举转 `int`） | 通过 | 通过 |
| `int x = 1; enum E e = x;`（**`int` 转枚举**） | **通过** | **失败**：`invalid conversion from 'int'` |
| 两个枚举有同名成员 | 失败 | 失败 |
| `enum E { BIG = 5000000000 };` | 通过 | 通过 |

**只有一行不同**：**C 允许把 `int` 直接赋给枚举变量，C++ 不允许。**

> [!WARNING]
> **C 里 `enum E e = 5;` 能编过，即使 5 不是任何枚举值。**
> 枚举在 C 里只是「整数的别名」，**没有任何取值检查**。
> C++ 关掉了这条隐式转换，但仍可以用 `static_cast` 强行转。

**这一点让 C 的 `enum` 几乎没有约束力**：
它省下的是「记住 200 表示成功」这件事，而不是「防止赋错值」。

## 3.3 `enum class`：C++ 的强类型枚举

**C++11 起提供了一个真正有约束的版本。**

`C++`

```cpp
// enum_class.cpp    编译：g++ -std=c++17 enum_class.cpp -o enum_class
#include <cstdio>

enum class Color { Red, Green, Blue };

int main() {
    Color c = Color::Red;
    std::printf("%d\n", (int)c);        // 要显式转换才能当整数用
    return 0;
}
```

`实测数据`

| 写法 | 结果 |
|---|---|
| `Color c = Color::Red;` | 通过 |
| `int x = c;` | **失败**：`cannot convert 'Color' to 'int' in initialization` |
| `Color d = 0;` | **失败**：`cannot convert 'int' to 'Color' in initialization` |
| `(int)c` | 通过（显式转换可以） |

**`enum class` 与普通 `enum` 的区别**：

| | `enum` | `enum class` |
|---|---|---|
| 名字作用域 | **泄漏到外层**（`RED` 直接可用） | **收在枚举名里**（`Color::Red`） |
| 隐式转 `int` | 可以 | **不可以** |
| 从 `int` 转 | 可以 | **不可以** |
| 两个枚举同名成员 | **冲突** | 不冲突 |

> [!IMPORTANT]
> **`enum class` 解决的是「枚举值泄漏成全局名字」与「悄悄当成整数用」这两个问题。**
> 上表最后一行尤其明显：普通 `enum` 里两个枚举不能有同名成员，
> 而 `enum class` 可以——因为名字被收在各自的枚举名里。

**C 没有 `enum class`**，因此 C 代码里只能用普通 `enum` 加命名前缀
（`COLOR_RED`、`COLOR_GREEN`）来避免名字冲突。

---

# 第 4 节 位域

**位域要解决的问题是：硬件寄存器与协议字段是按「位」划分的，不是按字节。**

**一份典型的寄存器手册会这样写**：

| 位 | 名字 | 含义 |
|---|---|---|
| 31 | `EN` | 使能 |
| 30 | `IRQ` | 中断允许 |
| 29–16 | `MODE` | 模式 |
| 15–0 | `ADDR` | 地址 |

**没有位域时，只能手工移位与掩码**：

`C`

```c
/* manual_bits.c    编译：gcc -std=c23 manual_bits.c -o manual_bits */
#include <stdint.h>
#include <stdio.h>

void set_mode(uint32_t *ctrl, uint32_t mode) {
    *ctrl = (*ctrl & ~(0x3FFFu << 16)) | ((mode & 0x3FFFu) << 16);
}

int main(void) {
    uint32_t ctrl = 0;
    set_mode(&ctrl, 3);
    printf("ctrl = 0x%08X\n", ctrl);     /* 0x00030000 */
    return 0;
}
```

**每改一个字段，都要算一次移位量、对一次掩码**，
**而手册上写的只是「第 29~16 位是 `MODE`」——两边对不上，就容易写错。**

**位域让布局可以照着手册写出来**：

`实测数据`
`C`

```c
/* ctrl_reg.c    编译：gcc -std=c23 ctrl_reg.c -o ctrl_reg */
#include <stdio.h>
#include <string.h>

struct Ctrl {
    unsigned int addr : 16;         /* 15..0  */
    unsigned int mode : 14;         /* 29..16 */
    unsigned int irq  : 1;          /* 30     */
    unsigned int en   : 1;          /* 31     */
};

int main(void) {
    struct Ctrl c;
    memset(&c, 0, sizeof c);
    c.en = 1;
    c.mode = 3;
    c.addr = 0x1234;

    unsigned char raw[4];
    memcpy(raw, &c, sizeof c);
    printf("sizeof = %zu\n", sizeof c);
    printf("高字节在前: %02X %02X %02X %02X\n", raw[3], raw[2], raw[1], raw[0]);
    return 0;
}
```

`实测数据`
`Text`

```text
sizeof = 4
高字节在前: 80 03 12 34
```

**`80 03 12 34` 与手册逐位对上了**：最高位是 `EN`，接着 `IRQ`，
然后 `MODE`，最后是 `ADDR`——**代码里写 `c.en = 1;`，就是手册上「第 31 位置 1」。**

> [!IMPORTANT]
> **位域的意图是：把「按位划分」从手工计算变成声明。**
> 布局写在结构体里，读代码的人不必再反推移位量与掩码。

**成员按声明顺序从低位往高位排**（本机实测）：

`实测数据`
`C`

```c
/* bitfield_bits.c    编译：gcc -std=c23 bitfield_bits.c -o bitfield_bits
 * 完整程序（含 S1、S2 的对照）见附录 A.9。
 */
#include <stdio.h>
#include <string.h>

struct Flags { unsigned int a:1, b:3, c:4; };   /* 共 8 位 */

static void show(const char *what, struct Flags f) {
    unsigned char raw[4];
    memcpy(raw, &f, sizeof f);
    printf("%-10s -> %02X %02X %02X %02X\n", what, raw[0], raw[1], raw[2], raw[3]);
}

int main(void) {
    struct Flags f = {0};       /* 先清零，剩下的位才是确定的 */
    f.a = 1;            show("a = 1", f);
    f.a = 0; f.b = 7;   show("b = 7", f);
    f.b = 0; f.c = 15;  show("c = 15", f);
    return 0;
}
```

`实测数据`
`Text`

```text
a = 1      -> 01 00 00 00      a 在第 0 位
b = 7      -> 0E 00 00 00      b 在第 1~3 位（0x0E = 0000 1110）
c = 15     -> F0 00 00 00      c 在第 4~7 位（0xF0 = 1111 0000）
```

**注意后三个字节全是 0**：8 个位域装进了一个 4 字节的存储单元，
**剩下的 24 位是没用到的填充**——这也是 `sizeof` 为 4 而不是 1 的原因。

## 4.1 代价：布局由实现决定

**标准只规定位域能表达，不规定怎么排。** 三件事都由实现自己定：

`实测数据`

| 问题 | 本机（gcc 15.2.0，Windows x64）实测 |
|---|---|
| 谁在高位 | **先声明的在低位**（`a`→第 0 位，`b`→第 1~3 位，`c`→第 4~7 位） |
| 能不能跨存储单元 | **不跨**：`{unsigned char a:6, b:6;}` 里 `b` 另起一个字节 |
| 存储单元多大 | 由声明类型决定：`unsigned int` 是 4 字节，`unsigned char` 是 1 字节 |

**「不跨单元」是有代价的**：

`实测数据`
`Text`

```text
S1: { unsigned char a:6, b:6; }   sizeof = 2
    只把 a 填满 -> 3F 00          a 独占第 1 个字节
    只把 b 填满 -> 00 3F          b 独占第 2 个字节

S2: { unsigned int a:20, b:20; }  sizeof = 8
    只把 a 填满 -> FF FF 0F 00 00 00 00 00     a 占前 4 字节的低 20 位
    只把 b 填满 -> 00 00 00 00 FF FF 0F 00     b 从第 2 个单元重新开始
```

**`S2` 的 40 位本来 5 字节就够，实测 `sizeof` 是 8**——
因为 `b` 不肯跨过 4 字节的边界，另起了一个单元。

> [!WARNING]
> **位域的布局不可移植。**
> 换一个编译器、换一个平台，上面这些结论都可能不同。
> **用位域直接对应硬件寄存器或网络协议时，必须实测目标平台。**

## 4.2 它本身是「移位 + 掩码」的语法糖

`实测数据`
`C`

```c
/* bitfield_asm.c    编译：gcc -std=c23 -O0 -S bitfield_asm.c -o bitfield_asm.s */
struct Flags { unsigned int a:1, b:3, c:4; };

void set_b(struct Flags *f, unsigned int v) { f->b = v; }
unsigned int get_b(struct Flags *f) { return f->b; }
```

`实测数据`
`Assembly`

```asm
set_b:
    movl    24(%rbp), %eax
    andl    $7, %eax                /* 掩码：只保留 3 位 */
    movq    16(%rbp), %rdx
    andl    $7, %eax
    leal    (%rax,%rax), %ecx       /* 左移 1 位（b 从第 1 位开始） */
    movzbl  (%rdx), %eax            /* 读出原来的字节 */
    andl    $-15, %eax              /* 清掉 b 占的那 3 位 */
    orl     %ecx, %eax              /* 或进去 */
    movb    %al, (%rdx)             /* 写回 */
    ret

get_b:
    movq    16(%rbp), %rax
    movzbl  (%rax), %eax            /* 读出一个字节 */
    shrb    %al                     /* 右移 1 位 */
    andl    $7, %eax                /* 再掩码 */
    ret
```

**编译器替你做的就是手工写法里那些 `&`、`|`、`<<`、`>>`。**
**理解这一点，就明白位域省掉的是什么、又没省掉什么**：
省掉的是算错的风险，**没省掉的是「布局必须与目标一致」这件事**。

## 4.3 两条限制

**第一，不能取地址**：

`实测数据`
`Text`

```text
error: cannot take address of bit-field 'a'
```

**位域可能只占一个字节里的几位，没有独立地址**，
因此也不能 `sizeof`、不能拿它的地址传给函数。

**第二，赋值会静默截断**：`f.b = 9`（3 位）之后得到 `1`，
`9 & 0b111 = 1`——**不报错，只截断**。

`实测数据`
`Text`

```text
sizeof(struct Flags) = 4
a=1 b=5 c=9
b 赋值 9 之后 = 1
```

## 4.4 什么时候用

| 场合 | 建议 |
|---|---|
| 自己定义的文件格式、自己的协议 | 位域好用，**在注释里写明布局** |
| 直接对应硬件寄存器 | **布局不可移植**，用「整型 + 位掩码」更稳 |
| 需要跨编译器、跨平台一致 | 同上；若一定要用位域，**逐平台实测后再定** |

**嵌入式里对应寄存器时通常改用「整型 + 位掩码」**，
做法见《01-编译器/03-嵌入式与交叉编译.md》第 4.3 小节。

---

# 术语表

| 术语 | 英文 | 含义 |
|---|---|---|
| 结构体 | Structure | 多个成员并排放 |
| 联合体 | Union | 多个成员叠放，共用内存 |
| 枚举 | Enumeration | 给一组整数起名字 |
| 位域 | Bit-field | 按位分配的结构体成员 |
| 填充 | Padding | 为对齐而插入的无名字节 |
| 匿名联合体 | Anonymous union | 没有名字的联合体，成员直接可用 |
| 类型双关 | Type punning | 用联合体把一块内存按另一种类型读 |

---

# 附录 A 复现本章节实测

**环境**：`gcc` / `g++` 15.2.0（MinGW-w64），Windows x64。

## A.1 成员顺序对大小的影响

`C`

```c
/* padding.c */
#include <stdio.h>
#include <stddef.h>
struct A { char c; int i; char d; };
struct B { int i; char c; char d; };
int main(void) {
    printf("A=%zu B=%zu\n", sizeof(struct A), sizeof(struct B));
    printf("偏移 c=%zu i=%zu d=%zu\n",
           offsetof(struct A, c), offsetof(struct A, i), offsetof(struct A, d));
    return 0;
}
```

`Bash`

```bash
gcc -std=c23 padding.c -o padding && ./padding
# A=12 B=8
# 偏移 c=0 i=4 d=8
```

## A.2 联合体与字节序

`C`

```c
/* union_bytes.c */
#include <stdio.h>
union U { unsigned int u; unsigned char b[4]; };
int main(void) {
    union U v;
    v.u = 0x41424344;
    printf("%02X %02X %02X %02X\n", v.b[0], v.b[1], v.b[2], v.b[3]);
    return 0;
}
```

`Bash`

```bash
gcc -std=c23 union_bytes.c -o u && ./u
# 44 43 42 41  —— 小端序
```

## A.3 `enum` 与 `enum class`

`C++`

```cpp
// enum_diff.cpp
enum E { A = 1 };
enum class C { Red };
int main() {
    E e = A;
    // int x = 1; e = x;        // C++ 失败，C 通过
    // int y = C::Red;          // 失败：enum class 不隐式转 int
    C c = C::Red;
    return (int)c;
}
```

`Bash`

```bash
g++ -std=c++17 -c enum_diff.cpp -o e.o
gcc -std=c23 -c enum_diff.c -o e.o    # C 版：int 可以直接赋给枚举
```

## A.4 位域的大小与截断

`C`

```c
/* bitfield.c */
#include <stdio.h>
struct A { unsigned int  a:1, b:3, c:4; };
struct B { unsigned char a:1, b:3, c:4; };
int main(void) {
    printf("int 位域 = %zu, char 位域 = %zu\n", sizeof(struct A), sizeof(struct B));
    struct A f = { 1, 5, 9 };
    f.b = 9;
    printf("b = %u\n", f.b);
    return 0;
}
```

`Bash`

```bash
gcc -std=c23 bitfield.c -o bf && ./bf
# int 位域 = 4, char 位域 = 1
# b = 1
```

## A.5 结构体不能整体比较

`C`

```c
/* cmp.c */
struct P { int x; int y; };
int main(void) {
    struct P a = { 1, 2 }, b = { 1, 2 };
    return a == b;              /* 失败 */
}
```

`Bash`

```bash
gcc -std=c23 -c cmp.c -o c.o
# invalid operands to binary ==
```

## A.6 `->` 与 `(*q).x` 的等价性

`C`

```c
/* arrow.c */
#include <stdio.h>

struct Point { int x; int y; };

int main(void) {
    struct Point p = { 1, 2 };
    struct Point *q = &p;
    printf("p.x    = %d\n", p.x);
    printf("q->x   = %d\n", q->x);
    printf("(*q).x = %d\n", (*q).x);
    return 0;
}
```

`C`

```c
/* arrow_same.c */
struct Point { int x; int y; };

int via_arrow(struct Point *q) { return q->x; }        /* 用 -> */
int via_deref(struct Point *q) { return (*q).x; }      /* 用 (*q).x */
```

`C`

```c
/* dot_on_pointer.c    编译：gcc -std=c23 -c dot_on_pointer.c （失败） */
struct Point { int x; int y; };
int f(struct Point *q) { return q.x; }
```

`C`

```c
/* star_dot.c    编译：gcc -std=c23 -c star_dot.c （失败） */
struct Point { int x; int y; };
int f(struct Point *q) { return *q.x; }
```

`Bash`

```bash
gcc -std=c23 arrow.c -o arrow && ./arrow
# p.x = 1 / q->x = 1 / (*q).x = 1

gcc -std=c23 -O0 -S arrow_same.c -o arrow_same.s
# via_arrow 与 via_deref 两个函数的汇编逐条相同

gcc -std=c23 -c dot_on_pointer.c -o d.o
# error: 'q' is a pointer; did you mean to use '->'?

gcc -std=c23 -c star_dot.c -o s.o
# error: 'q' is a pointer; did you mean to use '->'?（同一条）
```

## A.7 联合体：成员地址、大小与覆盖

`C`

```c
/* union_share.c */
#include <stddef.h>
#include <stdio.h>

union U  { unsigned int u; unsigned char b[4]; float f; };
struct S { unsigned int u; unsigned char b[4]; float f; };

int main(void) {
    union U v;
    struct S s;

    printf("sizeof(union U)  = %zu\n", sizeof(union U));
    printf("sizeof(struct S) = %zu\n", sizeof(struct S));

    printf("&v.u = %p, &v.b = %p, &v.f = %p\n",
           (void *)&v.u, (void *)&v.b, (void *)&v.f);
    printf("&s.u = %p, &s.b = %p, &s.f = %p\n",
           (void *)&s.u, (void *)&s.b, (void *)&s.f);

    printf("偏移：union u=%zu b=%zu f=%zu\n",
           offsetof(union U, u), offsetof(union U, b), offsetof(union U, f));
    printf("偏移：struct u=%zu b=%zu f=%zu\n",
           offsetof(struct S, u), offsetof(struct S, b), offsetof(struct S, f));

    v.u = 0x11223344;
    v.b[0] = 0xFF;                      /* 只改一个字节 */
    printf("写 v.b[0] 之后 v.u = 0x%08X\n", v.u);

    s.u = 0x11223344;
    s.b[0] = 0xFF;
    printf("写 s.b[0] 之后 s.u = 0x%08X\n", s.u);
    return 0;
}
```

`Bash`

```bash
gcc -std=c23 union_share.c -o union_share && ./union_share
# sizeof(union U) = 4 / sizeof(struct S) = 12
# 联合体三个成员地址相同；结构体依次相差 4
# 偏移：union 全是 0；struct 是 0 / 4 / 8
# 写 v.b[0] 之后 v.u = 0x112233FF（被覆盖）
# 写 s.b[0] 之后 s.u = 0x11223344（没变）
```

## A.8 枚举名有没有进调试信息

`C`

```c
/* enum_dwarf.c    编译：gcc -std=c23 -g enum_dwarf.c -o enum_dwarf */
enum Color { RED, GREEN, BLUE };

int main(void) {
    enum Color c = GREEN;
    return c;
}
```

`C`

```c
/* define_dwarf.c    编译：gcc -std=c23 -g define_dwarf.c -o define_dwarf */
#define RED   0
#define GREEN 1
#define BLUE  2

int main(void) {
    int c = GREEN;
    return c;
}
```

`Bash`

```bash
gcc -std=c23 -g enum_dwarf.c   -o enum_dwarf.exe
gcc -std=c23 -g define_dwarf.c -o define_dwarf.exe
objdump --dwarf=info enum_dwarf.exe   | grep -E ":( +)(RED|GREEN|BLUE|Color)$"
objdump --dwarf=info define_dwarf.exe | grep -E ":( +)(RED|GREEN|BLUE|Color)$"
```

`实测数据`
`Text`

```text
enum 版：
    DW_AT_name        : Color
    DW_AT_name        : RED
    DW_AT_name        : GREEN
    DW_AT_name        : BLUE

#define 版：
    （一个都没有）
```

**`enum` 生成的类型在调试信息里是完整的**：
`DW_TAG_enumeration_type`（名字 `Color`、底层是无符号 4 字节）
下面挂着三个 `DW_TAG_enumerator`，各自带名字与常量值。

## A.9 位域：位序、跨单元与汇编

`C`

```c
/* ctrl_reg.c */
#include <stdio.h>
#include <string.h>

struct Ctrl {
    unsigned int addr : 16;         /* 15..0  */
    unsigned int mode : 14;         /* 29..16 */
    unsigned int irq  : 1;          /* 30     */
    unsigned int en   : 1;          /* 31     */
};

int main(void) {
    struct Ctrl c;
    memset(&c, 0, sizeof c);
    c.en = 1;
    c.mode = 3;
    c.addr = 0x1234;

    unsigned char raw[4];
    memcpy(raw, &c, sizeof c);
    printf("sizeof = %zu\n", sizeof c);
    printf("高字节在前: %02X %02X %02X %02X\n", raw[3], raw[2], raw[1], raw[0]);
    return 0;
}
```

`C`

```c
/* bitfield_bits.c */
#include <stdio.h>
#include <string.h>

struct Flags { unsigned int a:1, b:3, c:4; };   /* 共 8 位 */
struct S1 { unsigned char a:6, b:6; };          /* 两个 6 位 */
struct S2 { unsigned int  a:20, b:20; };        /* 两个 20 位 */

int main(void) {
    struct Flags f;
    unsigned char raw[8];

    memset(&f, 0, sizeof f); f.a = 1;
    memcpy(raw, &f, sizeof f);
    printf("只把 a 置 1   -> %02X %02X %02X %02X\n", raw[0], raw[1], raw[2], raw[3]);

    memset(&f, 0, sizeof f); f.b = 7;
    memcpy(raw, &f, sizeof f);
    printf("只把 b 置 7   -> %02X %02X %02X %02X\n", raw[0], raw[1], raw[2], raw[3]);

    memset(&f, 0, sizeof f); f.c = 15;
    memcpy(raw, &f, sizeof f);
    printf("只把 c 置 15  -> %02X %02X %02X %02X\n", raw[0], raw[1], raw[2], raw[3]);

    struct S1 s1;
    memset(&s1, 0, sizeof s1); s1.a = 0x3F;
    memcpy(raw, &s1, sizeof s1);
    printf("S1: a 填满    -> %02X %02X   （sizeof=%zu）\n", raw[0], raw[1], sizeof s1);

    memset(&s1, 0, sizeof s1); s1.b = 0x3F;
    memcpy(raw, &s1, sizeof s1);
    printf("S1: b 填满    -> %02X %02X\n", raw[0], raw[1]);

    struct S2 s2;
    memset(&s2, 0, sizeof s2); s2.a = 0xFFFFF;
    memcpy(raw, &s2, sizeof s2);
    printf("S2: a 填满    -> %02X %02X %02X %02X %02X %02X %02X %02X   （sizeof=%zu）\n",
           raw[0], raw[1], raw[2], raw[3], raw[4], raw[5], raw[6], raw[7], sizeof s2);

    memset(&s2, 0, sizeof s2); s2.b = 0xFFFFF;
    memcpy(raw, &s2, sizeof s2);
    printf("S2: b 填满    -> %02X %02X %02X %02X %02X %02X %02X %02X\n",
           raw[0], raw[1], raw[2], raw[3], raw[4], raw[5], raw[6], raw[7]);
    return 0;
}
```

`C`

```c
/* bitfield_addr.c    编译：gcc -std=c23 -c bitfield_addr.c （失败） */
struct Flags { unsigned int a:1, b:3; };

int main(void) {
    struct Flags f;
    unsigned int *p = &f.a;         /* 取位域的地址 */
    (void)p;
    return 0;
}
```

`C`

```c
/* bitfield_asm.c    编译：gcc -std=c23 -O0 -S bitfield_asm.c -o bitfield_asm.s */
struct Flags { unsigned int a:1, b:3, c:4; };

void set_b(struct Flags *f, unsigned int v) { f->b = v; }
unsigned int get_b(struct Flags *f) { return f->b; }
```

`Bash`

```bash
gcc -std=c23 ctrl_reg.c        -o ctrl_reg        && ./ctrl_reg
gcc -std=c23 bitfield_bits.c   -o bitfield_bits   && ./bitfield_bits
gcc -std=c23 -c bitfield_addr.c -o bitfield_addr.o
gcc -std=c23 -O0 -S bitfield_asm.c -o bitfield_asm.s
```

**注意 `bitfield_bits.c` 里每次都先 `memset` 清零**：
位域只覆盖存储单元里的几个位，**剩下的位是填充**，
不清零就会打印出未初始化的字节。

---

# 附录 B 相关文档

| 文档 | 关系 |
|---|---|
| 《04-语法/02-数据类型与类型系统.md》第 4 节 | **前置**：用 `typedef` 给结构体起名 |
| 《04-语法/05-初始化.md》第 5 节 | **前置**：聚合初始化与指定初始化器 |
| 《04-语法/08-数组、指针与引用.md》 | **前置**：指针与 `->` |
| 《04-语法/10-字符串.md》第 1 节 | **后续**：字符串与字符数组 |
| 《04-语法/11-作用域、生存期与链接.md》第 3 节 | **后续**：结构体对象的存储 |
| 《05-类与面向对象/02-类是一种类型.md》第 1 节 | **后续**：`class` 与 `struct` 的唯一区别 |
| 《01-编译器/00-语言的实现.md》第 5.5 小节 | 结构体布局与填充的完整推导 |
| 《01-编译器/03-嵌入式与交叉编译.md》第 4 节 | 寄存器位操作与字节序 |
