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

> **约定**：标注以灰色小字给出——`实测数据` 表示实际执行验证过，
> 完整数据与复现命令见附录 A；`文档` 表示引自标准或官方资料；
> `待确认` 表示尚未验证。路径占位符的含义见《README.md》。

## 本章节定位

| 需要先知道 | 在哪 |
|---|---|
| 类型与宽度 | 《04-语法/02-数据类型与类型系统.md》第 2.6 小节 |
| 初始化（含指定初始化器） | 《04-语法/04-初始化.md》第 5 节 |
| 指针与 `&`、`*` | 《04-语法/07-数组、指针与引用.md》第 1 节 |

**相邻的章节**：`09-字符串`（字符串常放在结构体里）、
`10-作用域、生存期与链接`（结构体的存储）。

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

`C`

```c
struct Point p = { 1, 2 };      /* C：必须写 struct */
Point p = { 1, 2 };             /* C：编译失败 */
```

`C++`

```cpp
Point p = { 1, 2 };             // C++：直接写类型名
```

**实测**：C23 下不写 `struct` 会报
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

> [!TIP]
> **`q->x` 就是 `(*q).x` 的简写。**
> 括号不能省：`*q.x` 会被理解成 `*(q.x)`，而 `q.x` 本身就是错的。
> **`->` 存在的唯一理由就是省掉那对括号。**

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

**实测输出**：

`实测数据`
`Text`

```text
struct A { char; int; char; } = 12 字节
struct B { int; char; char; } = 8 字节
A 各成员偏移: c=0 i=4 d=8
```

**成员完全相同，只是换了顺序，就差 4 字节。**

`Text`

```
   struct A                          struct B
   ┌─┬───┬───┬───┬─┬───┬───┬───┐    ┌───┬───┬───┬───┬─┬─┬─┬─┐
   │c│ 填 │   i   │d│ 填 │ 填 │ 填│    │   i   │c│d│ 填  │
   └─┴───┴───┴───┴─┴───┴───┴───┘    └───┴───┴───┴───┴─┴─┴─┴─┘
       ↑ 为了对齐 i 补了 3 字节                    ↑ 只补到 4 的倍数
```

> [!TIP]
> **结构体成员按「宽的在前、窄的在后」排列**，
> 通常能省下填充字节。
> **代价是成员顺序不再反映逻辑关系**，可读性会下降——
> 只在内存紧张时这么做，并且写清注释。

---

# 第 2 节 联合体

## 2.1 成员共享同一块内存

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

**实测输出**：

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

## 2.2 联合体用来做什么

**用途一：让同一块内存有多种解释。**

`C`

```c
union {
    unsigned int  word;         /* 整体看 */
    unsigned char byte[4];      /* 分开看 */
} raw;
```

**用途二：节省空间——同一时刻只会用到其中一个成员。**

`C`

```c
struct Value {
    int type;                   /* 0 = int, 1 = float */
    union {
        int   i;
        float f;
    } data;                     /* 只占最大的那个成员的空间 */
};
```

> [!CAUTION]
> **读联合体里「不是最后写入的那个成员」是未定义行为**（C++ 里明确规定）。
> 唯一的例外是「共同初始序列」这种特殊情况。
> **用联合体做类型双关（type punning）在 C 里是常见做法，
> 但它不属于可移植代码**——依赖字节序与对齐。
> **需要按字节看一个值，用 `memcpy` 更安全。**

## 2.3 匿名联合体

**成员可以直接访问，不用写中间那层名字。**

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

**实测：C23 与 C++17 都通过，输出 `v.i = 42`。**

---

# 第 3 节 `enum`

## 3.1 它是什么

**给一组整数起名字。**

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

**实测**：

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

**在结构体里按「位」分配空间。**

`C`

```c
/* bitfield.c    编译：gcc -std=c23 bitfield.c -o bitfield */
#include <stdio.h>

struct Flags {
    unsigned int a : 1;         /* 占 1 位 */
    unsigned int b : 3;         /* 占 3 位 */
    unsigned int c : 4;         /* 占 4 位 */
};

int main(void) {
    printf("sizeof(struct Flags) = %zu\n", sizeof(struct Flags));
    struct Flags f = { 1, 5, 9 };
    printf("a=%u b=%u c=%u\n", f.a, f.b, f.c);
    f.b = 9;                    /* 3 位装不下 9 */
    printf("b 赋值 9 之后 = %u\n", f.b);
    return 0;
}
```

**实测输出**：

`实测数据`
`Text`

```text
sizeof(struct Flags) = 4
a=1 b=5 c=9
b 赋值 9 之后 = 1
```

**两个要点**：

**第一，`sizeof` 是 4 而不是 1。** 位域的大小由**声明的类型**决定：

`实测数据`

| 声明 | 大小 |
|---|---|
| `unsigned int a:1, b:3, c:4;` | **4 字节** |
| `unsigned char a:1, b:3, c:4;` | **1 字节** |
| 单个 `unsigned int a:1;` | 4 字节 |
| 单个 `unsigned char a:1;` | 1 字节 |

**位数之和只有 8 位，但 `unsigned int` 的存储单元是 4 字节。**

**第二，`b = 9` 之后变成了 1。** 3 位最多表示 0~7，
9 的高位被截掉了（`9 & 0b111 = 1`）。

> [!WARNING]
> **位域赋值不会报错，只会静默截断。**
> 而且位域的具体布局（谁在高位、能不能跨存储单元）
> **由实现决定，标准不作保证**——
> **用位域直接对应硬件寄存器或网络协议时不可移植。**

**嵌入式里对应寄存器时，通常改用「整型 + 位掩码」**，
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

---

# 附录 B 相关文档

| 文档 | 关系 |
|---|---|
| 《04-语法/02-数据类型与类型系统.md》第 4 节 | **前置**：用 `typedef` 给结构体起名 |
| 《04-语法/04-初始化.md》第 5 节 | **前置**：聚合初始化与指定初始化器 |
| 《04-语法/07-数组、指针与引用.md》 | **前置**：指针与 `->` |
| 《04-语法/09-字符串.md》第 1 节 | **后续**：字符串与字符数组 |
| 《04-语法/10-作用域、生存期与链接.md》第 3 节 | **后续**：结构体对象的存储 |
| 【待补：05-类与面向对象/】 | **后续**：`class` 与 `struct` 的唯一区别 |
| 《01-编译器/00-语言的实现.md》第 5.5 小节 | 结构体布局与填充的完整推导 |
| 《01-编译器/03-嵌入式与交叉编译.md》第 4 节 | 寄存器位操作与字节序 |
