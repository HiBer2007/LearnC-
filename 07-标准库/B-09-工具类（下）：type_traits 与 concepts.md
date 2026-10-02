# 工具类（下）：`<type_traits>` 与标准概念库

> **授权**：本章节属教材文档部分，采用 [CC BY-NC-ND 4.0](../LICENSE)
> 加[附加条款](../许可附加条款.md)授权：署名、非商业、禁止演绎、不允许二次分发。
> 文中引用的代码不受此限，各自保留原有许可证，详见[版权与许可说明.md](../版权与许可说明.md)。

**上一章讲的是「几个值」，本章节讲的是「类型本身」。**

`<type_traits>` 里的东西不装数据，也不管输入输出。它们的输入是一个**类型**，
输出是一个**编译期常量**或者另一个类型：`std::is_integral_v<T>` 给出 `true` 或 `false`，
`std::decay_t<T>` 给出一个变换过的类型。写模板的人每天都在用这一组工具，
而 `std::enable_if` 更是 C++17 里「按条件开关重载」的唯一标准手段。

同一批判断在 C++20 换了个写法：给它们起个名字写在参数表上，这就是标准概念库。
两者解决同一个问题——**把「这个类型得满足什么」写进签名**，
区别是旧写法藏在返回类型或默认实参里，新写法是一句能读的话。

本章节接在《05-类与面向对象/12-模板的高阶使用.md》后面。那一章第 4 节用 `void_t` 与 `enable_if`
自己写过一个 trait，第 5 节用 `requires` 写过一个概念，但都没有给出标准库里的清单，
只留下「完整清单见后续章节」这一处。那些名字的完整清单在这里补全，
`typeid` 与 `type_info` 的边界也一并交代——那是《05-类与面向对象/08-多态：重载、虚函数与它们的分工.md》第 8 节留下的另一处。

---

> **约定**：标注以行内代码单独成行——`实测数据` 表示实际执行验证过，
> 完整源码与复现命令见附录 A；`文档` 表示引自标准或官方资料；
> `待确认` 表示尚未验证。路径占位符（如 `<MinGW>`、`<工作区>`）的含义见《README.md》。

# 本章节定位

| 项 | 内容 |
|---|---|
| **前置知识** | 类模板与偏特化（《05-类与面向对象/11-模板.md》第 6 节）、`void_t` 与 `enable_if`（《05-类与面向对象/12-模板的高阶使用.md》第 4 节）、`if constexpr`（同章第 3 节）、`requires` 与 `concept`（同章第 5 节）、`static_assert`（《04-语法/12-编译期能力.md》第 4 节） |
| **相邻章节** | 上一章《07-标准库/B-08-工具类（上）：pair、tuple、optional、variant、any.md》的 `variant` 与 `any` 已经在用 `type_info`；本章第 5 节把它讲透 |
| **本章各节** | 第 1 节 `<type_traits>` 是什么；第 2 节 类型分类；第 3 节 类型属性；第 4 节 变换与工具件；第 5 节 `typeid` 与 `type_info` 的边界；第 6 节 C++20 标准概念库；第 7 节速查表 |
| **本机版本** | 第 1 到 5 节用 `-std=c++17`；第 6 节用 `-std=c++20`（`g++` 15.2.0 支持），其中 6.1 的反例故意用 `-std=c++17` 演示这些名字不存在 |

---

# 第 1 节 `<type_traits>`：把类型性质写成编译期常量

## 1.1 一个 trait 的骨架

**trait 这个词在本教材里指「把某种类型性质写成编译期常量（或类型）的类模板」**
（《05-类与面向对象/12-模板的高阶使用.md》第 4.5 小节）。标准库里的 `is_*` 一族都是这种形状：

`实测数据`
`C++`

```cpp
/* traits_bool.cpp    编译：g++ -std=c++17 -Wall -Wextra traits_bool.cpp -o traits_bool */
#include <cstdio>
#include <type_traits>

/* trait 的骨架：从 integral_constant 继承，得到一个「带 value 的类型」 */
template <class T>
struct is_pointer_like : std::false_type {};

template <class T>
struct is_pointer_like<T *> : std::true_type {};      /* 偏特化负责命中 */

int main() {
    std::printf("is_pointer_like<int*> = %d\n", (int)is_pointer_like<int *>::value);
    std::printf("is_pointer_like<int>  = %d\n", (int)is_pointer_like<int>::value);

    std::printf("is_integral<int>::value = %d\n", (int)std::is_integral<int>::value);
    std::printf("is_integral_v<int>      = %d（_v 后缀是 C++17 起的变量模板）\n",
                (int)std::is_integral_v<int>);

    std::printf("true_type::value = %d，false_type::value = %d\n",
                (int)std::true_type::value, (int)std::false_type::value);
    std::printf("is_same_v<true_type, integral_constant<bool, true>> = %d\n",
                (int)std::is_same_v<std::true_type, std::integral_constant<bool, true>>);

    static_assert(std::is_integral_v<int>);                       /* 条件不成立就编不过 */
    static_assert(!std::is_integral_v<double>);
    return 0;
}
```


`实测数据`
`Text`

```text
is_pointer_like<int*> = 1
is_pointer_like<int>  = 0
is_integral<int>::value = 1
is_integral_v<int>      = 1（_v 后缀是 C++17 起的变量模板）
true_type::value = 1，false_type::value = 0
is_same_v<true_type, integral_constant<bool, true>> = 1
```


骨架是 `std::integral_constant`：它把「一个类型 + 一个值」做成一个类型，
`std::true_type` 与 `std::false_type` 就是它的两个别名
（`integral_constant<bool, true>` 与 `integral_constant<bool, false>`，示例最后一行验证了这一点）。
从它继承，自己的 trait 就自动有了 `::value`、`::type` 与 `operator()`，
还能当函数实参传给别的模板——这一点在第 4 节分派时用得上。

## 1.2 `_v` 与 `_t` 两个后缀

标准库里每个 trait 都有三种写法，示例里出现了前两种：

| 写法 | 版本 | 用途 |
|---|---|---|
| `std::is_integral<T>::value` | C++11 | 原始形式，任何版本都能用 |
| `std::is_integral_v<T>` | **C++17** | 变量模板，少写 `::value` |
| `std::is_integral<T>::type` | C++11 | 结果是 `true_type` / `false_type` 本身 |
| `std::decay<T>::type` | C++11 | 变换类的 trait 用它取结果类型 |
| `std::decay_t<T>` | **C++14** | 别名模板，少写 `::type` |

书写习惯是：**新代码一律用 `_v` 与 `_t`**，只在需要那个「带 value 的类型」本身时才回到 `::type`。
`std::integral_constant` 除了当 trait 的基类，也可以直接拿来装自己的编译期常量：

`实测数据`
`C++`

```cpp
/* traits_void_t.cpp    编译：g++ -std=c++17 -Wall -Wextra traits_void_t.cpp -o traits_void_t */
#include <cstdio>
#include <type_traits>

/* 普通 int 上加 const，得到一个「带 value 的类型」再当条件用 */
template <int N>
using size_hint = std::integral_constant<int, N>;

int main() {
    std::printf("integral_constant<int, 3>::value = %d\n", size_hint<3>::value);
    std::printf("同一个类型可以当值用：size_hint<3>{} + 1 = %d\n", size_hint<3>{} + 1);
    std::printf("bool_constant<true> 就是 true_type：%d\n",
                (int)std::is_same_v<std::bool_constant<true>, std::true_type>);
    static_assert(size_hint<3>::value == 3);
    return 0;
}
```


`实测数据`
`Text`

```text
integral_constant<int, 3>::value = 3
同一个类型可以当值用：size_hint<3>{} + 1 = 4
bool_constant<true> 就是 true_type：1
```


---

# 第 2 节 分类：这个类型到底属于哪一类

## 2.1 主类型类别：一张真值表

标准把类型分成若干**主类别**，`<type_traits>` 给每一类配一个 `is_*`。
判断依据是语言本身的类型分类（《04-语法/02-数据类型与类型系统.md》第 2 节）：

`实测数据`
`C++`

```cpp
/* traits_union.cpp    编译：g++ -std=c++17 -Wall -Wextra traits_union.cpp -o traits_union */
#include <cstddef>
#include <cstdio>
#include <string>
#include <type_traits>

struct S { int v; };
class Poly { public: virtual ~Poly() = default; };
enum Plain { A, B };
enum class Scoped { X };
union U { int i; double d; };
using Func = int (*)(double);
using MemPtr = int S::*;

/* 每个类型打印一行：九项主类型类别 */
template <class T>
void row(const char *name) {
    std::printf("%-18s %2d %2d %2d %2d %2d %2d %2d %2d %2d\n", name,
                (int)std::is_void_v<T>,
                (int)std::is_integral_v<T>,
                (int)std::is_floating_point_v<T>,
                (int)std::is_array_v<T>,
                (int)std::is_pointer_v<T>,
                (int)std::is_reference_v<T>,
                (int)std::is_class_v<T>,
                (int)std::is_enum_v<T>,
                (int)std::is_union_v<T>);
}

int main() {
    std::printf("%-18s %2s %2s %2s %2s %2s %2s %2s %2s %2s\n", "类型",
                "V", "I", "F", "A", "P", "R", "C", "E", "U");
    row<void>("void");
    row<bool>("bool");
    row<char>("char");
    row<int>("int");
    row<unsigned long long>("unsigned long long");
    row<float>("float");
    row<double>("double");
    row<int[3]>("int[3]");
    row<const char *>("const char*");
    row<int &>("int&");
    row<int &&>("int&&");
    row<S>("struct S");
    row<Poly>("class Poly");
    row<Plain>("enum Plain");
    row<Scoped>("enum class Scoped");
    row<U>("union U");
    row<Func>("int(*)(double)");
    row<MemPtr>("int S::*");
    row<std::string>("std::string");
    row<std::nullptr_t>("nullptr_t");
    row<void *>("void*");
    return 0;
}
```


`实测数据`
`Text`

```text
类型              V  I  F  A  P  R  C  E  U
void                1  0  0  0  0  0  0  0  0
bool                0  1  0  0  0  0  0  0  0
char                0  1  0  0  0  0  0  0  0
int                 0  1  0  0  0  0  0  0  0
unsigned long long  0  1  0  0  0  0  0  0  0
float               0  0  1  0  0  0  0  0  0
double              0  0  1  0  0  0  0  0  0
int[3]              0  0  0  1  0  0  0  0  0
const char*         0  0  0  0  1  0  0  0  0
int&                0  0  0  0  0  1  0  0  0
int&&               0  0  0  0  0  1  0  0  0
struct S            0  0  0  0  0  0  1  0  0
class Poly          0  0  0  0  0  0  1  0  0
enum Plain          0  0  0  0  0  0  0  1  0
enum class Scoped   0  0  0  0  0  0  0  1  0
union U             0  0  0  0  0  0  0  0  1
int(*)(double)      0  0  0  0  1  0  0  0  0
int S::*            0  0  0  0  0  0  0  0  0
std::string         0  0  0  0  0  0  1  0  0
nullptr_t           0  0  0  0  0  0  0  0  0
void*               0  0  0  0  1  0  0  0  0
```


**这张表给出两条信息。** 第一条：**每一行里恰好有一个 1**——
类型分类是互斥的，标准把这句话写在主类别那一节的注里：

`文档`

> "The primary type categories correspond to the descriptions given in section 6.9 of the C++ standard."
>
> —— N4659 §23.15.4.1/1
>
> "[Note: For any given type T, exactly one of the primary type categories has a value member
> that evaluates to true. —end note]"
>
> —— N4659 §23.15.4.1/3（同节的 Table 40 列出全部主类别谓词）

第二条：**`cv` 限定符不影响分类结果**，`const int` 与 `int` 给出同样的答案：

`文档`

> "For any given type T, the result of applying one of these templates to T and to cv T shall yield the same result."
>
> —— N4659 §23.15.4.1/2

表里几处需要单独说明的地方：`bool` 与 `char` 都算整数类型（`is_integral`）；
`void*` 是 `is_pointer` 而函数指针不是（函数指针归 `is_pointer` 也为真，示例里 `int(*)(double)` 那一行是 1，
只因为它同时是标量；指向成员的指针 `int S::*` 则**不是** `is_pointer`，它有自己的 `is_member_pointer`）；
`nullptr_t` 既不是指针也不是类，它归 `is_null_pointer`，在下一小节的表里才出现；
`union` 不是 `is_class`，它有单独的 `is_union`。

## 2.2 复合类别

复合类别由主类别组合出来，用来回答「它算不算算术类型」「能不能当对象」这类问题：

`实测数据`
`C++`

```cpp
/* traits_compound.cpp    编译：g++ -std=c++17 -Wall -Wextra traits_compound.cpp -o traits_compound */
#include <cstddef>
#include <cstdio>
#include <string>
#include <type_traits>

struct S { int v; };
union U { int i; double d; };
enum Plain { A };
using MemPtr = int S::*;

/* 复合类别：由主类别组合出来 */
template <class T>
void row(const char *name) {
    std::printf("%-18s %2d %2d %2d %2d %2d %2d %2d %2d\n", name,
                (int)std::is_arithmetic_v<T>,
                (int)std::is_fundamental_v<T>,
                (int)std::is_scalar_v<T>,
                (int)std::is_object_v<T>,
                (int)std::is_compound_v<T>,
                (int)std::is_member_pointer_v<T>,
                (int)std::is_null_pointer_v<T>,
                (int)std::is_lvalue_reference_v<T>);
}

int main() {
    std::printf("%-18s %2s %2s %2s %2s %2s %2s %2s %2s\n", "类型",
                "Ar", "Fu", "Sc", "Ob", "Co", "MP", "NP", "LR");
    row<void>("void");
    row<int>("int");
    row<double>("double");
    row<bool>("bool");
    row<int *>("int*");
    row<int &>("int&");
    row<int[3]>("int[3]");
    row<S>("struct S");
    row<U>("union U");
    row<Plain>("enum Plain");
    row<int (*)(double)>("int(*)(double)");
    row<MemPtr>("int S::*");
    row<std::nullptr_t>("nullptr_t");
    row<std::string>("std::string");
    return 0;
}
```


`实测数据`
`Text`

```text
类型             Ar Fu Sc Ob Co MP NP LR
void                0  1  0  0  0  0  0  0
int                 1  1  1  1  0  0  0  0
double              1  1  1  1  0  0  0  0
bool                1  1  1  1  0  0  0  0
int*                0  0  1  1  1  0  0  0
int&                0  0  0  0  1  0  0  1
int[3]              0  0  0  1  1  0  0  0
struct S            0  0  0  1  1  0  0  0
union U             0  0  0  1  1  0  0  0
enum Plain          0  0  1  1  1  0  0  0
int(*)(double)      0  0  1  1  1  0  0  0
int S::*            0  0  1  1  1  1  0  0
nullptr_t           0  1  1  1  0  0  1  0
std::string         0  0  0  1  1  0  0  0
```


`文档`

> "These templates provide convenient compositions of the primary type categories,
> corresponding to the descriptions given in section 6.9."
>
> —— N4659 §23.15.4.2/1，同节的 Table 41 列出全部复合类别谓词

两条容易记错的：

| 判断 | 结果 |
|---|---|
| `void` 算不算「基本类型」 | 算，`is_fundamental_v<void>` 为真（示例第一行 `Fu` 列是 1） |
| 引用算不算「对象」 | **不算**，`is_object_v<int&>` 为假——这是 `optional<T&>` 写不出来的原因（《07-标准库/B-08-工具类（上）：pair、tuple、optional、variant、any.md》第 3.5 小节） |
| 函数指针算不算「标量」 | 算，示例里 `int(*)(double)` 那一行 `Sc` 列是 1 |
| `is_compound_v<T>` | 就是 `!is_fundamental_v<T>`，两者互补 |

## 2.3 分类能用来做什么：按类别分派

分类的价值在于**由编译器选择实现**。下面这个函数只接受算术类型，
整数与浮点走不同的分支：

`实测数据`
`C++`

```cpp
/* traits_dispatch.cpp    编译：g++ -std=c++17 -Wall -Wextra traits_dispatch.cpp -o traits_dispatch */
#include <cstdio>
#include <string>
#include <type_traits>

/* 按类别分派：整数走一条路，浮点走另一条，其余在编译期挡住 */
template <class T>
std::string describe(T) {
    static_assert(std::is_arithmetic_v<T>, "只接受算术类型");
    if constexpr (std::is_integral_v<T>) {
        return "整数类型";
    } else {
        return "浮点类型";
    }
}

int main() {
    std::printf("int    -> %s\n", describe(1).c_str());
    std::printf("long   -> %s\n", describe(1L).c_str());
    std::printf("float  -> %s\n", describe(1.0f).c_str());
    std::printf("double -> %s\n", describe(1.0).c_str());
    std::printf("is_arithmetic_v<int> = %d，is_arithmetic_v<std::string> = %d\n",
                (int)std::is_arithmetic_v<int>, (int)std::is_arithmetic_v<std::string>);
    return 0;
}
```


`实测数据`
`Text`

```text
int    -> 整数类型
long   -> 整数类型
float  -> 浮点类型
double -> 浮点类型
is_arithmetic_v<int> = 1，is_arithmetic_v<std::string> = 0
```


两个机制配合：`static_assert` 在编译期挡住不合法的类型（把 `std::string` 传进来会直接报错，
而不是在运行期出怪事），`if constexpr` 只编译被选中的分支
（《05-类与面向对象/12-模板的高阶使用.md》第 3.2 小节）。
**关键在于 `if constexpr` 不能用普通 `if` 代替**：普通 `if` 的两个分支对每种实例化都要合法。

---

# 第 3 节 属性：这个类型有什么本事

## 3.1 `const`、`volatile` 与引用

第一组属性问的是「这个类型上挂了什么限定符」：

`实测数据`
`C++`

```cpp
/* traits_cv.cpp    编译：g++ -std=c++17 -Wall -Wextra traits_cv.cpp -o traits_cv */
#include <cstdio>
#include <type_traits>

template <class T>
void row(const char *name) {
    std::printf("%-24s %2d %2d %2d %2d %2d %2d\n", name,
                (int)std::is_const_v<T>,
                (int)std::is_volatile_v<T>,
                (int)std::is_reference_v<T>,
                (int)std::is_lvalue_reference_v<T>,
                (int)std::is_rvalue_reference_v<T>,
                (int)std::is_pointer_v<T>);
}

int main() {
    std::printf("%-24s %2s %2s %2s %2s %2s %2s\n", "类型",
                "co", "vo", "re", "lv", "rv", "pt");
    row<int>("int");
    row<const int>("const int");
    row<volatile int>("volatile int");
    row<const int &>("const int&");
    row<int &&>("int&&");
    row<const char *>("const char*");
    row<char *const>("char* const");
    row<const char *const>("const char* const");
    return 0;
}
```


`实测数据`
`Text`

```text
类型                   co vo re lv rv pt
int                       0  0  0  0  0  0
const int                 1  0  0  0  0  0
volatile int              0  1  0  0  0  0
const int&                0  0  1  1  0  0
int&&                     0  0  1  0  1  0
const char*               0  0  0  0  0  1
char* const               1  0  0  0  0  1
const char* const         1  0  0  0  0  1
```


表里 `const char*` 与 `char* const` 的对照最有用：**前者的 `const` 在指向的对象上，
后者的 `const` 在指针自己身上**，所以只有后者 `is_const` 为真
（《04-语法/03-常量与 const.md》第 1 节讲的就是这层区分）。
`const char* const` 两个都为真。

## 3.2 平凡性、拷贝性与多态

第二组问的是「编译器能不能对这块内存做粗活」——能不能 `memcpy`、能不能不调用构造析构：

`实测数据`
`C++`

```cpp
/* traits_props.cpp    编译：g++ -std=c++17 -Wall -Wextra traits_props.cpp -o traits_props */
#include <cstdio>
#include <type_traits>

struct Trivial { int v; };                       /* 全靠编译器生成 */
struct WithCtor { WithCtor() {} int v; };        /* 自己写了构造函数 */
struct WithDtor { ~WithDtor() {} int v; };       /* 自己写了析构函数 */
struct Virtual1 { virtual void f(); int v; };    /* 有虚函数：多态类型 */
struct Abstract { virtual void f() = 0; };       /* 纯虚函数：抽象类 */
struct Empty {};
struct Last final {};

template <class T>
void row(const char *name) {
    std::printf("%-14s %2d %2d %2d %2d %2d %2d %2d %2d\n", name,
                (int)std::is_trivial_v<T>,
                (int)std::is_trivially_copyable_v<T>,
                (int)std::is_standard_layout_v<T>,
                (int)std::is_polymorphic_v<T>,
                (int)std::is_abstract_v<T>,
                (int)std::is_empty_v<T>,
                (int)std::is_aggregate_v<T>,
                (int)std::is_final_v<T>);
}

int main() {
    std::printf("%-14s %2s %2s %2s %2s %2s %2s %2s %2s\n", "类型",
                "tr", "tc", "sl", "po", "ab", "em", "ag", "fi");
    row<int>("int");
    row<Trivial>("Trivial");
    row<WithCtor>("WithCtor");
    row<WithDtor>("WithDtor");
    row<Virtual1>("Virtual1");
    row<Abstract>("Abstract");
    row<Empty>("Empty");
    row<Last>("Last final");
    return 0;
}
```


`实测数据`
`Text`

```text
类型         tr tc sl po ab em ag fi
int             1  1  1  0  0  0  0  0
Trivial         1  1  1  0  0  0  1  0
WithCtor        0  1  1  0  0  0  0  0
WithDtor        0  0  1  0  0  0  1  0
Virtual1        0  0  0  1  0  0  0  0
Abstract        0  0  0  1  1  0  0  0
Empty           1  1  1  0  0  1  1  0
Last final      1  1  1  0  0  1  1  1
```


几个名字的含义与用途：

| trait | 为真的意思 | 谁在用 |
|---|---|---|
| `is_trivial` | 默认构造、拷贝、析构都是编译器生成的，且没有任何虚的东西 | 判断能不能像 C 结构体一样对待 |
| `is_trivially_copyable` | 拷贝一个对象与拷贝它的字节等价 | 容器的增长、`memcpy` 优化（`09-高阶数据结构` 讲容器时会出现） |
| `is_standard_layout` | 布局规则与 C 兼容，可以用 `offsetof` | 与 C 交互的接口、序列化 |
| `is_polymorphic` | 带虚函数（至少一个） | 判断 `dynamic_cast` / `typeid` 能不能给出动态类型（第 5 节） |
| `is_abstract` | 有纯虚函数，不能直接创建对象 | 接口类 |
| `is_aggregate` | 是聚合类型，可以花括号逐成员初始化 | 判断能不能用聚合初始化 |
| `is_empty` | 没有非静态数据成员、没有虚函数 | 空基类优化的前提（`tuple` 那一节的对照） |

> [!CAUTION]
> **`is_trivially_copyable` 只管「能不能按字节拷贝」，不管「这样拷对不对」。**
> 一个持有指针的类完全可能同时满足「可平凡拷贝」与「浅拷贝会重复释放」——
> 比如一个用 `= default` 拷贝、却在析构里 `delete` 指针的类。
> 这个 trait 的用途是给库作者判断优化机会，而不是用来判断「我的类能不能拷贝」。

`WithDtor` 那一行还带着版本差异：`ag` 列是 1，说明**在 C++17 里，写了析构函数的类仍然是聚合类型**
（判据里没有析构函数这一条），这一条在 C++20 收紧了。同一行 `tc` 列是 0：
析构函数是「非平凡的」，因此不再可平凡拷贝。

---

# 第 4 节 变换与工具件

## 4.1 改类型：`remove_*`、`add_*`、`decay`、`conditional`、`enable_if`

前面两组 trait 给的是「是不是」，这一组给的是**换一个类型**。
判定结果不再打印真假，而是用 `std::is_same_v` 验证换出来的类型对不对：

`实测数据`
`C++`

```cpp
/* traits_transform.cpp    编译：g++ -std=c++17 -Wall -Wextra traits_transform.cpp -o traits_transform */
#include <cstdio>
#include <type_traits>

template <class From, class To>
void same(const char *what) {
    std::printf("%-58s %d\n", what, (int)std::is_same_v<From, To>);
}

int main() {
    same<std::remove_const_t<const int>, int>("remove_const_t<const int> == int");
    same<std::remove_const_t<const int *>, const int *>("remove_const_t<const int*> == const int*");
    same<std::remove_pointer_t<const int *>, const int>("remove_pointer_t<const int*> == const int");
    same<std::remove_reference_t<int &>, int>("remove_reference_t<int&> == int");
    same<std::add_const_t<int>, const int>("add_const_t<int> == const int");
    same<std::add_lvalue_reference_t<int>, int &>("add_lvalue_reference_t<int> == int&");
    same<std::decay_t<int[3]>, int *>("decay_t<int[3]> == int*");
    same<std::decay_t<int &>, int>("decay_t<int&> == int");
    same<std::decay_t<const int &>, int>("decay_t<const int&> == int");
    same<std::decay_t<int(double)>, int (*)(double)>("decay_t<int(double)> == int(*)(double)");
    same<std::conditional_t<true, int, double>, int>("conditional_t<true, int, double> == int");
    same<std::conditional_t<false, int, double>, double>("conditional_t<false, int, double> == double");
    same<std::common_type_t<int, double>, double>("common_type_t<int, double> == double");
    same<std::common_type_t<int, long>, long>("common_type_t<int, long> == long");
    same<std::enable_if_t<true, int>, int>("enable_if_t<true, int> == int");
    std::printf("sizeof(common_type_t<char, short>) = %zu（类型是 short）\n",
                sizeof(std::common_type_t<char, short>));
    return 0;
}
```


`实测数据`
`Text`

```text
remove_const_t<const int> == int                           1
remove_const_t<const int*> == const int*                   1
remove_pointer_t<const int*> == const int                  1
remove_reference_t<int&> == int                            1
add_const_t<int> == const int                              1
add_lvalue_reference_t<int> == int&                        1
decay_t<int[3]> == int*                                    1
decay_t<int&> == int                                       1
decay_t<const int&> == int                                 1
decay_t<int(double)> == int(*)(double)                     1
conditional_t<true, int, double> == int                    1
conditional_t<false, int, double> == double                1
common_type_t<int, double> == double                       1
common_type_t<int, long> == long                           1
enable_if_t<true, int> == int                              1
sizeof(common_type_t<char, short>) = 4（类型是 short）
```


其中最常用的几个，作用与典型用途如下：

| 名字 | 做什么 | 典型用途 |
|---|---|---|
| `remove_const_t<T>` / `add_const_t<T>` | 加/去顶层 `const` | 参数按值传递时去掉没意义的限定 |
| `remove_pointer_t<T>` / `add_pointer_t<T>` | 加/去指针 | 写「指针的指针」这类泛型代码 |
| `remove_reference_t<T>` / `add_lvalue_reference_t<T>` | 加/去引用 | 完美转发里的类型计算（《05-类与面向对象/05-拷贝与移动.md》第 4 节） |
| `decay_t<T>` | **一次做完三件事**：去引用、去顶层 `cv`、把数组与函数退化成指针 | 模拟「按值传参时实参变成什么类型」 |
| `conditional_t<B, X, Y>` | 编译期的三目运算符 | 按条件选类型 |
| `common_type_t<A, B>` | 两者做算术运算后的结果类型 | 写 `min`/`max` 这类模板时要定返回类型 |
| `enable_if_t<B, T>` | `B` 为真时是 `T`，为假时**没有 `type`** | 按条件开关重载（第 4.3 小节） |

**`decay` 的名字就是「退化」**：数组类型 `int[3]` 退化成 `int*`，函数类型退化成函数指针，
引用与顶层 `const` 一起消失。它的定义正是「按值传参时会发生什么」。

## 4.2 `declval`：不求值语境里的万能值

有些 trait 要判断「这个表达式能不能写出来」，可表达式里需要一个该类型的对象——
但又不能真的构造一个（可能没有默认构造函数）。`std::declval` 解决的正是这件事：

`实测数据`
`C++`

```cpp
/* traits_declval.cpp    编译：g++ -std=c++17 -Wall -Wextra traits_declval.cpp -o traits_declval */
#include <cstdio>
#include <string>
#include <type_traits>
#include <utility>

/* void_t 探测：表达式写得出来，偏特化就命中 */
template <class T, class = void>
struct has_size : std::false_type {};

template <class T>
struct has_size<T, std::void_t<decltype(std::declval<const T &>().size())>>
    : std::true_type {};

struct Empty {};

struct Base {};
struct Derived : Base {};

int f(double);

int main() {
    std::printf("has_size<std::string> = %d\n", (int)has_size<std::string>::value);
    std::printf("has_size<Empty>       = %d\n", (int)has_size<Empty>::value);

    /* declval 只在不求值语境里用：sizeof 的操作数不真求值 */
    std::printf("sizeof(declval<string&>().size()) = %zu\n",
                sizeof(std::declval<std::string &>().size()));

    std::printf("is_convertible_v<int, double>            = %d\n",
                (int)std::is_convertible_v<int, double>);
    std::printf("is_convertible_v<std::string, int>       = %d\n",
                (int)std::is_convertible_v<std::string, int>);
    std::printf("is_constructible_v<std::string, const char*> = %d\n",
                (int)std::is_constructible_v<std::string, const char *>);
    std::printf("is_assignable_v<int&, int>               = %d\n",
                (int)std::is_assignable_v<int &, int>);
    std::printf("is_base_of_v<Base, Derived>              = %d\n",
                (int)std::is_base_of_v<Base, Derived>);
    std::printf("is_base_of_v<Derived, Base>              = %d\n",
                (int)std::is_base_of_v<Derived, Base>);
    std::printf("is_invocable_v<decltype(&f), double>     = %d\n",
                (int)std::is_invocable_v<decltype(&f), double>);
    std::printf("is_invocable_v<decltype(&f), void*>      = %d\n",
                (int)std::is_invocable_v<decltype(&f), void *>);
    std::printf("is_same_v<decltype(f(1.0)), int>         = %d\n",
                (int)std::is_same_v<decltype(f(1.0)), int>);
    return 0;
}
```


`实测数据`
`Text`

```text
has_size<std::string> = 1
has_size<Empty>       = 0
sizeof(declval<string&>().size()) = 8
is_convertible_v<int, double>            = 1
is_convertible_v<std::string, int>       = 0
is_constructible_v<std::string, const char*> = 1
is_assignable_v<int&, int>               = 1
is_base_of_v<Base, Derived>              = 1
is_base_of_v<Derived, Base>              = 0
is_invocable_v<decltype(&f), double>     = 1
is_invocable_v<decltype(&f), void*>      = 0
is_same_v<decltype(f(1.0)), int>         = 1
```


`declval<T>()` 只返回 `T&&` 类型的**声明**，没有定义，因此**只能出现在不求值语境里**
（`decltype`、`sizeof`、`noexcept` 的操作数里）。示例里的三步需要逐一说明：

| 表达式 | 作用 |
|---|---|
| `decltype(std::declval<const T &>().size())` | 「如果有一个 `T`，能不能对它调 `size()`」——这就是 `void_t` 探测的核心写法（《05-类与面向对象/12-模板的高阶使用.md》第 4.5 小节） |
| `sizeof(std::declval<std::string &>().size())` | `sizeof` 的操作数不求值，所以这里可以放心用 `declval`，结果是 `size_t` 的 8 字节 |
| `std::is_invocable_v<decltype(&f), double>` | 「`f` 能不能用 `double` 调用」——不用写调用语句，只看类型 |

## 4.3 写错了会怎样：`enable_if` 挡住之后

`enable_if` 的用法在第 4.1 小节的表里有，它的错误现场需要单独说明。
下面这个函数只接受整数类型，条件写在返回类型上：

`实测数据`
`C++`

```cpp
/* traits_enable_if_fail.cpp    编译：g++ -std=c++17 traits_enable_if_fail.cpp -o traits_enable_if_fail （失败） */
#include <cstdio>
#include <type_traits>

/* 只接受整数类型：条件写在返回类型上 */
template <class T>
std::enable_if_t<std::is_integral_v<T>, T> twice(T v) { return v + v; }

int main() {
    std::printf("%d\n", twice(21));
    std::printf("%f\n", twice(2.5));      /* double 不满足条件，替换失败 */
    return 0;
}
```


传一个 `double` 进去，编译器给出的报错是：

`实测数据`
`Text`

```text
（只摘出与本例有关的三行，去掉了模板实例化链）

traits_enable_if_fail.cpp:11:30: error: no matching function for call to 'twice(double)'
traits_enable_if_fail.cpp:7:44: note: candidate 1: 'template<class T> std::enable_if_t<((bool)is_integral_v<T>), T> twice(T)'
traits_enable_if_fail.cpp:7:44: note: template argument deduction/substitution failed:
error: no type named 'type' in 'struct std::enable_if<false, double>'
```

**这三行的含义是**：候选函数存在（第二行把它的签名原样打出来），
替换阶段失败了（第三行），失败的原因是 `enable_if<false, double>` 里没有 `type`（第四行）。
这正是 SFINAE 的样子——候选安静出局，而不是硬错误；
如果这个函数是唯一的候选，出局之后就是「没有匹配的函数」。

> [!WARNING]
> **报错的位置不在调用处，而在返回类型上。**
> 第二行打出来的签名里混着 `((bool)is_integral_v<T>)` 这样的内部形式，
> 一眼看不出「这里要的是整数」。
> 条件越多、模板层次越深，这几行就越难读——第 6 节的概念就是冲着这一点来的。

---

# 第 5 节 `typeid` 与 `type_info` 的边界

## 5.1 一条规则：静态类型还是动态类型

`typeid` 有两种结果，分界线只有一条：**操作数是不是多态类类型的左值**
（《05-类与面向对象/08-多态：重载、虚函数与它们的分工.md》第 8.3 小节给出了规则的原文与第一个反例）。

`实测数据`
`C++`

```cpp
/* typeid_basic.cpp    编译：g++ -std=c++17 -Wall -Wextra typeid_basic.cpp -o typeid_basic */
#include <cstdio>
#include <typeinfo>

class NoVirtual { public: void f() {} };
class WithVirtual { public: virtual ~WithVirtual() = default; };
class Derived : public WithVirtual {};

int main() {
    NoVirtual nv;
    NoVirtual *pn = &nv;
    WithVirtual wv;
    WithVirtual *pw = &wv;
    Derived d;
    WithVirtual *pd = &d;

    std::printf("非多态 typeid(*pn).name() = %s\n", typeid(*pn).name());
    std::printf("多态   typeid(*pw).name() = %s\n", typeid(*pw).name());
    std::printf("多态   typeid(*pd).name() = %s（静态类型是 WithVirtual）\n", typeid(*pd).name());
    std::printf("指针本身 typeid(pd).name() = %s\n", typeid(pd).name());

    std::printf("typeid(Derived) == typeid(*pd)：%d\n",
                (int)(typeid(Derived) == typeid(*pd)));
    std::printf("typeid(NoVirtual) == typeid(*pn)：%d\n",
                (int)(typeid(NoVirtual) == typeid(*pn)));
    std::printf("typeid(*pw) == typeid(*pd)：%d（都是多态左值，看动态类型）\n",
                (int)(typeid(*pw) == typeid(*pd)));

    const int ci = 3;
    std::printf("typeid(int) == typeid(const int)：%d\n",
                (int)(typeid(int) == typeid(const int)));
    std::printf("typeid(ci).name() = %s\n", typeid(ci).name());
    std::printf("hash_code 相等：%d\n",
                (int)(typeid(int).hash_code() == typeid(const int).hash_code()));
    std::printf("typeid(int).before(typeid(double)) = %d（顺序由实现决定）\n",
                (int)typeid(int).before(typeid(double)));
    return 0;
}
```


`实测数据`
`Text`

```text
非多态 typeid(*pn).name() = 9NoVirtual
多态   typeid(*pw).name() = 11WithVirtual
多态   typeid(*pd).name() = 7Derived（静态类型是 WithVirtual）
指针本身 typeid(pd).name() = P11WithVirtual
typeid(Derived) == typeid(*pd)：1
typeid(NoVirtual) == typeid(*pn)：1
typeid(*pw) == typeid(*pd)：0（都是多态左值，看动态类型）
typeid(int) == typeid(const int)：1
typeid(ci).name() = i
hash_code 相等：1
typeid(int).before(typeid(double)) = 0（顺序由实现决定）
```


对照表：

| 操作数 | 得到什么 | 示例里的行 |
|---|---|---|
| 非多态类的左值（或对象本身） | **静态类型**，编译期就定下来 | `typeid(*pn)` 给 `9NoVirtual` |
| 多态类的左值 | **动态类型**，运行期查虚表 | `typeid(*pd)` 给 `7Derived` |
| 指针本身（不是解引用的结果） | 指针类型，静态 | `typeid(pd)` 给 `P11WithVirtual` |
| 带顶层 `cv` 的类型 | **忽略 `cv`** | `typeid(int) == typeid(const int)` 为真 |

最后一行对应标准里的一条：

`文档`

> "When typeid is applied to an expression other than a glvalue of a polymorphic class type,
> the result refers to a std::type_info object representing the static type of the expression.
> Lvalue-to-rvalue, array-to-pointer, and function-to-pointer conversions are not applied."
>
> —— N4659 §8.2.8/3

**「不求值」那半句同样重要**：非多态情形下操作数不求值，
所以 `typeid(*pn)` 里的 `pn` 是空指针也不会出事；多态情形要真求值，第 5.3 小节就是这件事的后果。

## 5.2 `name()` 与 `type_info` 的接口

`typeid` 的结果是一个 `const std::type_info &`。它能做的事只有四件：

| 成员 | 结果 |
|---|---|
| `name()` | 一个**实现定义**的字符串，本机是名字修饰的结果 |
| `operator==` / `!=` | 两个类型是否相同（这是判断类型的正确做法） |
| `hash_code()` | 一个可用于哈希的整数，同类型必相等 |
| `before()` | 实现定义的排序，用来把 `type_info` 放进有序容器 |

`name()` 的格式不可依赖，下面的输出说明了原因：

`实测数据`
`C++`

```cpp
/* typeid_names.cpp    编译：g++ -std=c++17 -Wall -Wextra typeid_names.cpp -o typeid_names */
#include <cstdio>
#include <string>
#include <typeinfo>

struct S {};
template <class T> struct Box {};

int main() {
    std::printf("%-20s %s\n", "int", typeid(int).name());
    std::printf("%-20s %s\n", "double", typeid(double).name());
    std::printf("%-20s %s\n", "char", typeid(char).name());
    std::printf("%-20s %s\n", "const char*", typeid(const char *).name());
    std::printf("%-20s %s\n", "int&", typeid(int &).name());
    std::printf("%-20s %s\n", "int[3]", typeid(int[3]).name());
    std::printf("%-20s %s\n", "void", typeid(void).name());
    std::printf("%-20s %s\n", "struct S", typeid(S).name());
    std::printf("%-20s %s\n", "Box<int>", typeid(Box<int>).name());
    std::printf("%-20s %s\n", "std::string", typeid(std::string).name());
    std::printf("%-20s %s\n", "int(*)(double)", typeid(int (*)(double)).name());
    std::printf("typeid(int) == typeid(long)：%d\n", (int)(typeid(int) == typeid(long)));
    std::printf("typeid(int&) == typeid(int)：%d（引用被剥掉）\n",
                (int)(typeid(int &) == typeid(int)));
    return 0;
}
```


`实测数据`
`Text`

```text
int                  i
double               d
char                 c
const char*          PKc
int&                 i
int[3]               A3_i
void                 v
struct S             1S
Box<int>             3BoxIiE
std::string          NSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEE
int(*)(double)       PFidE
typeid(int) == typeid(long)：0
typeid(int&) == typeid(int)：1（引用被剥掉）
```


`PKc` 是「指向 const char 的指针」，`A3_i` 是「3 个 int 的数组」，`NSt7__cxx...` 是
`std::string` 的完整修饰名。这是 Itanium C++ ABI 的名字修饰规则（`g++` 用这一套），
换一个编译器（MSVC）就是另一套写法，
所以标准才说名字与编码方式「均未规定」（原文见《05-类与面向对象/08-多态：重载、虚函数与它们的分工.md》第 8.3 小节）。
**要比较类型就写 `typeid(a) == typeid(b)`，永远不要去比字符串。**

`typeid(int&) == typeid(int)` 为真那一条也有用：`typeid` 会把引用剥掉，
结果是「被引用类型」的信息，这也说明它给出的是类型信息，不是表达式的形态。

## 5.3 空指针上的 `typeid`

多态情形要真求值，于是一个空指针就撞上了规则里写明的那一条：

`实测数据`
`C++`

```cpp
/* typeid_null.cpp    编译：g++ -std=c++17 -Wall -Wextra typeid_null.cpp -o typeid_null */
#include <cstdio>
#include <typeinfo>

class Poly { public: virtual ~Poly() = default; };

int main() {
    Poly *p = nullptr;
    try {
        std::printf("%s\n", typeid(*p).name());   /* 多态左值要真求值，空指针解引用 */
    } catch (const std::bad_typeid &e) {
        std::printf("抛出 bad_typeid：%s\n", e.what());
    }
    return 0;
}
```


`实测数据`
`Text`

```text
抛出 bad_typeid：std::bad_typeid
```


`文档`

> "If the glvalue expression is obtained by applying the unary* operator to a
> pointer and the pointer is a null pointer value (7.11), the typeid expression throws an exception (18.1)
> of a type that would match a handler of type std::bad_typeid exception (21.7.4)."
>
> —— N4659 §8.2.8/2

本机抛出的 `what()` 文本是 `std::bad_typeid`。
规则只覆盖「对指针解引用」这一种写法：`typeid(p)`（不解引用）永远安全，
因为那问的是指针自己的类型。

## 5.4 与 `dynamic_cast` 的分工

三个工具（虚函数、`typeid`、`dynamic_cast`）在同一个对象上各做一件事：

`实测数据`
`C++`

```cpp
/* rtti_choose.cpp    编译：g++ -std=c++17 -Wall -Wextra rtti_choose.cpp -o rtti_choose */
#include <cstdio>
#include <typeinfo>

struct Shape {
    virtual ~Shape() = default;
    virtual const char *kind() const { return "形状"; }
};
struct Circle : Shape {
    const char *kind() const override { return "圆"; }
    double radius = 1.0;
};
struct Square : Shape {
    const char *kind() const override { return "方"; }
};

/* 虚函数方案：调用点不必知道具体类型 */
void describe(const Shape &s) { std::printf("  虚函数：%s\n", s.kind()); }

/* typeid 方案：调用点自己判断类型 */
void describe_by_typeid(const Shape &s) {
    if (typeid(s) == typeid(Circle)) {
        std::printf("  typeid 认出圆\n");
    } else if (typeid(s) == typeid(Square)) {
        std::printf("  typeid 认出方\n");
    } else {
        std::printf("  typeid 不认识这个类型\n");
    }
}

/* dynamic_cast 方案：要访问派生类独有的成员时才需要 */
void describe_by_cast(const Shape &s) {
    if (const Circle *c = dynamic_cast<const Circle *>(&s)) {
        std::printf("  dynamic_cast 拿到 radius = %.1f\n", c->radius);
    } else {
        std::printf("  dynamic_cast 失败，不是圆\n");
    }
}

int main() {
    Circle c;
    Square q;
    describe(c);
    describe(q);
    describe_by_typeid(c);
    describe_by_typeid(q);
    describe_by_cast(c);
    describe_by_cast(q);
    std::printf("typeid(c).name() = %s，typeid(q).name() = %s\n",
                typeid(c).name(), typeid(q).name());
    return 0;
}
```


`实测数据`
`Text`

```text
  虚函数：圆
  虚函数：方
  typeid 认出圆
  typeid 认出方
  dynamic_cast 拿到 radius = 1.0
  dynamic_cast 失败，不是圆
typeid(c).name() = 6Circle，typeid(q).name() = 6Square
```


| 工具 | 回答的问题 | 什么时候用 |
|---|---|---|
| **虚函数** | 「这个操作该怎么做」 | 首选。调用点不需要知道具体类型（《05-类与面向对象/08-多态：重载、虚函数与它们的分工.md》第 3 节） |
| **`dynamic_cast`** | 「你是不是某一种，是的话我要用你独有的成员」 | 派生类独有的操作必须由外部代码调用时；指针版失败给空指针，要判空（《05-类与面向对象/08-多态：重载、虚函数与它们的分工.md》第 8.2 小节） |
| **`typeid`** | 「你到底是哪一种」 | 只做相等判断、不访问派生类成员时；或者要把类型本身当数据用 |

**顺序不要颠倒**：能用虚函数解决的，不要用后两个。
一串 `typeid` 比较或 `dynamic_cast` 加 `if` 摆在调用点上，等于把「将来新增一种子类」的代价
从「加一个类」变成「找出所有分支逐个补」。

`typeid` 还有一处专门用途：把类型当**数据**传递（日志里打类型名、做类型到工厂的映射）。
`std::any` 与 `std::variant` 的取值检查就是在做这件事
（《07-标准库/B-08-工具类（上）：pair、tuple、optional、variant、any.md》第 5.1 小节）。

---

# 第 6 节 C++20 的标准概念库

## 6.1 先说版本：C++17 下这些名字不存在

本章第 1 到 5 节的内容都在 C++17 里，第 6 节必须换标准。`g++` 15.2.0 的实际反应是：

`实测数据`
`C++`

```cpp
/* concepts_cxx17_fail.cpp    编译：g++ -std=c++17 concepts_cxx17_fail.cpp -o concepts_cxx17_fail （失败） */
#include <concepts>
#include <cstdio>

int main() {
    std::printf("integral<int> = %d\n", (int)std::integral<int>);
    return 0;
}
```


`实测数据`
`Text`

```text
concepts_cxx17_fail.cpp:6:51: error: 'integral' is not a member of 'std'
     std::printf("integral<int> = %d\n", (int)std::integral<int>);
                                                   ^~~~~~~~
concepts_cxx17_fail.cpp:6:51: note: 'std::integral' is only available from C++20 onwards
```

`<concepts>` 这个头文件本身能包含进来（本机的 libstdc++ 没有拦它），
**但里面没有 `std::integral` 这些名字**，编译器在报错里还补了一句「从 C++20 起才有」。
本节其余程序的编译命令都带 `-std=c++20`。

## 6.2 标准概念实测

标准概念库定义在 `<concepts>` 里，与 `<type_traits>` 的关系是**同一件事的两种写法**：
`std::integral<T>` 的内部实现就是 `is_integral_v<T>`。

`实测数据`
`C++`

```cpp
/* concepts_std.cpp    编译：g++ -std=c++20 -Wall -Wextra concepts_std.cpp -o concepts_std */
#include <concepts>
#include <cstdio>
#include <string>

struct Base { virtual ~Base() = default; };
struct Derived : Base {};
struct Other {};

int twice(int v);
bool is_even(int v);

int main() {
    std::printf("%-48s %d\n", "same_as<int, int>", (int)std::same_as<int, int>);
    std::printf("%-48s %d\n", "same_as<int, const int>", (int)std::same_as<int, const int>);
    std::printf("%-48s %d\n", "same_as<int, long>", (int)std::same_as<int, long>);
    std::printf("%-48s %d\n", "derived_from<Derived, Base>", (int)std::derived_from<Derived, Base>);
    std::printf("%-48s %d\n", "derived_from<Base, Derived>", (int)std::derived_from<Base, Derived>);
    std::printf("%-48s %d\n", "derived_from<Other, Base>", (int)std::derived_from<Other, Base>);
    std::printf("%-48s %d\n", "integral<int>", (int)std::integral<int>);
    std::printf("%-48s %d\n", "integral<double>", (int)std::integral<double>);
    std::printf("%-48s %d\n", "integral<bool>", (int)std::integral<bool>);
    std::printf("%-48s %d\n", "floating_point<double>", (int)std::floating_point<double>);
    std::printf("%-48s %d\n", "floating_point<float>", (int)std::floating_point<float>);
    std::printf("%-48s %d\n", "convertible_to<int, double>", (int)std::convertible_to<int, double>);
    std::printf("%-48s %d\n", "convertible_to<double, int>", (int)std::convertible_to<double, int>);
    std::printf("%-48s %d\n", "convertible_to<std::string, int>",
                (int)std::convertible_to<std::string, int>);
    std::printf("%-48s %d\n", "invocable<decltype(&twice), int>",
                (int)std::invocable<decltype(&twice), int>);
    std::printf("%-48s %d\n", "invocable<decltype(&twice), std::string>",
                (int)std::invocable<decltype(&twice), std::string>);
    std::printf("%-48s %d\n", "predicate<decltype(&is_even), int>",
                (int)std::predicate<decltype(&is_even), int>);
    std::printf("%-48s %d\n", "predicate<decltype(&twice), int>",
                (int)std::predicate<decltype(&twice), int>);
    std::printf("%-48s %d\n", "copyable<int>", (int)std::copyable<int>);
    std::printf("%-48s %d\n", "movable<std::string>", (int)std::movable<std::string>);
    std::printf("%-48s %d\n", "default_initializable<std::string>",
                (int)std::default_initializable<std::string>);
    std::printf("%-48s %d\n", "equality_comparable<int>", (int)std::equality_comparable<int>);
    std::printf("%-48s %d\n", "totally_ordered<int>", (int)std::totally_ordered<int>);
    std::printf("%-48s %d\n", "constructible_from<std::string, const char*>",
                (int)std::constructible_from<std::string, const char *>);
    std::printf("%-48s %d\n", "assignable_from<int&, int>", (int)std::assignable_from<int &, int>);
    return 0;
}
```


`实测数据`

| 概念 | 结果 |
|---|---|
| `same_as<int, int>` / `same_as<int, const int>` / `same_as<int, long>` | 1 / 0 / 0（与 `is_same` 一样，不看 `cv` 之外的东西） |
| `derived_from<Derived, Base>` / `<Base, Derived>` / `<Other, Base>` | 1 / 0 / 0 |
| `integral<int>` / `integral<double>` / `integral<bool>` | 1 / 0 / 1 |
| `floating_point<double>` / `floating_point<float>` | 1 / 1 |
| `convertible_to<int, double>` / `<double, int>` / `<std::string, int>` | 1 / 1 / 0（窄化不算障碍） |
| `invocable<decltype(&twice), int>` / `<..., std::string>` | 1 / 0 |
| `predicate<decltype(&is_even), int>` / `predicate<decltype(&twice), int>` | 1 / 1（返回值能转成 `bool` 就算） |
| `copyable<int>` / `movable<std::string>` / `default_initializable<std::string>` | 1 / 1 / 1 |
| `equality_comparable<int>` / `totally_ordered<int>` | 1 / 1 |
| `constructible_from<std::string, const char*>` / `assignable_from<int&, int>` | 1 / 1 |

几个概念比同名的 trait 严格一点，这一点需要留意：

| 概念 | 与 trait 的差别 |
|---|---|
| `derived_from<D, B>` | 比 `is_base_of` 多两条：`D` 必须是类类型，且 `D*` 能隐式转成 `const B*`（私有继承不算） |
| `convertible_to<From, To>` | 比 `is_convertible` 多一条：要求转换是**隐式且 `static_cast` 也成立**的，`explicit` 构造函数不算 |
| `predicate<F, Args...>` | 比 `invocable` 多一条：返回值要能转成 `bool`（示例里返回 `int` 的 `twice` 也算） |
| `invocable<F, Args...>` | 只看「能不能这样调用」，不看返回类型 |

## 6.3 怎么用在代码里

概念有三种落点，分别对应 `enable_if` 的三种老写法：

`实测数据`
`C++`

```cpp
/* concepts_usage.cpp    编译：g++ -std=c++20 -Wall -Wextra concepts_usage.cpp -o concepts_usage */
#include <concepts>
#include <cstdio>
#include <string>
#include <type_traits>

/* 一、概念写在模板参数上 */
template <std::integral T>
T twice(T v) { return v + v; }

/* 二、简写形式：auto 前面加概念 */
void show(std::floating_point auto v) { std::printf("浮点 %g\n", (double)v); }

/* 三、requires 子句 */
template <class T>
    requires std::convertible_to<T, const char *>
std::size_t length_of(T s) { return std::string(s).size(); }

/* 四、C++17 的等价写法（对照用） */
template <class T, class = std::enable_if_t<std::is_integral_v<T>>>
T twice_old(T v) { return v + v; }

int main() {
    std::printf("twice(3) = %d，twice_old(3) = %d\n", twice(3), twice_old(3));
    show(2.5);
    std::printf("length_of(\"abc\") = %zu\n", length_of("abc"));
    return 0;
}
```


`实测数据`
`Text`

```text
twice(3) = 6，twice_old(3) = 6
浮点 2.5
length_of("abc") = 3
```


| 写法 | 形状 | 对应的旧写法 |
|---|---|---|
| 模板参数上加概念 | `template <std::integral T> T twice(T);` | `enable_if` 写在模板参数的默认实参上 |
| 简写形式 | `void show(std::floating_point auto v);` | 没有直接的旧写法（这是 C++20 的新语法） |
| `requires` 子句 | `template <class T> requires std::convertible_to<T, const char *>` | `enable_if` 写在返回类型上 |

示例最后那个 `twice_old` 是 C++17 的等价物，两个函数在 `int` 上的行为完全一样
（输出第一行）。区别只在**类型不满足时**会发生什么，下一小节看这件事。

## 6.4 与 `enable_if` 的报错对照

同一个错误（传 `double` 给只收整数的函数），两种写法的报错对比：

`实测数据`
`C++`

```cpp
/* concepts_nomatch_fail.cpp    编译：g++ -std=c++20 concepts_nomatch_fail.cpp -o concepts_nomatch_fail （失败） */
#include <concepts>
#include <cstdio>

template <std::integral T>
T twice(T v) { return v + v; }

int main() {
    std::printf("%f\n", twice(2.5));      /* double 不满足 integral */
    return 0;
}
```


`实测数据`
`Text`

```text
（只摘出与本例有关的五行，去掉了库内部路径）

concepts_nomatch_fail.cpp:9:30: error: no matching function for call to 'twice(double)'
concepts_nomatch_fail.cpp:6:3: note: candidate 1: 'template<class T>  requires  integral<T> T twice(T)'
concepts_nomatch_fail.cpp:6:3: note: template argument deduction/substitution failed:
concepts_nomatch_fail.cpp:6:3: note: constraints not satisfied
concepts_nomatch_fail.cpp:6:3: note: the expression 'is_integral_v<_Tp> [with _Tp = double]' evaluated to 'false'
```

对比第 4.3 小节那份报错，差别在最后两行：`enable_if` 那一份说的是
「`enable_if<false, double>` 里没有 `type`」——一句关于模板机制的话；
这一份说的是「约束不满足」加上「`is_integral_v<double>` 算出来是假」——一句关于**要求**的话。
**报错的位置也不同**：旧写法指向返回类型那一长串，新写法指向候选者那一行的 `requires`。

本节提到的概念不在 N4659 里（N4659 是 C++17 草案，搜索 `same_as`、`invocable` 都没有结果），
因此本节的概念只标版本，不给条款号。

## 6.5 什么时候用 trait，什么时候用概念

| 场合 | 用什么 | 理由 |
|---|---|---|
| 要一个真假值做 `static_assert` 或 `if constexpr` | `<type_traits>` | 它就是编译期布尔值，哪里都能用 |
| 要给模板加约束、筛选重载 | **概念**（C++20） | 写在签名上、有名字、报错指向要求 |
| 要把类型改一个形态 | `<type_traits>` 的变换件 | 概念只回答真假，不做类型计算 |
| 代码要兼容 C++17 | `enable_if` | 没有别的选择 |

两者在一份代码里是并存的：约束交给概念，类型计算交给 trait。

`实测数据`
`C++`

```cpp
/* traits_with_concepts.cpp    编译：g++ -std=c++20 -Wall -Wextra traits_with_concepts.cpp -o traits_with_concepts */
#include <concepts>
#include <cstdio>
#include <string>
#include <type_traits>

/* 概念管「约束」：只接受整数类型 */
template <std::integral T>
T twice(T v) { return v + v; }

/* trait 管「查询与变换」：C++20 里照旧用，返回类型就是它算出来的 */
template <class T>
std::decay_t<T> clamp_positive(T &&v) {
    using V = std::decay_t<T>;
    if constexpr (std::is_unsigned_v<V>) {
        return V(v);                   /* 无符号类型不可能为负，这一支在编译期就定了 */
    } else {
        return v < T(0) ? V(0) : V(v);
    }
}

/* 概念本身就是靠 trait 实现的：下面两行断言的是同一个判断 */
static_assert(std::is_integral_v<int> == std::integral<int>);
static_assert(std::is_floating_point_v<double> == std::floating_point<double>);

int main() {
    std::printf("twice(21) = %d\n", twice(21));
    std::printf("clamp_positive(-3) = %d\n", clamp_positive(-3));
    std::printf("clamp_positive(7u) = %u\n", clamp_positive(7u));
    std::printf("clamp_positive(-2.5) = %.1f\n", clamp_positive(-2.5));
    std::printf("decay_t<const std::string&> 就是 std::string，占 %zu 字节\n",
                sizeof(std::decay_t<const std::string &>));
    std::printf("is_integral_v<int> = %d，integral<int> = %d（同一个判断）\n",
                (int)std::is_integral_v<int>, (int)std::integral<int>);
    return 0;
}
```

`实测数据`
`Text`

```text
twice(21) = 42
clamp_positive(-3) = 0
clamp_positive(7u) = 7
clamp_positive(-2.5) = 0.0
decay_t<const std::string&> 就是 std::string，占 32 字节
is_integral_v<int> = 1，integral<int> = 1（同一个判断）
```

`clamp_positive` 这个例子把两边的分工摆在一起：**签名上的 `std::integral` 是概念**，
它在重载决议阶段筛掉不合法的类型；**函数体里的 `is_unsigned_v` 与返回类型的 `decay_t` 是 trait**，
它们回答「这个类型有什么性质」与「它该变成什么类型」，概念做不到这两件事。
`clamp_positive(7u)` 与 `clamp_positive(-3)` 走的是同一个模板的两条不同分支，
分支由 `if constexpr` 在编译期选定（《05-类与面向对象/12-模板的高阶使用.md》第 3.2 小节）。

**`<type_traits>` 不会因为概念出现就被取代。** 概念管「约束」，trait 管「查询与变换」；
`std::integral_constant`、`decay_t`、`common_type_t` 这些在 C++20 里照旧用，
概念本身甚至就是靠它们实现的。

---

# 第 7 节 速查表

`实测数据`

| 名字 | 一句话用途 | 典型坑 |
|---|---|---|
| `is_void` / `is_integral` / `is_floating_point` / `is_array` / `is_pointer` / `is_reference` / `is_class` / `is_enum` / `is_union` | 主类型类别，互斥 | `bool`、`char` 都算整数；`union` 不是 `is_class` |
| `is_arithmetic` / `is_fundamental` / `is_scalar` / `is_object` / `is_compound` | 复合类别 | 引用不是对象（`is_object_v<int&>` 为假） |
| `is_null_pointer` / `is_member_pointer` | 两个单独的类别 | `nullptr_t` 不是指针；`int S::*` 也不是 |
| `is_const` / `is_volatile` | 顶层 `cv` | `const char*` 的 `is_const` 为假，`char* const` 才为真 |
| `is_trivial` / `is_trivially_copyable` | 编译器能不能做粗活 | 「可平凡拷贝」不等于「拷贝语义正确」 |
| `is_standard_layout` | 布局与 C 兼容，可用 `offsetof` | — |
| `is_polymorphic` / `is_abstract` | 有没有虚函数 / 纯虚函数 | `typeid`、`dynamic_cast` 需要多态类型 |
| `is_aggregate` | 能不能花括号逐成员初始化 | C++17 与 C++20 的判据不同（析构函数） |
| `is_base_of` / `is_convertible` | 继承关系 / 隐式转换 | 私有继承 `is_base_of` 为真、`is_convertible` 为假 |
| `is_invocable` / `is_constructible` / `is_assignable` | 能不能调用 / 构造 / 赋值 | 都要写成 `is_xxx_v<F, Args...>` 的形式 |
| `remove_const_t` / `remove_reference_t` / `remove_pointer_t` | 去掉一层限定 | `remove_const_t<const int*>` 去掉的是**指针自己**的 `const`，不是指向对象的 |
| `add_const_t` / `add_lvalue_reference_t` / `add_pointer_t` | 加上一层 | 加在引用上没有效果 |
| `decay_t` | 去引用、去顶层 `cv`、数组与函数退化成指针 | 三件事一起做，不要拿它当 `remove_cv_t` 用 |
| `conditional_t<B, X, Y>` | 编译期三目 | 两个分支的类型都要能写出来 |
| `common_type_t<A, B>` | 算术运算的结果类型 | 没有公共类型时是空（编译报错） |
| `enable_if_t<B, T>` | 按条件开关重载 | 失败信息落在返回类型上，难读（第 4.3 小节） |
| `declval<T>()` | 不求值语境里的万能值 | 只能用在 `decltype`、`sizeof`、`noexcept` 里 |
| `integral_constant<T, v>` / `true_type` / `false_type` | 把值和类型绑成一个类型 | 从它继承就自动有 `::value` 与 `::type` |
| `typeid(x)` | 取 `type_info` | 非多态是静态类型；对空指针解引用会抛 `bad_typeid` |
| `type_info::name()` | 类型名字符串 | 实现定义、不可比较、不可跨编译器 |
| `type_info::operator==` | 比较类型 | 这才是判断类型的正确做法 |
| `same_as` / `derived_from` / `convertible_to` | C++20 概念 | 比同名的 trait 严格；`-std=c++20` 起可用 |
| `integral` / `floating_point` | C++20 概念 | 与 `is_integral_v` / `is_floating_point_v` 一一对应 |
| `invocable` / `predicate` | C++20 概念 | `predicate` 多要求返回值能转成 `bool` |
| `copyable` / `movable` / `equality_comparable` / `totally_ordered` | C++20 概念 | 一组常用要求的打包 |

---

# 附录 A 复现本章节实测

主线的程序都是完整可编译的单文件，源码就在正文里。
第 1 到 5 节用 `g++ -std=c++17 -Wall -Wextra <名字>.cpp -o <名字>`，
第 6 节的四个程序用 `-std=c++20`（`concepts_cxx17_fail.cpp` 故意用 C++17，那正是它要演示的事），
`g++` 15.2.0，MinGW-w64，x86-64，全部程序无警告。

`实测数据`

| 在哪一节 | 程序 | 演示什么 |
|---|---|---|
| 第 1 节 | `traits_bool.cpp`、`traits_void_t.cpp` | trait 的骨架；`_v`/`_t` 后缀与 `integral_constant` |
| 第 2 节 | `traits_union.cpp`、`traits_compound.cpp`、`traits_dispatch.cpp` | 主类别真值表；复合类别真值表；按类别分派 |
| 第 3 节 | `traits_cv.cpp`、`traits_props.cpp` | cv 与引用；平凡性、拷贝性与多态 |
| 第 4 节 | `traits_transform.cpp`、`traits_declval.cpp`、`traits_enable_if_fail.cpp` | 变换件；`declval`、`void_t` 与可转换、可构造、可调用的查询；`enable_if` 挡住之后（**期望失败**） |
| 第 5 节 | `typeid_basic.cpp`、`typeid_names.cpp`、`typeid_null.cpp`、`rtti_choose.cpp` | 静态与动态类型；`name()`；空指针上的 `typeid`；与 `dynamic_cast` 的分工 |
| 第 6 节 | `concepts_std.cpp`、`concepts_usage.cpp`、`concepts_nomatch_fail.cpp`、`traits_with_concepts.cpp`、`concepts_cxx17_fail.cpp` | 标准概念实测（`-std=c++20`）；概念用法（`-std=c++20`）；约束不满足的报错（**期望失败**）；trait 与概念并用（`-std=c++20`）；C++17 下这些名字不存在（**期望失败**） |

## A.1 两处要说明的测量

`实测数据`

| 项目 | 取值 |
|---|---|
| 编译器 | `g++` 15.2.0（MinGW-w64，x86-64） |
| 语言标准 | 第 1 到 5 节 `-std=c++17`；第 6 节 `-std=c++20` |
| 警告选项 | `-Wall -Wextra`，全部程序无警告 |
| 名字修饰 | `typeid(...).name()` 的结果是 Itanium C++ ABI 的名字修饰，`g++` 用它 |

第 5.2 小节的类型名是**直接打印**出来的，没有做任何转换；
换编译器（例如 MSVC）会得到完全不同的字符串，这正是正文说「不可依赖」的原因。

## A.2 报错原文与输出

第 4.3、6.1、6.4 小节里的报错原文**只摘出了与本例有关的那几行**，去掉了模板实例化链与库内部路径；
对应的文件是 `traits_enable_if_fail.cpp`、`concepts_cxx17_fail.cpp`、`concepts_nomatch_fail.cpp`，
都是完整的单文件程序，直接编译即可得到原文。
正文里的程序输出都是**整份输出**，没有删行；只有第 6.2 小节把 25 行输出整理成了一张对照表，数值与输出逐项一致。

---

# 附录 B 相关文档

| 文档 | 关系 |
|---|---|
| 《05-类与面向对象/12-模板的高阶使用.md》第 4 节 | **前置**：SFINAE、`void_t`、`enable_if` 与 trait 的手写版 |
| 《05-类与面向对象/12-模板的高阶使用.md》第 3 节 | **前置**：`if constexpr` 只实例化选中的分支 |
| 《05-类与面向对象/12-模板的高阶使用.md》第 5 节 | **前置**：C++20 概念与 `requires` 的入门 |
| 《05-类与面向对象/08-多态：重载、虚函数与它们的分工.md》第 8 节 | **前置**：`dynamic_cast`、`typeid`、RTTI 的代价与关掉它 |
| 《05-类与面向对象/11-模板.md》第 6 节 | **前置**：全特化与偏特化（`is_*` 的实现方式） |
| 《04-语法/12-编译期能力.md》第 4 节 | **前置**：`static_assert` |
| 《04-语法/02-数据类型与类型系统.md》第 3 节 | **前置**：`const` 与 `volatile` |
| 《07-标准库/B-08-工具类（上）：pair、tuple、optional、variant、any.md》第 3.5 小节 | 相关：`is_object_v<int&>` 为假，`optional<T&>` 因此不存在 |
| 《07-标准库/B-08-工具类（上）：pair、tuple、optional、variant、any.md》第 5.1 小节 | 相关：`std::any` 用 `type_info` 做运行期检查 |
| 【待补：09-高阶数据结构/】 | **后续**：容器的增长为什么要看 `is_trivially_copyable` |

---

配套示例见 [`B-examples/07-standard-library/08-cpp-config-parser/`](../B-examples/07-standard-library/08-cpp-config-parser/)，配套练习见 [`C-templates/07-standard-library/08-cpp-config-parser/`](../C-templates/07-standard-library/08-cpp-config-parser/)。
