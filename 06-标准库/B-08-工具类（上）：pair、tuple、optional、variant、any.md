# 工具类（上）：`pair`、`tuple`、`optional`、`variant`、`any`

> **授权**：本章节属教材文档部分，采用 [CC BY-NC-ND 4.0](../LICENSE)
> 加[附加条款](../许可附加条款.md)授权：署名、非商业、禁止演绎、不允许二次分发。
> 文中引用的代码不受此限，各自保留原有许可证，详见[版权与许可说明.md](../版权与许可说明.md)。

**「一个函数只能返回一个值」这句话从 C++11 起就不再成立了。**

`std::pair` 与 `std::tuple` 把多个值捆成一个对象，`std::optional` 把「可能没有值」写进类型，
`std::variant` 表示「几种类型里选一种」，`std::any` 什么都能装。
它们都不是容器，也不参与算法，共同的作用是**在类型层面把数据的形状说清楚**：
有几项、每项什么类型、有没有可能缺一项。

**C 里没有这些类型。** 最接近的做法是一个结构体加一个 `enum` 标记，或者一个 `void *` 加一条调用约定。
两种写法的约定都只存在于注释与人的记忆里，写错了编译器不报错，要到运行期才暴露。
本章节的五个类型把这套约定搬进了类型系统，代价是编译期多算一些、个别场合多一次堆分配（第 5.2 小节实测）。

《05-类与面向对象/12-模板的高阶使用.md》第 6.6 小节用 `TypeList` 与 `contains` 演示了「在类型上递归」，
并指出标准库里的对应物是 `std::tuple`。本章节兑现那处承诺；
`<type_traits>` 的完整清单与 C++20 标准概念库留给《06-标准库/B-09-工具类（下）：type_traits 与 concepts.md》。

---

> **约定**：标注以行内代码单独成行——`实测数据` 表示实际执行验证过，
> 完整源码与复现命令见附录 A；`文档` 表示引自标准或官方资料；
> `待确认` 表示尚未验证。路径占位符（如 `<MinGW>`、`<工作区>`）的含义见《README.md》。

# 本章节定位

| 项 | 内容 |
|---|---|
| **前置知识** | 类模板（《05-类与面向对象/11-模板.md》第 4 节）、变参模板与推导指引（《05-类与面向对象/12-模板的高阶使用.md》第 1 节）、异常与 `what()`（《04-语法/13-异常.md》第 3 节）、`union` 共享内存（《04-语法/09-结构体、联合体与 enum.md》第 2 节） |
| **相邻章节** | 上一章《05-类与面向对象/12-模板的高阶使用.md》第 6.6 小节留下「标准库里的对应物是 `std::tuple`」这一处；下一章《06-标准库/B-09-工具类（下）：type_traits 与 concepts.md》讲 `<type_traits>` 与 C++20 标准概念库 |
| **本章各节** | 第 1 节 `pair`；第 2 节 `tuple` 与结构化绑定；第 3 节 `optional`；第 4 节 `variant`；第 5 节 `any`；第 6 节 `swap`、`exchange`、`as_const`；第 7 节速查表 |
| **实测环境** | `g++` 15.2.0（MinGW-w64，x86-64），一律 `-std=c++17 -Wall -Wextra`；大小与布局的数字随实现变化，第 4.5 小节另给一份 Linux 侧的对照 |

---

# 第 1 节 `pair`：把两个值绑在一起

## 1.1 两个成员，没有隐藏状态

`std::pair<T1, T2>` 就是「一个 `T1` 加一个 `T2`」，两个成员叫 `first` 与 `second`。
它不分配内存、不抛异常，拷贝它就是拷贝两个成员。

`实测数据`
`C++`

```cpp
/* pair_basic.cpp    编译：g++ -std=c++17 -Wall -Wextra pair_basic.cpp -o pair_basic */
#include <cstdio>
#include <string>
#include <tuple>
#include <utility>

struct Range {                        /* 两个 int 一起构造，不产生临时对象 */
    int lo, hi;
    Range(int a, int b) : lo(a), hi(b) {}
};

int main() {
    std::pair<int, std::string> p{1, "one"};
    std::printf("p.first = %d，p.second = %s\n", p.first, p.second.c_str());

    auto q = std::make_pair(2, 3.5);          /* 类型由实参推导：pair<int, double> */
    std::printf("q = (%d, %.1f)\n", q.first, q.second);

    std::pair<int, int> a{1, 2}, b{1, 3};
    std::printf("a == b：%d，a < b：%d\n", (int)(a == b), (int)(a < b));

    std::pair<long, std::string> wide = p;    /* 逐成员可隐式转换时，pair 整体可转换 */
    std::printf("转换后：%ld %s\n", wide.first, wide.second.c_str());

    auto [x, y] = p;                          /* 结构化绑定把两个成员拆开 */
    std::printf("拆开：%d 与 %s\n", x, y.c_str());

    std::pair<Range, Range> seg{std::piecewise_construct,
                                std::forward_as_tuple(1, 2),
                                std::forward_as_tuple(3, 4)};
    std::printf("分段构造：[%d, %d] 与 [%d, %d]\n",
                seg.first.lo, seg.first.hi, seg.second.lo, seg.second.hi);

    std::pair<int, std::string> other{9, "nine"};
    p.swap(other);
    std::printf("交换后：p.first = %d，other.first = %d\n", p.first, other.first);
    return 0;
}
```


`实测数据`
`Text`

```text
p.first = 1，p.second = one
q = (2, 3.5)
a == b：0，a < b：1
转换后：1 one
拆开：1 与 one
分段构造：[1, 2] 与 [3, 4]
交换后：p.first = 9，other.first = 1
```


构造有几条路：花括号初始化列表、`std::make_pair` 的推导、
逐成员可隐式转换时整体转换（示例里的 `std::pair<long, std::string> wide = p;`）。
`std::piecewise_construct` 那一条用在成员不适合「先造临时对象再移动」的场合：
它的后两个实参是 `std::tuple`，里面的值被直接转给成员的构造函数。

比较运算符按字典序逐成员比较：先比 `first`，相等再比 `second`。
示例里 `a < b` 为真，因为 `first` 相等而 `2 < 3`。这套顺序让 `pair` 能直接当有序容器的键。

## 1.2 大小：与手写结构体完全一样

`实测数据`
`C++`

```cpp
/* pair_size.cpp    编译：g++ -std=c++17 -Wall -Wextra pair_size.cpp -o pair_size */
#include <cstddef>
#include <cstdio>
#include <string>
#include <utility>

struct CharInt { char c; int i; };
struct CharDouble { char c; double d; };

template <class P>
void show(const char *name) {
    P p{};
    const char *base = reinterpret_cast<const char *>(&p);
    std::printf("%-26s sizeof = %2zu   两个成员相距 %d 字节\n",
                name, sizeof(P),
                (int)(reinterpret_cast<const char *>(&p.second) - base));
}

int main() {
    show<std::pair<char, char>>("pair<char, char>");
    show<std::pair<char, int>>("pair<char, int>");
    show<std::pair<int, char>>("pair<int, char>");
    show<std::pair<char, double>>("pair<char, double>");
    show<std::pair<int, int>>("pair<int, int>");
    show<std::pair<double, char>>("pair<double, char>");
    show<std::pair<std::string, char>>("pair<string, char>");
    std::printf("%-26s sizeof = %2zu\n", "struct{char; int}", sizeof(CharInt));
    std::printf("%-26s sizeof = %2zu\n", "struct{char; double}", sizeof(CharDouble));
    return 0;
}
```


`实测数据`

| 类型 | `sizeof` | 两个成员相距 | 对照结构体 |
|---|---|---|---|
| `pair<char, char>` | 2 | 1 | — |
| `pair<char, int>` | 8 | 4 | `struct { char; int }` 也是 8 |
| `pair<int, char>` | 8 | 4 | — |
| `pair<char, double>` | 16 | 8 | `struct { char; double }` 也是 16 |
| `pair<int, int>` | 8 | 4 | — |
| `pair<double, char>` | 16 | 8 | — |
| `pair<std::string, char>` | 40 | 32 | — |

**`pair` 不承担任何运行期开销。** 那几个空字节不是 `pair` 带来的，而是成员的对齐要求：
`int` 要落在 4 的倍数上，`double` 要落在 8 的倍数上，于是 `char` 后面必须留出填充字节
（《04-语法/09-结构体、联合体与 enum.md》第 1.5 小节）。
`pair<int, char>` 与 `pair<char, int>` 都是 8 字节，说明决定大小的是成员的类型，不是顺序。

`std::map` 的元素类型是 `pair<const Key, T>`，关联容器的 `insert` 返回 `pair<iterator, bool>`。
容器与算法归 `08-高阶数据结构`，在那之前只需认得出来：**出现 `first` 与 `second` 的地方，多数就是一个 `pair`。**

> [!IMPORTANT]
> **`pair` 是零开销的二元组**：布局与同序的结构体一致，
> 名字（`first`、`second`）是编译期的事，不占空间。

---

# 第 2 节 `tuple`：任意多个值

## 2.1 构造与两种取值

`std::tuple<Ts...>` 是 `pair` 的推广，成员个数从 0 到任意多个。
取值有两种写法：写下标 `std::get<0>`，或者写类型 `std::get<int>`。

`实测数据`
`C++`

```cpp
/* tuple_basic.cpp    编译：g++ -std=c++17 -Wall -Wextra tuple_basic.cpp -o tuple_basic */
#include <cstdio>
#include <string>
#include <tuple>
#include <utility>

std::tuple<int, std::string, double> make_record() {
    return std::make_tuple(7, std::string("seven"), 7.5);
}

int main() {
    std::tuple<int, std::string> t{1, "one"};
    std::printf("get<0> = %d，get<1> = %s\n", std::get<0>(t), std::get<1>(t).c_str());
    std::printf("按类型取：get<int> = %d\n", std::get<int>(t));
    std::printf("元素个数 %zu，第 0 个元素占 %zu 字节\n",
                std::tuple_size<decltype(t)>::value,
                sizeof(std::tuple_element_t<0, decltype(t)>));

    int id = 0;
    std::string name;
    double score = 0.0;
    std::tie(id, name, score) = make_record();          /* 拆到已有变量 */
    std::printf("tie 拆包：%d %s %.1f\n", id, name.c_str(), score);
    std::tie(id, std::ignore, score) = make_record();   /* 中间那个不要 */
    std::printf("忽略中间：%d %.1f\n", id, score);

    auto [a, b, c] = make_record();                     /* 结构化绑定一步到位 */
    std::printf("结构化绑定：%d %s %.1f\n", a, b.c_str(), c);

    auto t2 = std::tuple_cat(t, std::make_tuple(2.5));  /* 拼接 */
    std::printf("拼接后元素个数：%zu\n", std::tuple_size<decltype(t2)>::value);

    int sum = std::apply([](int x, int y) { return x + y; }, std::make_tuple(3, 4));
    std::printf("apply 展开求和：%d\n", sum);

    std::tuple<int, int> u{1, 2}, v{1, 3};
    std::printf("tuple 比较：u < v = %d\n", (int)(u < v));
    u.swap(v);
    std::printf("交换后 u = (%d, %d)\n", std::get<0>(u), std::get<1>(u));
    return 0;
}
```


`实测数据`
`Text`

```text
get<0> = 1，get<1> = one
按类型取：get<int> = 1
元素个数 2，第 0 个元素占 4 字节
tie 拆包：7 seven 7.5
忽略中间：7 7.5
结构化绑定：7 seven 7.5
拼接后元素个数：3
apply 展开求和：7
tuple 比较：u < v = 1
交换后 u = (1, 3)
```


取值的两种写法都是模板：`get<0>` 的下标是**模板实参**（这一点在
《05-类与面向对象/12-模板的高阶使用.md》第 6.6 小节已经出现过），
所以取第 0 个元素时编译器知道那是 `int`，不需要运行期查表。
按类型取的写法多一层限制：**这个类型在列表里必须只出现一次**，
否则编译器无法判断要哪一个（`variant` 上写错时的报错原文见第 4.3 小节）。

`std::tie` 造出一个引用组成的 `tuple`，把右边逐项写进已有的变量，`std::ignore` 占位表示这一项不要。
接收多个返回值时它比先建一个 `tuple` 再逐个 `get` 更直接。
`std::tuple_cat` 拼接两个 `tuple`，`std::apply` 把 `tuple` 摊开成实参表，
`std::tuple_size` 给出元素个数，`std::tuple_element_t<I, T>` 给出第 I 项的类型。
后两个是编译期常量：示例输出里的「元素个数 2」就是 `tuple_size<decltype(t)>::value`，
而「第 0 个元素占 4 字节」是 `sizeof(tuple_element_t<0, decltype(t)>)`。

## 2.2 结构化绑定：一步拆开

C++17 起可以用一行声明把 `pair`、`tuple`、数组、普通结构体拆成若干个名字：

`实测数据`
`C++`

```cpp
/* tuple_bind.cpp    编译：g++ -std=c++17 -Wall -Wextra tuple_bind.cpp -o tuple_bind */
#include <cstdio>
#include <string>
#include <tuple>
#include <utility>

struct Point { int x, y; };

int main() {
    std::pair<int, std::string> p{1, "one"};
    auto [id, name] = p;
    std::printf("pair 拆开：%d %s\n", id, name.c_str());

    std::tuple<int, double, char> t{2, 2.5, 'x'};
    auto [a, b, c] = t;
    std::printf("tuple 拆开：%d %.1f %c\n", a, b, c);

    int arr[3] = {10, 20, 30};
    auto [i0, i1, i2] = arr;
    std::printf("数组拆开：%d %d %d\n", i0, i1, i2);

    Point pt{7, 8};
    auto [px, py] = pt;
    std::printf("结构体拆开：%d %d\n", px, py);

    auto &[rx, ry] = pt;
    std::printf("绑定取地址与原成员相同：%d %d\n", (int)(&rx == &pt.x), (int)(&ry == &pt.y));
    rx = 70;
    std::printf("通过绑定改名后 pt.x = %d\n", pt.x);

    const auto &[cx, cy] = pt;
    std::printf("const 绑定：%d %d\n", cx, cy);

    std::pair<const char *, int> table[3] = {{"甲", 1}, {"乙", 2}, {"丙", 3}};
    for (const auto &[k, v] : table) {
        std::printf("  %s -> %d\n", k, v);
    }
    return 0;
}
```


`实测数据`
`Text`

```text
pair 拆开：1 one
tuple 拆开：2 2.5 x
数组拆开：10 20 30
结构体拆开：7 8
绑定取地址与原成员相同：1 1
通过绑定改名后 pt.x = 70
const 绑定：70 8
  甲 -> 1
  乙 -> 2
  丙 -> 3
```


绑定出来的名字不是新变量，而是**原对象成员的别名**：
示例里 `&rx == &pt.x` 为真、通过 `rx` 改名会改到 `pt.x`，说明这一点。
因此 `auto [x, y] = p;` 是一次拷贝（`p` 不变），
而 `auto &[x, y] = p;` 与 `const auto &[x, y] = p;` 分别给出可写与只读的别名。
结构化绑定也适用于数组与聚合结构体，这使它成为遍历「键值对数组」最直接的写法：

（下面是节选）

`C++`

```cpp
for (const auto &[k, v] : table) {
    std::printf("  %s -> %d\n", k, v);
}
```

最容易错的地方是**名字个数与元素个数不等**：

`实测数据`
`Text`

```text
sb_arity_fail.cpp:6:10: error: 3 names provided for structured binding
     auto [a, b, c] = std::pair<int, int>{1, 2};   /* 两个元素，三个名字 */
          ^~~~~~~~~
sb_arity_fail.cpp:6:10: note: while 'std::pair<int, int>' decomposes into 2 elements
```

条款依据是结构化绑定声明与 `tuple_size` 的配合：

`文档`

> "The expression std::tuple_size<E>::value shall be a well-formed integral constant expression
> and the number of elements in the identifier-list shall be equal to the value of that expression."
>
> —— N4659 §11.5/3

**位域是一个常被误解的场合。** 绑定到含位域成员的结构体是合法的，标准明确规定绑定出来的名字「就是那个位域」：

`文档`

> "The lvalue is a bit-field if that member is a bit-field."
>
> —— N4659 §11.5/4

本机 `g++` 15.2.0 对下面这种写法直接放过（`Flags` 的两个成员都是位域）：

（下面是节选）

`C++`

```cpp
struct Flags { unsigned a : 3; unsigned b : 5; };
Flags f{1, 2};
auto &[x, y] = f;                 /* 合法：x、y 就是 f.a 与 f.b 这两个位域 */
```

## 2.3 大小与布局：为什么不是三个成员相加

`实测数据`
`C++`

```cpp
/* tuple_sizeof.cpp    编译：g++ -std=c++17 -Wall -Wextra tuple_sizeof.cpp -o tuple_sizeof */
#include <cstddef>
#include <cstdio>
#include <string>
#include <tuple>

struct Empty {};
struct Plain { char c; int i; double d; };

template <class T>
void show(const char *name) {
    std::printf("%-36s sizeof = %2zu  alignof = %zu\n", name, sizeof(T), alignof(T));
}

int main() {
    show<std::tuple<>>("tuple<>");
    show<std::tuple<char>>("tuple<char>");
    show<std::tuple<char, char>>("tuple<char, char>");
    show<std::tuple<int, int>>("tuple<int, int>");
    show<std::tuple<char, int, double>>("tuple<char, int, double>");
    show<std::tuple<double, int, char>>("tuple<double, int, char>");
    show<std::tuple<int, int, int>>("tuple<int, int, int>");
    show<std::tuple<Empty, Empty>>("tuple<Empty, Empty>");
    show<std::tuple<Empty, int, Empty>>("tuple<Empty, int, Empty>");
    show<std::tuple<double, char>>("tuple<double, char>");
    show<std::tuple<std::string>>("tuple<string>");
    show<Plain>("struct{char; int; double}");

    std::tuple<char, int, double> t{'a', 1, 2.5};
    const char *base = reinterpret_cast<const char *>(&t);
    std::printf("tuple<char, int, double> 三个元素相对对象首地址的偏移：%d、%d、%d\n",
                (int)(reinterpret_cast<const char *>(&std::get<0>(t)) - base),
                (int)(reinterpret_cast<const char *>(&std::get<1>(t)) - base),
                (int)(reinterpret_cast<const char *>(&std::get<2>(t)) - base));
    return 0;
}
```


`实测数据`

| 类型 | `sizeof` | `alignof` |
|---|---|---|
| `tuple<>` | 1 | 1 |
| `tuple<char>` | 1 | 1 |
| `tuple<char, char>` | 2 | 1 |
| `tuple<int, int>` | 8 | 4 |
| `tuple<char, int, double>` | 16 | 8 |
| `tuple<double, int, char>` | 16 | 8 |
| `tuple<int, int, int>` | 12 | 4 |
| `tuple<Empty, Empty>` | 2 | 1 |
| `tuple<Empty, int, Empty>` | 8 | 4 |
| `tuple<std::string>` | 32 | 8 |
| `struct { char; int; double }` | 16 | 8 |

**「三个成员相加」是 1 + 4 + 8 = 13，实际是 16。** 多出来的 3 个字节来自对齐：
`double` 要落在 8 的倍数上，所以 `int` 之后要空出 4 个字节。
这与手写的 `struct` 完全一样，`tuple` 并没有额外的成员：没有「数量」字段，也没有标记字段。

**元素的排列顺序**是另一处与直觉不同的地方：本机把第 0 个元素放在最高的地址上。

`实测数据`
`Text`

```text
tuple<char, int, double> 三个元素相对对象首地址的偏移：12、8、0
```

`Text`

  ┌────────────┬──────────────┬────────────────┐
  │ get<2>     │ double       │ 偏移 0 到 7    │
  │ get<1>     │ int          │ 偏移 8 到 11   │
  │ get<0>     │ char         │ 偏移 12        │
  │ ——         │ 填充 3 字节  │ 偏移 13 到 15  │
  └────────────┴──────────────┴────────────────┘

这是 libstdc++ 用「递归继承」实现 `tuple` 的结果：
`tuple<char, int, double>` 继承自负责第 1、2 个元素的那一层，每一层把自己的元素当基类或成员，
最终布局是逆序的。《05-类与面向对象/11-模板.md》第 4 节讲过的类模板在这里变成了内存里的东西。
`tuple<Empty, Empty>` 是 2 字节这一行说明它也没有对空成员做「空基类优化」。

> [!TIP]
> **不要依赖元素顺序与 `sizeof`。** 标准没有规定 `tuple` 的布局，
> 换一个标准库实现（例如 MSVC 的 STL）顺序与大小都可能不同。
> 需要固定布局时写 `struct`，不要把 `tuple` 当二进制接口。

---

# 第 3 节 `optional`：把「可能没有值」写进类型

## 3.1 四种「可能没有」的写法

同一个任务——把一段文本解析成整数，解析不了时怎么办——有四代写法：

`实测数据`
`C++`

```cpp
/* optional_forms.cpp    编译：g++ -std=c++17 -Wall -Wextra optional_forms.cpp -o optional_forms */
#include <cstdio>
#include <cstdlib>
#include <optional>
#include <stdexcept>
#include <string>

/* 一、返回 bool，结果从出参拿 */
bool parse_bool(const char *s, int &out) {
    char *end = nullptr;
    long v = std::strtol(s, &end, 10);
    if (end == s || *end != '\0') return false;
    out = (int)v;
    return true;
}

/* 二、用哨兵值：约定 -1 表示没有 */
int parse_sentinel(const char *s) {
    char *end = nullptr;
    long v = std::strtol(s, &end, 10);
    if (end == s || *end != '\0') return -1;
    return (int)v;
}

/* 三、抛异常：没有值时走异常路径 */
int parse_throw(const char *s) {
    char *end = nullptr;
    long v = std::strtol(s, &end, 10);
    if (end == s || *end != '\0') throw std::invalid_argument("不是整数");
    return (int)v;
}

/* 四、optional：类型里就写着可能没有 */
std::optional<int> parse_opt(const char *s) {
    char *end = nullptr;
    long v = std::strtol(s, &end, 10);
    if (end == s || *end != '\0') return std::nullopt;
    return (int)v;
}

int main() {
    const char *good = "42";
    const char *bad = "x42";

    int v = -7;
    bool ok1 = parse_bool(good, v);
    std::printf("bool + 出参：\"%s\" ok=%d v=%d\n", good, (int)ok1, v);
    bool ok2 = parse_bool(bad, v);
    std::printf("             \"%s\" ok=%d v=%d（v 保持上一次的值，调用方不能忘）\n",
                bad, (int)ok2, v);

    std::printf("哨兵值：\"%s\" -> %d\n", good, parse_sentinel(good));
    std::printf("        \"%s\" -> %d（分不清「没有」与「值就是 -1」）\n",
                bad, parse_sentinel(bad));

    try {
        std::printf("抛异常：\"%s\" -> %d\n", good, parse_throw(good));
        std::printf("        \"%s\" -> %d\n", bad, parse_throw(bad));
    } catch (const std::invalid_argument &e) {
        std::printf("        抛出 %s\n", e.what());
    }

    std::optional<int> o1 = parse_opt(good);
    std::optional<int> o2 = parse_opt(bad);
    std::printf("optional：\"%s\" has_value=%d 值=%d\n", good, (int)o1.has_value(), *o1);
    std::printf("          \"%s\" has_value=%d value_or(-1)=%d\n",
                bad, (int)o2.has_value(), o2.value_or(-1));
    return 0;
}
```


`实测数据`
`Text`

```text
bool + 出参："42" ok=1 v=42
             "x42" ok=0 v=42（v 保持上一次的值，调用方不能忘）
哨兵值："42" -> 42
        "x42" -> -1（分不清「没有」与「值就是 -1」）
抛异常："42" -> 42
        抛出 不是整数
optional："42" has_value=1 值=42
          "x42" has_value=0 value_or(-1)=-1
```


| 写法 | 没有值时 | 调用方要做什么 | 主要问题 |
|---|---|---|---|
| `bool` 返回值加出参 | 返回 `false`，出参保持原值 | 检查返回值，并且**不要**用出参 | 两处信息要手动配对；出参是「写出去」的，容易忘 |
| 返回哨兵值（如 `-1`） | 返回约定值 | 与约定值比较 | 合法结果与该值撞车时分不清（`-1` 本身是合法整数） |
| 抛异常 | 走异常路径 | 用 `try` 包住调用 | 「没有值」是正常情况时异常太重，也打断控制流 |
| `std::optional<T>` | 返回 `nullopt` | 用 `has_value()`、`value_or()` 或 `if` 判断 | 多一个字节的状态标记，仅此而已 |

**`optional` 改变的是「谁保证这件事」。** 前三种写法里，「这次可能没有值」是一条注释里的约定；
`optional<int>` 把它写进了返回类型，调用方从函数签名就能看出来，
而且**忘记检查的代价从「读到垃圾」变成「抛异常」**（第 3.3 小节实测）。

## 3.2 接口：`value`、`value_or`、`operator*`

`实测数据`
`C++`

```cpp
/* optional_api.cpp    编译：g++ -std=c++17 -Wall -Wextra optional_api.cpp -o optional_api */
#include <cstdio>
#include <optional>
#include <string>

struct Config {
    std::string host = "localhost";
    int port = 80;
};

std::optional<Config> load(bool present) {
    if (!present) return std::nullopt;          /* 明确的「没有」 */
    return Config{"example.org", 8080};
}

int main() {
    std::optional<int> a;                       /* 默认构造：空 */
    std::printf("a 有值吗：%d\n", (int)a.has_value());

    a = 5;
    std::printf("a 有值吗：%d，*a = %d，value() = %d，value_or(9) = %d\n",
                (int)a.has_value(), *a, a.value(), a.value_or(9));

    a.reset();
    std::printf("reset 后：has_value = %d，value_or(9) = %d\n",
                (int)a.has_value(), a.value_or(9));

    a.emplace(7);
    std::printf("emplace 后：%d\n", *a);

    std::optional<std::string> s = std::nullopt;
    s = "hello";
    std::printf("s->size() = %zu\n", s->size());

    std::optional<Config> c1 = load(true);
    std::optional<Config> c2 = load(false);
    if (c1) {
        std::printf("c1：%s:%d\n", c1->host.c_str(), c1->port);
    }
    std::printf("c2 有值吗：%d\n", (int)c2.has_value());

    std::optional<int> x = 1, y = 1, z = 2;
    std::printf("比较：x==y %d，x<z %d，空 < 1 %d\n",
                (int)(x == y), (int)(x < z), (int)(std::optional<int>{} < x));

    c1.swap(c2);
    std::printf("swap 后 c1 有值吗：%d\n", (int)c1.has_value());
    return 0;
}
```


`实测数据`
`Text`

```text
a 有值吗：0
a 有值吗：1，*a = 5，value() = 5，value_or(9) = 5
reset 后：has_value = 0，value_or(9) = 9
emplace 后：7
s->size() = 5
c1：example.org:8080
c2 有值吗：0
比较：x==y 1，x<z 1，空 < 1 1
swap 后 c1 有值吗：0
```


取值口子的分工：

| 写法 | 空值时 | 什么时候用 |
|---|---|---|
| `*o` 与 `o->member` | **不做检查**，读未构造的存储 | 已经确认有值，写在 `if` 里面 |
| `o.value()` | 抛 `std::bad_optional_access` | 走到这里没值就是逻辑错了 |
| `o.value_or(x)` | 返回 `x` | 有默认值时最直接 |
| `if (o)` 或 `o.has_value()` | 判假 | 要先判断再决定怎么处理 |

`reset()` 把值丢掉回到空状态，`emplace(args...)` 原地构造一个新值，`swap` 交换两个 `optional`。
比较运算符与「有值」这件事一致：**空值小于任何有值的 `optional`**，
两个都有值时比较里面的值（示例里 `空 < 1` 为真）。

## 3.3 空值上取值的两种后果

`实测数据`
`C++`

```cpp
/* optional_empty_throw.cpp    编译：g++ -std=c++17 -Wall -Wextra optional_empty_throw.cpp -o optional_empty_throw */
#include <cstdio>
#include <optional>
#include <stdexcept>

std::optional<int> divide(int a, int b) {
    if (b == 0) return std::nullopt;
    return a / b;
}

int main() {
    std::optional<int> r = divide(10, 0);
    std::printf("has_value = %d\n", (int)r.has_value());
    try {
        std::printf("value() = %d\n", r.value());     /* 空值上调用 value() 会抛 */
    } catch (const std::bad_optional_access &e) {
        std::printf("捕获：%s\n", e.what());
    }
    std::printf("value_or(-1) = %d\n", r.value_or(-1));
    /* 下面这行不能写：*r 不做检查，读的是未构造的存储
       std::printf("%d\n", *r); */
    return 0;
}
```


`实测数据`
`Text`

```text
has_value = 0
捕获：bad optional access
value_or(-1) = -1
```


`value()` 抛出的类型是 `std::bad_optional_access`，本机 `what()` 的文本是 `bad optional access`。
异常里没有任何关于「为什么没有值」的信息，这也说明它适合表达
「按约定这里必然有值，没有就是 bug」，而不是用来传递业务错误。

> [!CAUTION]
> **`*o` 不检查有没有值。** 在空的 `optional` 上写 `*o` 读到的是未构造的存储，属于未定义行为：
> 可能得到一个垃圾值，也可能在开了优化之后表现得更奇怪。
> `operator*` 只是「取值」的简写，不做任何检查。

## 3.4 `optional` 有多大

`实测数据`
`C++`

```cpp
/* optional_size.cpp    编译：g++ -std=c++17 -Wall -Wextra optional_size.cpp -o optional_size */
#include <cstdio>
#include <optional>
#include <string>

template <class T>
void show(const char *name) {
    std::optional<T> o{};
    std::printf("%-30s sizeof = %2zu  alignof = %zu   sizeof(T) = %2zu\n",
                name, sizeof(o), alignof(std::optional<T>), sizeof(T));
}

int main() {
    show<char>("optional<char>");
    show<int>("optional<int>");
    show<long long>("optional<long long>");
    show<double>("optional<double>");
    show<const char *>("optional<const char*>");
    show<std::string>("optional<std::string>");

    std::optional<int> oi{5};
    std::printf("optional<int> 里值的偏移：%d\n",
                (int)(reinterpret_cast<const char *>(&*oi) - reinterpret_cast<const char *>(&oi)));
    std::optional<std::string> os{"x"};
    std::printf("optional<string> 里值的偏移：%d\n",
                (int)(reinterpret_cast<const char *>(&*os) - reinterpret_cast<const char *>(&os)));
    std::printf("sizeof(optional<char>) == sizeof(char)+1：%d\n",
                (int)(sizeof(std::optional<char>) == sizeof(char) + 1));
    return 0;
}
```


`实测数据`

| 类型 | `sizeof` | `alignof` | `sizeof(T)` |
|---|---|---|---|
| `optional<char>` | 2 | 1 | 1 |
| `optional<int>` | 8 | 4 | 4 |
| `optional<long long>` | 16 | 8 | 8 |
| `optional<double>` | 16 | 8 | 8 |
| `optional<const char *>` | 16 | 8 | 8 |
| `optional<std::string>` | 40 | 8 | 32 |

规律是 **`sizeof(T)` 加上一个字节的状态标记，再按 `alignof(T)` 向上取整**：
`optional<int>` 是 4 + 1 取整到 8，`optional<double>` 是 8 + 1 取整到 16。
本机的实现把值放在偏移 0、标记放在最后（实测两个偏移都是 0），但这一条同样不是标准规定的。

> [!TIP]
> **`optional<T>` 不会替 `T` 省任何东西。** 里面的值就是完整的 `T`，
> 因此 `optional<std::string>` 与 `std::string` 一样占 32 字节，只多出一个标记字节与它带来的填充。
> 链式的 `optional<optional<int>>` 没有意义，嵌套一层只会多出状态标记。

## 3.5 为什么没有 `optional<T&>`

`std::optional<T&>` 无法写出。下面两行就是全部代码（下面是节选）：

`C++`

```cpp
int v = 1;
std::optional<int &> r;        /* 引用不能做 optional 的模板实参 */
```

本机的报错是这样的：

`实测数据`
`Text`

```text
（只摘出与本例有关的三行，库内部路径已省去）

optional_ref_fail.cpp:6:26:   required from here
error: non-static data member 'std::_Optional_payload_base<int&>::_Storage<int&, true>::_M_value'
       in a union may not have reference type 'int&'
error: static assertion failed
note: 'std::is_object_v<int&>' evaluates to false
error: forming pointer to reference type 'int&'
```

三条报错指向同一件事：**引用不是对象类型**（`is_object_v<int&>` 为假，
这个判断本身见《06-标准库/B-09-工具类（下）：type_traits 与 concepts.md》第 2.2 小节）。
`optional` 内部要用一个联合体存放「值或空」，而联合体的成员不能是引用；
`operator->` 要形成指向值的指针，而「指向引用的指针」不存在。

设计上的理由更根本：**引用一旦绑定就不能改绑，而 `optional` 的语义是「值可以来去」。**
一个「可以是空、之后又能指向别处」的东西，本质上就是指针。
真要这个语义，应当写 `T *`（可能为空、可以改指），语义与限制都摆在明面上。

两种替代写法各写一遍：

`实测数据`
`C++`

```cpp
/* optional_ref_workaround.cpp    编译：g++ -std=c++17 -Wall -Wextra optional_ref_workaround.cpp -o optional_ref_workaround */
#include <cstdio>
#include <functional>
#include <optional>
#include <string>

struct Settings {
    std::string host = "localhost";
    int port = 80;
};

Settings g_user;                             /* 两个真实存在的配置对象 */
Settings g_system;

/* 一、指针版：可能为空、可以改指，语义与想要的 optional<T&> 完全一致 */
Settings *find_ptr(const std::string &key) {
    if (key == "user") return &g_user;
    if (key == "system") return &g_system;
    return nullptr;
}

/* 二、把引用包成对象：reference_wrapper 是可拷贝的对象，于是能放进 optional */
std::optional<std::reference_wrapper<Settings>> find_ref(const std::string &key) {
    if (Settings *p = find_ptr(key)) return std::ref(*p);
    return std::nullopt;
}

int main() {
    std::printf("sizeof(optional<reference_wrapper<Settings>>) = %zu，sizeof(optional<Settings*>) = %zu\n",
                sizeof(std::optional<std::reference_wrapper<Settings>>),
                sizeof(std::optional<Settings *>));

    if (Settings *p = find_ptr("missing")) {
        std::printf("不该走到这里：%s\n", p->host.c_str());
    } else {
        std::printf("指针版：键不存在，拿到空指针\n");
    }

    std::optional<std::reference_wrapper<Settings>> r = find_ref("user");
    if (r) {
        r->get().port = 8080;                /* 改的是 g_user 本身，不是副本 */
        std::printf("引用版取到 g_user：port = %d，地址相同 = %d\n",
                    g_user.port, (int)(&r->get() == &g_user));
    }

    r = std::ref(g_system);                  /* 可以改指：真引用做不到这件事 */
    std::printf("改指之后是 g_system：port = %d，地址相同 = %d\n",
                r->get().port, (int)(&r->get() == &g_system));

    r = std::nullopt;                        /* 也能回到「没有」 */
    std::printf("置空之后 has_value = %d\n", (int)r.has_value());
    return 0;
}
```

`实测数据`
`Text`

```text
sizeof(optional<reference_wrapper<Settings>>) = 16，sizeof(optional<Settings*>) = 16
指针版：键不存在，拿到空指针
引用版取到 g_user：port = 8080，地址相同 = 1
改指之后是 g_system：port = 80，地址相同 = 1
置空之后 has_value = 0
```

`std::reference_wrapper` 是 `<functional>` 里的一个可拷贝对象，内部就是一个指针，`get()` 拿回原来的引用。
它在 `optional` 里能做真引用做不到的两件事：**改指**（`r = std::ref(g_system);`）与**置空**
（`r = std::nullopt;`）——这正是 `optional` 需要的语义，也再次说明真引用为什么放不进来。
两种写法的大小一样（实测都是 16 字节），选哪一种只看可读性：指针版少一层包装，
`reference_wrapper` 版把「解引用前先判断」交回给 `optional` 自己的接口。

---

# 第 4 节 `variant`：多选一

## 4.1 与 `union` 的区别

C 与 C++ 都有 `union`：几个成员共享同一块内存（《04-语法/09-结构体、联合体与 enum.md》第 2 节）。
`union` 的问题在于**它不知道当前装的是哪一个成员**，也不会在换成员时构造新对象、析构旧对象。
`std::variant<Ts...>` 补上这两件事：它记住当前是第几个备选，并在切换时调构造与析构。

`实测数据`
`C++`

```cpp
/* variant_basic.cpp    编译：g++ -std=c++17 -Wall -Wextra variant_basic.cpp -o variant_basic */
#include <cstdio>
#include <string>
#include <variant>

int main() {
    std::variant<int, std::string> v;         /* 默认构造：装第一个备选 */
    std::printf("默认：index = %zu，holds int = %d\n",
                v.index(), (int)std::holds_alternative<int>(v));

    v = 42;
    std::printf("赋 int：index = %zu，值 = %d\n", v.index(), std::get<int>(v));

    v = std::string("文本");
    std::printf("赋 string：index = %zu，值 = %s\n",
                v.index(), std::get<std::string>(v).c_str());
    std::printf("现在 holds int = %d，holds string = %d\n",
                (int)std::holds_alternative<int>(v),
                (int)std::holds_alternative<std::string>(v));

    if (auto *p = std::get_if<int>(&v)) {
        std::printf("取到 int：%d\n", *p);
    } else {
        std::printf("get_if<int> 给空指针，值还在\n");
    }
    if (auto *p = std::get_if<std::string>(&v)) {
        std::printf("get_if<string> 取到：%s\n", p->c_str());
    }

    std::variant<int, std::string> w{std::in_place_type<std::string>, 3, 'x'};
    std::printf("按类型原地构造：%s\n", std::get<std::string>(w).c_str());

    std::variant<int, std::string> z{std::in_place_index<1>, "按次序"};
    std::printf("按下标原地构造：index = %zu，值 = %s\n",
                z.index(), std::get<1>(z).c_str());

    std::printf("相等比较：v == z ？%d\n", (int)(v == z));

    std::variant<std::monostate, int> m;      /* monostate 占一个「空」备选 */
    std::printf("monostate：index = %zu，holds monostate = %d\n",
                m.index(), (int)std::holds_alternative<std::monostate>(m));
    return 0;
}
```


`实测数据`
`Text`

```text
默认：index = 0，holds int = 1
赋 int：index = 0，值 = 42
赋 string：index = 1，值 = 文本
现在 holds int = 0，holds string = 1
get_if<int> 给空指针，值还在
get_if<string> 取到：文本
按类型原地构造：xxx
按下标原地构造：index = 1，值 = 按次序
相等比较：v == z ？0
monostate：index = 0，holds monostate = 1
```


| 操作 | 说明 |
|---|---|
| 默认构造 | 装**第一个**备选（示例里是 `int`，因此 `index()` 为 0） |
| `index()` | 当前是第几个备选，从 0 开始 |
| `holds_alternative<T>(v)` | 当前装的是不是 `T` |
| `get<T>(v)` / `get<I>(v)` | 取出值，类型不符时抛异常 |
| `get_if<T>(&v)` | 取出指针，类型不符时给空指针，**不抛异常** |
| `in_place_type<T>` / `in_place_index<I>` | 明确指定装哪一个备选 |
| `std::monostate` | 一个空类型，用来让 `variant` 有一个「什么都没有」的备选 |

`variant` 的比较运算符按「先比 `index`，再比值」的规则工作，
因此 `int` 与 `std::string` 混在一起也能比较：下标不同就先分出结果，不会走到比较值那一步。

## 4.2 `visit`：把「当前是哪个」变成一次调用

`variant` 的价值在 `std::visit` 上：它把「当前装的是哪一个」变成一次普通的函数调用，
调用点不必写 `if` 链，也不会漏掉某个备选——**漏掉时通不过编译**。

`实测数据`
`C++`

```cpp
/* variant_visit.cpp    编译：g++ -std=c++17 -Wall -Wextra variant_visit.cpp -o variant_visit */
#include <cstdio>
#include <string>
#include <type_traits>
#include <variant>

using Value = std::variant<int, double, std::string>;

/* 一组 lambda 合成一个可调用物：using 把各自的 operator() 都带进来 */
template <class... Ts>
struct overloaded : Ts... { using Ts::operator()...; };
template <class... Ts>
overloaded(Ts...) -> overloaded<Ts...>;        /* C++17 的推导指引 */

int main() {
    Value a = 7, b = 2.5, c = std::string("三");

    auto printer = overloaded{
        [](int x) { std::printf("  overloaded int：%d\n", x); },
        [](double x) { std::printf("  overloaded double：%.1f\n", x); },
        [](const std::string &x) { std::printf("  overloaded string：%s\n", x.c_str()); }
    };
    std::visit(printer, a);
    std::visit(printer, b);
    std::visit(printer, c);

    /* visit 可以有返回值：把当前值折算成一个整数 */
    int n = std::visit([](const auto &x) -> int {
        using T = std::decay_t<decltype(x)>;
        if constexpr (std::is_same_v<T, std::string>) return (int)x.size();
        else return (int)x;
    }, a);
    std::printf("visit 有返回值：%d\n", n);

    /* 一次访问两个 variant */
    auto text = [](const auto &x) -> std::string {
        if constexpr (std::is_same_v<std::decay_t<decltype(x)>, std::string>) return x;
        else return std::to_string(x);
    };
    std::variant<int, std::string> u = 3, w = std::string("abc");
    std::string joined = std::visit(
        [&](const auto &p, const auto &q) { return text(p) + "/" + text(q); }, u, w);
    std::printf("一次访问两个 variant：%s\n", joined.c_str());
    return 0;
}
```


`实测数据`
`Text`

```text
  overloaded int：7
  overloaded double：2.5
  overloaded string：三
visit 有返回值：7
一次访问两个 variant：3/abc
```


三种 visitor 写法各有位置：

| 写法 | 形状 | 适合 |
|---|---|---|
| 泛型 lambda 加 `if constexpr` | 一个 lambda，内部按类型分支 | 分支少、逻辑相近 |
| `overloaded` 加一组 lambda | 每个类型一个小 lambda | 每个类型处理方式差别大 |
| 普通函数对象 | 若干个 `operator()` | 需要复用、需要状态 |

`overloaded` 那几行是 C++17 的固定写法：继承一组 lambda 的类型，
用 `using Ts::operator()...` 把它们的 `operator()` 都带进来，再靠一条推导指引省掉显式指定模板实参。
它用到的两个机制（继承与 `using` 展开、类模板实参推导）分别见
《05-类与面向对象/07-继承.md》第 4 节与《05-类与面向对象/11-模板.md》第 4.8 小节。

`visit` 可以有返回值，也可以一次访问两个 `variant`（此时 visitor 收两个实参，四种组合都要能处理）。
`std::to_string` 对浮点固定输出 6 位小数，要别的格式须用 `std::ostringstream`
（《06-标准库/B-01-输入输出：iostream.md》第 6.2 小节写的就是拼字符串）。

## 4.3 取错类型的后果

`get<T>` 在类型不符时抛 `std::bad_variant_access`，`get_if` 则给空指针：

`实测数据`
`C++`

```cpp
/* variant_get_throw.cpp    编译：g++ -std=c++17 -Wall -Wextra variant_get_throw.cpp -o variant_get_throw */
#include <cstdio>
#include <string>
#include <variant>

int main() {
    std::variant<int, std::string> v = 3;
    try {
        std::printf("%s\n", std::get<std::string>(v).c_str());   /* 类型不对 */
    } catch (const std::bad_variant_access &e) {
        std::printf("get 错类型抛出：%s\n", e.what());
    }
    if (std::get_if<std::string>(&v) == nullptr) {
        std::printf("get_if 不抛，给空指针\n");
    }
    std::printf("值没丢：index = %zu，get<int> = %d\n", v.index(), std::get<int>(v));
    return 0;
}
```


`实测数据`
`Text`

```text
get 错类型抛出：std::get: wrong index for variant
get_if 不抛，给空指针
值没丢：index = 0，get<int> = 3
```


抛出的异常不会破坏 `variant` 的状态：示例最后一行打印出原来的 `index = 0`，值仍然保留在对象中。

**另一种「取错」根本到不了运行期**：备选里有两个相同类型时，按类型取无法成立。
出错的是下面这两行（下面是节选）：

`C++`

```cpp
std::variant<int, int, double> v{std::in_place_index<1>, 5};
std::printf("%d\n", std::get<int>(v));    /* 两个 int 备选：按类型取不成立 */
```

`实测数据`
`Text`

```text
（只摘出与本例有关的两行）

error: static assertion failed: T must occur exactly once in alternatives
note: 'std::__detail::__variant::__exactly_once<int, int, int, double>' evaluates to false
```

`variant<int, int, double>` 本身合法，按下标取也没问题，只有 `get<int>` 这一种写法被挡住：
要区分两个 `int`，只能用 `get<0>`、`get<1>`。

## 4.4 `valueless_by_exception`：一个真实存在的第三状态

`variant` 有一个别的类型没有的状态：**既不是某一个备选，也不是「空」**。
它出现在「换备选的过程中构造新值抛了异常」的时候：

`实测数据`
`C++`

```cpp
/* variant_valueless.cpp    编译：g++ -std=c++17 -Wall -Wextra variant_valueless.cpp -o variant_valueless */
#include <cstdio>
#include <stdexcept>
#include <variant>

/* 移动构造会抛的备选类型：换备选的过程中抛异常，variant 可能丢掉当前值 */
struct Bomb {
    int v = 0;
    Bomb() = default;
    Bomb(int x) : v(x) {}
    Bomb(const Bomb &) = default;                              /* 拷贝不抛 */
    Bomb(Bomb &&) { throw std::runtime_error("移动构造抛了"); }  /* 移动会抛 */
    Bomb &operator=(const Bomb &) = default;
    Bomb &operator=(Bomb &&) { throw std::runtime_error("移动赋值抛了"); }
};

int main() {
    std::variant<int, Bomb> v;
    std::printf("开始：index = %zu，valueless = %d\n",
                v.index(), (int)v.valueless_by_exception());

    try {
        v = Bomb{1};                    /* 当前装 int，要换成 Bomb */
    } catch (const std::runtime_error &e) {
        std::printf("赋值抛出：%s\n", e.what());
    }
    std::printf("之后：valueless = %d，index = %zu（variant_npos = %zu）\n",
                (int)v.valueless_by_exception(), v.index(), std::variant_npos);

    try {
        std::printf("取 int：%d\n", std::get<int>(v));
    } catch (const std::bad_variant_access &e) {
        std::printf("取值抛出：%s\n", e.what());
    }
    (void)std::get_if<0>(&v);           /* get_if 在 valueless 时也给空指针 */

    v.emplace<0>(5);                    /* 重新装一个值就恢复 */
    std::printf("emplace 之后：valueless = %d，值 = %d\n",
                (int)v.valueless_by_exception(), std::get<0>(v));
    return 0;
}
```


`实测数据`
`Text`

```text
开始：index = 0，valueless = 0
赋值抛出：移动构造抛了
之后：valueless = 1，index = 18446744073709551615（variant_npos = 18446744073709551615）
取值抛出：std::get: variant is valueless
emplace 之后：valueless = 0，值 = 5
```


`Bomb` 的移动构造会抛。赋值时 `variant` 先析构掉原来的 `int`，再去构造 `Bomb`，
构造失败于是**两个备选都不再持有**。此时 `valueless_by_exception()` 为真，
`index()` 返回 `std::variant_npos`（本机是 `18446744073709551615`，也就是 `size_t` 的 -1），
任何 `get` 都抛 `std::bad_variant_access`。

标准对这一点的措辞是「可能没有值」，不是「必然没有值」——实现可以选择别的做法：

`文档`

> "If an exception is thrown during the initialization of the contained value,
> the variant object might not hold a value."
>
> —— N4659 §23.7.3.3/16.2

走到这条路上的条件是：**备选类型的拷贝构造不抛、而移动构造会抛**时，
实现选择「直接原地构造」而不是「先造临时对象再移动」，中途抛异常就会丢掉当前值。
`emplace` 也一样（N4659 §23.7.3.4/11：初始化过程中抛出的任何异常都会传播出去）。
备选类型的移动构造写成 `noexcept`（这是好的做法，见《05-类与面向对象/05-拷贝与移动.md》第 4 节）
就不会走到这条路上。

> [!WARNING]
> **`valueless_by_exception()` 必须当成第三种状态处理。**
> 只写 `if (holds_alternative<int>(v)) ... else ...` 是不完整的：
> 走到 `else` 分支时可能既不是 `int` 也不是别的备选，而是 valueless。
> 恢复办法很简单——`v.emplace<0>(5)` 重新装一个值（示例最后一行）。

## 4.5 大小，以及与继承方案的取舍

`实测数据`
`C++`

```cpp
/* variant_size.cpp    编译：g++ -std=c++17 -Wall -Wextra variant_size.cpp -o variant_size */
#include <cstddef>
#include <cstdio>
#include <string>
#include <variant>

struct Big { char data[40]; };

template <class V>
void show(const char *name) {
    std::printf("%-34s sizeof = %2zu  alignof = %zu  备选数 = %zu\n",
                name, sizeof(V), alignof(V), std::variant_size<V>::value);
}

int main() {
    show<std::variant<int>>("variant<int>");
    show<std::variant<char, char>>("variant<char, char>");
    show<std::variant<char, int>>("variant<char, int>");
    show<std::variant<int, double>>("variant<int, double>");
    show<std::variant<int, std::string>>("variant<int, string>");
    show<std::variant<int, Big>>("variant<int, Big>");
    show<std::variant<std::monostate, int>>("variant<monostate, int>");
    std::printf("对照：sizeof(int) = %zu，sizeof(double) = %zu，sizeof(string) = %zu，sizeof(Big) = %zu\n",
                sizeof(int), sizeof(double), sizeof(std::string), sizeof(Big));
    return 0;
}
```


`实测数据`

| 类型 | `sizeof` | `alignof` | 备选数 |
|---|---|---|---|
| `variant<int>` | 8 | 4 | 1 |
| `variant<char, char>` | 2 | 1 | 2 |
| `variant<char, int>` | 8 | 4 | 2 |
| `variant<int, double>` | 16 | 8 | 2 |
| `variant<int, std::string>` | 40 | 8 | 2 |
| `variant<int, Big>`（`Big` 是 40 字节） | 44 | 4 | 2 |
| `variant<std::monostate, int>` | 8 | 4 | 2 |

规律与 `optional` 类似：**最大备选的大小，加上一个下标，再按最大对齐向上取整**。
`variant<int, Big>` 是 40 + 1 取整到 44，`variant<int, double>` 是 8 + 1 取整到 16。

同一份测试程序在 WSL 的 `g++` 13.3.0（Linux x86-64，同样是 libstdc++）下实测，数字与上表逐行一致：
`tuple<char, int, double>` 是 16（元素偏移 12、8、0），`tuple<Empty, Empty>` 是 2，`pair<char, int>` 是 8，
`optional<int>` 是 8，`optional<std::string>` 是 40，`variant<int, double>` 是 16，
`variant<int, std::string>` 是 40，`variant<int, Big>` 是 44，`std::any` 是 16，`std::string` 是 32。
两边是同一个实现，因此这只说明**同一份实现在这两个平台上的结果一致**，不能说明 MSVC 的 STL 也一样。

**与继承加虚函数相比，`variant` 的取舍是明确的。**

| 维度 | `variant` 加 `visit` | 基类加虚函数 |
|---|---|---|
| 备选集合 | **封闭**：编译期写死，加一种要改类型 | **开放**：可以在别的头文件里新增派生类 |
| 分派方式 | 编译期生成跳转，不需要虚表 | 一次虚表间接跳转（《05-类与面向对象/08-多态：重载、虚函数与它们的分工.md》第 6 节） |
| 漏掉一种备选 | **通不过编译** | 编译通过，运行期行为由基类默认实现决定 |
| 大小 | 最大备选加下标（示例里 44 字节） | 对象本身加一个虚指针（64 位下 8 字节） |
| 值语义 | 就是值，可以拷贝、比较、放进容器 | 通常要用指针或引用，涉及所有权 |

**判断标准一句话**：类型的种类在编译期就定死、而且需要值语义时用 `variant`；
需要别人在不改原有代码的前提下增加新类型时用虚函数。

---

# 第 5 节 `any`：什么都能装

## 5.1 用法

`std::any` 可以装任何**可拷贝构造**的类型，取值时用 `any_cast<T>`：

`实测数据`
`C++`

```cpp
/* any_basic.cpp    编译：g++ -std=c++17 -Wall -Wextra any_basic.cpp -o any_basic */
#include <any>
#include <cstdio>
#include <string>

int main() {
    std::any a;
    std::printf("空 any：has_value = %d\n", (int)a.has_value());

    a = 42;
    std::printf("装 int：type().name() = %s，any_cast<int> = %d\n",
                a.type().name(), std::any_cast<int>(a));

    a = std::string("文本");
    std::printf("装 string：type().name() = %s\n", a.type().name());
    std::printf("          值 = %s\n", std::any_cast<std::string>(a).c_str());

    a = 2.5;
    if (double *p = std::any_cast<double>(&a)) {     /* 指针版：失败给空指针 */
        std::printf("指针版取到：%.1f\n", *p);
    }
    if (std::any_cast<int>(&a) == nullptr) {
        std::printf("按 int 取：空指针\n");
    }

    a.emplace<std::string>(3, 'x');                  /* 原地构造 */
    std::printf("emplace 后：%s\n", std::any_cast<std::string>(a).c_str());

    std::any b = a;
    std::printf("拷贝后类型相同：%d\n", (int)(b.type() == a.type()));
    a.reset();
    std::printf("reset 后：has_value = %d\n", (int)a.has_value());
    b.swap(a);
    std::printf("swap 后 b 空 = %d\n", (int)!b.has_value());
    return 0;
}
```


`实测数据`
`Text`

```text
空 any：has_value = 0
装 int：type().name() = i，any_cast<int> = 42
装 string：type().name() = NSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEE
          值 = 文本
指针版取到：2.5
按 int 取：空指针
emplace 后：xxx
拷贝后类型相同：1
reset 后：has_value = 0
swap 后 b 空 = 1
```


取值有两个版本：`any_cast<T>(a)` 返回值的副本，类型不符时抛 `std::bad_any_cast`；
`any_cast<T>(&a)` 返回指针，类型不符时给空指针。
`a.type()` 返回一个 `std::type_info`，`name()` 给出的是**实现定义的名字修饰结果**——
示例里 `int` 是 `i`，`std::string` 是一长串。

> [!WARNING]
> **`any_cast<std::string>(a).c_str()` 里的临时对象只活到语句结束。**
> 把 `c_str()` 的结果存进一个指针，出了这一行就是悬垂指针。
> 需要留住字符串就先拷贝一份 `std::string`。

## 5.2 代价一：堆分配

`实测数据`
`C++`

```cpp
/* any_cost.cpp    编译：g++ -std=c++17 -Wall -Wextra any_cost.cpp -o any_cost */
#include <any>
#include <cstdio>
#include <cstdlib>
#include <new>
#include <string>

static long g_allocs = 0;

/* 替换全局 operator new：数一数 any 构造时在堆上分配了几次 */
void *operator new(std::size_t n) {
    ++g_allocs;
    if (void *p = std::malloc(n)) return p;
    throw std::bad_alloc();
}
void operator delete(void *p) noexcept { std::free(p); }
void operator delete(void *p, std::size_t) noexcept { std::free(p); }

struct Point { int x, y; };
struct Big { char data[32]; };

template <class T>
void measure(const char *name, const T &value) {
    long before = g_allocs;
    std::any a = value;
    std::printf("%-28s sizeof(T) = %2zu   构造时堆分配 %ld 次\n",
                name, sizeof(T), g_allocs - before);
    (void)a;
}

int main() {
    std::printf("sizeof(std::any) = %zu，内部缓冲区 = %zu 字节（一个指针那么大）\n",
                sizeof(std::any), sizeof(void *));
    measure<int>("int", 42);
    measure<double>("double", 2.5);
    measure<Point>("Point{int,int}", Point{1, 2});
    measure<Big>("Big{char[32]}", Big{});
    measure<std::string>("std::string（短，走 SSO）", std::string("short"));
    return 0;
}
```


`实测数据`
`Text`

```text
sizeof(std::any) = 16，内部缓冲区 = 8 字节（一个指针那么大）
int                          sizeof(T) =  4   构造时堆分配 0 次
double                       sizeof(T) =  8   构造时堆分配 0 次
Point{int,int}               sizeof(T) =  8   构造时堆分配 0 次
Big{char[32]}                sizeof(T) = 32   构造时堆分配 1 次
std::string（短，走 SSO） sizeof(T) = 32   构造时堆分配 1 次
```


本机的 `std::any` 是 16 字节，内部有一个**指针大小（8 字节）的缓冲区**：
装得下就放在里面（前三行都没有分配），装不下就在堆上分配一块，
一个 32 字节的结构体因此多了一次 `operator new`。
一次堆分配在多数程序里不算什么，但在「每个元素都装进 `any`」的循环里就是另一种量级。

`std::string` 那一行需要单独说明：字符串短到走 SSO 时它自己不分配，
可 `std::string` 对象本身是 32 字节，装不进 8 字节的缓冲区，于是这一次分配来自 `any`。
**「小对象优化」在这里只覆盖到 8 字节**——这是本机实现的取值，不是标准的要求。

## 5.3 代价二：运行期比较，以及取错类型

`any_cast` 在运行期比较 `type_info`：先看类型对不对，再决定是返回副本还是抛异常。

`实测数据`
`C++`

```cpp
/* any_cast_throw.cpp    编译：g++ -std=c++17 -Wall -Wextra any_cast_throw.cpp -o any_cast_throw */
#include <any>
#include <cstdio>
#include <string>

int main() {
    std::any a = std::string("文本");
    try {
        int v = std::any_cast<int>(a);          /* 类型不对 */
        std::printf("%d\n", v);
    } catch (const std::bad_any_cast &e) {
        std::printf("any_cast 抛出：%s\n", e.what());
    }
    std::printf("换成指针版：%d\n", (int)(std::any_cast<int>(&a) == nullptr));
    return 0;
}
```


`实测数据`
`Text`

```text
any_cast 抛出：bad any_cast
换成指针版：1
```


`bad_any_cast` 的 `what()` 文本是 `bad any_cast`，同样不含任何细节。

**`any` 丢掉的是类型信息，而类型信息正是编译器用来检查的东西。**
三处代价合起来决定了它该出现在哪：

| 情形 | 该用什么 | 理由 |
|---|---|---|
| 备选类型在编译期就能列出来 | `std::variant` | 类型安全，取值不抛异常，大小可控 |
| 需要的是「任意类型都能用同一套逻辑」 | 模板（或 C++20 的概念） | 编译期分派，没有堆分配，没有 `type_info` 比较 |
| 要在运行期跨越模块边界传递「什么都有可能」的值 | `any` | 这是它不易替代的位置 |

典型的该用 `any` 的场合是插件参数、脚本绑定、属性表这类
**「值的类型由运行期决定、写代码时列不全」**的地方。
如果一个 `any` 在代码里只装过两三种类型，那它应该是一个 `variant`：
后者会让「漏掉一种」变成编译错误，而 `any` 只会在运行期抛出 `bad_any_cast`。

---

# 第 6 节 `swap`、`exchange`、`as_const`

`std::swap` 的默认实现是「三次移动」，对 `std::string` 这类类型已经是搬指针的代价。
一个类如果能把交换做得更省（例如只换几个指针），就给它写一个自己的 `swap`，调用方按固定套路来写：

`实测数据`
`C++`

```cpp
/* algo_swap.cpp    编译：g++ -std=c++17 -Wall -Wextra algo_swap.cpp -o algo_swap */
#include <cstdio>
#include <string>
#include <type_traits>
#include <utility>

/* 自带 swap 的类：ADL 会找到它，比 std::swap 的「三次移动」更省 */
class Buffer {
public:
    explicit Buffer(int n) : size_(n) {}
    int size() const { return size_; }
    friend void swap(Buffer &a, Buffer &b) noexcept {
        int t = a.size_;
        a.size_ = b.size_;
        b.size_ = t;
        std::printf("  （调用的是 Buffer 自己的 swap）\n");
    }
private:
    int size_;
};

void print_const(const std::string &s) { std::printf("const 引用收到：%s\n", s.c_str()); }

int main() {
    int a = 1, b = 2;
    std::swap(a, b);
    std::printf("交换 int：%d %d\n", a, b);

    std::string s1 = "甲", s2 = "乙";
    std::swap(s1, s2);
    std::printf("交换 string：%s %s\n", s1.c_str(), s2.c_str());

    int m[3] = {1, 2, 3}, n[3] = {4, 5, 6};
    std::swap(m, n);                      /* 数组也有 swap 重载 */
    std::printf("交换数组：%d %d %d\n", m[0], m[1], m[2]);

    Buffer x(1), y(2);
    using std::swap;                      /* 先引入 std::swap，再让 ADL 挑更好的 */
    swap(x, y);
    std::printf("交换 Buffer：%d %d\n", x.size(), y.size());

    bool flag = false;
    bool old = std::exchange(flag, true);  /* 换上新值，返回旧值 */
    std::printf("exchange：旧值 %d，新值 %d\n", (int)old, (int)flag);

    std::string s = "不可改";
    print_const(std::as_const(s));
    std::printf("as_const 给的是 const 引用：%d\n",
                (int)std::is_const_v<std::remove_reference_t<decltype(std::as_const(s))>>);
    return 0;
}
```


`实测数据`
`Text`

```text
交换 int：2 1
交换 string：乙 甲
交换数组：4 5 6
  （调用的是 Buffer 自己的 swap）
交换 Buffer：2 1
exchange：旧值 0，新值 1
const 引用收到：不可改
as_const 给的是 const 引用：1
```


固定套路是先 `using std::swap;`，再**不加限定地**写 `swap(a, b)`。
这样做是因为实参依赖查找（ADL）：如果类自己提供了 `swap`，它会被优先选中；没有提供时退回 `std::swap`。
直接写 `std::swap(a, b)` 会强制用默认版本，绕开那个更省的实现。
数组也有 `swap` 重载（示例里两个 `int[3]` 被整体交换），这一点与「数组不能赋值」的直觉相反，
但它是标准提供的便利。

`std::exchange(obj, new_value)` 把新值写进去，**返回旧值**（示例里 `exchange` 那一行）。
它把「取出旧状态、写入新状态」压成一句，省掉一个临时变量，也避免了两句写反顺序的错误；
典型用场是状态机里的「取走当前状态」，以及移动构造里把源对象置空。

`std::as_const(x)` 返回 `x` 的 `const` 引用（示例最后两行），它不拷贝，返回的引用指向原对象。

| 场合 | 作用 |
|---|---|
| 重载决议 | 强制走 `const` 版本的重载 |
| 模板推导 | 阻止模板把实参推导成非 `const` 引用，也阻止 `auto` 拷贝 |
| 范围 `for` | `for (const auto &x : std::as_const(c))` 明确只要读，不触发容器的隐式拷贝 |

`exchange` 与 `as_const` 都在 `<utility>` 里，`swap` 在 `<utility>`（C++11 起）与 `<algorithm>`（旧名字）里都有。

> [!NOTE]
> **第 6 节小结**：`swap` 的写法是「`using std::swap;` 加不加限定的调用」，
> 这条套路让自定义的交换函数被 ADL 找到；
> `exchange` 用返回值代替临时变量，`as_const` 用类型表达「只读」。

---

# 第 7 节 速查表

`实测数据`

| 名字 | 一句话用途 | 典型坑 |
|---|---|---|
| `std::pair<T1, T2>` | 两个值的组合，成员是 `first`、`second` | 没有坑，大小与手写结构体一致 |
| `std::make_pair` | 由实参推导 `pair` 的类型 | 推导结果可能带引用退化，需要精确类型时显式写 |
| `std::tuple<Ts...>` | 任意多个值的组合 | 元素顺序由实现决定，不要当二进制接口 |
| `std::get<I>` / `std::get<T>` | 按下标或按类型取值 | 下标越界是编译错误；按类型取要求类型唯一 |
| `std::tie` / `std::ignore` | 拆到已有变量、跳过某一项 | `tie` 造出的是引用组成的 `tuple`，右值绑不上去 |
| 结构化绑定 `auto [a, b]` | 一步拆开 `pair`、`tuple`、数组、结构体 | 名字个数必须与元素个数相等；名字是别名不是副本 |
| `std::tuple_size` / `std::tuple_element_t` | 元素个数、第 I 项的类型 | 用在 `decltype(t)` 上，不要漏掉 `decltype` |
| `std::apply` / `std::tuple_cat` | 摊开成实参表、拼接两个 `tuple` | 拼接结果的类型会变长，元素个数用 `tuple_size` 取 |
| `std::optional<T>` | 「可能没有值」 | `*o` 不检查；`optional<T&>` 不存在；`sizeof` 比 `T` 大 |
| `std::nullopt` | 明确的「空」 | 与默认构造等价，写出来只是为了读得清 |
| `value()` / `value_or()` / `has_value()` | 三种取值方式 | `value()` 在空值上抛 `bad_optional_access` |
| `std::variant<Ts...>` | 多选一 | 默认装第一个；`get` 错类型抛异常；存在 valueless 状态 |
| `std::visit` | 按当前备选分派 | 漏掉一种备选通不过编译；visitor 的返回类型要一致 |
| `std::get_if` / `holds_alternative` | 不抛异常的两种问法 | `get_if` 收的是指针 |
| `std::monostate` | 让 `variant` 有一个「空」备选 | 它不是 `nullptr`，是一个占位类型 |
| `valueless_by_exception()` | 第三种状态 | 只判断备选会漏掉它；用 `emplace` 恢复 |
| `std::any` | 装任何可拷贝类型 | 超过 8 字节就堆分配；取错类型抛 `bad_any_cast` |
| `any_cast<T>(&a)` | 不抛异常的取值 | 返回的是指针，用之前判空 |
| `std::swap` | 交换两个值 | 类自定义了 `swap` 时，要写 `using std::swap;` 再加无限定调用 |
| `std::exchange` | 写入新值并返回旧值 | 返回的是旧值，不要把两个顺序记反 |
| `std::as_const` | 取 `const` 引用 | 它不拷贝，返回的引用指向原对象 |

---

# 附录 A 复现本章节实测

主线的程序都是完整可编译的单文件，源码就在正文里，一律用
`g++ -std=c++17 -Wall -Wextra <名字>.cpp -o <名字>` 编译（`g++` 15.2.0，MinGW-w64，x86-64，全部程序无警告）。

`实测数据`

| 在哪一节 | 程序 | 演示什么 |
|---|---|---|
| 第 1 节 | `pair_basic.cpp`、`pair_size.cpp` | 构造、比较、交换；大小与两个成员的距离 |
| 第 2 节 | `tuple_basic.cpp`、`tuple_bind.cpp`、`tuple_sizeof.cpp` | 两种取值与 `tie`；结构化绑定；大小与元素偏移 |
| 第 3 节 | `optional_forms.cpp`、`optional_api.cpp`、`optional_empty_throw.cpp`、`optional_size.cpp`、`optional_ref_workaround.cpp` | 四种写法；接口；空值取值的后果；大小；`T*` 与 `reference_wrapper` 两种替代写法 |
| 第 4 节 | `variant_basic.cpp`、`variant_visit.cpp`、`variant_get_throw.cpp`、`variant_valueless.cpp`、`variant_size.cpp` | 取值；`visit`；错类型；valueless；大小 |
| 第 5 节 | `any_basic.cpp`、`any_cost.cpp`、`any_cast_throw.cpp` | 接口；堆分配；取错类型 |
| 第 6 节 | `algo_swap.cpp` | `swap` 与 ADL、`exchange`、`as_const` |

## A.1 环境与两处测量方法

`实测数据`

| 项目 | 取值 |
|---|---|
| 编译器（主线） | `g++` 15.2.0（MinGW-w64，x86-64） |
| 语言标准 | `-std=c++17` |
| 警告选项 | `-Wall -Wextra` |
| 对照编译器 | WSL Ubuntu 24.04 的 `g++` 13.3.0（同样是 libstdc++） |
| 对照命令 | `wsl -d Ubuntu -e bash -c "g++ -std=c++17 size_cross.cpp -o sc && ./sc"` |

`std::any` 的堆分配次数是把全局 `operator new` 替换成一个计数版本测出来的（源码见 `any_cost.cpp`），
它统计的是 `any` 构造期间的全部分配，因此 `std::string` 那一行的数字包含了字符串自身可能的分配。
跨实现的对照用了一个只打印 `sizeof` 与偏移的小程序 `size_cross.cpp`，它不在正文里。

## A.2 报错原文与节选

第 2.2、3.5、4.3 小节里的报错原文**只摘出了与本例有关的那几行**，去掉了模板实例化链与库内部路径。
出错的那几行代码以节选形式给出（对应的文件是 `sb_arity_fail.cpp`、`optional_ref_fail.cpp`、`variant_dup_get_fail.cpp`），
补上 `#include` 与 `int main` 就能复现报错。
正文里的程序输出都是**整份输出**，没有删行。

---

# 附录 B 相关文档

| 文档 | 关系 |
|---|---|
| 《05-类与面向对象/12-模板的高阶使用.md》第 6.6 小节 | **前置**：类型列表与 `std::tuple` 的对应物 |
| 《05-类与面向对象/11-模板.md》第 4.8 小节 | **前置**：类模板实参推导（`overloaded` 的推导指引） |
| 《05-类与面向对象/05-拷贝与移动.md》第 4 节 | **前置**：移动构造与 `noexcept` |
| 《05-类与面向对象/07-继承.md》第 4 节 | **前置**：继承与名字隐藏 |
| 《05-类与面向对象/08-多态：重载、虚函数与它们的分工.md》第 6 节 | 相关：虚函数的代价，与 `variant` 的取舍对照 |
| 《04-语法/09-结构体、联合体与 enum.md》第 2 节 | **前置**：`union` 共享内存的机制 |
| 《04-语法/09-结构体、联合体与 enum.md》第 1.5 小节 | **前置**：成员顺序与填充 |
| 《04-语法/13-异常.md》第 3 节 | **前置**：异常的捕获与 `what()` |
| 《06-标准库/B-09-工具类（下）：type_traits 与 concepts.md》第 5 节 | **后续**：`typeid` 与 `type_info` 的边界，`any` 的运行期检查靠它 |
| 《06-标准库/B-09-工具类（下）：type_traits 与 concepts.md》第 2.2 小节 | **后续**：`is_object_v<int&>` 为假，`optional<T&>` 因此不存在 |
| 《06-标准库/B-09-工具类（下）：type_traits 与 concepts.md》第 6 节 | **后续**：C++20 标准概念库 |
| 《06-标准库/B-01-输入输出：iostream.md》第 6.2 小节 | 相关：用 `ostringstream` 拼字符串（第 4.2 小节用到） |
| 《06-标准库/B-01-输入输出：iostream.md》第 3 节 | 相关：`<iomanip>` 的格式化件 |
| 【待补：08-高阶数据结构/】 | **后续**：`std::map` 与 `pair`、容器与迭代器 |

---

配套示例见 [`B-examples/06-standard-library/08-cpp-config-parser/`](../B-examples/06-standard-library/08-cpp-config-parser/)，配套练习见 [`C-templates/06-standard-library/08-cpp-config-parser/`](../C-templates/06-standard-library/08-cpp-config-parser/)。
